#ifndef PYBTRFS_MODULE_H
#define PYBTRFS_MODULE_H

#define PY_SSIZE_T_CLEAN
#include <Python.h>
#include <structmember.h>
#include <limits.h>
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

/*
 * Parse a path-or-fd argument.
 * Returns:  1 → fd mode  (*fd set, *path=NULL, *path_obj=NULL)
 *           0 → path mode (*path set, *path_obj set — caller must Py_XDECREF)
 *          -1 → error (exception set)
 */
static inline int
parse_path_or_fd(PyObject *arg, PyObject **path_obj,
                 const char **path, int *fd)
{
    *path_obj = NULL;
    if (PyLong_Check(arg) && !PyBool_Check(arg)) {
        *path = NULL;
        int overflow;
        long val = PyLong_AsLongAndOverflow(arg, &overflow);
        if (overflow || val < 0 || val > INT_MAX) {
            PyErr_SetString(PyExc_ValueError,
                            "file descriptor out of range");
            return -1;
        }
        if (val == -1 && PyErr_Occurred())
            return -1;
        *fd = (int)val;
        return 1;
    }
    /* str / pathlib.Path / any os.PathLike */
    PyObject *str_obj = PyOS_FSPath(arg);
    if (!str_obj)
        return -1;
    if (!PyUnicode_Check(str_obj)) {
        Py_DECREF(str_obj);
        PyErr_SetString(PyExc_TypeError,
                        "expected str, int (fd), or path-like object");
        return -1;
    }
    *path = PyUnicode_AsUTF8(str_obj);
    if (!*path) {
        Py_DECREF(str_obj);
        return -1;
    }
    *path_obj = str_obj;  /* caller must Py_XDECREF after use */
    *fd = -1;
    return 0;
}

#endif /* PYBTRFS_MODULE_H */
