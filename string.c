#pragma once

#include <stdint.h>
#include <string.h>
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
    if (str->longStr.buf == nullptr) return;

    free(str->longStr.buf);
    str->longStr.buf = nullptr;
}

bool StringEquals(union string strA, union string strB) {
    uint8_t *bufA = GetStringBuf(&strA);
    uint32_t lenA = GetStringLen(&strA);

    uint8_t *bufB = GetStringBuf(&strB);
    uint32_t lenB = GetStringLen(&strB);

    if (lenA !=  lenB) return false;

    return memcmp(bufA, bufB, lenA) == 0;
}

union string CopyString(union string str) {
    uint8_t *buf = GetStringBuf(&str);
    uint32_t len = GetStringLen(&str);

    if (len == 0) return nullString;

    if (len > maxShortString) {
        // should check if buf is null
        struct longString longStr = { .len = len, .buf = malloc(len) };
        if (longStr.buf == NULL) return nullString;

        memcpy(longStr.buf, buf, len);

        return (union string){ .longStr = longStr };
    } else {
        struct shortString shortStr = { .len = len };
        memcpy(shortStr.buf, buf, len);

        return (union string){ .shortStr = shortStr };
    }
}

union string FromCStrUnsafe(const char *str) {
    size_t len = strlen(str);

    if (len == 0) return nullString;

    return (union string){ .longStr = { .len = len, .buf = str } };
}

union string FromCStr(const char *str) {
    return CopyString(FromCStrUnsafe(str));
}

// Splits the string from source (leaving the first part in the dest) and puts remainder into src
// Does not do short string optimization, dest gets the same type (short/long) as src.
// Returns whether it was successful
bool SplitString(union string *src, union string *dest, uint8_t delimiter) {
    if (src == nullptr || dest == nullptr) return false;

    uint8_t *buf = GetStringBuf(src);
    uint32_t len = GetStringLen(src);

    if (buf == NULL || len == 0) return false;

    unsigned char *bounds = memchr(buf, delimiter, len);

    if (bounds == NULL || bounds < buf) return false;

    uint32_t size = (char*)bounds - (char*)buf;

    if (src->shortStr.len > 0) {
        struct shortString destStr = { .len = size };
        memcpy(destStr.buf, src->shortStr.buf, size);

        dest->shortStr = destStr;

        // 1 to remove the delimiter itself
        src->shortStr.len = len - size - 1;
        memmove(
            src->shortStr.buf,
            (char *)src->shortStr.buf + size + 1,
            src->shortStr.len
        );
    } else {
        dest->longStr = (struct longString){ .len = size, .buf = buf };

        src->longStr = (struct longString){
            .len = len - size - 1,
            .buf = (char *)buf + size + 1
        };
    }

    return true;
}