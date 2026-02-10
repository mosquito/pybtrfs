import os
import pytest

import pybtrfs


def _require_root():
    if os.getuid() != 0:
        pytest.fail("must run as root (sudo)")


def get_btrfs_root():
    _require_root()
    path = os.environ.get("BTRFS")
    if not path:
        pytest.skip("BTRFS env var not set")
    if not os.path.isdir(path):
        pytest.fail(f"BTRFS={path} is not a directory")
    return path


@pytest.fixture
def btrfs():
    """Root path of a mounted btrfs filesystem."""
    return get_btrfs_root()


@pytest.fixture
def subvol(btrfs, tmp_path_factory):
    """Create a temporary subvolume, yield its path, delete on teardown."""
    name = tmp_path_factory.mktemp("subvol_").name
    path = os.path.join(btrfs, name)
    pybtrfs.create_subvolume(path)
    yield path
    try:
        pybtrfs.delete_subvolume(path, recursive=True)
    except Exception:
        pass
