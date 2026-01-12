#pragma once

#include <stdint.h>
#include <string.h>
#include "../string.c"

enum tcpState {
    REQUEST_LINE = 0,
    HEADER = 1,
    BODY = 2
};

struct tcpConnCommon {
    unsigned char *recvBuf;
    uint32_t recvOffset;
    enum tcpState state;
};

void SetupCommonConn(struct tcpConnCommon *conn, uint32_t recvLen) {
    conn->recvBuf = calloc(recvLen, sizeof(char));
    conn->recvOffset = 0;
    conn->state = REQUEST_LINE;
}

void CleanupCommonConn(struct tcpConnCommon *conn) {
    free(conn->recvBuf);
}

bool GetLine(struct tcpConnCommon* conn, uint32_t offset, union string *str) {
    if (offset >= conn->recvOffset) return false;

    unsigned char *buf = (unsigned char*)conn->recvBuf + offset;
    unsigned char *bounds = memchr(buf, '\n', conn->recvOffset - offset);

    if (bounds == NULL || bounds < buf) return false;

    uint32_t size = (char*)bounds - (char*)buf;

    if (size > maxShortString || size == 0) {
        *str = (union string){ .longStr = { .buf = buf, .len = size } };
        return true;
    } else {
        struct shortString shortStr = { .len = size };
        memcpy(&shortStr.buf, buf, size);

        *str = (union string){ .shortStr = shortStr };
        return true;
    }
}

void ProcessLines(struct tcpConnCommon* conn) {
    union string line;
    uint32_t offset = 0;

    while (conn->state != BODY && GetLine(conn, offset, &line)) {
        offset += GetStringLen(&line) + 1;

        if (conn->state == REQUEST_LINE) {
            conn->state = HEADER;
            printf("Request Line: %.*s\n", GetStringLen(&line), GetStringBuf(&line));
        } else {
            printf("Header Line: %.*s\n", GetStringLen(&line), GetStringBuf(&line));
        }
    }

    uint32_t remaining = conn->recvOffset - offset;

    if (remaining == 0) {
        conn->recvOffset = 0;
        return;
    }

    memmove(conn->recvBuf, (unsigned char *)conn->recvBuf + offset, remaining);
    conn->recvOffset = remaining;
}