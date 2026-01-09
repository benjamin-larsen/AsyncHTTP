#pragma once

#ifdef _WIN32
#include "./tcp_win.c"
#else
#error "Unsupported OS"
#endif