/* SPDX-FileCopyrightText: 2026 Jarosław Pelczar <jarek@jpelczar.com>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Minimal freestanding libc shims for the -nostdlib build. Compiled as
 * plain C (not C++) so these plain C-ABI definitions can never collide
 * with libstdc++'s <cstring> overload/asm-redirection tricks for memchr
 * et al. seen when building main.cpp with a hosted (glibc) cross
 * toolchain. The compiler can synthesize calls to these even when the
 * source never calls them directly (e.g. loop idiom recognition), so
 * they must be provided whenever no real libc is linked in.
 */

#include <stddef.h>

size_t strlen(const char *s) {
  size_t n = 0;
  while (s[n] != '\0') {
    ++n;
  }
  return n;
}

void *memchr(const void *ptr, int value, size_t num) {
  const unsigned char *p = (const unsigned char *)ptr;
  unsigned char target = (unsigned char)value;
  size_t i;

  for (i = 0; i < num; ++i) {
    if (p[i] == target) {
      return (void *)(p + i);
    }
  }

  return (void *)0;
}

void *memcpy(void *dest, const void *src, size_t num) {
  unsigned char *d = (unsigned char *)dest;
  const unsigned char *s = (const unsigned char *)src;
  size_t i;

  for (i = 0; i < num; ++i) {
    d[i] = s[i];
  }

  return dest;
}

void *memset(void *ptr, int value, size_t num) {
  unsigned char *p = (unsigned char *)ptr;
  size_t i;

  for (i = 0; i < num; ++i) {
    p[i] = (unsigned char)value;
  }

  return ptr;
}

int memcmp(const void *lhs, const void *rhs, size_t num) {
  const unsigned char *a = (const unsigned char *)lhs;
  const unsigned char *b = (const unsigned char *)rhs;
  size_t i;

  for (i = 0; i < num; ++i) {
    if (a[i] != b[i]) {
      return (int)a[i] - (int)b[i];
    }
  }

  return 0;
}
