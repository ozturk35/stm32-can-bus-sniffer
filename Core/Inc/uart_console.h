#ifndef UART_CONSOLE_H
#define UART_CONSOLE_H

#include <stddef.h>

void uart_console_init(void);
void uart_write(const char *s);
void uart_writef(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
void uart_process_input(void);   /* call from main loop */

#endif /* UART_CONSOLE_H */
