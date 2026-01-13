// Async State Machines

#pragma once

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

// First Parameter is a parameter that can be passed in.
// Returns a ptr to be saved as Async State
// Can pass in pointer to stack here, as constructor is called immediately on AwaitAsync.
typedef void *(*async_constructor)(void *);

// Parameter is ptr to Async State, this function is responsible for freeing
typedef void (*async_destructor)(void *);

// Parameter is ptr to Async State
// Returns whether subroutine ended
typedef bool (*async_subroutine)(void *);

struct async_descriptor {
    async_constructor constructor;
    async_destructor destructor;
    async_subroutine subroutine;
};

struct async_state {
    struct async_descriptor descriptor;
    void *state;
    struct async_state *awaiting;
};

_Thread_local struct async_state *currentAsync = NULL;
_Thread_local struct async_state *waitingAsync = NULL;

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

// for IO operations structs must start with a io_result object

void RunAsync(struct async_state *machineState) {
    if (machineState == NULL) return;
    if (!IsValidDescriptor(machineState->descriptor)) return;
    if (machineState->state == NULL) return;

    // Defer execution until this is done.
    // Expects no more than one more async to be scheduled at once
    if (currentAsync != NULL) {
        if (waitingAsync != NULL) {
            fprintf(stderr, "critical error: two async called at once.");
            // perhaps kill the currentAsync, waitingAsync and machineState async
            return;
        }

        waitingAsync = machineState;
        return;
    }

    currentAsync = machineState;

    bool finished = machineState->descriptor.subroutine(machineState->state);

    currentAsync = NULL;
    struct async_state *awaiting = NULL;

    if (finished) {
        awaiting = machineState->awaiting;

        machineState->descriptor.destructor(machineState->state);
        free(machineState);
    }

    // Run waiting async, before any others
    if (waitingAsync != NULL) {
        // Reset waiting async before running.
        struct async_state *waitedAsync = waitingAsync;
        waitingAsync = NULL;

        RunAsync(waitedAsync);
    }

    if (awaiting == NULL) return;

    RunAsync(awaiting);
}

void StartAsync(struct async_state *machineState, void *constructParam) {
    if (machineState == NULL) return;
    if (!IsValidDescriptor(machineState->descriptor)) return;

    if (currentAsync != NULL) {
        machineState->awaiting = currentAsync;
    }

    machineState->state = machineState->descriptor.constructor(constructParam);

    RunAsync(machineState);
}

void AwaitAsync(struct async_descriptor descriptor, void *constructParam) {
    struct async_state *machineState = calloc(1, sizeof(struct async_state));
    machineState->descriptor = descriptor;

    StartAsync(machineState, constructParam);
}