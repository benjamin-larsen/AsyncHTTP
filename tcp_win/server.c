#pragma once

// Globals
#include <stdatomic.h>
#include <winsock2.h>
#include <stdio.h>

// Local External
#include "../safe_pointer.c"
#include "../io.h"

// // Local Internal
#include "./event_loop.c"
#include "./consts.h"

#define WSA_UNINITIALIZED 0
#define WSA_STARTING      1
#define WSA_INITIALIZED   2

WSADATA wsaData;
atomic_int WSASetupPhase = WSA_UNINITIALIZED;

// We pass in SOCKET cast as a void *ptr, currently SOCKET is a UINT_PTR, but incase Microsoft ever decides to change that we must have this.
static_assert(sizeof(void *) == sizeof(SOCKET),
    "SOCKET is not the size of Pointer. Contact Developer (benjamin-larsen) for patch.");

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

DWORD CountLogicalProcessors() {
    SYSTEM_INFO sysInfo;
    GetSystemInfo(&sysInfo);

    return sysInfo.dwNumberOfProcessors;
}

void StartServer(const char *addr, uint16_t port) {
    InitWSA();

    __attribute__((__cleanup__(ReleaseShared))) struct shared_retainer ioHandler_retainer = MakeShared(sizeof(struct io_handler), NULL);
    struct io_handler *ioHandler = ioHandler_retainer.descriptor->ptr;

    *ioHandler = CreateIOHandler();

    for (int i = 0; i < CountLogicalProcessors(); i++) {
        if (RetainShared(ioHandler_retainer).descriptor == NULL) {
            fprintf(stderr, "panic: Error retaining IO Handler for thread.\n");
            abort();
        }

        SpawnWorker(ioHandler_retainer.descriptor);
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

    bool dontLinger = true;
    err = setsockopt(serverSock, SOL_SOCKET, SO_DONTLINGER, (char *)&dontLinger, sizeof(dontLinger));

    if (err == SOCKET_ERROR) {
        fprintf(stderr, "panic: Server Socket setoption (Don't Linger / Non-Blocking Close) failed: %i\n", WSAGetLastError());
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

        struct io_op *op = CreateIOOperation(IO_STARTCLIENT, (union op_data){
            .ptr = (void *)client
        });
        ResolveIOOperation(ioHandler, op);
    }
}