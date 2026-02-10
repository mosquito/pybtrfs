import pytest

import pybtrfs


def test_error_on_bad_path():
    with pytest.raises(pybtrfs.BtrfsUtilError) as exc_info:
        pybtrfs.sync("/no/such/path")
    e = exc_info.value
    assert isinstance(e, OSError)
    assert hasattr(e, "btrfsutil_errno")
    assert e.btrfsutil_errno != 0


def test_error_str():
    with pytest.raises(pybtrfs.BtrfsUtilError) as exc_info:
        pybtrfs.subvolume_id("/no/such/path")
    s = str(exc_info.value)
    assert len(s) > 0


def test_error_errno():
    with pytest.raises(pybtrfs.BtrfsUtilError) as exc_info:
        pybtrfs.sync("/no/such/path")
    assert exc_info.value.errno != 0
