import os
from multiprocessing.pool import ThreadPool

import pybtrfs


CONCURRENCY = int(os.environ.get("BTRFS_STRESS_CONCURRENCY", "1000"))
WORKERS = 64


def _fresh_root(btrfs, name):
    """Create a fresh subvolume, removing stale leftovers if any."""
    root = os.path.join(btrfs, name)
    if os.path.exists(root):
        pybtrfs.delete_subvolume(root, recursive=True)
    pybtrfs.create_subvolume(root)
    return root


class TestStressSubvolumes:
    def test_create_threaded(self, btrfs):
        root = _fresh_root(btrfs, "_stress_sv")
        paths = [os.path.join(root, f"sv_{i:05d}") for i in range(CONCURRENCY)]

        try:
            # create
            pool = ThreadPool(WORKERS)
            errors = list(
                pool.imap_unordered(_create_one, paths, chunksize=128),
            )
            pool.close()
            pool.join()
            failed = [e for e in errors if e is not None]
            assert not failed, f"create errors: {failed[:5]}"

            # verify count via iterator
            with pybtrfs.SubvolumeIterator(root) as it:
                found = list(it)
            assert len(found) == CONCURRENCY

            # verify each is a subvolume
            pool = ThreadPool(WORKERS)
            checks = list(
                pool.imap_unordered(_check_one, paths, chunksize=128),
            )
            pool.close()
            pool.join()
            not_subvol = [p for p, ok in checks if not ok]
            assert not not_subvol, f"not subvolumes: {not_subvol[:5]}"

            # delete
            pool = ThreadPool(WORKERS)
            errors = list(
                pool.imap_unordered(_delete_one, paths, chunksize=128),
            )
            pool.close()
            pool.join()
            failed = [e for e in errors if e is not None]
            assert not failed, f"delete errors: {failed[:5]}"

        finally:
            try:
                pybtrfs.delete_subvolume(root, recursive=True)
            except Exception:
                pass


class TestStressSnapshots:
    def test_snapshot_threaded(self, btrfs):
        root = _fresh_root(btrfs, "_stress_snap")
        source = os.path.join(root, "source")
        pybtrfs.create_subvolume(source)

        with open(os.path.join(source, "payload.txt"), "w") as f:
            f.write("snapshot stress data")

        snap_paths = [
            os.path.join(root, f"snap_{i:05d}") for i in range(CONCURRENCY)
        ]

        try:
            # create snapshots
            pool = ThreadPool(WORKERS)
            errors = list(
                pool.imap_unordered(
                    _snap_one,
                    [(source, p) for p in snap_paths],
                    chunksize=128,
                ),
            )
            pool.close()
            pool.join()
            failed = [e for e in errors if e is not None]
            assert not failed, f"snapshot create errors: {failed[:5]}"

            # verify count via iterator (source + snapshots)
            with pybtrfs.SubvolumeIterator(root) as it:
                found = list(it)
            assert len(found) == CONCURRENCY + 1  # +1 for source

            # verify each snapshot is a subvolume
            pool = ThreadPool(WORKERS)
            checks = list(
                pool.imap_unordered(_check_one, snap_paths, chunksize=128),
            )
            pool.close()
            pool.join()
            not_subvol = [p for p, ok in checks if not ok]
            assert not not_subvol, f"not subvolumes: {not_subvol[:5]}"

            # verify parent_uuid for all snapshots
            src_info = pybtrfs.subvolume_info(source)
            pool = ThreadPool(WORKERS)
            uuid_results = list(
                pool.imap_unordered(
                    _parent_uuid_one, snap_paths, chunksize=128,
                ),
            )
            pool.close()
            pool.join()
            bad_uuid = [
                p for p, parent_uuid in uuid_results
                if parent_uuid != src_info.uuid
            ]
            assert not bad_uuid, f"wrong parent_uuid: {bad_uuid[:5]}"

            # spot-check data integrity
            for p in snap_paths[:100]:
                with open(os.path.join(p, "payload.txt")) as f:
                    assert f.read() == "snapshot stress data"

            # delete snapshots
            pool = ThreadPool(WORKERS)
            errors = list(
                pool.imap_unordered(_delete_one, snap_paths, chunksize=128),
            )
            pool.close()
            pool.join()
            failed = [e for e in errors if e is not None]
            assert not failed, f"snapshot delete errors: {failed[:5]}"

        finally:
            try:
                pybtrfs.delete_subvolume(root, recursive=True)
            except Exception:
                pass

    def test_snapshot_read_only_threaded(self, btrfs):
        root = _fresh_root(btrfs, "_stress_snap_ro")
        source = os.path.join(root, "source")
        pybtrfs.create_subvolume(source)

        with open(os.path.join(source, "data.bin"), "wb") as f:
            f.write(os.urandom(4096))

        snap_paths = [
            os.path.join(root, f"rosnap_{i:05d}") for i in range(CONCURRENCY)
        ]

        try:
            # create read-only snapshots
            pool = ThreadPool(WORKERS)
            errors = list(
                pool.imap_unordered(
                    _snap_ro_one,
                    [(source, p) for p in snap_paths],
                    chunksize=128,
                ),
            )
            pool.close()
            pool.join()
            failed = [e for e in errors if e is not None]
            assert not failed, f"ro snapshot create errors: {failed[:5]}"

            # verify all are read-only
            pool = ThreadPool(WORKERS)
            ro_checks = list(
                pool.imap_unordered(
                    _check_read_only, snap_paths, chunksize=128,
                ),
            )
            pool.close()
            pool.join()
            not_ro = [p for p, ro in ro_checks if not ro]
            assert not not_ro, f"not read-only: {not_ro[:5]}"

            # delete (must unset read-only first)
            pool = ThreadPool(WORKERS)
            errors = list(
                pool.imap_unordered(
                    _unro_and_delete_one, snap_paths, chunksize=128,
                ),
            )
            pool.close()
            pool.join()
            failed = [e for e in errors if e is not None]
            assert not failed, f"ro snapshot delete errors: {failed[:5]}"

        finally:
            try:
                pybtrfs.delete_subvolume(root, recursive=True)
            except Exception:
                pass


def _create_one(path):
    try:
        pybtrfs.create_subvolume(path)
        return None
    except Exception as e:
        return f"{path}: {e}"


def _check_one(path):
    try:
        return path, pybtrfs.is_subvolume(path)
    except Exception:
        return path, False


def _delete_one(path):
    try:
        pybtrfs.delete_subvolume(path)
        return None
    except Exception as e:
        return f"{path}: {e}"


def _snap_one(args):
    source, path = args
    try:
        pybtrfs.create_snapshot(source, path)
        return None
    except Exception as e:
        return f"{path}: {e}"


def _snap_ro_one(args):
    source, path = args
    try:
        pybtrfs.create_snapshot(source, path, read_only=True)
        return None
    except Exception as e:
        return f"{path}: {e}"


def _parent_uuid_one(path):
    try:
        info = pybtrfs.subvolume_info(path)
        return path, info.parent_uuid
    except Exception:
        return path, None


def _check_read_only(path):
    try:
        return path, pybtrfs.get_subvolume_read_only(path)
    except Exception:
        return path, False


def _unro_and_delete_one(path):
    try:
        pybtrfs.set_subvolume_read_only(path, False)
        pybtrfs.delete_subvolume(path)
        return None
    except Exception as e:
        return f"{path}: {e}"
