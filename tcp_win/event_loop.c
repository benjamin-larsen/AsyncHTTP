#pragma once

// Globals
#include <windows.h>
#include <stdio.h>

// Local External
#include "../io.h"
#include "../safe_pointer.c"
#include "../atomics.c"

// Local Internal
#include "../tcp_common/consts.h"
#include "./conn.c"
#include "./io_async.c"

DWORD StartWorker(void *param) {
    struct shared_ptr *ptr = param;
    __attribute__((__cleanup__(ReleaseShared))) struct shared_retainer ioHandler_retainer = RetainerFromShared(ptr);

    if (ioHandler_retainer.ptr == NULL) {
        fprintf(stderr, "panic: Error restoring retainer IO Handler for thread.\n");
        abort();
    }

    struct io_handler *ioHandler = ioHandler_retainer.ptr;

    for (;;) {
        bool ok;
        DWORD bytesTransferred;
        uint32_t err;
        struct io_op *op = RunIO(ioHandler, &ok, &bytesTransferred, &err);

        if (op == NULL) {
            if (err == IO_ERR_CLOSED) {
                printf("RunIO finished\n");
                return 0;
            }

            fprintf(stderr, "panic: RunIO failed: %u\n", err);
            abort();
        }

        {
            struct async_state *asyncState = op->data;
            struct io_async_state *ioAsyncState = asyncState->state;

            ioAsyncState->ok = ok;
            ioAsyncState->bytesTransferred = bytesTransferred;

            ResumeFromIO(asyncState);
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