#include <stdio.h>
#include "./io.h"

int main(void) {
    struct io_handler ioHandler = CreateIOHandler();

    if (!IsValidIOHandler(&ioHandler)) {
        printf("IOCP failed: %lu\n", GetLastError());
        return 1;
    }

    printf("IOHandler: %p\n", ioHandler.iocp_handle);

    HANDLE file = CreateFile(
        "asyncio-test.txt",
        GENERIC_READ | GENERIC_WRITE,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        NULL,
        CREATE_ALWAYS,
        FILE_FLAG_OVERLAPPED,
        NULL
    );

    if (file == INVALID_HANDLE_VALUE) {
        printf("Create File failed: %lu\n", GetLastError());
        return 1;
    }

    printf("File: %p\n", file);

    HANDLE port = w32_CreateIOPort(&ioHandler, file);

    if (port == NULL) {
        printf("File Port: %lu\n", GetLastError());
        return 1;
    }

    printf("File Port: %p\n", port);

    struct io_op *op = CreateIOOperation();

    char data[] = "Hello World!";

    bool ok = WriteFile(
        file,
        data,
        sizeof(data),
        NULL,
        (OVERLAPPED*)(op)
    );

    if (!ok) {
        DWORD err = GetLastError();

        if (err != ERROR_IO_PENDING) {
            printf("Write Error: %lu\n", err);
            return 1;
        }
    }

    RunIO(&ioHandler);

    return 0;
}