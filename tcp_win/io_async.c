#pragma once

#include <winsock2.h>

#include "../state_machine.c"

struct io_async_state {
    bool ok;
    DWORD bytesTransferred;
};

const struct io_async_state nullIOAsyncState = { .ok = false, .bytesTransferred = 0 };
