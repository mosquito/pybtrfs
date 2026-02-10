import pybtrfs


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


def test_qgroup_inherit_empty():
    qg = pybtrfs.QgroupInherit()
    assert qg.get_groups() == []


def test_qgroup_inherit_add():
    qg = pybtrfs.QgroupInherit()
    qg.add_group(0)
    qg.add_group(1)
    assert qg.get_groups() == [0, 1]
