import os

import pytest

import pybtrfs


class TestIsSubvolume:
    def test_root_is_subvolume(self, btrfs):
        assert pybtrfs.is_subvolume(btrfs) is True

    def test_regular_dir_is_not(self, btrfs):
        d = os.path.join(btrfs, "_test_not_subvol")
        os.makedirs(d, exist_ok=True)
        try:
            assert pybtrfs.is_subvolume(d) is False
        finally:
            os.rmdir(d)

    def test_non_btrfs(self, tmp_path):
        assert pybtrfs.is_subvolume(str(tmp_path)) is False


class TestSubvolumeId:
    def test_root(self, btrfs):
        sid = pybtrfs.subvolume_id(btrfs)
        assert isinstance(sid, int)
        assert sid >= 5  # BTRFS_FS_TREE_OBJECTID

    def test_created(self, subvol):
        sid = pybtrfs.subvolume_id(subvol)
        assert sid > 5


class TestSubvolumePath:
    def test_root(self, btrfs):
        p = pybtrfs.subvolume_path(btrfs, 0)
        assert isinstance(p, str)

    def test_by_id(self, btrfs, subvol):
        sid = pybtrfs.subvolume_id(subvol)
        p = pybtrfs.subvolume_path(btrfs, sid)
        assert os.path.basename(subvol) in p


class TestSubvolumeInfo:
    def test_root(self, btrfs):
        info = pybtrfs.subvolume_info(btrfs)
        assert isinstance(info, pybtrfs.SubvolumeInfo)
        assert info.id >= 5
        assert isinstance(info.uuid, bytes)
        assert len(info.uuid) == 16
        assert isinstance(info.otime, float)

    def test_created(self, subvol):
        info = pybtrfs.subvolume_info(subvol)
        assert info.id > 5
        assert info.parent_id >= 5
        assert info.generation > 0

    def test_repr(self, subvol):
        info = pybtrfs.subvolume_info(subvol)
        r = repr(info)
        assert "SubvolumeInfo" in r
        assert str(info.id) in r

    def test_by_id(self, btrfs, subvol):
        sid = pybtrfs.subvolume_id(subvol)
        info = pybtrfs.subvolume_info(btrfs, sid)
        assert info.id == sid

    def test_not_found(self, btrfs):
        with pytest.raises(pybtrfs.BtrfsUtilError):
            pybtrfs.subvolume_info(btrfs, 99999999)


class TestReadOnly:
    def test_default_not_readonly(self, subvol):
        assert pybtrfs.get_subvolume_read_only(subvol) is False

    def test_set_readonly(self, subvol):
        pybtrfs.set_subvolume_read_only(subvol, True)
        assert pybtrfs.get_subvolume_read_only(subvol) is True

    def test_clear_readonly(self, subvol):
        pybtrfs.set_subvolume_read_only(subvol, True)
        pybtrfs.set_subvolume_read_only(subvol, False)
        assert pybtrfs.get_subvolume_read_only(subvol) is False


class TestDefaultSubvolume:
    def test_get(self, btrfs):
        sid = pybtrfs.get_default_subvolume(btrfs)
        assert isinstance(sid, int)
        assert sid >= 5

    def test_set_and_restore(self, btrfs):
        original = pybtrfs.get_default_subvolume(btrfs)
        pybtrfs.set_default_subvolume(btrfs, original)
        assert pybtrfs.get_default_subvolume(btrfs) == original


class TestCreateDelete:
    def test_create_and_delete(self, btrfs):
        path = os.path.join(btrfs, "_test_create_del")
        pybtrfs.create_subvolume(path)
        try:
            assert pybtrfs.is_subvolume(path) is True
            assert pybtrfs.subvolume_id(path) > 5
        finally:
            pybtrfs.delete_subvolume(path)
        assert not os.path.exists(path)

    def test_delete_nonexistent(self, btrfs):
        with pytest.raises(pybtrfs.BtrfsUtilError):
            pybtrfs.delete_subvolume(
                os.path.join(btrfs, "_no_such_subvol"))

    def test_recursive_delete(self, btrfs):
        outer = os.path.join(btrfs, "_test_recursive")
        pybtrfs.create_subvolume(outer)
        inner = os.path.join(outer, "child")
        pybtrfs.create_subvolume(inner)
        pybtrfs.delete_subvolume(outer, recursive=True)
        assert not os.path.exists(outer)


class TestSnapshot:
    def test_snapshot(self, subvol, btrfs):
        snap = os.path.join(btrfs, "_test_snap")
        try:
            pybtrfs.create_snapshot(subvol, snap)
            assert pybtrfs.is_subvolume(snap) is True

            snap_info = pybtrfs.subvolume_info(snap)
            src_info = pybtrfs.subvolume_info(subvol)
            assert snap_info.parent_uuid == src_info.uuid
        finally:
            try:
                pybtrfs.delete_subvolume(snap)
            except Exception:
                pass

    def test_snapshot_read_only(self, subvol, btrfs):
        snap = os.path.join(btrfs, "_test_snap_ro")
        try:
            pybtrfs.create_snapshot(subvol, snap, read_only=True)
            assert pybtrfs.get_subvolume_read_only(snap) is True
        finally:
            try:
                pybtrfs.set_subvolume_read_only(snap, False)
                pybtrfs.delete_subvolume(snap)
            except Exception:
                pass


class TestDeletedSubvolumes:
    def test_returns_list(self, btrfs):
        ids = pybtrfs.deleted_subvolumes(btrfs)
        assert isinstance(ids, list)
