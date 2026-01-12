#pragma once

#include <stdio.h>
#include "../string.c"

struct HTTPRequest {
    union string method;
    union string path;
    union string version;
};

void CleanupHTTPRequest(struct HTTPRequest *req) {
    printf("Cleaning up:\n  Method: %.*s\n  Path: %.*s\n  Version: %.*s\n", GetStringLen(&req->method), GetStringBuf(&req->method), GetStringLen(&req->path), GetStringBuf(&req->path), GetStringLen(&req->version), GetStringBuf(&req->version));
    FreeString(&req->method);
    FreeString(&req->path);
    FreeString(&req->version);
}

// Returns non-safe strings, strings must be copied with malloc after.
bool DecodeRequestLine(union string str, struct HTTPRequest *req) {
    union string method;
    union string path;

    if (!SplitString(&str, &method, ' ')) return false;
    if (!SplitString(&str, &path, ' ')) return false;

    req->method = method;
    req->path = path;
    req->version = str;

    return true;
}