#pragma once

// Globals
#include <windows.h>
#include <stdio.h>

// Local External
#include "../io.h"
#include "../safe_pointer.c"

// Local Internal
#include "./consts.h"
#include "./conn.c"

void HandleTCP_OP(const struct io_handler *ioHandler, const struct io_op op, DWORD bytesTransferred) {
    switch (op.type) {
        case IO_STARTCLIENT:
            return StartClient(ioHandler, (SOCKET)op.data.ptr);
        case IO_READ:
            return ProcessRead(op, bytesTransferred);
        default:
            printf("error: Unknown Operation Type %i (processor)\n", op.type);
    }
}

void HandleTCP_OPErr(const struct io_handler *ioHandler, const struct io_op op) {
    switch (op.type) {
        case IO_READ: {
            struct shared_retainer retainer = RetainerFromShared(op.data.ptr);
            ReleaseShared(&retainer);
            return;
        }
        default:
            printf("error: Unknown Operation Type %i (error handler)\n", op.type);
    }
}

DWORD StartWorker(void *param) {
    struct shared_ptr *ptr = param;
    __attribute__((__cleanup__(ReleaseShared))) struct shared_retainer ioHandler_retainer = RetainerFromShared(ptr);

    if (ioHandler_retainer.descriptor == NULL) {
        fprintf(stderr, "panic: Error restoring retainer IO Handler for thread.\n");
        abort();
    }

    struct io_handler *ioHandler = ioHandler_retainer.descriptor->ptr;

    for (;;) {
        bool ok;
        DWORD bytesTransferred;
        struct io_op *op = RunIO(ioHandler, &ok, &bytesTransferred);

        if (op == NULL) {
            fprintf(stderr, "panic: RunIO failed\n");
            abort();
        }

        printf("OK: %u, %i, %lu\nBytes Read: %lu\nOP Type: %u\nData:\n  void *ptr: %p\n  uint32_t u32: %u\n  uint64_t u64: %llu\n", ok, WSAGetLastError(), GetLastError(), bytesTransferred, op->type, op->data.ptr, op->data.u32, op->data.u64);

        if (ok) {
            HandleTCP_OP(ioHandler, *op, bytesTransferred);
        } else {
            HandleTCP_OPErr(ioHandler, *op);
        }

        free(op);
    }

    return 0;
}

void SpawnWorker(struct shared_ptr *ptr) {
    HANDLE thread = CreateThread(
        NULL,
        0,
        StartWorker,
        ptr,
        0,
        NULL
    );

    if (thread == NULL) {
        fprintf(stderr, "panic: CreateThread failed: %lu\n", GetLastError());
        abort();
    }
}