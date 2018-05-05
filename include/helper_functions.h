#pragma once

#include <sys/types.h>

#ifndef DIRECTORY
#define DIRECTORY "db"
#endif

void
strlwr(char *string);

size_t
count_num_segments(void);

size_t
count_digits(size_t num);

