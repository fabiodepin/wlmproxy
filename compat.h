#ifndef _COMPAT_H_
#define _COMPAT_H_
#pragma once

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef strnlen
size_t strnlen(const char *, size_t);
#endif

#ifndef strndup
char *strndup(const char *, size_t);
#endif

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif // _COMPAT_H_
