/*
 * pybtrfs.mkfs — Python C extension for creating btrfs filesystems.
 *
 * Wraps btrfs-progs mkfs functionality.  Static helper functions are copied
 * from vendor/btrfs-progs/mkfs/main.c because they are declared static and
 * cannot be linked against directly.
 *
 * SPDX-License-Identifier: GPL-2.0
 */

#define PY_SSIZE_T_CLEAN
#include <Python.h>

#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <uuid/uuid.h>

#include "kerncompat.h"
#include "kernel-lib/list.h"
#include "kernel-lib/sizes.h"
#include "kernel-lib/rbtree.h"
#include "kernel-shared/accessors.h"
#include "kernel-shared/ctree.h"
#include "kernel-shared/disk-io.h"
#include "kernel-shared/extent_io.h"
#include "kernel-shared/transaction.h"
#include "kernel-shared/volumes.h"
#include "kernel-shared/zoned.h"
#include "kernel-shared/uapi/btrfs_tree.h"
#include "crypto/hash.h"
#include "common/defs.h"
#include "common/internal.h"
#include "common/messages.h"
#include "common/cpu-utils.h"
#include "common/utils.h"
#include "common/device-utils.h"
#include "common/device-scan.h"
#include "common/fsfeatures.h"
#include "common/extent-cache.h"
#include "common/root-tree-utils.h"
#include "common/rbtree-utils.h"
#include "common/string-utils.h"
#include "check/qgroup-verify.h"
#include "mkfs/common.h"

/* ── structs from mkfs/main.c ─────────────────────────────────── */

struct mkfs_allocation {
	u64 data;
	u64 metadata;
	u64 mixed;
	u64 system;
	u64 remap;
};

struct prepare_device_progress {
	int fd;
	char *file;
	u64 dev_byte_count;
	u64 byte_count;
	int oflags;
	int zero_end;
	int discard;
	int zoned;
	int ret;
};

/* ── static helpers copied from mkfs/main.c ──────────────────── */
/*
 * These functions are static in main.c so we must copy them here.
 * The exit(1) in create_one_raid_group has been replaced with
 * return -ENOSPC.
 */

static int create_metadata_block_groups(struct btrfs_root *root,
					u64 incompat_flags,
					struct mkfs_allocation *allocation,
					u64 metadata_profile)
{
	struct btrfs_fs_info *fs_info = root->fs_info;
	struct btrfs_trans_handle *trans;
	struct btrfs_space_info *sinfo;
	u64 flags = BTRFS_BLOCK_GROUP_METADATA;
	u64 chunk_start = 0;
	u64 chunk_size = 0;
	u64 system_group_size = BTRFS_MKFS_SYSTEM_GROUP_SIZE;
	const bool mixed = incompat_flags & BTRFS_FEATURE_INCOMPAT_MIXED_GROUPS;
	const bool remap_tree = incompat_flags & BTRFS_FEATURE_INCOMPAT_REMAP_TREE;
	int ret;

	if (btrfs_is_zoned(fs_info))
		system_group_size = fs_info->zone_size;

	if (mixed)
		flags |= BTRFS_BLOCK_GROUP_DATA;

	ret = update_space_info(fs_info, flags, 0, 0, &sinfo);
	if (ret < 0)
		return ret;

	trans = btrfs_start_transaction(root, 1);
	if (IS_ERR(trans)) {
		ret = PTR_ERR(trans);
		errno = -ret;
		error_msg(ERROR_MSG_START_TRANS, "%m");
		return ret;
	}

	root->fs_info->system_allocs = 1;
	allocation->system += system_group_size;
	if (ret)
		return ret;

	if (mixed) {
		ret = btrfs_alloc_chunk(trans, fs_info, &chunk_start, &chunk_size,
					BTRFS_BLOCK_GROUP_METADATA |
					BTRFS_BLOCK_GROUP_DATA);
		if (ret == -ENOSPC) {
			error("no space to allocate data/metadata chunk");
			goto err;
		}
		if (ret)
			return ret;
		ret = btrfs_make_block_group(trans, fs_info, 0,
					     BTRFS_BLOCK_GROUP_METADATA |
					     BTRFS_BLOCK_GROUP_DATA,
					     chunk_start, chunk_size);
		if (ret)
			return ret;
		allocation->mixed += chunk_size;
	} else {
		ret = btrfs_alloc_chunk(trans, fs_info, &chunk_start, &chunk_size,
					BTRFS_BLOCK_GROUP_METADATA);
		if (ret == -ENOSPC) {
			error("no space to allocate metadata chunk");
			goto err;
		}
		if (ret)
			return ret;
		ret = btrfs_make_block_group(trans, fs_info, 0,
					     BTRFS_BLOCK_GROUP_METADATA,
					     chunk_start, chunk_size);
		allocation->metadata += chunk_size;
		if (ret)
			return ret;
	}

	if (remap_tree) {
		ret = btrfs_alloc_chunk(trans, fs_info, &chunk_start, &chunk_size,
					BTRFS_BLOCK_GROUP_METADATA_REMAP);
		if (ret == -ENOSPC) {
			error("no space to allocate remap chunk");
			goto err;
		}
		if (ret)
			return ret;
		ret = btrfs_make_block_group(trans, fs_info, 0,
					     BTRFS_BLOCK_GROUP_METADATA_REMAP,
					     chunk_start, chunk_size);
		allocation->remap += chunk_size;
		if (ret)
			return ret;
	}

	root->fs_info->system_allocs = 0;
	ret = btrfs_commit_transaction(trans, root);
	if (ret) {
		errno = -ret;
		error_msg(ERROR_MSG_COMMIT_TRANS, "%m");
	}
err:
	return ret;
}

static int create_data_block_groups(struct btrfs_trans_handle *trans,
				    struct btrfs_root *root, bool mixed,
				    struct mkfs_allocation *allocation)
{
	struct btrfs_fs_info *fs_info = root->fs_info;
	u64 chunk_start = 0;
	u64 chunk_size = 0;
	int ret = 0;

	if (!mixed) {
		struct btrfs_space_info *sinfo;

		ret = update_space_info(fs_info, BTRFS_BLOCK_GROUP_DATA,
					0, 0, &sinfo);
		if (ret < 0)
			return ret;

		ret = btrfs_alloc_chunk(trans, fs_info, &chunk_start, &chunk_size,
					BTRFS_BLOCK_GROUP_DATA);
		if (ret == -ENOSPC) {
			error("no space to allocate data chunk");
			goto err;
		}
		if (ret)
			return ret;
		ret = btrfs_make_block_group(trans, fs_info, 0,
					     BTRFS_BLOCK_GROUP_DATA,
					     chunk_start, chunk_size);
		allocation->data += chunk_size;
		if (ret)
			return ret;
	}

err:
	return ret;
}

static int make_root_dir(struct btrfs_trans_handle *trans,
			 struct btrfs_root *root)
{
	struct btrfs_key location;
	int ret;

	ret = btrfs_make_root_dir(trans, root->fs_info->tree_root,
				  BTRFS_ROOT_TREE_DIR_OBJECTID);
	if (ret)
		goto err;
	ret = btrfs_make_root_dir(trans, root, BTRFS_FIRST_FREE_OBJECTID);
	if (ret)
		goto err;
	memcpy(&location, &root->fs_info->fs_root->root_key, sizeof(location));
	location.offset = (u64)-1;
	ret = btrfs_insert_dir_item(trans, root->fs_info->tree_root,
				    "default", 7,
				    btrfs_super_root_dir(root->fs_info->super_copy),
				    &location, BTRFS_FT_DIR, 0);
	if (ret)
		goto err;

	ret = btrfs_insert_inode_ref(trans, root->fs_info->tree_root,
				     "default", 7, location.objectid,
				     BTRFS_ROOT_TREE_DIR_OBJECTID, 0);
	if (ret)
		goto err;

err:
	return ret;
}

static int __recow_root(struct btrfs_trans_handle *trans,
			struct btrfs_root *root)
{
	struct btrfs_path path = { 0 };
	struct btrfs_key key;
	int ret;

	key.objectid = 0;
	key.type = 0;
	key.offset = 0;

	ret = btrfs_search_slot(NULL, root, &key, &path, 0, 0);
	if (ret < 0)
		return ret;

	while (true) {
		struct btrfs_key found_key;

		if (btrfs_header_generation(path.nodes[0]) == trans->transid)
			goto next;

		btrfs_item_key_to_cpu(path.nodes[0], &key, 0);
		btrfs_release_path(&path);

		ret = btrfs_search_slot(trans, root, &key, &path, 0, 1);
		if (ret < 0)
			goto out;
		ret = 0;
		btrfs_item_key_to_cpu(path.nodes[0], &found_key, 0);
		UASSERT(btrfs_comp_cpu_keys(&key, &found_key) == 0);

next:
		ret = btrfs_next_leaf(root, &path);
		if (ret < 0)
			goto out;
		if (ret > 0) {
			ret = 0;
			goto out;
		}
	}
out:
	btrfs_release_path(&path);
	return ret;
}

static int recow_global_roots(struct btrfs_trans_handle *trans)
{
	struct btrfs_fs_info *fs_info = trans->fs_info;
	struct btrfs_root *root;
	struct rb_node *n;
	int ret = 0;

	for (n = rb_first(&fs_info->global_roots_tree); n; n = rb_next(n)) {
		root = rb_entry(n, struct btrfs_root, rb_node);
		ret = __recow_root(trans, root);
		if (ret)
			return ret;
	}

	return ret;
}

static int recow_roots(struct btrfs_trans_handle *trans,
		       struct btrfs_root *root)
{
	struct btrfs_fs_info *info = root->fs_info;
	int ret;

	ret = __recow_root(trans, info->fs_root);
	if (ret)
		return ret;
	ret = __recow_root(trans, info->tree_root);
	if (ret)
		return ret;
	ret = __recow_root(trans, info->chunk_root);
	if (ret)
		return ret;
	ret = __recow_root(trans, info->dev_root);
	if (ret)
		return ret;

	if (btrfs_fs_compat_ro(info, BLOCK_GROUP_TREE)) {
		ret = __recow_root(trans, info->block_group_root);
		if (ret)
			return ret;
	}
	if (btrfs_fs_incompat(info, RAID_STRIPE_TREE)) {
		ret = __recow_root(trans, info->stripe_root);
		if (ret)
			return ret;
	}
	ret = recow_global_roots(trans);
	if (ret)
		return ret;
	return 0;
}

/*
 * NOTE: the original exit(1) on -ENOSPC has been replaced with
 *       return -ENOSPC so we don't kill the Python interpreter.
 */
static int create_one_raid_group(struct btrfs_trans_handle *trans,
				 struct btrfs_root *root, u64 type,
				 struct mkfs_allocation *allocation)
{
	struct btrfs_fs_info *fs_info = root->fs_info;
	u64 chunk_start;
	u64 chunk_size;
	int ret;

	ret = btrfs_alloc_chunk(trans, fs_info, &chunk_start, &chunk_size, type);
	if (ret == -ENOSPC) {
		error("not enough free space to allocate chunk");
		return -ENOSPC;
	}
	if (ret)
		return ret;

	ret = btrfs_make_block_group(trans, fs_info, 0, type,
				     chunk_start, chunk_size);

	type &= BTRFS_BLOCK_GROUP_TYPE_MASK;
	if (type == BTRFS_BLOCK_GROUP_DATA) {
		allocation->data += chunk_size;
	} else if (type == BTRFS_BLOCK_GROUP_METADATA) {
		allocation->metadata += chunk_size;
	} else if (type == BTRFS_BLOCK_GROUP_SYSTEM) {
		allocation->system += chunk_size;
	} else if (type == (BTRFS_BLOCK_GROUP_METADATA |
			    BTRFS_BLOCK_GROUP_DATA)) {
		allocation->mixed += chunk_size;
	} else {
		error("unrecognized profile type: 0x%llx",
		      (unsigned long long)type);
		ret = -EINVAL;
	}

	return ret;
}

static int create_raid_groups(struct btrfs_trans_handle *trans,
			      struct btrfs_root *root, u64 data_profile,
			      u64 metadata_profile, bool mixed,
			      struct mkfs_allocation *allocation)
{
	int ret = 0;

	if (metadata_profile) {
		u64 meta_flags = BTRFS_BLOCK_GROUP_METADATA;

		ret = create_one_raid_group(trans, root,
					    BTRFS_BLOCK_GROUP_SYSTEM |
					    metadata_profile, allocation);
		if (ret)
			return ret;

		if (mixed)
			meta_flags |= BTRFS_BLOCK_GROUP_DATA;

		ret = create_one_raid_group(trans, root,
					    meta_flags | metadata_profile,
					    allocation);
		if (ret)
			return ret;
	}
	if (!mixed && data_profile) {
		ret = create_one_raid_group(trans, root,
					    BTRFS_BLOCK_GROUP_DATA |
					    data_profile, allocation);
		if (ret)
			return ret;
	}

	return ret;
}

static bool is_temp_block_group(struct extent_buffer *node,
				struct btrfs_block_group_item *bgi,
				u64 data_profile, u64 meta_profile,
				u64 sys_profile)
{
	u64 flag = btrfs_block_group_flags(node, bgi);
	u64 flag_type = flag & BTRFS_BLOCK_GROUP_TYPE_MASK;
	u64 flag_profile = flag & BTRFS_BLOCK_GROUP_PROFILE_MASK;
	u64 used = btrfs_block_group_used(node, bgi);

	if (used != 0)
		return false;
	switch (flag_type) {
	case BTRFS_BLOCK_GROUP_DATA:
	case BTRFS_BLOCK_GROUP_DATA | BTRFS_BLOCK_GROUP_METADATA:
		data_profile &= BTRFS_BLOCK_GROUP_PROFILE_MASK;
		if (flag_profile != data_profile)
			return true;
		break;
	case BTRFS_BLOCK_GROUP_METADATA:
		meta_profile &= BTRFS_BLOCK_GROUP_PROFILE_MASK;
		if (flag_profile != meta_profile)
			return true;
		break;
	case BTRFS_BLOCK_GROUP_SYSTEM:
		sys_profile &= BTRFS_BLOCK_GROUP_PROFILE_MASK;
		if (flag_profile != sys_profile)
			return true;
		break;
	}
	return false;
}

static int next_block_group(struct btrfs_root *root,
			    struct btrfs_path *path)
{
	struct btrfs_key key;
	int ret = 0;

	while (1) {
		ret = btrfs_next_item(root, path);
		if (ret)
			goto out;
		btrfs_item_key_to_cpu(path->nodes[0], &key, path->slots[0]);
		if (key.type == BTRFS_BLOCK_GROUP_ITEM_KEY)
			goto out;
	}
out:
	return ret;
}

static int cleanup_temp_chunks(struct btrfs_fs_info *fs_info,
			       struct mkfs_allocation *alloc,
			       u64 data_profile, u64 meta_profile,
			       u64 sys_profile, int do_discard)
{
	struct btrfs_trans_handle *trans = NULL;
	struct btrfs_block_group_item *bgi;
	struct btrfs_root *root = btrfs_block_group_root(fs_info);
	struct btrfs_key key;
	struct btrfs_key found_key;
	struct btrfs_path path = { 0 };
	int ret = 0;

	trans = btrfs_start_transaction(root, 1);
	if (IS_ERR(trans)) {
		ret = PTR_ERR(trans);
		errno = -ret;
		error_msg(ERROR_MSG_START_TRANS, "%m");
		return ret;
	}

	key.objectid = 0;
	key.type = BTRFS_BLOCK_GROUP_ITEM_KEY;
	key.offset = 0;

	while (1) {
		ret = btrfs_search_slot(trans, root, &key, &path, 0, 0);
		if (ret < 0)
			goto out;
		if (ret > 0)
			ret = 0;

		btrfs_item_key_to_cpu(path.nodes[0], &found_key,
				      path.slots[0]);
		if (found_key.objectid < key.objectid)
			goto out;
		if (found_key.type != BTRFS_BLOCK_GROUP_ITEM_KEY) {
			ret = next_block_group(root, &path);
			if (ret < 0)
				goto out;
			if (ret > 0) {
				ret = 0;
				goto out;
			}
			btrfs_item_key_to_cpu(path.nodes[0], &found_key,
					      path.slots[0]);
		}

		bgi = btrfs_item_ptr(path.nodes[0], path.slots[0],
				     struct btrfs_block_group_item);
		if (is_temp_block_group(path.nodes[0], bgi,
					data_profile, meta_profile,
					sys_profile)) {
			u64 flags = btrfs_block_group_flags(path.nodes[0], bgi);

			(void)do_discard; /* discard handled separately */

			ret = btrfs_remove_block_group(trans,
					found_key.objectid, found_key.offset);
			if (ret < 0)
				goto out;

			if ((flags & BTRFS_BLOCK_GROUP_TYPE_MASK) ==
			    BTRFS_BLOCK_GROUP_DATA)
				alloc->data -= found_key.offset;
			else if ((flags & BTRFS_BLOCK_GROUP_TYPE_MASK) ==
				 BTRFS_BLOCK_GROUP_METADATA)
				alloc->metadata -= found_key.offset;
			else if ((flags & BTRFS_BLOCK_GROUP_TYPE_MASK) ==
				 BTRFS_BLOCK_GROUP_SYSTEM)
				alloc->system -= found_key.offset;
			else if ((flags & BTRFS_BLOCK_GROUP_TYPE_MASK) ==
				 (BTRFS_BLOCK_GROUP_METADATA |
				  BTRFS_BLOCK_GROUP_DATA))
				alloc->mixed -= found_key.offset;
		}
		btrfs_release_path(&path);
		key.objectid = found_key.objectid + found_key.offset;
	}
out:
	if (trans) {
		ret = btrfs_commit_transaction(trans, root);
		if (ret) {
			errno = -ret;
			error_msg(ERROR_MSG_COMMIT_TRANS, "%m");
		}
	}
	btrfs_release_path(&path);
	return ret;
}

static int create_global_root(struct btrfs_trans_handle *trans, u64 objectid,
			      int root_id)
{
	struct btrfs_fs_info *fs_info = trans->fs_info;
	struct btrfs_root *root;
	struct btrfs_key key = {
		.objectid = objectid,
		.type = BTRFS_ROOT_ITEM_KEY,
		.offset = root_id,
	};
	int ret = 0;

	root = btrfs_create_tree(trans, &key);
	if (IS_ERR(root)) {
		ret = PTR_ERR(root);
		goto out;
	}
	ret = btrfs_global_root_insert(fs_info, root);
out:
	if (ret)
		btrfs_abort_transaction(trans, ret);
	return ret;
}

static int create_global_roots(struct btrfs_trans_handle *trans,
			       int nr_global_roots)
{
	int ret, i;

	for (i = 1; i < nr_global_roots; i++) {
		ret = create_global_root(trans, BTRFS_EXTENT_TREE_OBJECTID, i);
		if (ret)
			return ret;
		ret = create_global_root(trans, BTRFS_CSUM_TREE_OBJECTID, i);
		if (ret)
			return ret;
		ret = create_global_root(trans, BTRFS_FREE_SPACE_TREE_OBJECTID, i);
		if (ret)
			return ret;
	}

	btrfs_set_super_nr_global_roots(trans->fs_info->super_copy,
					nr_global_roots);

	return 0;
}

static int setup_raid_stripe_tree_root(struct btrfs_fs_info *fs_info)
{
	struct btrfs_trans_handle *trans;
	struct btrfs_root *stripe_root;
	struct btrfs_key key = {
		.objectid = BTRFS_RAID_STRIPE_TREE_OBJECTID,
		.type = BTRFS_ROOT_ITEM_KEY,
	};
	int ret;

	trans = btrfs_start_transaction(fs_info->tree_root, 0);
	if (IS_ERR(trans)) {
		ret = PTR_ERR(trans);
		errno = -ret;
		error_msg(ERROR_MSG_START_TRANS, "%m");
		return ret;
	}

	stripe_root = btrfs_create_tree(trans, &key);
	if (IS_ERR(stripe_root)) {
		ret = PTR_ERR(stripe_root);
		btrfs_abort_transaction(trans, ret);
		return ret;
	}
	fs_info->stripe_root = stripe_root;
	add_root_to_dirty_list(stripe_root);

	ret = btrfs_commit_transaction(trans, fs_info->tree_root);
	if (ret) {
		errno = -ret;
		error_msg(ERROR_MSG_COMMIT_TRANS, "%m");
		return ret;
	}

	return 0;
}

static int setup_remap_tree_root(struct btrfs_fs_info *fs_info)
{
	struct btrfs_trans_handle *trans;
	struct btrfs_root *remap_root;
	struct btrfs_key key = {
		.objectid = BTRFS_REMAP_TREE_OBJECTID,
		.type = BTRFS_ROOT_ITEM_KEY,
		.offset = 0
	};
	int ret;

	trans = btrfs_start_transaction(fs_info->tree_root, 0);
	if (IS_ERR(trans)) {
		ret = PTR_ERR(trans);
		errno = -ret;
		error_msg(ERROR_MSG_START_TRANS, "%m");
		return ret;
	}

	remap_root = btrfs_create_tree(trans, &key);
	if (IS_ERR(remap_root)) {
		ret = PTR_ERR(remap_root);
		btrfs_abort_transaction(trans, ret);
		return ret;
	}
	fs_info->remap_root = remap_root;
	add_root_to_dirty_list(remap_root);

	btrfs_set_super_remap_root(fs_info->super_copy,
				   remap_root->root_item.bytenr);
	btrfs_set_super_remap_root_generation(fs_info->super_copy,
					      remap_root->root_item.generation);
	btrfs_set_super_remap_root_level(fs_info->super_copy,
					 remap_root->root_item.level);

	ret = btrfs_commit_transaction(trans, fs_info->tree_root);
	if (ret) {
		errno = -ret;
		error_msg(ERROR_MSG_COMMIT_TRANS, "%m");
		return ret;
	}

	return 0;
}

/* Thread callback for device preparation */
static void *prepare_one_device(void *ctx)
{
	struct prepare_device_progress *p = ctx;

	p->fd = open(p->file, p->oflags);
	if (p->fd < 0) {
		error("unable to open %s: %m", p->file);
		p->ret = -errno;
		return NULL;
	}
	p->ret = btrfs_prepare_device(p->fd, p->file,
				      &p->dev_byte_count,
				      p->byte_count,
				      (p->zero_end ? PREP_DEVICE_ZERO_END : 0) |
				      (p->discard  ? PREP_DEVICE_DISCARD  : 0) |
				      (p->zoned    ? PREP_DEVICE_ZONED    : 0));
	return NULL;
}

/* ── Python wrapper ──────────────────────────────────────────── */

PyDoc_STRVAR(pybtrfs_mkfs_doc,
"mkfs(*devices: str, label: str = \"\", nodesize: int = 16384, sectorsize: int = 4096,\n"
"     byte_count: int = 0, metadata_profile: int = -1, data_profile: int = 0,\n"
"     mixed: bool = False, features: int = 0, csum_type: int = 0, uuid: str = \"\",\n"
"     force: bool = False, no_discard: bool = False) -> dict\n\n"
"Create a btrfs filesystem on one or more block devices.\n\n"
"Returns a dict with keys 'uuid' (str) and 'num_bytes' (int).\n"
"Raises OSError on failure.");

static PyObject *
pybtrfs_mkfs(PyObject *self, PyObject *args, PyObject *kwargs)
{
	static char *kwlist[] = {
		"label", "nodesize", "sectorsize",
		"byte_count", "metadata_profile", "data_profile",
		"mixed", "features", "csum_type", "uuid",
		"force", "no_discard", NULL
	};

	const char *label = "";
	unsigned int nodesize = 16384;
	unsigned int sectorsize = 4096;
	unsigned long long byte_count = 0;
	long long metadata_profile = -1;  /* -1 = auto */
	unsigned long long data_profile = 0;
	int mixed = 0;
	unsigned long long features_arg = 0;
	int csum_type = 0;
	const char *fs_uuid_str = "";
	int force = 0;
	int no_discard = 0;

	/*
	 * All positional args are device paths — they arrive in 'args'.
	 * Only keyword args are parsed via PyArg_ParseTupleAndKeywords
	 * using an empty positional tuple.
	 */
	PyObject *empty_tuple = PyTuple_New(0);
	if (!empty_tuple)
		return NULL;

	int parse_ok = PyArg_ParseTupleAndKeywords(
		empty_tuple, kwargs, "|sIIKLKpKis$pp:mkfs",
		kwlist,
		&label, &nodesize, &sectorsize,
		&byte_count,
		&metadata_profile, &data_profile,
		&mixed, &features_arg,
		&csum_type, &fs_uuid_str,
		&force, &no_discard);
	Py_DECREF(empty_tuple);
	if (!parse_ok)
		return NULL;

	/* args itself is the devices tuple */
	Py_ssize_t device_count = PyTuple_GET_SIZE(args);
	if (device_count < 1) {
		PyErr_SetString(PyExc_ValueError,
				"at least one device is required");
		return NULL;
	}

	/* Extract device paths */
	const char **device_paths = calloc(device_count, sizeof(char *));
	if (!device_paths)
		return PyErr_NoMemory();

	for (Py_ssize_t i = 0; i < device_count; i++) {
		PyObject *item = PyTuple_GET_ITEM(args, i);
		if (!PyUnicode_Check(item)) {
			free(device_paths);
			PyErr_SetString(PyExc_TypeError,
					"device paths must be strings");
			return NULL;
		}
		device_paths[i] = PyUnicode_AsUTF8(item);
		if (!device_paths[i]) {
			free(device_paths);
			return NULL;
		}
	}

	/* Validate label length */
	if (strlen(label) >= BTRFS_LABEL_SIZE) {
		free(device_paths);
		PyErr_Format(PyExc_ValueError,
			     "label too long (max %d)", BTRFS_LABEL_SIZE - 1);
		return NULL;
	}

	/* Validate UUID if provided */
	char fs_uuid[BTRFS_UUID_UNPARSED_SIZE] = { 0 };
	if (fs_uuid_str[0]) {
		uuid_t dummy;
		if (uuid_parse(fs_uuid_str, dummy) != 0) {
			free(device_paths);
			PyErr_Format(PyExc_ValueError,
				     "invalid UUID: %s", fs_uuid_str);
			return NULL;
		}
		strncpy(fs_uuid, fs_uuid_str, BTRFS_UUID_UNPARSED_SIZE - 1);
	}

	/* Auto-select metadata profile based on device count */
	u64 meta_profile;
	if (metadata_profile < 0) {
		if (!mixed) {
			meta_profile = (device_count > 1)
				? BTRFS_MKFS_DEFAULT_META_MULTI_DEVICE
				: BTRFS_MKFS_DEFAULT_META_ONE_DEVICE;
		} else {
			meta_profile = 0;
		}
	} else {
		meta_profile = (u64)metadata_profile;
	}

	u64 data_prof = data_profile;
	if (!mixed && data_prof == 0 && device_count > 1) {
		data_prof = BTRFS_MKFS_DEFAULT_DATA_MULTI_DEVICE;
	}

	/* Build feature flags */
	struct btrfs_mkfs_features mkfs_features = btrfs_mkfs_default_features;
	mkfs_features.incompat_flags |= features_arg;

	if (mixed)
		mkfs_features.incompat_flags |= BTRFS_FEATURE_INCOMPAT_MIXED_GROUPS;

	if ((data_prof | meta_profile) & BTRFS_BLOCK_GROUP_RAID56_MASK)
		mkfs_features.incompat_flags |= BTRFS_FEATURE_INCOMPAT_RAID56;

	if ((data_prof | meta_profile) &
	    (BTRFS_BLOCK_GROUP_RAID1C3 | BTRFS_BLOCK_GROUP_RAID1C4))
		mkfs_features.incompat_flags |= BTRFS_FEATURE_INCOMPAT_RAID1C34;

	if (nodesize > sysconf(_SC_PAGE_SIZE))
		mkfs_features.incompat_flags |= BTRFS_FEATURE_INCOMPAT_BIG_METADATA;

	if (mixed && nodesize != sectorsize)
		nodesize = sectorsize;

	/* Do the work with GIL released */
	int ret = 0;
	int close_ret = 0;
	char result_uuid[BTRFS_UUID_UNPARSED_SIZE] = { 0 };
	u64 result_num_bytes = 0;

	Py_BEGIN_ALLOW_THREADS

	cpu_detect_flags();
	hash_init_accel();
	btrfs_config_init();

	/* Validate devices */
	if (!force) {
		for (Py_ssize_t i = 0; i < device_count; i++) {
			if (!test_dev_for_mkfs(device_paths[i], force)) {
				ret = -EINVAL;
				goto out_free;
			}
		}
	}

	/* Prepare all devices in parallel */
	pthread_t *t_prepare = calloc(device_count, sizeof(pthread_t));
	struct prepare_device_progress *prepare_ctx =
		calloc(device_count, sizeof(*prepare_ctx));

	if (!t_prepare || !prepare_ctx) {
		ret = -ENOMEM;
		free(t_prepare);
		free(prepare_ctx);
		goto out_free;
	}

	int oflags = O_RDWR;
	int do_discard = !no_discard;

	for (Py_ssize_t i = 0; i < device_count; i++) {
		prepare_ctx[i].file = (char *)device_paths[i];
		prepare_ctx[i].byte_count = byte_count;
		prepare_ctx[i].dev_byte_count = byte_count;
		prepare_ctx[i].oflags = oflags;
		prepare_ctx[i].zero_end = (byte_count == 0);
		prepare_ctx[i].discard = do_discard;
		prepare_ctx[i].zoned = 0;
		ret = pthread_create(&t_prepare[i], NULL,
				     prepare_one_device, &prepare_ctx[i]);
		if (ret) {
			/* Close already-launched threads' fds on error */
			for (Py_ssize_t j = 0; j < i; j++) {
				pthread_join(t_prepare[j], NULL);
				if (prepare_ctx[j].fd >= 0)
					close(prepare_ctx[j].fd);
			}
			free(t_prepare);
			free(prepare_ctx);
			ret = -ret;
			goto out_free;
		}
	}

	for (Py_ssize_t i = 0; i < device_count; i++)
		pthread_join(t_prepare[i], NULL);

	ret = prepare_ctx[0].ret;
	if (ret) {
		goto out_close;
	}

	u64 dev_byte_count = prepare_ctx[0].dev_byte_count;
	if (byte_count && byte_count > dev_byte_count) {
		ret = -ENOSPC;
		goto out_close;
	}

	/* Fill mkfs config */
	struct btrfs_mkfs_config mkfs_cfg;
	memset(&mkfs_cfg, 0, sizeof(mkfs_cfg));
	mkfs_cfg.label = label[0] ? label : NULL;
	memcpy(mkfs_cfg.fs_uuid, fs_uuid, sizeof(mkfs_cfg.fs_uuid));
	mkfs_cfg.num_bytes = dev_byte_count;
	mkfs_cfg.nodesize = nodesize;
	mkfs_cfg.sectorsize = sectorsize;
	mkfs_cfg.stripesize = sectorsize;
	mkfs_cfg.features = mkfs_features;
	mkfs_cfg.csum_type = csum_type;
	mkfs_cfg.leaf_data_size = __BTRFS_LEAF_DATA_SIZE(nodesize);
	mkfs_cfg.zone_size = 0;

	ret = make_btrfs(prepare_ctx[0].fd, &mkfs_cfg);
	if (ret) {
		goto out_close;
	}

	/* Open the freshly-created filesystem */
	struct open_ctree_args oca = { 0 };
	oca.filename = device_paths[0];
	oca.flags = OPEN_CTREE_WRITES | OPEN_CTREE_TEMPORARY_SUPER |
		    OPEN_CTREE_EXCLUSIVE;

	struct btrfs_fs_info *fs_info = open_ctree_fs_info(&oca);
	if (!fs_info) {
		ret = -EIO;
		goto out_close;
	}

	struct btrfs_root *root = fs_info->fs_root;

	/* Create metadata block groups */
	struct mkfs_allocation allocation = { 0 };
	ret = create_metadata_block_groups(root,
					   mkfs_features.incompat_flags,
					   &allocation, meta_profile);
	if (ret)
		goto out_ctree;

	/* Optional tree roots */
	if (mkfs_features.incompat_flags &
	    BTRFS_FEATURE_INCOMPAT_RAID_STRIPE_TREE) {
		ret = setup_raid_stripe_tree_root(fs_info);
		if (ret)
			goto out_ctree;
	}

	if (mkfs_features.incompat_flags &
	    BTRFS_FEATURE_INCOMPAT_REMAP_TREE) {
		ret = setup_remap_tree_root(fs_info);
		if (ret)
			goto out_ctree;
	}

	/* Start transaction for data block groups + root dir */
	struct btrfs_trans_handle *trans;
	trans = btrfs_start_transaction(root, 1);
	if (IS_ERR(trans)) {
		ret = PTR_ERR(trans);
		goto out_ctree;
	}

	ret = create_data_block_groups(trans, root, mixed, &allocation);
	if (ret)
		goto out_ctree;

	if (mkfs_features.incompat_flags &
	    BTRFS_FEATURE_INCOMPAT_EXTENT_TREE_V2) {
		int nr_global_roots = sysconf(_SC_NPROCESSORS_ONLN);
		ret = create_global_roots(trans, nr_global_roots);
		if (ret)
			goto out_ctree;
	}

	ret = make_root_dir(trans, root);
	if (ret)
		goto out_ctree;

	ret = btrfs_commit_transaction(trans, root);
	if (ret)
		goto out_ctree;

	/* Add extra devices */
	trans = btrfs_start_transaction(root, 1);
	if (IS_ERR(trans)) {
		ret = PTR_ERR(trans);
		goto out_ctree;
	}

	for (Py_ssize_t i = 1; i < device_count; i++) {
		if (prepare_ctx[i].ret) {
			ret = prepare_ctx[i].ret;
			goto out_ctree;
		}

		ret = btrfs_device_already_in_root(root, prepare_ctx[i].fd,
						   BTRFS_SUPER_INFO_OFFSET);
		if (ret)
			continue;

		u64 extra_dev_bytes = prepare_ctx[i].dev_byte_count;
		ret = btrfs_add_to_fsid(trans, root, prepare_ctx[i].fd,
					prepare_ctx[i].file, extra_dev_bytes,
					sectorsize, sectorsize, sectorsize);
		if (ret)
			goto out_ctree;
	}

	/* Create RAID groups */
	ret = create_raid_groups(trans, root, data_prof, meta_profile,
				 mixed, &allocation);
	if (ret)
		goto out_ctree;

	/*
	 * Commit and re-COW all tree blocks to new raid groups.
	 */
	ret = btrfs_commit_transaction(trans, root);
	if (ret)
		goto out_ctree;

	trans = btrfs_start_transaction(root, 1);
	if (IS_ERR(trans)) {
		ret = PTR_ERR(trans);
		goto out_ctree;
	}

	ret = recow_roots(trans, root);
	if (ret)
		goto out_ctree;

	/* Create data reloc tree */
	if (!(mkfs_features.incompat_flags &
	      BTRFS_FEATURE_INCOMPAT_REMAP_TREE)) {
		ret = btrfs_make_subvolume(trans,
					   BTRFS_DATA_RELOC_TREE_OBJECTID,
					   false);
		if (ret)
			goto out_ctree;
	}

	ret = btrfs_commit_transaction(trans, root);
	if (ret)
		goto out_ctree;

	/* Rebuild UUID tree */
	ret = btrfs_rebuild_uuid_tree(fs_info);
	if (ret)
		goto out_ctree;

	/* Clean up temporary chunks */
	ret = cleanup_temp_chunks(fs_info, &allocation, data_prof,
				  meta_profile, meta_profile, do_discard);
	if (ret)
		goto out_ctree;

	/* Capture results before closing */
	strncpy(result_uuid, mkfs_cfg.fs_uuid, BTRFS_UUID_UNPARSED_SIZE - 1);
	result_num_bytes = btrfs_super_total_bytes(fs_info->super_copy);

	fs_info->finalize_on_close = 1;

out_ctree:
	close_ret = close_ctree(root);
	if (!ret && close_ret)
		ret = close_ret;

out_close:
	for (Py_ssize_t i = 0; i < device_count; i++) {
		if (prepare_ctx[i].fd >= 0)
			close(prepare_ctx[i].fd);
	}
	free(t_prepare);
	free(prepare_ctx);

out_free:
	btrfs_close_all_devices();

	Py_END_ALLOW_THREADS

	free(device_paths);

	if (ret) {
		errno = -ret;
		return PyErr_SetFromErrno(PyExc_OSError);
	}

	return Py_BuildValue("{s:s,s:K}", "uuid", result_uuid,
			     "num_bytes", result_num_bytes);
}

/* ── method table ────────────────────────────────────────────── */

static PyMethodDef mkfs_methods[] = {
	{"mkfs", (PyCFunction)pybtrfs_mkfs, METH_VARARGS | METH_KEYWORDS,
	 pybtrfs_mkfs_doc},
	{NULL, NULL, 0, NULL},
};

/* ── module definition ───────────────────────────────────────── */

static struct PyModuleDef mkfs_module = {
	PyModuleDef_HEAD_INIT,
	.m_name    = "pybtrfs.mkfs",
	.m_doc     = "Create btrfs filesystems (wraps btrfs-progs mkfs).",
	.m_size    = -1,
	.m_methods = mkfs_methods,
};

PyMODINIT_FUNC
PyInit_mkfs(void)
{
	PyObject *m = PyModule_Create(&mkfs_module);
	if (!m)
		return NULL;

	/* Checksum type constants */
	PyModule_AddIntConstant(m, "CSUM_TYPE_CRC32",   BTRFS_CSUM_TYPE_CRC32);
	PyModule_AddIntConstant(m, "CSUM_TYPE_XXHASH",  BTRFS_CSUM_TYPE_XXHASH);
	PyModule_AddIntConstant(m, "CSUM_TYPE_SHA256",  BTRFS_CSUM_TYPE_SHA256);
	PyModule_AddIntConstant(m, "CSUM_TYPE_BLAKE2",  BTRFS_CSUM_TYPE_BLAKE2);

	/* RAID profile constants */
	PyModule_AddIntConstant(m, "RAID_SINGLE", 0);
	PyModule_AddIntConstant(m, "RAID_RAID0",
				BTRFS_BLOCK_GROUP_RAID0);
	PyModule_AddIntConstant(m, "RAID_RAID1",
				BTRFS_BLOCK_GROUP_RAID1);
	PyModule_AddIntConstant(m, "RAID_RAID1C3",
				BTRFS_BLOCK_GROUP_RAID1C3);
	PyModule_AddIntConstant(m, "RAID_RAID1C4",
				BTRFS_BLOCK_GROUP_RAID1C4);
	PyModule_AddIntConstant(m, "RAID_RAID5",
				BTRFS_BLOCK_GROUP_RAID5);
	PyModule_AddIntConstant(m, "RAID_RAID6",
				BTRFS_BLOCK_GROUP_RAID6);
	PyModule_AddIntConstant(m, "RAID_RAID10",
				BTRFS_BLOCK_GROUP_RAID10);
	PyModule_AddIntConstant(m, "RAID_DUP",
				BTRFS_BLOCK_GROUP_DUP);

	/* Feature flag constants */
	PyModule_AddIntConstant(m, "FEATURE_MIXED_GROUPS",
				BTRFS_FEATURE_INCOMPAT_MIXED_GROUPS);
	PyModule_AddIntConstant(m, "FEATURE_RAID56",
				BTRFS_FEATURE_INCOMPAT_RAID56);
	PyModule_AddIntConstant(m, "FEATURE_RAID1C34",
				BTRFS_FEATURE_INCOMPAT_RAID1C34);
	PyModule_AddIntConstant(m, "FEATURE_ZONED",
				BTRFS_FEATURE_INCOMPAT_ZONED);
	PyModule_AddIntConstant(m, "FEATURE_NO_HOLES",
				BTRFS_FEATURE_INCOMPAT_NO_HOLES);

	return m;
}
