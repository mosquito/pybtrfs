import os

import pytest

from pybtrfs import mount, umount, mount_data, MountFlags, UmountFlags


@pytest.fixture
def mountpoint(tmp_path):
    mp = tmp_path / "mnt"
    mp.mkdir()
    yield str(mp)
    # best-effort cleanup in case test didn't umount
    try:
        umount(str(mp))
    except OSError:
        pass


def test_mount_umount_tmpfs(mountpoint):
    mount("none", mountpoint, fstype="tmpfs")
    # verify it's actually mounted
    assert os.path.ismount(mountpoint)
    umount(mountpoint)
    assert not os.path.ismount(mountpoint)


def test_mount_creates_writable_fs(mountpoint):
    mount("none", mountpoint, fstype="tmpfs")
    testfile = os.path.join(mountpoint, "hello")
    with open(testfile, "w") as f:
        f.write("world")
    with open(testfile) as f:
        assert f.read() == "world"
    umount(mountpoint)


def test_mount_rdonly(mountpoint):
    mount("none", mountpoint, fstype="tmpfs", flags=MountFlags.RDONLY)
    assert os.path.ismount(mountpoint)
    testfile = os.path.join(mountpoint, "nope")
    with pytest.raises(OSError):
        open(testfile, "w")
    umount(mountpoint)


def test_mount_bad_source(mountpoint):
    with pytest.raises(OSError):
        mount("/dev/nonexistent", mountpoint, fstype="ext4")


def test_umount_not_mounted(mountpoint):
    with pytest.raises(OSError):
        umount(mountpoint)


def test_umount_mnt_detach(mountpoint):
    mount("none", mountpoint, fstype="tmpfs")
    umount(mountpoint, flags=UmountFlags.DETACH)
    assert not os.path.ismount(mountpoint)


def test_mount_flags_are_int():
    assert isinstance(MountFlags.RDONLY, int)
    assert isinstance(UmountFlags.FORCE, int)
    assert MountFlags.RDONLY | MountFlags.NODEV == MountFlags.RDONLY + MountFlags.NODEV


def test_mount_data_single():
    assert mount_data(size="512M") == "size=512M"


def test_mount_data_multiple():
    result = mount_data(size="1G", mode="0755")
    parts = result.split(",")
    assert "size=1G" in parts
    assert "mode=0755" in parts
    assert len(parts) == 2


def test_mount_data_empty():
    assert mount_data() == ""


def test_mount_data_with_tmpfs(mountpoint):
    mount("none", mountpoint, fstype="tmpfs", data=mount_data(size="1M"))
    assert os.path.ismount(mountpoint)
    # verify size limit works — statvfs reports the constrained size
    st = os.statvfs(mountpoint)
    total = st.f_blocks * st.f_frsize
    assert total <= 1024 * 1024  # 1M
    umount(mountpoint)
