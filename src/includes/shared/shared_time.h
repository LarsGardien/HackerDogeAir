#ifndef SHARED_TIME_H
#define SHARED_TIME_H

#include <time.h>

#define SHARED_TIME_ERROR_LOCALTIME 1101
#define SHARED_TIME_ERROR_PRINT 1102

int shared_time_time_t_to_string(time_t *src, char *dest, const char *__restrict __format, int maxsize);

#endif