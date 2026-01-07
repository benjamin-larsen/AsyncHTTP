#ifndef ASYNCHTTP_TCP_H
#define ASYNCHTTP_TCP_H

#ifdef _WIN32
#include "./tcp_win.c"
#else
#error "Unsupported OS"
#endif

#endif