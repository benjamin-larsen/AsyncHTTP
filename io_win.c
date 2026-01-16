#include <windows.h>
#include <stdio.h>
#include <stdint.h>
#include "./atomics.c"

// "WIOP" (Windows Operation) in hex
#define IOOperationMagic 0x57494f50

struct io_handler {
    HANDLE iocp_handle;
};

void CloseIOHandler(struct io_handler *ioHandler) {
    if (ioHandler == NULL) return;

    CloseHandle(ioHandler->iocp_handle);
    ioHandler->iocp_handle = NULL;
}

void CleanupIOHandler(void *ioHandler) {
    CloseIOHandler(ioHandler);
}

struct io_op {
    OVERLAPPED overlapped;
    uint32_t magic;
    uint32_t type;
    void *data;
};

struct io_handler CreateIOHandler() {
    const HANDLE handle = CreateIoCompletionPort(
        INVALID_HANDLE_VALUE,
        NULL,
        0,
        0
    );

    return (struct io_handler){
        .iocp_handle = handle
    };
}

bool IsValidIOHandler(const struct io_handler *ioHandler) {
    if (ioHandler == NULL) return false;

    return ioHandler->iocp_handle != NULL;
}

struct io_op *CreateIOOperation(uint32_t type, void *data) {
    struct io_op *op = calloc(1, sizeof(struct io_op));
    op->magic = IOOperationMagic;
    op->type = type;
    op->data = data;

    return op;
}

bool ResolveIOOperation(const struct io_handler *ioHandler, const struct io_op *op) {
    return PostQueuedCompletionStatus(ioHandler->iocp_handle, 0, 0, (OVERLAPPED*)op);
}

HANDLE w32_CreateIOPort(const struct io_handler *ioHandler, const HANDLE fHandle) {
    return CreateIoCompletionPort(
        fHandle,
        ioHandler->iocp_handle,
        0,
        0
    );
}

#define IO_ERR_NULL_HANDLER 0
#define IO_ERR_NULL_OUTPUT 1
#define IO_ERR_CLOSED 2

struct io_op *RunIO(const struct io_handler *ioHandler, bool *okOut, DWORD *bytesTransferred, uint32_t *error) {
    if (ioHandler == NULL) {
        if (error != NULL) *error = IO_ERR_NULL_HANDLER;
        return NULL;
    }

    if (ioHandler->iocp_handle == NULL) {
        if (error != NULL) *error = IO_ERR_CLOSED;
        return NULL;
    }

    if (okOut == NULL || bytesTransferred == NULL) {
        if (error != NULL) *error = IO_ERR_CLOSED;
        return NULL;
    }

    Attempt:
    ULONG_PTR completionKey;
    LPOVERLAPPED overlapped;

    bool ok = GetQueuedCompletionStatus(
        ioHandler->iocp_handle,
        bytesTransferred,
        &completionKey,
        &overlapped,
        INFINITE
    );

    *okOut = ok;

    if (overlapped == NULL) {
        DWORD winErr = GetLastError();
        if (ok == false && (winErr == ERROR_ABANDONED_WAIT_0 || winErr == ERROR_INVALID_HANDLE)) {
            if (error != NULL) *error = IO_ERR_CLOSED;
            return NULL;
        }

        goto Attempt;
    }

    struct io_op *op = (struct io_op*)overlapped;

    if (op->magic != IOOperationMagic) {
        fprintf(stderr, "panic: received overlapped from IOCP that isn't a io_op struct.\n");
        abort();
    }

    return op;
}