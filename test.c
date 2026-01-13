#include "./state_machine.c"
#include <stdio.h>
#include <stdlib.h>

enum httpStage {
    A,
    B
};

struct otherState {
    enum httpStage stage;
    uint32_t nesting;
};

void *otherConstructor(void *param) {
    struct otherState *state = calloc(1, sizeof(struct otherState));

    state->stage = A;
    state->nesting = *(uint32_t *)param;

    return state;
}

void otherDestructor(void *state) {
    return free(state);
}

extern const struct async_descriptor otherAsync;

bool runOther(struct otherState *state) {
    switch (state->stage) {
        case A: {
            state->stage = B;

            uint32_t nesting = state->nesting;
            printf("Running Other\n");
            if (nesting > 0) {
                uint32_t nextNesting = state->nesting - 1;
                AwaitAsync(otherAsync, &nextNesting);
                return false;
            }

            printf("Finished Other Instant\n");
            return true;
        }
        case B: {
            printf("Finished Other\n");
            return true;
        }
    }

    return true;
}

const struct async_descriptor otherAsync = {
    .constructor = otherConstructor,
    .destructor = otherDestructor,
    .subroutine = runOther,
};

struct httpState {
    enum httpStage stage;
};

void *httpConstructor(void *param) {
    struct httpState *state = calloc(1, sizeof(struct httpState));
    state->stage = A;

    printf("%p\n", state);
    return state;
}

void httpDestructor(void *state) {
    return free(state);
}

bool runHttp(struct httpState *state) {
    printf("Stage: %u\n", state->stage);

    switch (state->stage) {
        case A: {
            state->stage = B;
            uint32_t nesting = 5;
            AwaitAsync(otherAsync, &nesting);
            return false;
        }

        case B: {
            return true;
        }
    }
}

const struct async_descriptor httpAsync = {
    .constructor = httpConstructor,
    .destructor = httpDestructor,
    .subroutine = runHttp,
};

int main(void) {
    printf("Starting %p\n", httpAsync.subroutine);
    AwaitAsync(httpAsync, NULL);
    return 0;
}
