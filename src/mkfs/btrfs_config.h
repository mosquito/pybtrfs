/*
 * Static configuration header for pybtrfs mkfs extension.
 * Replaces autotools-generated config.h from btrfs-progs.
 */
#ifndef PYBTRFS_BTRFS_CONFIG_H
#define PYBTRFS_BTRFS_CONFIG_H

/* Use builtin crypto (no external library dependency) */
#define CRYPTOPROVIDER_BUILTIN  1
#define CRYPTOPROVIDER          "builtin"

/* Enable zoned device support */
#define BTRFS_ZONED             1

/* Disable optional compression backends */
#define COMPRESSION_LZO         0
#define COMPRESSION_ZSTD        0

/* Disable experimental features */
#define EXPERIMENTAL            0

/* Package metadata (silences references in vendor code) */
#define PACKAGE_STRING          "pybtrfs-mkfs"
#define PACKAGE_URL             "https://github.com/mosquito/pybtrfs"

/* Feature detection flags used by btrfs-progs build */
#define HAVE_REALLOCARRAY       1

#endif /* PYBTRFS_BTRFS_CONFIG_H */
