#include "helper_functions.h"
#include <sys/types.h>
#include <string.h>
#include <ctype.h>
#include <regex.h>
#include <stdio.h>
#include <dirent.h>

/*
 * Lowercases a string
 */
void
strlwr(char *string)
{
    for (size_t i = 0; string[i] != '\0'; i++) {
        string[i] = tolower(string[i]);
    }
}

/*
 * Counts the number of log file segments in the database folder
 */
size_t
count_num_segments(void)
{
    size_t count = 0;
    struct dirent* dir;
    DIR* dirp = opendir(DIRECTORY);
    regex_t regex;

    /* bit_db001 but not bit_db001.tb */
    if (regcomp(&regex, "^bit_db[0-9]+$", REG_EXTENDED) != 0)
        return 0;

    while ((dir = readdir(dirp)) != NULL)
        if (regexec(&regex, dir->d_name, 0, NULL, 0) == 0)
            count++;

    regfree(&regex);
    closedir(dirp);
    return (count > 0) ? count : 1;
}

/*
 * Counts the number of digits in a number
 */
size_t
count_digits(size_t num)
{
    return snprintf(NULL, 0, "%lu", (unsigned long)num) - (num < 0);
}

