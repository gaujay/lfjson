/**
 * Copyright 2022 Guillaume AUJAY. All rights reserved.
 *
 */

#ifndef LFJSON_UTILS_H
#define LFJSON_UTILS_H

// Is 64-bits platform
#if defined(__amd64__) || defined(__amd64) || defined(__x86_64__) || defined(__x86_64) \
  || defined(_M_X64) || defined(_M_AMD64) || defined(__ppc64__) || defined(_WIN64)
  #define LFJ_64BIT
#endif

#endif // LFJSON_UTILS_H
