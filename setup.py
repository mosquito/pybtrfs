from pathlib import Path

from setuptools import setup, Extension

long_description = Path("README.md").read_text(encoding="utf-8")

pybtrfs = Extension(
    "pybtrfs.btrfsutils",
    sources=[
        "src/btrfsutils/module.c",
        "src/btrfsutils/error.c",
        "src/btrfsutils/subvol_info.c",
        "src/btrfsutils/iterator.c",
        "src/btrfsutils/qgroup.c",
        "src/btrfsutils/sync.c",
        "src/btrfsutils/subvolume.c",
        "vendor/btrfs-progs/libbtrfsutil/errors.c",
        "vendor/btrfs-progs/libbtrfsutil/filesystem.c",
        "vendor/btrfs-progs/libbtrfsutil/qgroup.c",
        "vendor/btrfs-progs/libbtrfsutil/subvolume.c",
        "vendor/btrfs-progs/libbtrfsutil/stubs.c",
    ],
    include_dirs=["src/btrfsutils", "vendor/btrfs-progs/libbtrfsutil"],
    define_macros=[("_GNU_SOURCE", "1"), ("HAVE_REALLOCARRAY", "1")],
)

mount_ext = Extension(
    "pybtrfs.mount",
    sources=["src/mount/mount.c"],
    define_macros=[("_GNU_SOURCE", "1")],
)

_VENDOR = "vendor/btrfs-progs"

mkfs_ext = Extension(
    "pybtrfs.mkfs",
    sources=[
        "src/mkfs/mkfs.c",
        "src/mkfs/compat.c",
        # mkfs
        f"{_VENDOR}/mkfs/common.c",
        # kernel-lib
        f"{_VENDOR}/kernel-lib/list_sort.c",
        f"{_VENDOR}/kernel-lib/raid56.c",
        f"{_VENDOR}/kernel-lib/rbtree.c",
        f"{_VENDOR}/kernel-lib/tables.c",
        # kernel-shared
        f"{_VENDOR}/kernel-shared/accessors.c",
        f"{_VENDOR}/kernel-shared/async-thread.c",
        f"{_VENDOR}/kernel-shared/backref.c",
        f"{_VENDOR}/kernel-shared/ctree.c",
        f"{_VENDOR}/kernel-shared/delayed-ref.c",
        f"{_VENDOR}/kernel-shared/dir-item.c",
        f"{_VENDOR}/kernel-shared/disk-io.c",
        f"{_VENDOR}/kernel-shared/extent-io-tree.c",
        f"{_VENDOR}/kernel-shared/extent-tree.c",
        f"{_VENDOR}/kernel-shared/extent_io.c",
        f"{_VENDOR}/kernel-shared/file-item.c",
        f"{_VENDOR}/kernel-shared/file.c",
        f"{_VENDOR}/kernel-shared/free-space-cache.c",
        f"{_VENDOR}/kernel-shared/free-space-tree.c",
        f"{_VENDOR}/kernel-shared/inode-item.c",
        f"{_VENDOR}/kernel-shared/inode.c",
        f"{_VENDOR}/kernel-shared/locking.c",
        f"{_VENDOR}/kernel-shared/messages.c",
        f"{_VENDOR}/kernel-shared/print-tree.c",
        f"{_VENDOR}/kernel-shared/root-tree.c",
        f"{_VENDOR}/kernel-shared/transaction.c",
        f"{_VENDOR}/kernel-shared/tree-checker.c",
        f"{_VENDOR}/kernel-shared/ulist.c",
        f"{_VENDOR}/kernel-shared/uuid-tree.c",
        f"{_VENDOR}/kernel-shared/volumes.c",
        f"{_VENDOR}/kernel-shared/zoned.c",
        # common
        f"{_VENDOR}/common/array.c",
        f"{_VENDOR}/common/compat.c",
        f"{_VENDOR}/common/cpu-utils.c",
        f"{_VENDOR}/common/device-scan.c",
        f"{_VENDOR}/common/device-utils.c",
        f"{_VENDOR}/common/extent-cache.c",
        f"{_VENDOR}/common/extent-tree-utils.c",
        f"{_VENDOR}/common/root-tree-utils.c",
        f"{_VENDOR}/common/filesystem-utils.c",
        f"{_VENDOR}/common/format-output.c",
        f"{_VENDOR}/common/fsfeatures.c",
        f"{_VENDOR}/common/help.c",
        f"{_VENDOR}/common/inject-error.c",
        f"{_VENDOR}/common/messages.c",
        f"{_VENDOR}/common/open-utils.c",
        f"{_VENDOR}/common/parse-utils.c",
        f"{_VENDOR}/common/path-utils.c",
        f"{_VENDOR}/common/rbtree-utils.c",
        f"{_VENDOR}/common/send-stream.c",
        f"{_VENDOR}/common/send-utils.c",
        f"{_VENDOR}/common/sort-utils.c",
        f"{_VENDOR}/common/string-table.c",
        f"{_VENDOR}/common/string-utils.c",
        f"{_VENDOR}/common/sysfs-utils.c",
        f"{_VENDOR}/common/task-utils.c",
        f"{_VENDOR}/common/units.c",
        f"{_VENDOR}/common/utils.c",
        # check
        f"{_VENDOR}/check/qgroup-verify.c",
        f"{_VENDOR}/check/repair.c",
        # cmds (receive-dump needed by send-utils)
        f"{_VENDOR}/cmds/receive-dump.c",
        # crypto
        f"{_VENDOR}/crypto/crc32c.c",
        f"{_VENDOR}/crypto/hash.c",
        f"{_VENDOR}/crypto/xxhash.c",
        f"{_VENDOR}/crypto/sha224-256.c",
        f"{_VENDOR}/crypto/blake2b-ref.c",
        f"{_VENDOR}/crypto/blake2b-sse2.c",
        f"{_VENDOR}/crypto/blake2b-sse41.c",
        f"{_VENDOR}/crypto/blake2b-avx2.c",
        f"{_VENDOR}/crypto/sha256-x86.c",
        # libbtrfsutil (stubs + subvolume needed by volumes.c)
        f"{_VENDOR}/libbtrfsutil/stubs.c",
        f"{_VENDOR}/libbtrfsutil/subvolume.c",
    ],
    include_dirs=[
        "src/mkfs",
        _VENDOR,
        f"{_VENDOR}/include",
        f"{_VENDOR}/libbtrfsutil",
    ],
    libraries=[],
    extra_compile_args=[
        "-std=gnu11",
        "-include", "src/mkfs/btrfs_config.h",
        "-fno-strict-aliasing",
        "-Wno-unused-function",
        "-Wno-unused-variable",
        "-Wno-unused-but-set-variable",
        "-Wno-address-of-packed-member",
    ],
    define_macros=[
        ("_GNU_SOURCE", "1"),
        ("BTRFS_FLAT_INCLUDES", "1"),
    ],
)

setup(
    name="pybtrfs",
    version="0.5.0",
    description="Static Python bindings for btrfs ioctl operations",
    long_description=long_description,
    long_description_content_type="text/markdown",
    license="GPL-2.0",
    packages=["pybtrfs"],
    package_data={"pybtrfs": ["py.typed", "*.pyi"]},
    ext_modules=[pybtrfs, mount_ext, mkfs_ext],
)
