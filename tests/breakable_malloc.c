#include <stdlib.h>
#include <stdbool.h>
#include <errno.h>

extern void *__real_malloc(size_t size);
extern bool malloc_works = true;

/* Failure is togglable with this wrapper */
void *
__wrap_malloc(size_t size)
{
        if (malloc_works) {
                return __real_malloc(size);
        }
        else {
                errno = ENOMEM;
                return NULL;
        }
}

