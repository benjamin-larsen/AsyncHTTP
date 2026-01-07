#include <stdio.h>
#include "./io.h"

// Input/Output Completion Port (IOCP / Windows)
// EPOLL (Linux)
// KQueue (Mac)

// get file


// C10k Problem

/* Blocking Connections
 * Accept Connection (client kobler til)

 LoopClient:
 * Read Message (henter og parser melding)
 * Process Message
 * Repeat LoopClient

 * Close Connection
 */

// Single-threaded Application
// Multi-threaded Application

// Concurrency = Overlapping Execution

// Worker Thread = Software Threads (virtual concept)
// CPU Threads   = Hardware Threads (physical)

/* Non-blocking Connections
 Setup:
 * Spin up N worker threads (N = CPU Threads/Cores)

 Main Thread: (blocking, accept loop)
 * Accept Connection (main thread, blocking)
(Forward Client to worker threads)
 * Create IOCP (for Client)
 * Resolve IOCP

 // Async

 Worker Thread:
 IOCP Loop:
 * Get IOCP (er det noen ting som er klare) (blocking)
 * Handle Event
 * Repeat IOCP Loop

 Client:
 LoopClient:
  * Create IOCP (for Read Packet)
  * Read Packet (Use IOCP port)
  * "Yield"
  * Process Packet
  * Repeat LoopClient


 */

int main(void) {
    printf("Hello, World!\n");
    return 0;
}