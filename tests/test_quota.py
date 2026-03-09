import os
import subprocess
import tempfile

import pytest

import pybtrfs
from pybtrfs import (
    mkfs,
    mount,
    umount,
    quota_enable,
    quota_enable_simple,
    quota_disable,
    quota_rescan,
    quota_rescan_status,
    quota_rescan_wait,
    qgroup_create,
    qgroup_destroy,
    qgroup_assign,
    qgroup_remove,
    qgroup_limit,
    qgroup_info,
    QuotaCtl,
    QgroupStatusFlags,
    QgroupLimitFlags,
)


# ── fixtures ─────────────────────────────────────────────────────────


def _create_loop_device(size_mb=256):
    tmp = tempfile.NamedTemporaryFile(delete=False, suffix=".img")
    tmp.truncate(size_mb * 1024 * 1024)
    tmp.close()
    out = subprocess.check_output(
        ["losetup", "--find", "--show", tmp.name],
        text=True,
    ).strip()
    return out, tmp.name


def _destroy_loop_device(loop_dev, backing_file):
    subprocess.call(["losetup", "-d", loop_dev])
    try:
        os.unlink(backing_file)
    except OSError:
        pass


@pytest.fixture
def btrfs_mount(tmp_path):
    """Create a fresh btrfs filesystem on a loop device, mount it,
    yield the mountpoint, then tear everything down."""
    dev, img = _create_loop_device()
    mp = str(tmp_path / "btrfs_mnt")
    os.makedirs(mp, exist_ok=True)
    try:
        mkfs(dev, force=True)
        mount(dev, mp)
        yield mp
    finally:
        try:
            umount(mp)
        except OSError:
            pass
        _destroy_loop_device(dev, img)


@pytest.fixture
def quota_enabled(btrfs_mount):
    """A btrfs mountpoint with quotas enabled; disables on teardown."""
    quota_enable(btrfs_mount)
    quota_rescan(btrfs_mount)
    quota_rescan_wait(btrfs_mount)
    yield btrfs_mount
    try:
        quota_disable(btrfs_mount)
    except OSError:
        pass


# ── tests ────────────────────────────────────────────────────────────


class TestQuotaEnableDisable:
    def test_enable_disable(self, btrfs_mount):
        quota_enable(btrfs_mount)
        quota_disable(btrfs_mount)

    def test_enable_simple(self, btrfs_mount):
        quota_enable_simple(btrfs_mount)
        quota_disable(btrfs_mount)

    def test_double_enable_raises(self, btrfs_mount):
        quota_enable(btrfs_mount)
        with pytest.raises(OSError):
            quota_enable(btrfs_mount)
        quota_disable(btrfs_mount)

    def test_disable_without_enable_raises(self, btrfs_mount):
        with pytest.raises(OSError):
            quota_disable(btrfs_mount)


class TestQuotaRescan:
    def test_rescan_and_wait(self, quota_enabled):
        quota_rescan(quota_enabled)
        quota_rescan_wait(quota_enabled)

    def test_rescan_status(self, quota_enabled):
        status = quota_rescan_status(quota_enabled)
        assert isinstance(status, dict)
        assert "flags" in status
        assert "progress" in status


class TestQgroupInfo:
    def test_returns_list(self, quota_enabled):
        info = qgroup_info(quota_enabled)
        assert isinstance(info, list)
        assert len(info) >= 1  # at least the default 0/5 qgroup

    def test_dict_keys(self, quota_enabled):
        info = qgroup_info(quota_enabled)
        expected_keys = {
            "qgroupid", "rfer", "excl",
            "rfer_cmpr", "excl_cmpr",
            "max_rfer", "max_excl",
        }
        for entry in info:
            assert set(entry.keys()) == expected_keys

    def test_values_are_int(self, quota_enabled):
        info = qgroup_info(quota_enabled)
        for entry in info:
            for v in entry.values():
                assert isinstance(v, int)


class TestQgroupCreateDestroy:
    def test_create_and_destroy(self, quota_enabled):
        # level 1 qgroup: 1/100
        qgid = (1 << 48) | 100
        qgroup_create(quota_enabled, qgid)

        # verify it appears in info
        info = qgroup_info(quota_enabled)
        ids = {e["qgroupid"] for e in info}
        assert qgid in ids

        qgroup_destroy(quota_enabled, qgid)

        # verify it's gone
        info = qgroup_info(quota_enabled)
        ids = {e["qgroupid"] for e in info}
        assert qgid not in ids

    def test_destroy_nonexistent_raises(self, quota_enabled):
        qgid = (1 << 48) | 9999
        with pytest.raises(OSError):
            qgroup_destroy(quota_enabled, qgid)


class TestQgroupAssignRemove:
    def test_assign_and_remove(self, quota_enabled):
        # create a parent qgroup 1/1
        parent = (1 << 48) | 1
        qgroup_create(quota_enabled, parent)

        # create a subvolume so we have a 0/X qgroup to assign
        subvol_path = os.path.join(quota_enabled, "sub_assign")
        pybtrfs.create_subvolume(subvol_path)
        child = pybtrfs.subvolume_id(subvol_path)

        qgroup_assign(quota_enabled, child, parent)
        qgroup_remove(quota_enabled, child, parent)

        # cleanup
        pybtrfs.delete_subvolume(subvol_path)
        qgroup_destroy(quota_enabled, parent)


class TestQgroupLimit:
    def test_set_max_rfer(self, quota_enabled):
        # get the default qgroup for root subvol (0/5)
        info = qgroup_info(quota_enabled)
        assert len(info) >= 1
        qgid = info[0]["qgroupid"]

        limit_bytes = 1024 * 1024 * 100  # 100 MiB
        qgroup_limit(quota_enabled, qgid, max_rfer=limit_bytes)

        info = qgroup_info(quota_enabled)
        entry = next(e for e in info if e["qgroupid"] == qgid)
        assert entry["max_rfer"] == limit_bytes

    def test_set_max_excl(self, quota_enabled):
        info = qgroup_info(quota_enabled)
        qgid = info[0]["qgroupid"]

        limit_bytes = 1024 * 1024 * 50  # 50 MiB
        qgroup_limit(quota_enabled, qgid, max_excl=limit_bytes)

        info = qgroup_info(quota_enabled)
        entry = next(e for e in info if e["qgroupid"] == qgid)
        assert entry["max_excl"] == limit_bytes

    def test_set_both_limits(self, quota_enabled):
        info = qgroup_info(quota_enabled)
        qgid = info[0]["qgroupid"]

        qgroup_limit(
            quota_enabled, qgid,
            max_rfer=200 * 1024 * 1024,
            max_excl=100 * 1024 * 1024,
        )

        info = qgroup_info(quota_enabled)
        entry = next(e for e in info if e["qgroupid"] == qgid)
        assert entry["max_rfer"] == 200 * 1024 * 1024
        assert entry["max_excl"] == 100 * 1024 * 1024


class TestConstants:
    def test_quota_ctl_values(self):
        assert QuotaCtl.ENABLE == 1
        assert QuotaCtl.DISABLE == 2
        assert QuotaCtl.ENABLE_SIMPLE == 4

    def test_qgroup_status_flags_are_powers_of_two(self):
        for flag in QgroupStatusFlags:
            assert flag & (flag - 1) == 0  # single bit set

    def test_qgroup_limit_flags_are_powers_of_two(self):
        for flag in QgroupLimitFlags:
            assert flag & (flag - 1) == 0

    def test_qgroup_status_flag_values(self):
        assert QgroupStatusFlags.ON == 1
        assert QgroupStatusFlags.RESCAN == 2
        assert QgroupStatusFlags.INCONSISTENT == 4
        assert QgroupStatusFlags.SIMPLE_MODE == 8

    def test_qgroup_limit_flag_values(self):
        assert QgroupLimitFlags.MAX_RFER == 1
        assert QgroupLimitFlags.MAX_EXCL == 2
        assert QgroupLimitFlags.RSV_RFER == 4
        assert QgroupLimitFlags.RSV_EXCL == 8

    def test_isinstance_int(self):
        assert isinstance(QuotaCtl.ENABLE, int)
        assert isinstance(QgroupStatusFlags.ON, int)
        assert isinstance(QgroupLimitFlags.MAX_RFER, int)
