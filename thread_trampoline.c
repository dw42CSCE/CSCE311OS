/* trampoline called when a new thread is started.
   The assembly context_switch will set ra to this address
   so that returning will jump here.
*/
#include "thread.h"
#include "uart.h"      /* <--- add for debug prints */

/* implemented in thread.c */
void thread_start_run(void);

void thread_trampoline(void) {
    uart_puts("[trampoline] enter\n");
    thread_start_run();
    uart_puts("[trampoline] ERROR: thread_start_run returned\n");
    while (1) asm volatile("wfi");
}

