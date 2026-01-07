#include <stdatomic.h>
#include <winsock2.h>
#include <stdio.h>
#include "./io.h"

#define IO_STARTCLIENT 0
#define IO_READ        1
#define IO_WRITE       2

#define WSA_UNINITIALIZED 0
#define WSA_STARTING      1
#define WSA_INITIALIZED   2

WSADATA wsaData;
atomic_int WSASetupPhase = WSA_UNINITIALIZED;

void InitWSA() {
    int prevState = WSA_UNINITIALIZED;
    if (!atomic_compare_exchange_weak(&WSASetupPhase, &prevState, WSA_STARTING) || prevState != WSA_UNINITIALIZED) {
        // spinlock until the thread that is initializing WSA is finished.
        while (prevState == WSA_STARTING) {
            prevState = atomic_load(&WSASetupPhase);
        }
        return;
    }

    int err = WSAStartup(
        MAKEWORD(2, 2),
        &wsaData
    );

    if (err != 0) {
        fprintf(stderr, "panic: WSAStartup failed: %i\n", err);
        abort();
    }

    atomic_store(&WSASetupPhase, WSA_INITIALIZED);
}

DWORD StartWorker(void *param) {
    struct io_handler *ioHandler = param;
    for (;;) {
        struct io_op *op = RunIO(ioHandler);
        printf("OP Type: %u\n", op->type);
        free(op);
    }

    return 0;
}

void SpawnWorker(const struct io_handler *ioHandler) {
    HANDLE thread = CreateThread(
        NULL,
        0,
        StartWorker,
        ioHandler,
        0,
        NULL
    );

    if (thread == NULL) {
        fprintf(stderr, "panic: CreateThread failed: %lu\n", GetLastError());
        abort();
    }
}

DWORD CountLogicalProcessors() {
    SYSTEM_INFO sysInfo;
    GetSystemInfo(&sysInfo);

    return sysInfo.dwNumberOfProcessors;
}

void StartServer(const char *addr, uint16_t port) {
    InitWSA();

    struct io_handler ioHandler = CreateIOHandler();

    for (int i = 0; i < CountLogicalProcessors(); i++) {
        SpawnWorker(&ioHandler);
    }

    SOCKET serverSock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

    if (serverSock == INVALID_SOCKET) {
        fprintf(stderr, "panic: Server Socket creation failed: %i\n", WSAGetLastError());
        abort();
    }

    bool reuseAddr = false;
    int err = setsockopt(serverSock, SOL_SOCKET, SO_REUSEADDR, (char *)&reuseAddr, sizeof(reuseAddr));

    if (err == SOCKET_ERROR) {
        fprintf(stderr, "panic: Server Socket setoption (Reuse Address) failed: %i\n", WSAGetLastError());
        abort();
    }

    struct sockaddr_in endpoint = {
        .sin_family = AF_INET,
        .sin_port = htons(port),
        .sin_addr.s_addr = inet_addr(addr),
    };

    err = bind(serverSock, (struct sockaddr *) &endpoint, sizeof(endpoint));

    if (err == SOCKET_ERROR) {
        fprintf(stderr, "panic: Server Socket bind failed: %i\n", WSAGetLastError());
        abort();
    }

    err = listen(serverSock, SOMAXCONN);

    if (err == SOCKET_ERROR) {
        fprintf(stderr, "panic: Server Socket listen failed: %i\n", WSAGetLastError());
        abort();
    }

    printf("Listening\n");

    for (;;) {
        // for future we might want the IP Address
        SOCKET client = accept(serverSock, NULL, NULL);

        if (client == INVALID_SOCKET) {
            fprintf(stderr, "Server accept failed: %i\n", WSAGetLastError());
            continue;
        }

        printf("Client connect\n");

        struct io_op *op = CreateIOOperation(IO_STARTCLIENT, NULL);
        ResolveIOOperation(&ioHandler, op);
    }
}