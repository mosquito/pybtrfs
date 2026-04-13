import os
import pathlib

import pytest

import pybtrfs
from pybtrfs import qgroupid, qgroupstr


def test_import():
    assert hasattr(pybtrfs, "sync")
    assert hasattr(pybtrfs, "SubvolumeInfo")
    assert hasattr(pybtrfs, "SubvolumeIterator")
    assert hasattr(pybtrfs, "QgroupInherit")
    assert hasattr(pybtrfs, "BtrfsUtilError")


def test_error_is_oserror():
    assert issubclass(pybtrfs.BtrfsUtilError, OSError)


def test_error_constants():
    assert pybtrfs.BtrfsUtilErrno.OK == 0
    assert pybtrfs.BtrfsUtilErrno.NOT_BTRFS != 0
    assert pybtrfs.BtrfsUtilErrno.NOT_SUBVOLUME != 0


class TestQGroupID:
    def test_from_string(self):
        assert qgroupid("1/256") == (1 << 48) | 256

    def test_from_string_level_zero(self):
        assert qgroupid("0/5") == 5

    def test_from_string_zero_zero(self):
        assert qgroupid("0/0") == 0

    def test_from_two_args(self):
        assert qgroupid(1, 256) == (1 << 48) | 256

    def test_from_two_args_level_zero(self):
        assert qgroupid(0, 5) == 5

    def test_from_raw_int(self):
        raw = (1 << 48) | 256
        assert qgroupid(raw) == raw

    def test_from_raw_int_zero(self):
        assert qgroupid(0) == 0

    def test_returns_int(self):
        assert isinstance(qgroupid("0/5"), int)
        assert isinstance(qgroupid(1, 256), int)

    def test_string_and_two_args_equal(self):
        assert qgroupid("1/256") == qgroupid(1, 256)

    def test_arithmetic(self):
        assert qgroupid("0/5") + 1 == 6

    def test_usable_as_dict_key(self):
        d = {qgroupid("1/1"): "parent"}
        assert d[qgroupid(1, 1)] == "parent"

    def test_invalid_string_no_slash(self):
        with pytest.raises(ValueError, match="expected 'level/id' format"):
            qgroupid("bad")

    def test_invalid_string_level(self):
        with pytest.raises(ValueError, match="invalid level"):
            qgroupid("abc/5")

    def test_invalid_string_id(self):
        with pytest.raises(ValueError, match="invalid id"):
            qgroupid("1/xyz")

    def test_wrong_arg_count_three(self):
        with pytest.raises(TypeError):
            qgroupid(1, 2, 3)

    def test_no_args(self):
        with pytest.raises(TypeError):
            qgroupid()

    def test_level_overflow_two_args(self):
        with pytest.raises(ValueError, match="level must fit in 16 bits"):
            qgroupid(0x10000, 0)

    def test_id_overflow_two_args(self):
        with pytest.raises(ValueError, match="id must fit in 48 bits"):
            qgroupid(0, 1 << 48)

    def test_level_overflow_string(self):
        with pytest.raises(ValueError, match="level must fit in 16 bits"):
            qgroupid("65536/0")

    def test_id_overflow_string(self):
        with pytest.raises(ValueError, match="id must fit in 48 bits"):
            qgroupid("0/281474976710656")

    def test_max_valid_level(self):
        assert qgroupid(0xFFFF, 0) == 0xFFFF << 48

    def test_max_valid_id(self):
        max_id = (1 << 48) - 1
        assert qgroupid(0, max_id) == max_id


class TestQGroupStr:
    def test_level_zero(self):
        assert qgroupstr(5) == "0/5"

    def test_level_one(self):
        assert qgroupstr((1 << 48) | 256) == "1/256"

    def test_zero(self):
        assert qgroupstr(0) == "0/0"

    def test_returns_str(self):
        assert isinstance(qgroupstr(5), str)

    def test_roundtrip_from_string(self):
        assert qgroupstr(qgroupid("1/256")) == "1/256"

    def test_roundtrip_from_two_args(self):
        assert qgroupstr(qgroupid(2, 100)) == "2/100"

    def test_roundtrip_inverse(self):
        assert qgroupid(qgroupstr((1 << 48) | 42)) == (1 << 48) | 42


def test_qgroup_inherit_empty():
    qg = pybtrfs.QgroupInherit()
    assert qg.get_groups() == []


def test_qgroup_inherit_add():
    qg = pybtrfs.QgroupInherit()
    qg.add_group(0)
    qg.add_group(1)
    assert qg.get_groups() == [0, 1]


class TestPathOrFdArgParsing:
    """Test that functions accept str, int (fd), and PathLike arguments.

    These tests verify argument parsing at the C boundary. They do NOT
    require a real btrfs filesystem — they just confirm that the right
    type dispatch happens (and that bad types are rejected).
    """

    # -- type rejection tests (work everywhere) -----------------------

    @pytest.mark.parametrize("func_name", [
        "sync", "start_sync", "wait_sync",
        "is_subvolume", "subvolume_id", "subvolume_path",
        "subvolume_info", "get_subvolume_read_only",
        "set_subvolume_read_only", "get_default_subvolume",
        "set_default_subvolume", "create_subvolume",
        "delete_subvolume", "deleted_subvolumes",
    ])
    def test_btrfsutils_rejects_bad_type(self, func_name):
        func = getattr(pybtrfs, func_name)
        with pytest.raises(TypeError):
            func(3.14)

    @pytest.mark.parametrize("func_name", [
        "sync", "start_sync", "wait_sync",
        "is_subvolume", "subvolume_id", "subvolume_path",
        "subvolume_info", "get_subvolume_read_only",
        "set_subvolume_read_only", "get_default_subvolume",
        "set_default_subvolume", "create_subvolume",
        "delete_subvolume", "deleted_subvolumes",
    ])
    def test_btrfsutils_rejects_bytes(self, func_name):
        func = getattr(pybtrfs, func_name)
        with pytest.raises(TypeError):
            func(b"/mnt/btrfs")

    @pytest.mark.parametrize("func_name", [
        "quota_enable", "quota_enable_simple", "quota_disable",
        "quota_rescan", "quota_rescan_status", "quota_rescan_wait",
        "qgroup_info",
    ])
    def test_quota_rejects_bad_type(self, func_name):
        func = getattr(pybtrfs, func_name)
        with pytest.raises(TypeError):
            func(3.14)

    @pytest.mark.parametrize("func_name", [
        "quota_enable", "quota_enable_simple", "quota_disable",
        "quota_rescan", "quota_rescan_status", "quota_rescan_wait",
        "qgroup_info",
    ])
    def test_quota_rejects_bytes(self, func_name):
        func = getattr(pybtrfs, func_name)
        with pytest.raises(TypeError):
            func(b"/mnt/btrfs")

    # -- PathLike acceptance (str path goes through PyOS_FSPath) ------

    @pytest.mark.parametrize("func_name", [
        "sync", "start_sync", "wait_sync",
        "is_subvolume", "subvolume_id", "subvolume_path",
        "subvolume_info", "get_subvolume_read_only",
        "set_subvolume_read_only", "get_default_subvolume",
        "set_default_subvolume", "create_subvolume",
        "delete_subvolume", "deleted_subvolumes",
    ])
    def test_btrfsutils_accepts_pathlib(self, func_name):
        """PathLike is parsed without TypeError (fails later on non-btrfs)."""
        func = getattr(pybtrfs, func_name)
        p = pathlib.PurePosixPath("/nonexistent/btrfs/path")
        with pytest.raises(OSError):
            func(p)

    # -- fd-specific create/delete validation -------------------------

    def test_create_subvolume_fd_requires_name(self, tmp_path):
        fd = os.open(str(tmp_path), os.O_RDONLY)
        try:
            with pytest.raises(ValueError, match="name is required"):
                pybtrfs.create_subvolume(fd)
        finally:
            os.close(fd)

    def test_delete_subvolume_fd_requires_name(self, tmp_path):
        fd = os.open(str(tmp_path), os.O_RDONLY)
        try:
            with pytest.raises(ValueError, match="name is required"):
                pybtrfs.delete_subvolume(fd)
        finally:
            os.close(fd)

    # -- bool rejection (bool is not a valid fd) -----------------------

    @pytest.mark.parametrize("func_name", [
        "sync", "start_sync", "wait_sync",
        "is_subvolume", "subvolume_id", "subvolume_path",
        "subvolume_info", "get_subvolume_read_only",
        "set_subvolume_read_only", "get_default_subvolume",
        "set_default_subvolume", "create_subvolume",
        "delete_subvolume", "deleted_subvolumes",
    ])
    def test_btrfsutils_rejects_bool(self, func_name):
        func = getattr(pybtrfs, func_name)
        with pytest.raises(TypeError):
            func(True)

    @pytest.mark.parametrize("func_name", [
        "quota_enable", "quota_enable_simple", "quota_disable",
        "quota_rescan", "quota_rescan_status", "quota_rescan_wait",
        "qgroup_info",
    ])
    def test_quota_rejects_bool(self, func_name):
        func = getattr(pybtrfs, func_name)
        with pytest.raises(TypeError):
            func(True)

    # -- negative fd rejection ----------------------------------------

    def test_negative_fd_rejected(self):
        with pytest.raises(ValueError, match="file descriptor out of range"):
            pybtrfs.sync(-1)

    # -- overflow fd rejection ----------------------------------------

    def test_overflow_fd_rejected(self):
        with pytest.raises(ValueError, match="file descriptor out of range"):
            pybtrfs.sync(2**63)

    # -- create_snapshot source accepts fd ----------------------------

    def test_create_snapshot_rejects_bad_source_type(self):
        with pytest.raises(TypeError):
            pybtrfs.create_snapshot(3.14, "/tmp/snap")
