#ifndef PYBTRFS_MODULE_H
#define PYBTRFS_MODULE_H

#define PY_SSIZE_T_CLEAN
#include <Python.h>
#include <structmember.h>
#include "btrfsutil.h"

/* BtrfsUtilError exception — defined in error.c */
extern PyObject *BtrfsUtilError;
PyObject *set_error(enum btrfs_util_error err);

/* SubvolumeInfo — defined in subvol_info.c */
extern PyTypeObject SubvolumeInfoType;
PyObject *SubvolumeInfo_from_struct(const struct btrfs_util_subvolume_info *info);

/* SubvolumeIterator — defined in iterator.c */
extern PyTypeObject SubvolumeIteratorType;

/* QgroupInherit — defined in qgroup.c */
typedef struct {
    PyObject_HEAD
    struct btrfs_util_qgroup_inherit *inherit;
} QgroupInheritObject;

extern PyTypeObject QgroupInheritType;

/* Method tables exported by each translation unit */
extern PyMethodDef sync_methods[];
extern PyMethodDef subvolume_methods[];

#endif /* PYBTRFS_MODULE_H */
