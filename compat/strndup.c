#include "compat.h"

#include <string.h>
#include <stdlib.h>

#ifndef strndup
char *
strndup(char const *str, size_t max)
{
	size_t len = strnlen(str, max);
	char *res = (char *)malloc(len + 1);

	if (res == NULL)
		return NULL;

	res[len] = '\0';
	return (char *)memcpy(res, str, len);
}
#endif
