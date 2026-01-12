#pragma once

// Globals
#include <stdio.h>
#include <winsock2.h>

// Local External
#include "../io.h"
#include "../safe_pointer.c"
#include "../tcp_common/conn.c"

// Local Internal
#include "./consts.h"

#define RECV_LEN 1024

// constraint: a tcpConn shall only have one ongoing async I/O at once, as I/O operations rely on the conn structure
// like for example for receive buffer
struct tcpConn {
    struct tcpConnCommon common;
    SOCKET sock;
    WSABUF WSArecvBuf;
    DWORD flags;
};

void QueueRead(struct shared_retainer connOPRetainer) {
    struct tcpConn* conn = connOPRetainer.ptr;

    if (conn->common.recvOffset >= RECV_LEN) {
        printf("Receive Buffer went to max\n");
        ReleaseShared(&connOPRetainer);
        return;
    }

    conn->WSArecvBuf = (WSABUF){
        .len = RECV_LEN - conn->common.recvOffset,
        .buf = (unsigned char *)conn->common.recvBuf + conn->common.recvOffset,
    };

    struct io_op *op = CreateIOOperation(IO_READ, (union op_data){ .ptr = SharedFromRetainer(connOPRetainer) });

    int err = WSARecv(
        conn->sock,
        &conn->WSArecvBuf,
        1,
        NULL,
        &conn->flags,
        (OVERLAPPED*)op,
        NULL
    );

    if (err == SOCKET_ERROR) {
        int wsaErr = WSAGetLastError();

        if (wsaErr != WSA_IO_PENDING) {
            printf("Instant Read Error: %i\n", wsaErr);
            ReleaseShared(&connOPRetainer);
            free(op);
        }
    }
}

void ProcessRead(const struct io_op op, DWORD bytesTransferred) {
    __attribute__((__cleanup__(ReleaseShared))) struct shared_retainer connRetainer = RetainerFromShared(op.data.ptr);
    struct tcpConn* conn = connRetainer.ptr;

    if (bytesTransferred == 0) return;

    struct shared_retainer connOPRetainer = RetainShared(connRetainer);

    if (connOPRetainer.ptr == NULL) {
        fprintf(stderr, "error: Failed to retain connection pointer.\n");
        return;
    }

    conn->common.recvOffset += bytesTransferred;

    if (!ProcessLines(&conn->common)) {
        ReleaseShared(&connOPRetainer);
        return;
    }

    QueueRead(connOPRetainer);
}

    QueueRead(connOPRetainer);
}

void CloseConn(struct tcpConn* conn) {
    closesocket(conn->sock);
    CleanupCommonConn(&conn->common);
}

void CleanupConn(void *ptr) {
    CloseConn(ptr);
}

void StartClient(const struct io_handler *ioHandler, SOCKET sock) {
    unsigned long nonBlocking = 1;
    int err = ioctlsocket(sock, FIONBIO, &nonBlocking);

    if (err == SOCKET_ERROR) {
        fprintf(stderr, "error: Failed to mark Connection as Non-Blocking: %i\n", WSAGetLastError());
        closesocket(sock); // ignore error, this should be non-blocking because of SO_DONTLINGER.
        return;
    }

    HANDLE ioPort = w32_CreateIOPort(ioHandler, (HANDLE)sock);

    if (ioPort != ioHandler->iocp_handle) {
        fprintf(stderr, "error: Connection IOCP Port is invalid.\n");
        closesocket(sock); // ignore error, this should be non-blocking because of SO_DONTLINGER.
        return;
    }

    __attribute__((__cleanup__(ReleaseShared))) struct shared_retainer connRetainer = MakeShared(sizeof(struct tcpConn), CleanupConn);
    struct tcpConn* conn = connRetainer.ptr;
    conn->sock = sock;
    SetupCommonConn(&conn->common, RECV_LEN);
    conn->flags = 0;

    struct shared_retainer connOPRetainer = RetainShared(connRetainer);

    if (connOPRetainer.ptr == NULL) {
        fprintf(stderr, "error: Failed to retain connection pointer.\n");
        return;
    }

    QueueRead(connOPRetainer);
}