#pragma once

#ifdef _WIN32
#include "./io_win.c"
#else
#error "Unsupported OS"
#endif