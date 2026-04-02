#define PY_SSIZE_T_CLEAN
#include <Python.h>
#include <sys/mount.h>

/* -- mount(source, target, flags=0, data="") ----------------------- */

PyDoc_STRVAR(pybtrfs_mount_doc,
"mount(source: str, target: str, fstype: str = \"btrfs\", flags: int = 0, data: str = \"\") -> None\n\n"
"Mount a filesystem.\n\n"
"Wraps the ``mount(2)`` system call. Defaults to ``fstype='btrfs'``.\n"
"Use the *flags* parameter for mount options like ``MS_RDONLY`` and\n"
"*data* for filesystem-specific options (e.g. ``'compress=zstd'``).\n\n"
"Raises ``OSError`` on failure.\n\n"
"Example::\n\n"
"    >>> import pybtrfs\n"
"    >>> pybtrfs.mount('/dev/sda1', '/mnt/btrfs')\n"
"    >>> pybtrfs.mount(\n"
"    ...     '/dev/sda1', '/mnt/btrfs',\n"
"    ...     data='compress=zstd,subvol=@home'\n"
"    ... )\n"
"    >>> pybtrfs.mount(\n"
"    ...     '/dev/sda1', '/mnt/ro',\n"
"    ...     flags=pybtrfs.MountFlags.RDONLY\n"
"    ... )\n");

static PyObject *
pybtrfs_mount(PyObject *self, PyObject *args, PyObject *kwargs)
{
    const char *source;
    const char *target;
    const char *fstype = "btrfs";
    unsigned long flags = 0;
    const char *data = "";

    static char *kwlist[] = {"source", "target", "fstype", "flags", "data", NULL};

    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "ss|sks:mount",
                                     kwlist, &source, &target, &fstype, &flags, &data))
        return NULL;

    int ret;
    Py_BEGIN_ALLOW_THREADS
    ret = mount(source, target, fstype, flags, data);
    Py_END_ALLOW_THREADS

    if (ret != 0)
        return PyErr_SetFromErrno(PyExc_OSError);

    Py_RETURN_NONE;
}

/* -- umount(target, flags=0) --------------------------------------- */

PyDoc_STRVAR(pybtrfs_umount_doc,
"umount(target: str, flags: int = 0) -> None\n\n"
"Unmount a filesystem.\n\n"
"Wraps the ``umount2(2)`` system call. Use *flags* for options like\n"
"``MNT_FORCE`` or ``MNT_DETACH``.\n\n"
"Raises ``OSError`` on failure.\n\n"
"Example::\n\n"
"    >>> pybtrfs.umount('/mnt/btrfs')\n"
"    >>> pybtrfs.umount('/mnt/btrfs', flags=pybtrfs.UmountFlags.DETACH)\n");

static PyObject *
pybtrfs_umount(PyObject *self, PyObject *args, PyObject *kwargs)
{
    const char *target;
    int flags = 0;

    static char *kwlist[] = {"target", "flags", NULL};

    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "s|i:umount",
                                     kwlist, &target, &flags))
        return NULL;

    int ret;
    Py_BEGIN_ALLOW_THREADS
    ret = umount2(target, flags);
    Py_END_ALLOW_THREADS

    if (ret != 0)
        return PyErr_SetFromErrno(PyExc_OSError);

    Py_RETURN_NONE;
}

/* -- method table -------------------------------------------------- */

static PyMethodDef mount_methods[] = {
    {"mount",  (PyCFunction)pybtrfs_mount,  METH_VARARGS | METH_KEYWORDS, pybtrfs_mount_doc},
    {"umount", (PyCFunction)pybtrfs_umount, METH_VARARGS | METH_KEYWORDS, pybtrfs_umount_doc},
    {NULL, NULL, 0, NULL},
};

/* -- module definition --------------------------------------------- */

static struct PyModuleDef mount_module = {
    PyModuleDef_HEAD_INIT,
    .m_name    = "pybtrfs.mount",
    .m_doc     = "Low-level mount/umount helpers.",
    .m_size    = -1,
    .m_methods = mount_methods,
};

PyMODINIT_FUNC
PyInit_mount(void)
{
    PyObject *m = PyModule_Create(&mount_module);
    if (!m)
        return NULL;

    /* MS_* flags */
    PyModule_AddIntMacro(m, MS_RDONLY);
    PyModule_AddIntMacro(m, MS_NOSUID);
    PyModule_AddIntMacro(m, MS_NODEV);
    PyModule_AddIntMacro(m, MS_NOEXEC);
    PyModule_AddIntMacro(m, MS_REMOUNT);
    PyModule_AddIntMacro(m, MS_BIND);
    PyModule_AddIntMacro(m, MS_REC);

    /* MNT_* flags */
    PyModule_AddIntMacro(m, MNT_FORCE);
    PyModule_AddIntMacro(m, MNT_DETACH);
    PyModule_AddIntMacro(m, MNT_EXPIRE);

    return m;
}
