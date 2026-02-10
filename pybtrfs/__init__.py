from enum import IntEnum

from .btrfsutils import (
    BtrfsUtilError,
    QgroupInherit,
    SubvolumeInfo,
    SubvolumeIterator,
    create_snapshot,
    create_subvolume,
    delete_subvolume,
    deleted_subvolumes,
    get_default_subvolume,
    get_subvolume_read_only,
    is_subvolume,
    set_default_subvolume,
    set_subvolume_read_only,
    start_sync,
    subvolume_id,
    subvolume_info,
    subvolume_path,
    sync,
    wait_sync,
)
from .btrfsutils import (
    BTRFS_UTIL_CREATE_SNAPSHOT_READ_ONLY,
    BTRFS_UTIL_CREATE_SNAPSHOT_RECURSIVE,
    BTRFS_UTIL_DELETE_SUBVOLUME_RECURSIVE,
    BTRFS_UTIL_SUBVOLUME_ITERATOR_POST_ORDER,
    ERROR_INVALID_ARGUMENT,
    ERROR_NOT_BTRFS,
    ERROR_NOT_SUBVOLUME,
    ERROR_NO_MEMORY,
    ERROR_OK,
    ERROR_STOP_ITERATION,
    ERROR_SUBVOLUME_NOT_FOUND,
)
from .mount import mount, umount
from .mount import (
    MS_RDONLY,
    MS_NOSUID,
    MS_NODEV,
    MS_NOEXEC,
    MS_REMOUNT,
    MS_BIND,
    MS_REC,
    MNT_FORCE,
    MNT_DETACH,
    MNT_EXPIRE,
)
from .mkfs import mkfs as _mkfs
from .mkfs import (
    CSUM_TYPE_CRC32,
    CSUM_TYPE_XXHASH,
    CSUM_TYPE_SHA256,
    CSUM_TYPE_BLAKE2,
    RAID_SINGLE,
    RAID_RAID0,
    RAID_RAID1,
    RAID_RAID1C3,
    RAID_RAID1C4,
    RAID_RAID5,
    RAID_RAID6,
    RAID_RAID10,
    RAID_DUP,
    FEATURE_MIXED_GROUPS,
    FEATURE_RAID56,
    FEATURE_RAID1C34,
    FEATURE_ZONED,
    FEATURE_NO_HOLES,
)


class CreateSnapshotFlags(IntEnum):
    READ_ONLY = BTRFS_UTIL_CREATE_SNAPSHOT_READ_ONLY
    RECURSIVE = BTRFS_UTIL_CREATE_SNAPSHOT_RECURSIVE


class DeleteSubvolumeFlags(IntEnum):
    RECURSIVE = BTRFS_UTIL_DELETE_SUBVOLUME_RECURSIVE


class IteratorFlags(IntEnum):
    POST_ORDER = BTRFS_UTIL_SUBVOLUME_ITERATOR_POST_ORDER


class BtrfsUtilErrno(IntEnum):
    OK = ERROR_OK
    NO_MEMORY = ERROR_NO_MEMORY
    INVALID_ARGUMENT = ERROR_INVALID_ARGUMENT
    NOT_BTRFS = ERROR_NOT_BTRFS
    NOT_SUBVOLUME = ERROR_NOT_SUBVOLUME
    SUBVOLUME_NOT_FOUND = ERROR_SUBVOLUME_NOT_FOUND
    STOP_ITERATION = ERROR_STOP_ITERATION


class MountFlags(IntEnum):
    RDONLY = MS_RDONLY
    NOSUID = MS_NOSUID
    NODEV = MS_NODEV
    NOEXEC = MS_NOEXEC
    REMOUNT = MS_REMOUNT
    BIND = MS_BIND
    REC = MS_REC


class UmountFlags(IntEnum):
    FORCE = MNT_FORCE
    DETACH = MNT_DETACH
    EXPIRE = MNT_EXPIRE


class CsumType(IntEnum):
    CRC32 = CSUM_TYPE_CRC32
    XXHASH = CSUM_TYPE_XXHASH
    SHA256 = CSUM_TYPE_SHA256
    BLAKE2 = CSUM_TYPE_BLAKE2


class RaidProfile(IntEnum):
    SINGLE = RAID_SINGLE
    RAID0 = RAID_RAID0
    RAID1 = RAID_RAID1
    RAID1C3 = RAID_RAID1C3
    RAID1C4 = RAID_RAID1C4
    RAID5 = RAID_RAID5
    RAID6 = RAID_RAID6
    RAID10 = RAID_RAID10
    DUP = RAID_DUP


def mount_data(**kwargs: str) -> str:
    """Build a comma-separated mount data string from keyword arguments.

    >>> mount_data(size="512M", mode="0755")
    'size=512M,mode=0755'
    """
    return ",".join(f"{k}={v}" for k, v in kwargs.items())


def mkfs(
    *devices: str,
    label: str = "",
    nodesize: int = 16384,
    sectorsize: int = 4096,
    byte_count: int = 0,
    metadata_profile: int = -1,
    data_profile: int = 0,
    mixed: bool = False,
    features: int = 0,
    csum_type: int = 0,
    uuid: str = "",
    force: bool = False,
    no_discard: bool = False,
) -> None:
    """Create a Btrfs filesystem on the specified devices."""
    return _mkfs(
        *devices,
        label=label,
        nodesize=nodesize,
        sectorsize=sectorsize,
        byte_count=byte_count,
        metadata_profile=metadata_profile,
        data_profile=data_profile,
        mixed=mixed,
        features=features,
        csum_type=csum_type,
        uuid=uuid,
        force=force,
        no_discard=no_discard,
    )


__all__ = [
    # btrfsutils classes
    "BtrfsUtilError",
    "QgroupInherit",
    "SubvolumeInfo",
    "SubvolumeIterator",
    # btrfsutils functions
    "create_snapshot",
    "create_subvolume",
    "delete_subvolume",
    "deleted_subvolumes",
    "get_default_subvolume",
    "get_subvolume_read_only",
    "is_subvolume",
    "set_default_subvolume",
    "set_subvolume_read_only",
    "start_sync",
    "subvolume_id",
    "subvolume_info",
    "subvolume_path",
    "sync",
    "wait_sync",
    # mount functions
    "mount",
    "umount",
    "mount_data",
    # mkfs function
    "mkfs",
    # enum classes
    "CreateSnapshotFlags",
    "DeleteSubvolumeFlags",
    "IteratorFlags",
    "BtrfsUtilErrno",
    "MountFlags",
    "UmountFlags",
    "CsumType",
    "RaidProfile",
]
