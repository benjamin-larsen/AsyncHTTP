#ifndef ASYNCHTTP_IO_H
#define ASYNCHTTP_IO_H

#ifdef _WIN32
#include "./io_win.c"
#else
#error "Unsupported OS"
#endif

#endif