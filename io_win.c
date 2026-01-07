#include <windows.h>
#include <stdio.h>
#include <stdint.h>

// "WINAsync" in hex
#define IOOperationMagic 0x57494e4173796e63

struct io_handler {
    HANDLE iocp_handle;
};

struct io_op {
    OVERLAPPED overlapped;
    uint64_t magic;
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

/*
const HANDLE handle = CreateIoCompletionPort(
        fHandle,
        ioHandler->iocp_handle,
        0,
        0
    );
*/

struct io_op *CreateIOOperation() {
    struct io_op *op = calloc(1, sizeof(struct io_op));
    op->magic = IOOperationMagic;

    return op;
}

HANDLE w32_CreateIOPort(const struct io_handler *ioHandler, const HANDLE fHandle) {
    return CreateIoCompletionPort(
        fHandle,
        ioHandler->iocp_handle,
        0,
        0
    );
}

void PrintHex(unsigned char *bytes, size_t size) {
    for (size_t i = 0; i < size; i++) {
        printf("%02X", bytes[i]);
    }
    printf("\n");
}

void RunIO(const struct io_handler *ioHandler) {
    if (ioHandler == NULL) {
        // return some error
        return;
    }

    if (ioHandler->iocp_handle == NULL) {
        // return some error
        return;
    }

    DWORD bytesTransferred;
    ULONG_PTR completionKey;
    LPOVERLAPPED overlapped;

    bool ok = GetQueuedCompletionStatus(
        ioHandler->iocp_handle,
        &bytesTransferred,
        &completionKey,
        &overlapped,
        INFINITE
    );

    if (!ok || overlapped == NULL) {
        // do continue
        return;
    }

    struct io_op *op = (struct io_op*)overlapped;

    if (op->magic != IOOperationMagic) {
        // panic: received overlapped that isn't an io_op.
        fprintf(stderr, "panic: received overlapped from IOCP that isn't a io_op struct.\n");
        abort();
    }

    PrintHex((unsigned char *)op, sizeof(struct io_op));
}