/**
 * bsp/nucleo_f446re/src/syscalls.c
 * * Architecture Layer: Board support / C library glue
 * Responsibility: Enforces the zero-heap policy. newlib calls _sbrk() to grow the heap for malloc();
 *                 refusing it makes any accidental dynamic allocation fail immediately (malloc returns
 *                 NULL) instead of silently eating the RAM reserved for the main stack.
 *
 *                 The firmware never uses stdio streams (all output goes through SerialPort), but
 *                 newlib's stdio cleanup code references the file syscalls: they are stubbed here
 *                 so the link is warning-free. Everything else comes from libnosys.
 */
#include <errno.h>
#include <stddef.h>
#include <sys/types.h>

void* _sbrk(ptrdiff_t increment);
int _close(int file);
off_t _lseek(int file, off_t offset, int whence);
int _read(int file, char* data, int length);
int _write(int file, const char* data, int length);

void* _sbrk(ptrdiff_t increment) {
    (void)increment;
    errno = ENOMEM;
    return (void*)-1;
}

int _close(int file) {
    (void)file;
    errno = EBADF;
    return -1;
}

off_t _lseek(int file, off_t offset, int whence) {
    (void)file;
    (void)offset;
    (void)whence;
    errno = EBADF;
    return -1;
}

int _read(int file, char* data, int length) {
    (void)file;
    (void)data;
    (void)length;
    errno = EBADF;
    return -1;
}

int _write(int file, const char* data, int length) {
    (void)file;
    (void)data;
    (void)length;
    errno = EBADF;
    return -1;
}
