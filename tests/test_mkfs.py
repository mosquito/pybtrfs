import os
import subprocess
import tempfile
import uuid

import pytest

from pybtrfs import mkfs, mount, umount, CsumType, RaidProfile


def _create_loop_device(size_mb=256):
    """Create a loop device backed by a temporary file."""
    tmp = tempfile.NamedTemporaryFile(delete=False, suffix=".img")
    tmp.truncate(size_mb * 1024 * 1024)
    tmp.close()
    out = subprocess.check_output(
        ["losetup", "--find", "--show", tmp.name],
        text=True,
    ).strip()
    return out, tmp.name


def _destroy_loop_device(loop_dev, backing_file):
    """Detach a loop device and remove its backing file."""
    subprocess.call(["losetup", "-d", loop_dev])
    try:
        os.unlink(backing_file)
    except OSError:
        pass


@pytest.fixture
def loop_device():
    dev, img = _create_loop_device()
    yield dev
    _destroy_loop_device(dev, img)


@pytest.fixture
def two_loop_devices():
    dev1, img1 = _create_loop_device()
    dev2, img2 = _create_loop_device()
    yield dev1, dev2
    _destroy_loop_device(dev1, img1)
    _destroy_loop_device(dev2, img2)


@pytest.fixture
def mountpoint(tmp_path):
    mp = tmp_path / "mnt"
    mp.mkdir()
    yield str(mp)
    try:
        umount(str(mp))
    except OSError:
        pass


class TestMkfsSingleDevice:
    def test_basic_mkfs(self, loop_device):
        result = mkfs(loop_device, force=True)
        assert "uuid" in result
        assert "num_bytes" in result
        # UUID should be valid
        uuid.UUID(result["uuid"])
        assert result["num_bytes"] > 0

    def test_mkfs_with_label(self, loop_device):
        result = mkfs(loop_device, label="testlabel", force=True)
        assert result["uuid"]

    def test_mkfs_with_nodesize(self, loop_device):
        result = mkfs(loop_device, nodesize=32768, force=True)
        assert result["num_bytes"] > 0

    def test_mkfs_with_csum_xxhash(self, loop_device):
        result = mkfs(
            loop_device,
            csum_type=CsumType.XXHASH,
            force=True,
        )
        assert result["uuid"]

    def test_mkfs_with_custom_uuid(self, loop_device):
        custom_uuid = str(uuid.uuid4())
        result = mkfs(loop_device, uuid=custom_uuid, force=True)
        assert result["uuid"] == custom_uuid

    def test_mkfs_mount_write(self, loop_device, mountpoint):
        mkfs(loop_device, force=True)
        mount(loop_device, mountpoint)
        testfile = os.path.join(mountpoint, "hello.txt")
        with open(testfile, "w") as f:
            f.write("world")
        with open(testfile) as f:
            assert f.read() == "world"
        umount(mountpoint)

    def test_mkfs_no_discard(self, loop_device):
        result = mkfs(loop_device, force=True, no_discard=True)
        assert result["uuid"]

    def test_force_overwrite(self, loop_device):
        mkfs(loop_device, force=True)
        # Second mkfs with force should succeed
        result = mkfs(loop_device, force=True)
        assert result["uuid"]


class TestMkfsMultiDevice:
    def test_two_devices_raid1(self, two_loop_devices):
        dev1, dev2 = two_loop_devices
        result = mkfs(
            dev1, dev2,
            metadata_profile=RaidProfile.RAID1,
            data_profile=RaidProfile.RAID1,
            force=True,
        )
        assert result["uuid"]
        assert result["num_bytes"] > 0


class TestMkfsErrors:
    def test_bad_device_path(self):
        with pytest.raises(OSError):
            mkfs("/dev/nonexistent_device_xyz", force=True)

    def test_no_devices(self):
        with pytest.raises((TypeError, ValueError)):
            mkfs()

    def test_invalid_uuid(self, loop_device):
        with pytest.raises(ValueError, match="invalid UUID"):
            mkfs(loop_device, uuid="not-a-uuid", force=True)

    def test_label_too_long(self, loop_device):
        with pytest.raises(ValueError, match="label too long"):
            mkfs(loop_device, label="x" * 256, force=True)


class TestConstants:
    def test_csum_types_are_int(self):
        assert isinstance(CsumType.CRC32, int)
        assert isinstance(CsumType.XXHASH, int)
        assert isinstance(CsumType.SHA256, int)
        assert isinstance(CsumType.BLAKE2, int)

    def test_raid_profiles_are_int(self):
        assert isinstance(RaidProfile.SINGLE, int)
        assert isinstance(RaidProfile.RAID1, int)
        assert isinstance(RaidProfile.DUP, int)

    def test_csum_type_values_distinct(self):
        values = [e.value for e in CsumType]
        assert len(values) == len(set(values))

    def test_raid_profile_single_is_zero(self):
        assert RaidProfile.SINGLE == 0
