#include <sys/stat.h>
#include <errno.h>
#include "stm32f4xx_hal.h"

extern UART_HandleTypeDef huart3;

int _write(int fd, char *ptr, int len)
{
    (void)fd;
    HAL_UART_Transmit(&huart3, (uint8_t *)ptr, (uint16_t)len, HAL_MAX_DELAY);
    return len;
}

int _read(int fd, char *ptr, int len)  { (void)fd; (void)ptr; (void)len; return 0; }
int _close(int fd)                     { (void)fd; return -1; }
int _fstat(int fd, struct stat *st)    { (void)fd; st->st_mode = S_IFCHR; return 0; }
int _isatty(int fd)                    { (void)fd; return 1; }
int _lseek(int fd, int ptr, int dir)   { (void)fd; (void)ptr; (void)dir; return 0; }

extern char _end;
static char *heap_end = 0;

void *_sbrk(ptrdiff_t incr)
{
    extern char _estack;
    char *prev_heap_end;

    if (heap_end == 0) heap_end = &_end;
    prev_heap_end = heap_end;

    if (heap_end + incr > &_estack) {
        errno = ENOMEM;
        return (void *)-1;
    }
    heap_end += incr;
    return (void *)prev_heap_end;
}
