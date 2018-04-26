#include <stdlib.h>
#include <stdbool.h>
#include <errno.h>

extern void *__real_malloc(size_t size);
extern void *__real_calloc(size_t nmemb, size_t size);
extern bool alloc_works = true;

/* Failure is togglable with this wrapper */
void *
__wrap_malloc(size_t size)
{
        if (alloc_works) {
                return __real_malloc(size);
        }
        else {
                errno = ENOMEM;
                return NULL;
        }
}

void *
__wrap_calloc(size_t nmemb, size_t size)
{
	if (alloc_works) {
		return __real_calloc(nmemb, size);
	}
	else {
		errno = ENOMEM;
		return NULL;
	}
}

