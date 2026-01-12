#pragma once

#include <stdint.h>
#include <string.h>
#include "../string.c"
#include "./http.c"

enum tcpState {
    // When expecting request line (new request)
    RECV_REQUEST_LINE = 0,
    // When expecting header
    RECV_HEADER = 1,

    // might replace these with user-specified internal state machine

    // When expecting body
    RECV_BODY = 2,
    // When sending response line
    SEND_RESPONSE_LINE = 3,
    // When sending header
    SEND_HEADER = 4,
    // When body is sent as one whole by the write loop
    SEND_BODY = 5,
    // When body is sent as fragmented stream, rather than one.
    SEND_BODY_STREAM = 6
};

struct tcpConnCommon {
    unsigned char *recvBuf;
    uint32_t recvOffset;
    enum tcpState state;
    struct HTTPRequest currentReq;
};

void SetupCommonConn(struct tcpConnCommon *conn, uint32_t recvLen) {
    conn->recvBuf = calloc(recvLen, sizeof(char));
    conn->recvOffset = 0;
    conn->state = RECV_REQUEST_LINE;
}

void CleanupCommonConn(struct tcpConnCommon *conn) {
    free(conn->recvBuf);
    CleanupHTTPRequest(&conn->currentReq);
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
        memcpy(shortStr.buf, buf, size);

        *str = (union string){ .shortStr = shortStr };
        return true;
    }
}

void TrimChar(union string *line, uint8_t c) {
    if (line->shortStr.len > 0) {
        if (line->shortStr.buf[line->shortStr.len - 1] == c) {
            line->shortStr.len -= 1;
        }
    } else {
        if (line->longStr.buf[line->longStr.len - 1] == c) {
            line->longStr.len -= 1;
        }
    }
}

void TrimLine(union string *line) {
    TrimChar(line, '\n');
    TrimChar(line, '\r');
}

// Returns whether process lines was succesful
bool ProcessLines(struct tcpConnCommon* conn) {
    union string line;
    uint32_t offset = 0;

    while ((conn->state == RECV_HEADER || conn->state == RECV_REQUEST_LINE) && GetLine(conn, offset, &line)) {
        offset += GetStringLen(&line) + 1;
        TrimLine(&line);

        if (conn->state == RECV_REQUEST_LINE) {
            struct HTTPRequest req;
            if (!DecodeRequestLine(line, &req)) return false;

            if (
                !StringEquals(req.version, FromCStrUnsafe("HTTP/1.0")) &&
                !StringEquals(req.version, FromCStrUnsafe("HTTP/1.1"))
            ) return false;

            // Copy the strings, because the current string reference a soon to be teared down receive buffer.
            req.method = CopyString(req.method);
            req.path = CopyString(req.path);
            req.version = CopyString(req.version);

            conn->state = RECV_HEADER;
            conn->currentReq = req;
        } else {
            if (GetStringLen(&line) == 0) {
                // check if actually expecting body
                conn->state = RECV_BODY;
                printf("Request Done\n");
                break;
            } else {
                printf("Header Line: %.*s\n", GetStringLen(&line), GetStringBuf(&line));
            }
        }
    }

    uint32_t remaining = conn->recvOffset - offset;

    if (remaining == 0) {
        conn->recvOffset = 0;
        return true;
    }

    memmove(conn->recvBuf, (uint8_t *)conn->recvBuf + offset, remaining);
    conn->recvOffset = remaining;

    return true;
}