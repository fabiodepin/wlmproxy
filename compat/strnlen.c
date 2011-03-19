#include "compat.h"

#include <string.h>

#ifndef strnlen
size_t
strnlen(const char *str, size_t max)
{
	const char *end = (const char *)memchr(str, '\0', max);
	return end ? (size_t)(end - str) : max;
}
#endif
