#pragma once

// Globals
#include <stdio.h>
#include <winsock2.h>

// Local External
#include "../io.h"
#include "../safe_pointer.c"

// Local Internal
#include "./consts.h"

struct tcpConn {
    SOCKET sock;
    WSABUF testBuf;
    DWORD flags;
};

void QueueRead(struct shared_retainer connOPRetainer) {
    struct shared_ptr *sharedConn = connOPRetainer.descriptor;
    struct io_op *op = CreateIOOperation(IO_READ, (union op_data){ .ptr = sharedConn });

    struct tcpConn* conn = sharedConn->ptr;

    int err = WSARecv(
        conn->sock,
        &conn->testBuf,
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
        }
    }
}

void ProcessRead(struct tcpConn* conn, const struct io_op op, DWORD bytesTransferred) {
    __attribute__((__cleanup__(ReleaseShared))) struct shared_retainer connRetainer = RetainerFromShared(op.data.ptr);

    if (bytesTransferred == 0) return;

    struct shared_retainer connOPRetainer = RetainShared(connRetainer);

    if (connOPRetainer.descriptor == NULL) {
        fprintf(stderr, "error: Failed to retain connection pointer.\n");
        return;
    }

    QueueRead(connOPRetainer);
}

void CloseConn(struct tcpConn* conn) {
    printf("Flags: %lu, Closing buf: %p\n", conn->flags, conn->testBuf.buf);
    closesocket(conn->sock);
    free(conn->testBuf.buf);
    // should prob cancel or use shared_ptr just incase we got queued using the conn pointer
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
    struct tcpConn* conn = connRetainer.descriptor->ptr;
    conn->sock = sock;
    char *underlyingTestBuf = calloc(50, sizeof(char));
    conn->testBuf = (WSABUF){
        .len = 50,
        .buf = underlyingTestBuf
    };
    conn->flags = 0;

    struct shared_retainer connOPRetainer = RetainShared(connRetainer);

    if (connOPRetainer.descriptor == NULL) {
        fprintf(stderr, "error: Failed to retain connection pointer.\n");
        return;
    }

    QueueRead(connOPRetainer);
}