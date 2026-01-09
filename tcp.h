#pragma once

#ifdef _WIN32
#include "./tcp_win/server.c"
#else
#error "Unsupported OS"
#endif