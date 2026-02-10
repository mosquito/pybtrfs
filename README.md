# pybtrfs

Native Python bindings for btrfs — create filesystems, manage subvolumes and snapshots, mount and unmount. No shell commands, no subprocess, no runtime dependencies beyond libc.

Built as C extensions directly from vendored [btrfs-progs](https://github.com/kdave/btrfs-progs) source. Pre-built binary wheels are available on [PyPI](https://pypi.org/project/pybtrfs/) — no compiler needed.

## Why not the existing `btrfs` package?

The [`btrfs`](https://pypi.org/project/btrfs/) package on PyPI requires `libbtrfsutil` installed as a system library. That means distro packages, broken virtualenvs, and pain on minimal containers or custom builds.

`pybtrfs` statically compiles everything from vendored source — the only build-time requirement is a C compiler and Python headers. The resulting `.so` files link only to libc.

All blocking operations (ioctl calls, sync, mkfs, mount/umount) release the GIL, so they can safely run in parallel from multiple threads without blocking the interpreter.

## Requirements

- Linux with btrfs support
- Python 3.10+

## Installation

Pre-built wheels for x86_64 and aarch64 (manylinux_2_28):

```bash
pip install pybtrfs
```

Releases with wheels are also available on [GitHub](https://github.com/mosquito/pybtrfs/releases).

To build from source (requires GCC and Python headers):

```bash
pip install .
```

Or with make:

```bash
make build      # compile extensions
make install    # pip install .
```

## Usage

### Subvolumes and snapshots

```python
import pybtrfs

# Create a subvolume
pybtrfs.create_subvolume("/mnt/data/project")

# Snapshot before a risky operation
pybtrfs.create_snapshot("/mnt/data/project", "/mnt/data/project-snap",
                        read_only=True)

# Check snapshot relationship
src = pybtrfs.subvolume_info("/mnt/data/project")
snap = pybtrfs.subvolume_info("/mnt/data/project-snap")
assert snap.parent_uuid == src.uuid

# Clean up
pybtrfs.delete_subvolume("/mnt/data/project-snap")
```

### List all subvolumes

```python
with pybtrfs.SubvolumeIterator("/mnt/data", info=True) as it:
    for path, info in it:
        print(f"{path} (id={info.id}, gen={info.generation})")
```

### Create a filesystem

```python
from pybtrfs import mkfs, CsumType, RaidProfile

# Single device
result = mkfs("/dev/sdb", label="data", force=True)
print(result["uuid"])

# RAID1 mirror with xxhash checksums
result = mkfs("/dev/sdb", "/dev/sdc",
              data_profile=RaidProfile.RAID1,
              metadata_profile=RaidProfile.RAID1,
              csum_type=CsumType.XXHASH,
              force=True)
```

### Mount and unmount

```python
from pybtrfs import mount, umount, mount_data, MountFlags

mount("/dev/sdb", "/mnt/data",
      data=mount_data(compress="zstd", space_cache="v2"))

mount("/dev/sdb", "/mnt/readonly", flags=MountFlags.RDONLY)

umount("/mnt/data")
```

### Error handling

```python
import pybtrfs

try:
    pybtrfs.sync("/not/btrfs")
except pybtrfs.BtrfsUtilError as e:
    print(e.btrfsutil_errno)  # e.g. BtrfsUtilErrno.NOT_BTRFS
    print(e.errno)            # OS errno
```

## API reference

The package ships with `.pyi` stubs — full signatures and docstrings are available via `help(pybtrfs)`, `help(pybtrfs.mkfs)`, `help(pybtrfs.mount)`, and your IDE's autocomplete.

## Testing

Tests require root and a mounted btrfs filesystem:

```bash
truncate -s 1G /tmp/btrfs.img
mkfs.btrfs /tmp/btrfs.img
mkdir -p ~/btrfs
mount /tmp/btrfs.img ~/btrfs

sudo BTRFS=~/btrfs PYTHONPATH=. pytest -v
```

## License

GPL-2.0 — see [LICENSE](LICENSE).

This project statically compiles code from [btrfs-progs](https://github.com/kdave/btrfs-progs) (GPL-2.0), including `libbtrfsutil` (LGPL-2.1) and substantial portions of the core btrfs-progs codebase (kernel-shared, mkfs, crypto, etc.) which are GPL-2.0. Since the resulting binaries are derivative works of GPL-2.0 code, this project is licensed under GPL-2.0 to comply with the upstream license terms.
