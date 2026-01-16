#pragma once

#include <winsock2.h>

#include "../tcp_common/conn.c"

#include "../state_machine.c"
#include "../io.h"
#include "./io_async.c"
#include "../tcp_common/consts.h"

#define RECV_LEN 1024

enum connStage {
    SetupConn,
    ConnRead,
    ConnProcess,
};

struct connState {
    struct io_async_state io_state;

    struct tcpConnCommon common;
    SOCKET sock;
    struct io_handler *io_handler;
    enum connStage stage;
    DWORD flags;
    WSABUF WSArecvBuf;
};

struct connSetupParams {
    struct io_handler *io_handler;
    SOCKET sock;
};

// Params is a stack pointer from Event Loop
void *connConstructor(void *param) {
    struct connSetupParams params = *(struct connSetupParams *)param;

    struct connState *state = calloc(1, sizeof(struct connState));

    state->io_state = nullIOAsyncState;
    state->sock = params.sock;
    state->io_handler = params.io_handler;
    state->stage = SetupConn;

    SetupCommonConn(&state->common, RECV_LEN);

    printf("Conn Setup\n");

    return state;
}

void connDestructor(struct connState *state) {
    closesocket(state->sock);
    CleanupCommonConn(&state->common);
    free(state);

    printf("Conn Destroy\n");
}

struct subroutine_result connSubroutine(struct connState *state) {
    // Verify Current Async is "this"
    if (currentAsync == NULL || currentAsync->state != state) return subroutine_finish;

    StageSwitch:
    switch (state->stage) {
        case SetupConn: {
            unsigned long nonBlocking = 1;
            int err = ioctlsocket(state->sock, FIONBIO, &nonBlocking);

            if (err == SOCKET_ERROR) {
                fprintf(stderr, "error: Failed to mark Connection as Non-Blocking: %i\n", WSAGetLastError());
                return subroutine_finish;
            }

            HANDLE ioPort = w32_CreateIOPort(state->io_handler, (HANDLE)state->sock);

            if (ioPort != state->io_handler->iocp_handle) {
                fprintf(stderr, "error: Connection IOCP Port is invalid.\n");
                return subroutine_finish;
            }

            state->stage = ConnRead;
            goto StageSwitch;
        }

        case ConnRead: {
            state->stage = ConnProcess;

            state->flags = 0;
            state->WSArecvBuf = (WSABUF){
                .len = RECV_LEN - state->common.recvOffset,
                .buf = (uint8_t *)state->common.recvBuf + state->common.recvOffset,
            };

            PrepareIO();

            struct io_op *op = CreateIOOperation(0, currentAsync);

            int err = WSARecv(
                state->sock,
                &state->WSArecvBuf,
                1,
                NULL,
                &state->flags,
                (OVERLAPPED*)op,
                NULL
            );

            if (err == SOCKET_ERROR) {
                int wsaErr = WSAGetLastError();

                if (wsaErr != WSA_IO_PENDING) {
                    printf("Instant Read Error: %i\n", wsaErr);
                    free(op);
                    CancelIO();

                    return subroutine_finish;
                }
            }

            return subroutine_yield_io;
        }

        case ConnProcess: {
            if (
                !state->io_state.ok ||
                state->io_state.bytesTransferred == 0
            ) return subroutine_finish;

            state->common.recvOffset += state->io_state.bytesTransferred;

            if (!ProcessLines(&state->common)) return subroutine_finish;

            state->stage = ConnRead;
            goto StageSwitch;
        }

        default: {
            printf("Unknown Stage\n");
            return subroutine_finish;
        }
    }
}

const struct async_descriptor connAsync = {
    .constructor = connConstructor,
    .destructor = (async_destructor)connDestructor,
    .subroutine = (async_subroutine)connSubroutine,
};