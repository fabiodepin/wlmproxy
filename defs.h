/* vim:set ts=2 sw=2 et cindent: */
/*
 * Copyright (c) 2011 William Lima <wlima@primate.com.br>
 * All rights reserved.
 */

#ifndef DEFS_H_
#define DEFS_H_
#pragma once

#include <endian.h>
#include <byteswap.h>

#define SET_BIT(bit)    (1U << (bit))
#define SET_BIT_32(bit) (UINT32_C(1) << (bit))
#define SET_BIT_64(bit) (UINT64_C(1) << (bit))

#define arraysize(array) (sizeof(array) / sizeof(*(array)))

#if __BYTE_ORDER == __BIG_ENDIAN
#define ntohll(x) (x)
#define htonll(x) (x)
#elif __BYTE_ORDER == __LITTLE_ENDIAN
#define ntohll(x) __bswap_64 (x)
#define htonll(x) __bswap_64 (x)
#else
#error Unsupported host byte order!
#endif

#endif // DEFS_H_
