/*
 * Minimal vendored blkid API — replaces libblkid to avoid system library dependency.
 * Provides stubs sufficient for btrfs-progs mkfs code paths.
 */
#ifndef PYBTRFS_BLKID_COMPAT_H
#define PYBTRFS_BLKID_COMPAT_H

#include <stdint.h>
#include <sys/types.h>

/* Types */
typedef long long blkid_loff_t;

/* Opaque types */
typedef struct blkid_struct_probe *blkid_probe;
typedef struct blkid_struct_cache *blkid_cache;
typedef struct blkid_struct_dev *blkid_dev;
typedef struct blkid_struct_dev_iterate *blkid_dev_iterate;

/* Probe API */
blkid_probe blkid_new_probe(void);
blkid_probe blkid_new_probe_from_filename(const char *filename);
void        blkid_free_probe(blkid_probe pr);
int         blkid_probe_set_device(blkid_probe pr, int fd,
                                   int64_t off, int64_t size);
int64_t     blkid_probe_get_size(blkid_probe pr);
dev_t       blkid_probe_get_devno(blkid_probe pr);
int         blkid_do_fullprobe(blkid_probe pr);
int         blkid_probe_enable_partitions(blkid_probe pr, int enable);
int         blkid_probe_lookup_value(blkid_probe pr, const char *name,
                                     const char **data, size_t *len);

/* Cache API */
int         blkid_get_cache(blkid_cache *cache, const char *filename);
void        blkid_put_cache(blkid_cache cache);
int         blkid_probe_all(blkid_cache cache);
blkid_dev   blkid_verify(blkid_cache cache, blkid_dev dev);

/* Device iteration */
blkid_dev_iterate blkid_dev_iterate_begin(blkid_cache cache);
int         blkid_dev_set_search(blkid_dev_iterate iter,
                                 const char *type, const char *value);
int         blkid_dev_next(blkid_dev_iterate iter, blkid_dev *dev);
void        blkid_dev_iterate_end(blkid_dev_iterate iter);

/* Device info */
const char *blkid_dev_devname(blkid_dev dev);

/* Misc */
int         blkid_devno_to_wholedisk(dev_t devno, char *diskname,
                                     size_t len, dev_t *diskdevno);
int         blkid_get_library_version(const char **ver_string,
                                      const char **date_string);

#endif /* PYBTRFS_BLKID_COMPAT_H */
