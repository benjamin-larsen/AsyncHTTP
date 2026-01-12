#pragma once

// Globals
#include <stdio.h>
#include <winsock2.h>

// Local External
#include "../io.h"
#include "../safe_pointer.c"

// Local Internal
#include "./consts.h"

#define RECV_LEN 1024

// constraint: a tcpConn shall only have one ongoing async I/O at once, as I/O operations rely on the conn structure
// like for example for receive buffer
struct tcpConn {
    SOCKET sock;
    unsigned char *recvBuf;
    uint32_t recvOffset;
    WSABUF WSArecvBuf;
    DWORD flags;
};

void QueueRead(struct shared_retainer connOPRetainer) {
    struct tcpConn* conn = connOPRetainer.ptr;

    if (conn->recvOffset >= RECV_LEN) {
        printf("Receive Buffer went to max\n");
        ReleaseShared(&connOPRetainer);
        return;
    }

    conn->WSArecvBuf = (WSABUF){
        .len = RECV_LEN - conn->recvOffset,
        .buf = (unsigned char *)conn->recvBuf + conn->recvOffset,
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

struct tcpString {
    unsigned char *buf;
    uint32_t len;
};

const struct tcpString nullString = { .buf = NULL, .len = 0 };

struct tcpString GetLine(struct tcpConn* conn, uint32_t offset) {
    if (offset >= conn->recvOffset) return nullString;

    unsigned char *buf = (unsigned char*)conn->recvBuf + offset;
    unsigned char *bounds = memchr(buf, '\n', conn->recvOffset - offset);

    if (bounds == NULL || bounds < buf) return nullString;

    uint32_t size = (char*)bounds - (char*)buf;

    return (struct tcpString){ .buf = buf, .len = size };
}

void ProcessLines(struct tcpConn* conn) {
    struct tcpString line;
    uint32_t offset = 0;

    while ((line = GetLine(conn, offset)).buf != NULL) {
        printf("Line: %.*s\n", line.len, line.buf);
        offset += line.len + 1;
    }

    uint32_t remaining = conn->recvOffset - offset;

    if (remaining == 0) {
        conn->recvOffset = 0;
        return;
    }

    printf("Remaining: %u\n", remaining);

    memmove(conn->recvBuf, (unsigned char *)conn->recvBuf + offset, remaining);
    conn->recvOffset = remaining;
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

    ProcessLines(conn);
    conn->recvOffset += bytesTransferred;

    QueueRead(connOPRetainer);
}

void CloseConn(struct tcpConn* conn) {
    closesocket(conn->sock);
    free(conn->recvBuf);
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
    struct tcpConn* conn = connRetainer.ptr;
    conn->sock = sock;
    conn->recvBuf = calloc(RECV_LEN, sizeof(char));
    conn->recvOffset = 0;
    conn->flags = 0;

    struct shared_retainer connOPRetainer = RetainShared(connRetainer);

    if (connOPRetainer.ptr == NULL) {
        fprintf(stderr, "error: Failed to retain connection pointer.\n");
        return;
    }

    QueueRead(connOPRetainer);
}