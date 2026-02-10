/*
 * Minimal vendored UUID API — replaces libuuid to avoid system library dependency.
 * Implements the subset used by btrfs-progs.
 */
#ifndef PYBTRFS_UUID_COMPAT_H
#define PYBTRFS_UUID_COMPAT_H

#include <stdint.h>
#include <string.h>

typedef unsigned char uuid_t[16];

/* 36 chars + NUL */
#define UUID_STR_LEN 37

void uuid_clear(uuid_t uu);
int  uuid_compare(const uuid_t uu1, const uuid_t uu2);
void uuid_copy(uuid_t dst, const uuid_t src);
void uuid_generate(uuid_t out);
void uuid_generate_time(uuid_t out);
int  uuid_is_null(const uuid_t uu);
int  uuid_parse(const char *in, uuid_t uu);
void uuid_unparse(const uuid_t uu, char *out);
void uuid_unparse_upper(const uuid_t uu, char *out);

#endif /* PYBTRFS_UUID_COMPAT_H */
