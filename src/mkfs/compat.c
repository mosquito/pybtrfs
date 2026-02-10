/*
 * Minimal vendored implementations of libuuid and libblkid APIs.
 * Avoids system library dependencies for the pybtrfs.mkfs extension.
 *
 * SPDX-License-Identifier: GPL-2.0
 */

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <linux/fs.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>
#include <unistd.h>

#include "uuid/uuid.h"
#include "blkid/blkid.h"

/* ── UUID implementation ─────────────────────────────────────── */

void uuid_clear(uuid_t uu)
{
	memset(uu, 0, 16);
}

int uuid_compare(const uuid_t uu1, const uuid_t uu2)
{
	return memcmp(uu1, uu2, 16);
}

void uuid_copy(uuid_t dst, const uuid_t src)
{
	memcpy(dst, src, 16);
}

static int read_urandom(void *buf, size_t len)
{
	int fd = open("/dev/urandom", O_RDONLY);
	if (fd < 0)
		return -1;
	ssize_t n = read(fd, buf, len);
	close(fd);
	return (n == (ssize_t)len) ? 0 : -1;
}

void uuid_generate(uuid_t out)
{
	if (read_urandom(out, 16) < 0) {
		/* Last resort fallback */
		for (int i = 0; i < 16; i++)
			out[i] = (unsigned char)rand();
	}
	/* Set version 4 (random) */
	out[6] = (out[6] & 0x0F) | 0x40;
	/* Set variant 1 (RFC 4122) */
	out[8] = (out[8] & 0x3F) | 0x80;
}

void uuid_generate_time(uuid_t out)
{
	/* Fallback to random — good enough for mkfs */
	uuid_generate(out);
}

int uuid_is_null(const uuid_t uu)
{
	for (int i = 0; i < 16; i++)
		if (uu[i])
			return 0;
	return 1;
}

static int hex_nibble(char c)
{
	if (c >= '0' && c <= '9') return c - '0';
	if (c >= 'a' && c <= 'f') return c - 'a' + 10;
	if (c >= 'A' && c <= 'F') return c - 'A' + 10;
	return -1;
}

int uuid_parse(const char *in, uuid_t uu)
{
	/* Expected: "xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx" (36 chars) */
	if (strlen(in) != 36)
		return -1;
	if (in[8] != '-' || in[13] != '-' || in[18] != '-' || in[23] != '-')
		return -1;

	int idx = 0;
	for (int i = 0; i < 36; i++) {
		if (i == 8 || i == 13 || i == 18 || i == 23)
			continue;
		int hi = hex_nibble(in[i]);
		i++;
		if (i == 8 || i == 13 || i == 18 || i == 23)
			i++;
		int lo = hex_nibble(in[i]);
		if (hi < 0 || lo < 0)
			return -1;
		uu[idx++] = (unsigned char)((hi << 4) | lo);
	}
	return 0;
}

static void uuid_fmt(const uuid_t uu, char *out, int upper)
{
	const char *fmt = upper
		? "%02X%02X%02X%02X-%02X%02X-%02X%02X-%02X%02X-%02X%02X%02X%02X%02X%02X"
		: "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x";
	sprintf(out, fmt,
		uu[0], uu[1], uu[2], uu[3], uu[4], uu[5], uu[6], uu[7],
		uu[8], uu[9], uu[10], uu[11], uu[12], uu[13], uu[14], uu[15]);
}

void uuid_unparse(const uuid_t uu, char *out)
{
	uuid_fmt(uu, out, 0);
}

void uuid_unparse_upper(const uuid_t uu, char *out)
{
	uuid_fmt(uu, out, 1);
}

/* ── BLKID implementation (stubs + minimal real logic) ───────── */

struct blkid_struct_probe {
	int fd;
	int owns_fd;
	int64_t offset;
	int64_t size;
	dev_t devno;
};

struct blkid_struct_cache {
	int dummy;
};

struct blkid_struct_dev {
	int dummy;
};

struct blkid_struct_dev_iterate {
	int done;
};

blkid_probe blkid_new_probe(void)
{
	struct blkid_struct_probe *pr = calloc(1, sizeof(*pr));
	if (pr)
		pr->fd = -1;
	return pr;
}

blkid_probe blkid_new_probe_from_filename(const char *filename)
{
	struct blkid_struct_probe *pr = blkid_new_probe();
	if (!pr)
		return NULL;

	pr->fd = open(filename, O_RDONLY);
	if (pr->fd < 0) {
		free(pr);
		return NULL;
	}
	pr->owns_fd = 1;

	struct stat st;
	if (fstat(pr->fd, &st) == 0)
		pr->devno = st.st_rdev;

	/* Determine size */
	if (S_ISBLK(st.st_mode)) {
		uint64_t sz = 0;
		if (ioctl(pr->fd, BLKGETSIZE64, &sz) == 0)
			pr->size = (int64_t)sz;
	} else {
		pr->size = st.st_size;
	}

	return pr;
}

void blkid_free_probe(blkid_probe pr)
{
	if (!pr)
		return;
	if (pr->owns_fd && pr->fd >= 0)
		close(pr->fd);
	free(pr);
}

int blkid_probe_set_device(blkid_probe pr, int fd,
			   int64_t off, int64_t size)
{
	if (!pr)
		return -1;
	pr->fd = fd;
	pr->owns_fd = 0;
	pr->offset = off;

	if (size) {
		pr->size = size;
	} else {
		struct stat st;
		if (fstat(fd, &st) < 0)
			return -1;
		pr->devno = st.st_rdev;
		if (S_ISBLK(st.st_mode)) {
			uint64_t sz = 0;
			if (ioctl(fd, BLKGETSIZE64, &sz) == 0)
				pr->size = (int64_t)sz;
		} else {
			pr->size = st.st_size;
		}
	}
	return 0;
}

int64_t blkid_probe_get_size(blkid_probe pr)
{
	return pr ? pr->size : -1;
}

dev_t blkid_probe_get_devno(blkid_probe pr)
{
	return pr ? pr->devno : 0;
}

int blkid_do_fullprobe(blkid_probe pr)
{
	(void)pr;
	/* Return 1 = nothing found (no existing filesystem) */
	return 1;
}

int blkid_probe_enable_partitions(blkid_probe pr, int enable)
{
	(void)pr;
	(void)enable;
	return 0;
}

int blkid_probe_lookup_value(blkid_probe pr, const char *name,
			     const char **data, size_t *len)
{
	(void)pr;
	(void)name;
	(void)data;
	(void)len;
	return -1; /* Not found */
}

int blkid_get_cache(blkid_cache *cache, const char *filename)
{
	(void)filename;
	*cache = calloc(1, sizeof(struct blkid_struct_cache));
	return *cache ? 0 : -1;
}

void blkid_put_cache(blkid_cache cache)
{
	free(cache);
}

int blkid_probe_all(blkid_cache cache)
{
	(void)cache;
	return 0;
}

blkid_dev blkid_verify(blkid_cache cache, blkid_dev dev)
{
	(void)cache;
	(void)dev;
	return NULL;
}

blkid_dev_iterate blkid_dev_iterate_begin(blkid_cache cache)
{
	(void)cache;
	struct blkid_struct_dev_iterate *iter =
		calloc(1, sizeof(struct blkid_struct_dev_iterate));
	return iter;
}

int blkid_dev_set_search(blkid_dev_iterate iter,
			 const char *type, const char *value)
{
	(void)iter;
	(void)type;
	(void)value;
	return 0;
}

int blkid_dev_next(blkid_dev_iterate iter, blkid_dev *dev)
{
	(void)dev;
	if (!iter)
		return -1;
	/* Always return "end of iteration" */
	return -1;
}

void blkid_dev_iterate_end(blkid_dev_iterate iter)
{
	free(iter);
}

const char *blkid_dev_devname(blkid_dev dev)
{
	(void)dev;
	return NULL;
}

int blkid_devno_to_wholedisk(dev_t devno, char *diskname,
			     size_t len, dev_t *diskdevno)
{
	/* Read from /sys/dev/block/MAJ:MIN/../dev to find parent */
	char path[256];
	unsigned int maj = major(devno);
	unsigned int min = minor(devno);

	snprintf(path, sizeof(path),
		 "/sys/dev/block/%u:%u/device/../dev", maj, min);

	/* Try to find the whole disk device name */
	char link[256];
	snprintf(link, sizeof(link), "/sys/dev/block/%u:%u", maj, min);
	char resolved[PATH_MAX];
	if (!realpath(link, resolved))
		goto fallback;

	/* Extract device name from sysfs path */
	char *base = strrchr(resolved, '/');
	if (!base)
		goto fallback;
	base++;

	if (diskname && len > 0)
		snprintf(diskname, len, "%s", base);
	if (diskdevno)
		*diskdevno = devno;
	return 0;

fallback:
	if (diskname && len > 0)
		snprintf(diskname, len, "dev%u_%u", maj, min);
	if (diskdevno)
		*diskdevno = devno;
	return 0;
}

int blkid_get_library_version(const char **ver_string,
			      const char **date_string)
{
	if (ver_string)
		*ver_string = "2.40.0";
	if (date_string)
		*date_string = "2024-01-01";
	/* Return version as integer (2.40 = 240) — high enough for zoned checks */
	return 2400;
}
