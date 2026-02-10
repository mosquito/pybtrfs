import os

import pytest

import pybtrfs


class TestSnapshotBasic:
    def test_create_snapshot(self, subvol, btrfs):
        snap = os.path.join(btrfs, "_snap_basic")
        try:
            pybtrfs.create_snapshot(subvol, snap)
            assert pybtrfs.is_subvolume(snap) is True
        finally:
            pybtrfs.delete_subvolume(snap)

    def test_snapshot_preserves_data(self, subvol, btrfs):
        with open(os.path.join(subvol, "file.txt"), "w") as f:
            f.write("hello snapshot")

        snap = os.path.join(btrfs, "_snap_data")
        try:
            pybtrfs.create_snapshot(subvol, snap)
            with open(os.path.join(snap, "file.txt")) as f:
                assert f.read() == "hello snapshot"
        finally:
            pybtrfs.delete_subvolume(snap)

    def test_snapshot_is_independent(self, subvol, btrfs):
        with open(os.path.join(subvol, "orig.txt"), "w") as f:
            f.write("before")

        snap = os.path.join(btrfs, "_snap_indep")
        try:
            pybtrfs.create_snapshot(subvol, snap)

            # modify source after snapshot
            with open(os.path.join(subvol, "orig.txt"), "w") as f:
                f.write("after")
            with open(os.path.join(subvol, "new.txt"), "w") as f:
                f.write("new file")

            # snapshot still has old data
            with open(os.path.join(snap, "orig.txt")) as f:
                assert f.read() == "before"
            assert not os.path.exists(os.path.join(snap, "new.txt"))
        finally:
            pybtrfs.delete_subvolume(snap)

    def test_snapshot_parent_uuid(self, subvol, btrfs):
        snap = os.path.join(btrfs, "_snap_uuid")
        try:
            pybtrfs.create_snapshot(subvol, snap)
            src_info = pybtrfs.subvolume_info(subvol)
            snap_info = pybtrfs.subvolume_info(snap)
            assert snap_info.parent_uuid == src_info.uuid
            assert snap_info.id != src_info.id
        finally:
            pybtrfs.delete_subvolume(snap)


class TestSnapshotReadOnly:
    def test_read_only_snapshot(self, subvol, btrfs):
        with open(os.path.join(subvol, "data.txt"), "w") as f:
            f.write("ro test")

        snap = os.path.join(btrfs, "_snap_ro")
        try:
            pybtrfs.create_snapshot(subvol, snap, read_only=True)
            assert pybtrfs.get_subvolume_read_only(snap) is True

            with open(os.path.join(snap, "data.txt")) as f:
                assert f.read() == "ro test"

            with pytest.raises(OSError):
                with open(os.path.join(snap, "blocked.txt"), "w") as f:
                    f.write("should fail")
        finally:
            pybtrfs.set_subvolume_read_only(snap, False)
            pybtrfs.delete_subvolume(snap)

    def test_unset_read_only(self, subvol, btrfs):
        snap = os.path.join(btrfs, "_snap_unro")
        try:
            pybtrfs.create_snapshot(subvol, snap, read_only=True)
            assert pybtrfs.get_subvolume_read_only(snap) is True

            pybtrfs.set_subvolume_read_only(snap, False)
            assert pybtrfs.get_subvolume_read_only(snap) is False

            # now writable
            with open(os.path.join(snap, "ok.txt"), "w") as f:
                f.write("writable now")
        finally:
            pybtrfs.delete_subvolume(snap)


class TestSnapshotRecursive:
    def test_recursive_snapshot(self, subvol, btrfs):
        child = os.path.join(subvol, "child")
        pybtrfs.create_subvolume(child)
        with open(os.path.join(child, "nested.txt"), "w") as f:
            f.write("nested data")

        snap = os.path.join(btrfs, "_snap_rec")
        try:
            pybtrfs.create_snapshot(subvol, snap, recursive=True)
            assert pybtrfs.is_subvolume(snap) is True
            assert pybtrfs.is_subvolume(
                os.path.join(snap, "child")) is True

            with open(os.path.join(snap, "child", "nested.txt")) as f:
                assert f.read() == "nested data"
        finally:
            pybtrfs.delete_subvolume(snap, recursive=True)

    def test_non_recursive_skips_children(self, subvol, btrfs):
        child = os.path.join(subvol, "sub")
        pybtrfs.create_subvolume(child)
        with open(os.path.join(child, "inner.txt"), "w") as f:
            f.write("inner")

        snap = os.path.join(btrfs, "_snap_norec")
        try:
            pybtrfs.create_snapshot(subvol, snap)
            # child dir exists but is empty (non-recursive doesn't
            # snapshot nested subvolumes)
            child_in_snap = os.path.join(snap, "sub")
            assert os.path.isdir(child_in_snap)
            assert not os.path.exists(
                os.path.join(child_in_snap, "inner.txt"))
        finally:
            pybtrfs.delete_subvolume(snap, recursive=True)


class TestSnapshotChain:
    def test_snapshot_of_snapshot(self, subvol, btrfs):
        with open(os.path.join(subvol, "gen1.txt"), "w") as f:
            f.write("generation 1")

        snap1 = os.path.join(btrfs, "_snap_chain1")
        snap2 = os.path.join(btrfs, "_snap_chain2")
        try:
            pybtrfs.create_snapshot(subvol, snap1)

            with open(os.path.join(snap1, "gen2.txt"), "w") as f:
                f.write("generation 2")

            pybtrfs.create_snapshot(snap1, snap2)

            with open(os.path.join(snap2, "gen1.txt")) as f:
                assert f.read() == "generation 1"
            with open(os.path.join(snap2, "gen2.txt")) as f:
                assert f.read() == "generation 2"

            # chain: snap2.parent_uuid == snap1.uuid
            info1 = pybtrfs.subvolume_info(snap1)
            info2 = pybtrfs.subvolume_info(snap2)
            assert info2.parent_uuid == info1.uuid
        finally:
            for p in (snap2, snap1):
                try:
                    pybtrfs.delete_subvolume(p)
                except Exception:
                    pass

    def test_multiple_snapshots_from_same_source(self, subvol, btrfs):
        snaps = [os.path.join(btrfs, f"_snap_multi_{i}") for i in range(5)]
        try:
            for s in snaps:
                pybtrfs.create_snapshot(subvol, s)

            src_info = pybtrfs.subvolume_info(subvol)
            ids = set()
            for s in snaps:
                info = pybtrfs.subvolume_info(s)
                assert info.parent_uuid == src_info.uuid
                ids.add(info.id)

            # all distinct ids
            assert len(ids) == 5
        finally:
            for s in snaps:
                try:
                    pybtrfs.delete_subvolume(s)
                except Exception:
                    pass


class TestSnapshotWithLargeData:
    def test_binary_data_preserved(self, subvol, btrfs):
        data = os.urandom(1024 * 1024)  # 1 MiB
        with open(os.path.join(subvol, "blob.bin"), "wb") as f:
            f.write(data)

        snap = os.path.join(btrfs, "_snap_blob")
        try:
            pybtrfs.create_snapshot(subvol, snap)
            with open(os.path.join(snap, "blob.bin"), "rb") as f:
                assert f.read() == data
        finally:
            pybtrfs.delete_subvolume(snap)

    def test_many_files_preserved(self, subvol, btrfs):
        for i in range(200):
            with open(os.path.join(subvol, f"f_{i:04d}.txt"), "w") as f:
                f.write(f"content {i}")

        snap = os.path.join(btrfs, "_snap_many")
        try:
            pybtrfs.create_snapshot(subvol, snap)
            for i in range(200):
                with open(os.path.join(snap, f"f_{i:04d}.txt")) as f:
                    assert f.read() == f"content {i}"
        finally:
            pybtrfs.delete_subvolume(snap)


class TestSnapshotIterator:
    def test_snapshot_visible_in_iterator(self, subvol, btrfs):
        snap = os.path.join(subvol, "snap_iter")
        pybtrfs.create_snapshot(subvol, snap)

        with pybtrfs.SubvolumeIterator(subvol) as it:
            paths = [p for p, _ in it]
        assert "snap_iter" in paths

    def test_snapshot_info_via_iterator(self, subvol, btrfs):
        snap = os.path.join(subvol, "snap_info_iter")
        pybtrfs.create_snapshot(subvol, snap)

        src_info = pybtrfs.subvolume_info(subvol)
        with pybtrfs.SubvolumeIterator(subvol, info=True) as it:
            for path, info in it:
                if path == "snap_info_iter":
                    assert info.parent_uuid == src_info.uuid
                    break
            else:
                pytest.fail("snapshot not found in iterator")
