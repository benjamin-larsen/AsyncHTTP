#include <windows.h>
#include <stdio.h>
#include <stdint.h>

// "WIOP" (Windows Operation) in hex
#define IOOperationMagic 0x57494f50

struct io_handler {
    HANDLE iocp_handle;
};

// TODO: Make Global, not OS-specific
union op_data {
    void *ptr;
    uint32_t u32;
    uint64_t u64;
};

struct io_op {
    OVERLAPPED overlapped;
    uint32_t magic;
    uint32_t type;
    union op_data data;
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

struct io_op *CreateIOOperation(uint32_t type, union op_data data) {
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

struct io_op *RunIO(const struct io_handler *ioHandler, bool *okOut, DWORD *bytesTransferred) {
    if (ioHandler == NULL) {
        // return some error
        return NULL;
    }

    if (ioHandler->iocp_handle == NULL) {
        // return some error
        return NULL;
    }

    if (okOut == NULL || bytesTransferred == NULL) {
        return NULL;
    }

    Attempt:
    ULONG_PTR completionKey;
    LPOVERLAPPED overlapped;

    *okOut = GetQueuedCompletionStatus(
        ioHandler->iocp_handle,
        bytesTransferred,
        &completionKey,
        &overlapped,
        INFINITE
    );

    if (overlapped == NULL) goto Attempt;

    struct io_op *op = (struct io_op*)overlapped;

    if (op->magic != IOOperationMagic) {
        fprintf(stderr, "panic: received overlapped from IOCP that isn't a io_op struct.\n");
        abort();
    }

    return op;
}