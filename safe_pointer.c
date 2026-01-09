#pragma once

// Randomly Generated Magic
#define SHARED_PTR_MAGIC 0xa5f47ae3

#include <stdio.h>

#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdatomic.h>

typedef void (*shared_destructor)(void *);
typedef _Atomic uint32_t atomic_uint32;

atomic_uint32 sharedPtrMagicCounter = SHARED_PTR_MAGIC;

struct shared_ptr {
    atomic_uint32 refs;
    uint32_t magic;
    shared_destructor destructor;
    void *ptr;
};

struct shared_retainer {
    struct shared_ptr *descriptor;
    uint32_t magic;
    bool released;
};

const struct shared_retainer INVALID_RETAINER = {
    .descriptor = NULL,
    .magic = 0,
    .released = true
};

// Does extra check if released or not.
struct shared_retainer RetainShared(struct shared_retainer retainer) {
    if (retainer.released || retainer.descriptor == NULL) {
        return INVALID_RETAINER;
    }

    // must be volatile to prevent compiler from optimizing by for example trying to copy the struct.
    // the descriptor may be modified by another thread
    volatile struct shared_ptr *descriptor = retainer.descriptor;

    uint32_t oldRefs = atomic_fetch_add(&descriptor->refs, 1);

    if (oldRefs == 0) {
        fprintf(stderr, "panic: Shared Pointer attempted to be retained after being reference by none.\n");
        abort();
    }

    // Must check magic after, since it's possible that another thread set refs to 0.
    // Must secure refs before doing this sanity check.
    if (descriptor->magic != retainer.magic) {
        fprintf(stderr, "panic: Shared Pointer attempted to be retained after being freed or corrupted.\n");
        abort();
    }

    return (struct shared_retainer){
        .descriptor = descriptor,
        .magic = retainer.magic,
        .released = false
    };
}

struct shared_retainer TransferOwnershipShared(struct shared_retainer *retainer) {
    if (retainer == NULL) {
        fprintf(stderr, "panic: attempted to call ReleaseShared on NULL retainer.\n");
        abort();
    }

    if (retainer->released || retainer->descriptor == NULL) {
        return INVALID_RETAINER;
    }

    struct shared_retainer transferredRetainer = {
        .descriptor = retainer->descriptor,
        .magic = retainer->magic,
        .released = false
    };

    retainer->descriptor = NULL;
    retainer->released = true;

    return transferredRetainer;
}

void ReleaseShared(struct shared_retainer *retainer) {
    if (retainer == NULL) {
        fprintf(stderr, "panic: attempted to call ReleaseShared on NULL retainer.\n");
        abort();
    }

    if (retainer->released || retainer->descriptor == NULL) {
        return;
    }

    // must be volatile to prevent compiler from optimizing by for example trying to copy the struct.
    // the descriptor may be modified by another thread
    volatile struct shared_ptr *descriptor = retainer->descriptor;

    uint32_t oldRefs = atomic_fetch_sub(&descriptor->refs, 1);

    if (oldRefs == 0) {
        fprintf(stderr, "panic: Shared Pointer attempted to be released after being reference by none.\n");
        abort();
    }

    if (descriptor->magic != retainer->magic) {
        fprintf(stderr, "panic: Shared Pointer attempted to be released after being freed or corrupted.\n");
        abort();
    }

    retainer->descriptor = NULL;
    retainer->released = true;

    if (oldRefs > 1) return;

    if (descriptor->destructor != NULL) {
        descriptor->destructor(descriptor->ptr);
    }

    descriptor->magic = 0;
    descriptor->ptr = NULL;

    free(descriptor);
}

struct shared_retainer MakeShared(size_t size, shared_destructor destructor) {
    struct shared_ptr *sharedPtr = calloc(1, sizeof(struct shared_ptr) + size);

    if (sharedPtr == NULL) {
        return INVALID_RETAINER;
    }

    sharedPtr->refs = 1;
    sharedPtr->magic = atomic_fetch_add(&sharedPtrMagicCounter, 1);
    sharedPtr->destructor = destructor;

    // We do + 1 as Pointer Arithmetic advances per the size of the Pointer Type (in this case shared_ptr)
    // As we want the data after the shared pointer header
    sharedPtr->ptr = sharedPtr + 1;

    //printf("%p\n", sharedPtr);
    //printf("%llu\n", *(uintptr_t*)sharedPtr->ptr);

    return (struct shared_retainer){
        .descriptor = sharedPtr,
        .magic = sharedPtr->magic,
        .released = false
    };
}

// Only use these APIs if you know what you're doing

// Used for when you want to make a Retainer from a Pointer, without incrementing refs
// This would be used for example when the Caller Thread incremented refs for you, but passes you the shared pointer only.
struct shared_retainer RetainerFromShared(struct shared_ptr *sharedPtr) {
    if (sharedPtr == NULL) {
        return INVALID_RETAINER;
    }

    return (struct shared_retainer){
        .descriptor = sharedPtr,
        .magic = sharedPtr->magic,
        .released = false
    };
}
