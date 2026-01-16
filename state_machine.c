// Async State Machines

#pragma once

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdatomic.h>
#include <immintrin.h>
#include "./atomics.c"

// First Parameter is a parameter that can be passed in.
// Returns a ptr to be saved as Async State
// Can pass in pointer to stack here, as constructor is called immediately on AwaitAsync.
typedef void *(*async_constructor)(void *);

// Parameter is ptr to Async State, this function is responsible for freeing
typedef void (*async_destructor)(void *);

// Parameter is ptr to Async State
// Returns whether subroutine ended
typedef struct subroutine_result (*async_subroutine)(void *);

struct async_descriptor {
    async_constructor constructor;
    async_destructor destructor;
    async_subroutine subroutine;
};

enum machine_flags {
    // State Machine is currently running
    MACHINE_RUNNING = 1 << 0,
    // State Machine is currently suspended pending a I/O operation
    MACHINE_SUSPENDED_IO = 1 << 1,
    // State Machine is currently suspended waiting for another State Machine to finish.
    MACHINE_SUSPENDED_AWAIT = 1 << 2,
    // Will call KillAsync when it finishes, regardless of result. Used if catastrophic failure occurs
    MACHINE_FORCE_DESTROY = 1 << 3
};

struct async_state {
    struct async_descriptor descriptor;
    void *state;
    struct async_state *awaiting;
    atomic_uint32 flags;
};

enum subroutine_result_type {
    SUBROUTINE_YIELD_IO,
    SUBROUTINE_AWAIT,
    SUBROUTINE_FINISHED
};

struct subroutine_result {
    enum subroutine_result_type type;
    // Used when SUBROUTINE_AWAIT is used, pointer to async_state
    struct async_state *await;
};

const struct subroutine_result subroutine_yield_io = { .type = SUBROUTINE_YIELD_IO, .await = NULL };
const struct subroutine_result subroutine_finish = { .type = SUBROUTINE_FINISHED, .await = NULL };

struct subroutine_result subroutine_await(struct async_state *state) {
    return (struct subroutine_result){ .type = SUBROUTINE_AWAIT, .await = state };
}

_Thread_local struct async_state *currentAsync = NULL;

const struct async_descriptor nullAsync = {
    .constructor = NULL,
    .destructor = NULL,
    .subroutine = NULL
};

bool IsValidDescriptor(struct async_descriptor descriptor) {
    if (descriptor.constructor == NULL) return false;
    if (descriptor.destructor == NULL) return false;
    if (descriptor.subroutine == NULL) return false;

    return true;
}

void KillAsync(struct async_state *machineState) {
    if (machineState == NULL) return;
    if (!IsValidDescriptor(machineState->descriptor)) return;
    if (machineState->state == NULL) return;

    // check if running, if so set to "force destroy"
    uint32_t flags = atomic_load(&machineState->flags);

    if ((flags & MACHINE_RUNNING) != 0) {
        printf("Failed to kill\n");
        atomic_fetch_or(&machineState->flags, MACHINE_FORCE_DESTROY);
        return;
    }

    struct async_state *awaiting = machineState->awaiting;

    machineState->descriptor.destructor(machineState->state);
    free(machineState);

    if (awaiting == NULL) return;

    KillAsync(awaiting);
}

// for IO operations structs must start with a io_result object

extern void ResumeFromAwait(struct async_state *machineState);

// Constraint: RunAsync assumes machine is not running, use only after being initalized or use ResumeFromAwait or Resume
// Caller is responsible for setting RUNNING flag.
void RunAsync(struct async_state *machineState) {
    if (machineState == NULL) return;
    if (!IsValidDescriptor(machineState->descriptor)) return;
    if (machineState->state == NULL) return;

    // also check if already running

    // Expects no more than one more async to be
    if (currentAsync != NULL) {
        fprintf(stderr, "critical error: async called during running async.");
        KillAsync(machineState);
        return;
    }

    currentAsync = machineState;

    struct subroutine_result result = machineState->descriptor.subroutine(machineState->state);

    currentAsync = NULL;

    if ((atomic_load(&machineState->flags) & MACHINE_FORCE_DESTROY) != 0) {
        atomic_store(&machineState->flags, 0);
        KillAsync(machineState);
        return;
    }

    switch (result.type) {
        case SUBROUTINE_YIELD_IO: {
            printf("Yield IO\n");
            // for IO, have a PrepareIO/AbortIO command that is called before any IO operation is called.
            // also have a CheckIO function that unsets the suspened IO flag and will return wether it was set.
            // if CheckIO returns true, spinlock until no longer running (expected to be short.)

            atomic_fetch_and(&machineState->flags, ~MACHINE_RUNNING);
            break;
        }
        case SUBROUTINE_AWAIT: {
            printf("Await\n");
            if (result.await == NULL || result.await->awaiting != machineState) {
                KillAsync(machineState);
                printf("Invalid Await\n");
                return;
            }

            // check that is not running and not suspended (aka newly created)
            uint32_t awaitFlags = atomic_load(&result.await->flags);

            if ((awaitFlags & MACHINE_RUNNING) != 0) {
                printf("Await can't use a running State Machine.\n");
                return;
            }

            if ((awaitFlags & (MACHINE_SUSPENDED_IO | MACHINE_SUSPENDED_AWAIT)) != 0) {
                printf("Await can't use a suspended State Machine.\n");
                return;
            }

            // should make this atomic with CAS loop
            atomic_fetch_or(&machineState->flags, MACHINE_SUSPENDED_AWAIT);
            atomic_fetch_and(&machineState->flags, ~MACHINE_RUNNING);

            RunAsync(result.await);

            // check that await is not NULL, and that await's awaiting is currentAsync/machineState
            break;
        }
        case SUBROUTINE_FINISHED: {
            printf("Finished\n");
            struct async_state *awaiting = machineState->awaiting;

            machineState->descriptor.destructor(machineState->state);
            free(machineState);

            if (awaiting == NULL) return;

            ResumeFromAwait(awaiting);
            break;
        }
    }
}

void PrepareIO() {
    if (currentAsync == NULL) return;

    atomic_fetch_or(&currentAsync->flags, MACHINE_SUSPENDED_IO);
}

// Only to be used if IO failed to be queued.
void CancelIO() {
    if (currentAsync == NULL) return;

    atomic_fetch_and(&currentAsync->flags, ~MACHINE_SUSPENDED_IO);
}

void ResumeFromIO(struct async_state *machineState) {
    if (machineState == NULL) return;
    if (!IsValidDescriptor(machineState->descriptor)) return;
    if (machineState->state == NULL) return;

    for (;;) {
        uint32_t flags = atomic_load(&machineState->flags);

        if ((flags & MACHINE_SUSPENDED_IO) == 0) {
            printf("Resumed from IO when not expected\n");
            return;
        }

        if (atomic_compare_exchange_weak(&machineState->flags, &flags, flags & ~MACHINE_SUSPENDED_IO)) break;
    }

    while ((atomic_load(&machineState->flags) & MACHINE_RUNNING) != 0) {
        _mm_pause();
    }

    atomic_fetch_or(&machineState->flags, MACHINE_RUNNING);

    // atomic unset bits

    RunAsync(machineState);
}

void ResumeFromAwait(struct async_state *machineState) {
    if (machineState == NULL) return;
    if (!IsValidDescriptor(machineState->descriptor)) return;
    if (machineState->state == NULL) return;

    for (;;) {
        uint32_t flags = atomic_load(&machineState->flags);

        if ((flags & MACHINE_SUSPENDED_AWAIT) == 0) {
            printf("Resumed from Await when not expected\n");
            return;
        }

        if ((flags & MACHINE_RUNNING) != 0) {
            printf("Resumed from Await when already running\n");
            return;
        }

        uint32_t newFlags = flags;
        newFlags &= ~MACHINE_SUSPENDED_AWAIT;
        newFlags |= MACHINE_RUNNING;

        if (atomic_compare_exchange_weak(&machineState->flags, &flags, newFlags)) break;
    }

    RunAsync(machineState);
}

struct async_state *AwaitAsync(struct async_descriptor descriptor, void *constructParam) {
    if (!IsValidDescriptor(descriptor)) return NULL;

    struct async_state *machineState = calloc(1, sizeof(struct async_state));
    machineState->descriptor = descriptor;

    if (currentAsync != NULL) {
        machineState->awaiting = currentAsync;
    }

    machineState->state = machineState->descriptor.constructor(constructParam);

    if (machineState->state == NULL) {
        free(machineState);
        return NULL;
    }

    return machineState;
}