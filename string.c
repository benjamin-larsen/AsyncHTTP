#pragma once

#include <stdint.h>
#include <stdlib.h>

struct longString {
    uint8_t reserved;
    uint32_t len;
    unsigned char *buf;
};

#define maxShortString sizeof(struct longString) - sizeof(uint8_t)

struct __attribute__((packed)) shortString {
    uint8_t len;
    unsigned char buf[maxShortString];
};

union string {
    struct longString longStr;
    struct shortString shortStr;
};

static_assert(sizeof(struct shortString) == sizeof(struct longString),
    "Short String is not the size of Regular String. Contact Developer (benjamin-larsen) for patch.");

unsigned char *GetStringBuf(union string *str) {
    if (str == nullptr) return nullptr;
    
    if (str->shortStr.len > 0) {
        return str->shortStr.buf;
    } else {
        return str->longStr.buf;
    }
}

uint32_t GetStringLen(union string *str) {
    if (str == NULL) return 0;

    if (str->shortStr.len > 0) {
        return str->shortStr.len;
    } else {
        return str->longStr.len;
    }
}

const union string nullString = { .longStr = { .buf = NULL, .len = 0, .reserved = 0 } };

bool IsEmptyString(union string str) {
    return str.longStr.len == 0 && str.shortStr.len == 0;
}

void FreeString(union string *str) {
    if (str == nullptr) return;
    if (str->shortStr.len > 0) return;

    free(str->longStr.buf);
}