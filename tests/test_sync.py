import pybtrfs


def test_sync(btrfs):
    pybtrfs.sync(btrfs)


def test_start_and_wait_sync(btrfs):
    transid = pybtrfs.start_sync(btrfs)
    assert isinstance(transid, int)
    assert transid > 0
    pybtrfs.wait_sync(btrfs, transid)


def test_wait_sync_zero(btrfs):
    pybtrfs.wait_sync(btrfs, 0)
