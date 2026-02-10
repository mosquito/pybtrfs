import os

import pybtrfs


class TestSubvolumeIterator:
    def test_empty(self, subvol):
        with pybtrfs.SubvolumeIterator(subvol) as it:
            items = list(it)
        assert items == []

    def test_lists_children(self, subvol):
        child = os.path.join(subvol, "child1")
        pybtrfs.create_subvolume(child)

        with pybtrfs.SubvolumeIterator(subvol) as it:
            items = list(it)

        assert len(items) == 1
        path, subvol_id = items[0]
        assert path == "child1"
        assert isinstance(subvol_id, int)

    def test_nested(self, subvol):
        c1 = os.path.join(subvol, "a")
        pybtrfs.create_subvolume(c1)
        c2 = os.path.join(c1, "b")
        pybtrfs.create_subvolume(c2)

        with pybtrfs.SubvolumeIterator(subvol) as it:
            paths = sorted(p for p, _ in it)

        assert paths == ["a", "a/b"]

    def test_post_order(self, subvol):
        c1 = os.path.join(subvol, "x")
        pybtrfs.create_subvolume(c1)
        c2 = os.path.join(c1, "y")
        pybtrfs.create_subvolume(c2)

        with pybtrfs.SubvolumeIterator(subvol, post_order=True) as it:
            paths = [p for p, _ in it]

        assert paths == ["x/y", "x"]

    def test_with_info(self, subvol):
        child = os.path.join(subvol, "infotest")
        pybtrfs.create_subvolume(child)

        with pybtrfs.SubvolumeIterator(subvol, info=True) as it:
            items = list(it)

        assert len(items) == 1
        path, info = items[0]
        assert path == "infotest"
        assert isinstance(info, pybtrfs.SubvolumeInfo)
        assert info.id > 5

    def test_fd_property(self, subvol):
        with pybtrfs.SubvolumeIterator(subvol) as it:
            assert isinstance(it.fd, int)
            assert it.fd >= 0

    def test_close(self, subvol):
        it = pybtrfs.SubvolumeIterator(subvol)
        it.close()
        # double close is fine
        it.close()

    def test_iterate_after_close(self, subvol):
        it = pybtrfs.SubvolumeIterator(subvol)
        it.close()
        try:
            next(it)
            assert False, "should have raised"
        except ValueError:
            pass


class TestSubvolumeIteratorTop:
    def test_top_5(self, btrfs):
        """Iterate from FS tree root (id=5)."""
        with pybtrfs.SubvolumeIterator(btrfs, top=5) as it:
            items = list(it)
        # just check it works, number depends on fs state
        for path, subvol_id in items:
            assert isinstance(path, str)
            assert isinstance(subvol_id, int)
