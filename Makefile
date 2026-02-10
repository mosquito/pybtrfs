PYTHON  ?= python3
BTRFS   ?= $(HOME)/btrfs
EXT_SUFFIX := $(shell $(PYTHON) -c "import sysconfig; print(sysconfig.get_config_var('EXT_SUFFIX'))")
SO       := pybtrfs/btrfsutils$(EXT_SUFFIX)
MOUNT_SO := pybtrfs/mount$(EXT_SUFFIX)
MKFS_SO  := pybtrfs/mkfs$(EXT_SUFFIX)

MANYLINUX_IMAGE ?= quay.io/pypa/manylinux_2_28_x86_64

.PHONY: all build test stubs clean distclean install wheels sdist dist

all: build stubs

build: $(SO) $(MOUNT_SO) $(MKFS_SO)

$(SO): src/btrfsutils/*.c src/btrfsutils/*.h vendor/btrfs-progs/libbtrfsutil/*.c vendor/btrfs-progs/libbtrfsutil/*.h setup.py
	$(PYTHON) setup.py build_ext --inplace

$(MOUNT_SO): src/mount/mount.c setup.py
	$(PYTHON) setup.py build_ext --inplace

$(MKFS_SO): src/mkfs/mkfs.c src/mkfs/btrfs_config.h setup.py
	$(PYTHON) setup.py build_ext --inplace

test: $(SO)
	sudo BTRFS=$(BTRFS) PYTHONPATH=. pytest -v

stubs: $(SO) $(MOUNT_SO) $(MKFS_SO) gen_stubs.py
	PYTHONPATH=. $(PYTHON) gen_stubs.py

install: $(SO)
	$(PYTHON) -m pip install .

wheels:
	docker run --rm -v $(CURDIR):/io -w /io $(MANYLINUX_IMAGE) bash build.sh

sdist:
	$(PYTHON) setup.py sdist --dist-dir dist

dist: wheels sdist

clean:
	rm -rf build *.egg-info
	rm -f pybtrfs/*.so pybtrfs/*.pyi

distclean: clean
	rm -rf dist .pytest_cache tests/__pycache__ __pycache__ pybtrfs/__pycache__
