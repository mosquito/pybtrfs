#define PY_SSIZE_T_CLEAN
#include <Python.h>
#include <fcntl.h>
#include <limits.h>
#include <sys/ioctl.h>
#include <endian.h>

#include <string.h>

#include "kernel-shared/uapi/btrfs.h"
#include "kernel-shared/uapi/btrfs_tree.h"

/* -- helpers ------------------------------------------------------- */

static int
open_path(const char *path)
{
    int fd = open(path, O_RDONLY);
    if (fd < 0)
        PyErr_SetFromErrnoWithFilename(PyExc_OSError, path);
    return fd;
}

/*
 * Parse a path-or-fd argument.
 * Returns:  1 → fd mode  (*fd set, *path=NULL, *path_obj=NULL)
 *           0 → path mode (*path set, *path_obj set — caller must Py_XDECREF)
 *          -1 → error (exception set)
 */
static int
parse_path_or_fd(PyObject *arg, PyObject **path_obj,
                 const char **path, int *fd)
{
    *path_obj = NULL;
    if (PyLong_Check(arg) && !PyBool_Check(arg)) {
        *path = NULL;
        int overflow;
        long val = PyLong_AsLongAndOverflow(arg, &overflow);
        if (val == -1 && PyErr_Occurred())
            return -1;
        if (overflow || val < 0 || val > INT_MAX) {
            PyErr_SetString(PyExc_ValueError,
                            "file descriptor out of range");
            return -1;
        }
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

/*
 * Resolve path-or-fd to an fd for ioctl use.
 * If path mode: opens the path, sets *opened = 1 (caller must close).
 * If fd mode: uses fd directly, sets *opened = 0.
 * Returns fd on success, -1 on error (exception set).
 */
static int
resolve_fd(int is_fd, const char *path, int fd, int *opened)
{
    if (is_fd) {
        *opened = 0;
        return fd;
    }
    *opened = 1;
    return open_path(path);
}

/* -- quota_enable(path) -------------------------------------------- */

PyDoc_STRVAR(quota_enable_doc,
"quota_enable(path: str | int | os.PathLike) -> None\n\n"
"Enable btrfs quotas on the filesystem at *path*.\n\n"
"*path* may be a filesystem path, a path-like object, or an open\n"
"file descriptor (int).\n\n"
"Calls BTRFS_IOC_QUOTA_CTL with BTRFS_QUOTA_CTL_ENABLE.\n\n"
"Example::\n\n"
"    >>> pybtrfs.quota_enable('/mnt/btrfs')\n"
"    >>> pybtrfs.quota_rescan_wait('/mnt/btrfs')  # wait for initial rescan\n");

static PyObject *
pybtrfs_quota_enable(PyObject *self, PyObject *args)
{
    PyObject *path_or_fd;
    if (!PyArg_ParseTuple(args, "O:quota_enable", &path_or_fd))
        return NULL;

    const char *path; int fd;
    PyObject *path_obj;
    int is_fd = parse_path_or_fd(path_or_fd, &path_obj, &path, &fd);
    if (is_fd < 0)
        return NULL;

    int opened;
    fd = resolve_fd(is_fd, path, fd, &opened);
    if (fd < 0) { Py_XDECREF(path_obj); return NULL; }

    struct btrfs_ioctl_quota_ctl_args qargs = {
        .cmd = BTRFS_QUOTA_CTL_ENABLE,
    };

    int ret;
    Py_BEGIN_ALLOW_THREADS
    ret = ioctl(fd, BTRFS_IOC_QUOTA_CTL, &qargs);
    Py_END_ALLOW_THREADS

    if (opened) close(fd);
    Py_XDECREF(path_obj);
    if (ret < 0) {
        if (path)
            return PyErr_SetFromErrnoWithFilename(PyExc_OSError, path);
        return PyErr_SetFromErrno(PyExc_OSError);
    }

    Py_RETURN_NONE;
}

/* -- quota_enable_simple(path) ------------------------------------- */

PyDoc_STRVAR(quota_enable_simple_doc,
"quota_enable_simple(path: str | int | os.PathLike) -> None\n\n"
"Enable simple quotas (squota) on the filesystem at *path*.\n\n"
"*path* may be a filesystem path, a path-like object, or an open\n"
"file descriptor (int).\n\n"
"Simple quotas have lower overhead than full quotas and do not require\n"
"a rescan. Available since Linux 6.7.\n\n"
"Calls BTRFS_IOC_QUOTA_CTL with BTRFS_QUOTA_CTL_ENABLE_SIMPLE_QUOTA.\n\n"
"Example::\n\n"
"    >>> pybtrfs.quota_enable_simple('/mnt/btrfs')\n");

static PyObject *
pybtrfs_quota_enable_simple(PyObject *self, PyObject *args)
{
    PyObject *path_or_fd;
    if (!PyArg_ParseTuple(args, "O:quota_enable_simple", &path_or_fd))
        return NULL;

    const char *path; int fd;
    PyObject *path_obj;
    int is_fd = parse_path_or_fd(path_or_fd, &path_obj, &path, &fd);
    if (is_fd < 0)
        return NULL;

    int opened;
    fd = resolve_fd(is_fd, path, fd, &opened);
    if (fd < 0) { Py_XDECREF(path_obj); return NULL; }

    struct btrfs_ioctl_quota_ctl_args qargs = {
        .cmd = BTRFS_QUOTA_CTL_ENABLE_SIMPLE_QUOTA,
    };

    int ret;
    Py_BEGIN_ALLOW_THREADS
    ret = ioctl(fd, BTRFS_IOC_QUOTA_CTL, &qargs);
    Py_END_ALLOW_THREADS

    if (opened) close(fd);
    Py_XDECREF(path_obj);
    if (ret < 0) {
        if (path)
            return PyErr_SetFromErrnoWithFilename(PyExc_OSError, path);
        return PyErr_SetFromErrno(PyExc_OSError);
    }

    Py_RETURN_NONE;
}

/* -- quota_disable(path) ------------------------------------------- */

PyDoc_STRVAR(quota_disable_doc,
"quota_disable(path: str | int | os.PathLike) -> None\n\n"
"Disable btrfs quotas on the filesystem at *path*.\n\n"
"*path* may be a filesystem path, a path-like object, or an open\n"
"file descriptor (int).\n\n"
"Calls BTRFS_IOC_QUOTA_CTL with BTRFS_QUOTA_CTL_DISABLE.\n\n"
"Example::\n\n"
"    >>> pybtrfs.quota_disable('/mnt/btrfs')\n");

static PyObject *
pybtrfs_quota_disable(PyObject *self, PyObject *args)
{
    PyObject *path_or_fd;
    if (!PyArg_ParseTuple(args, "O:quota_disable", &path_or_fd))
        return NULL;

    const char *path; int fd;
    PyObject *path_obj;
    int is_fd = parse_path_or_fd(path_or_fd, &path_obj, &path, &fd);
    if (is_fd < 0)
        return NULL;

    int opened;
    fd = resolve_fd(is_fd, path, fd, &opened);
    if (fd < 0) { Py_XDECREF(path_obj); return NULL; }

    struct btrfs_ioctl_quota_ctl_args qargs = {
        .cmd = BTRFS_QUOTA_CTL_DISABLE,
    };

    int ret;
    Py_BEGIN_ALLOW_THREADS
    ret = ioctl(fd, BTRFS_IOC_QUOTA_CTL, &qargs);
    Py_END_ALLOW_THREADS

    if (opened) close(fd);
    Py_XDECREF(path_obj);
    if (ret < 0) {
        if (path)
            return PyErr_SetFromErrnoWithFilename(PyExc_OSError, path);
        return PyErr_SetFromErrno(PyExc_OSError);
    }

    Py_RETURN_NONE;
}

/* -- quota_rescan(path) -------------------------------------------- */

PyDoc_STRVAR(quota_rescan_doc,
"quota_rescan(path: str | int | os.PathLike) -> None\n\n"
"Start a quota rescan on the filesystem at *path*.\n\n"
"*path* may be a filesystem path, a path-like object, or an open\n"
"file descriptor (int).\n\n"
"A rescan recalculates all qgroup counters by walking the extent tree.\n"
"Use :func:`quota_rescan_wait` to block until the rescan completes.\n\n"
"Calls BTRFS_IOC_QUOTA_RESCAN.\n\n"
"Example::\n\n"
"    >>> pybtrfs.quota_rescan('/mnt/btrfs')\n"
"    >>> pybtrfs.quota_rescan_wait('/mnt/btrfs')\n");

static PyObject *
pybtrfs_quota_rescan(PyObject *self, PyObject *args)
{
    PyObject *path_or_fd;
    if (!PyArg_ParseTuple(args, "O:quota_rescan", &path_or_fd))
        return NULL;

    const char *path; int fd;
    PyObject *path_obj;
    int is_fd = parse_path_or_fd(path_or_fd, &path_obj, &path, &fd);
    if (is_fd < 0)
        return NULL;

    int opened;
    fd = resolve_fd(is_fd, path, fd, &opened);
    if (fd < 0) { Py_XDECREF(path_obj); return NULL; }

    struct btrfs_ioctl_quota_rescan_args rargs;
    memset(&rargs, 0, sizeof(rargs));

    int ret;
    Py_BEGIN_ALLOW_THREADS
    ret = ioctl(fd, BTRFS_IOC_QUOTA_RESCAN, &rargs);
    Py_END_ALLOW_THREADS

    if (opened) close(fd);
    Py_XDECREF(path_obj);
    if (ret < 0) {
        if (path)
            return PyErr_SetFromErrnoWithFilename(PyExc_OSError, path);
        return PyErr_SetFromErrno(PyExc_OSError);
    }

    Py_RETURN_NONE;
}

/* -- quota_rescan_status(path) ------------------------------------- */

PyDoc_STRVAR(quota_rescan_status_doc,
"quota_rescan_status(path: str | int | os.PathLike) -> dict\n\n"
"Return the current quota rescan status as ``{\"flags\": int, \"progress\": int}``.\n\n"
"*path* may be a filesystem path, a path-like object, or an open\n"
"file descriptor (int).\n\n"
"The *flags* field is non-zero while a rescan is in progress.\n"
"The *progress* field indicates how far the rescan has advanced.\n\n"
"Calls BTRFS_IOC_QUOTA_RESCAN_STATUS.\n\n"
"Example::\n\n"
"    >>> pybtrfs.quota_rescan_status('/mnt/btrfs')\n"
"    {'flags': 0, 'progress': 0}\n");

static PyObject *
pybtrfs_quota_rescan_status(PyObject *self, PyObject *args)
{
    PyObject *path_or_fd;
    if (!PyArg_ParseTuple(args, "O:quota_rescan_status", &path_or_fd))
        return NULL;

    const char *path; int fd;
    PyObject *path_obj;
    int is_fd = parse_path_or_fd(path_or_fd, &path_obj, &path, &fd);
    if (is_fd < 0)
        return NULL;

    int opened;
    fd = resolve_fd(is_fd, path, fd, &opened);
    if (fd < 0) { Py_XDECREF(path_obj); return NULL; }

    struct btrfs_ioctl_quota_rescan_args rargs;
    memset(&rargs, 0, sizeof(rargs));

    int ret;
    Py_BEGIN_ALLOW_THREADS
    ret = ioctl(fd, BTRFS_IOC_QUOTA_RESCAN_STATUS, &rargs);
    Py_END_ALLOW_THREADS

    if (opened) close(fd);
    Py_XDECREF(path_obj);
    if (ret < 0) {
        if (path)
            return PyErr_SetFromErrnoWithFilename(PyExc_OSError, path);
        return PyErr_SetFromErrno(PyExc_OSError);
    }

    return Py_BuildValue("{s:K,s:K}",
                         "flags",    (unsigned long long)rargs.flags,
                         "progress", (unsigned long long)rargs.progress);
}

/* -- quota_rescan_wait(path) --------------------------------------- */

PyDoc_STRVAR(quota_rescan_wait_doc,
"quota_rescan_wait(path: str | int | os.PathLike) -> None\n\n"
"Block until the current quota rescan completes.\n\n"
"*path* may be a filesystem path, a path-like object, or an open\n"
"file descriptor (int).\n\n"
"Releases the GIL while waiting so other Python threads can run.\n\n"
"Calls BTRFS_IOC_QUOTA_RESCAN_WAIT.\n\n"
"Example::\n\n"
"    >>> pybtrfs.quota_rescan('/mnt/btrfs')\n"
"    >>> pybtrfs.quota_rescan_wait('/mnt/btrfs')  # blocks until done\n");

static PyObject *
pybtrfs_quota_rescan_wait(PyObject *self, PyObject *args)
{
    PyObject *path_or_fd;
    if (!PyArg_ParseTuple(args, "O:quota_rescan_wait", &path_or_fd))
        return NULL;

    const char *path; int fd;
    PyObject *path_obj;
    int is_fd = parse_path_or_fd(path_or_fd, &path_obj, &path, &fd);
    if (is_fd < 0)
        return NULL;

    int opened;
    fd = resolve_fd(is_fd, path, fd, &opened);
    if (fd < 0) { Py_XDECREF(path_obj); return NULL; }

    int ret;
    Py_BEGIN_ALLOW_THREADS
    ret = ioctl(fd, BTRFS_IOC_QUOTA_RESCAN_WAIT, NULL);
    Py_END_ALLOW_THREADS

    if (opened) close(fd);
    Py_XDECREF(path_obj);
    if (ret < 0) {
        if (path)
            return PyErr_SetFromErrnoWithFilename(PyExc_OSError, path);
        return PyErr_SetFromErrno(PyExc_OSError);
    }

    Py_RETURN_NONE;
}

/* -- qgroup_create(path, qgroupid) -------------------------------- */

PyDoc_STRVAR(qgroup_create_doc,
"qgroup_create(path: str | int | os.PathLike, qgroupid: int) -> None\n\n"
"Create a new qgroup with the given *qgroupid*.\n\n"
"*path* may be a filesystem path, a path-like object, or an open\n"
"file descriptor (int).\n\n"
"The qgroupid is a 64-bit value encoding ``(level << 48) | id``.\n"
"Level-0 qgroups are created automatically for each subvolume;\n"
"use this for higher-level qgroups.\n\n"
"Calls BTRFS_IOC_QGROUP_CREATE with create=1.\n\n"
"Example::\n\n"
"    >>> from pybtrfs import qgroupid, qgroup_create\n"
"    >>> qgroup_create('/mnt/btrfs', qgroupid(1, 100))\n");

static PyObject *
pybtrfs_qgroup_create(PyObject *self, PyObject *args)
{
    PyObject *path_or_fd;
    unsigned long long qgroupid;
    if (!PyArg_ParseTuple(args, "OK:qgroup_create", &path_or_fd, &qgroupid))
        return NULL;

    const char *path; int fd;
    PyObject *path_obj;
    int is_fd = parse_path_or_fd(path_or_fd, &path_obj, &path, &fd);
    if (is_fd < 0)
        return NULL;

    int opened;
    fd = resolve_fd(is_fd, path, fd, &opened);
    if (fd < 0) { Py_XDECREF(path_obj); return NULL; }

    struct btrfs_ioctl_qgroup_create_args cargs = {
        .create = 1,
        .qgroupid = qgroupid,
    };

    int ret;
    Py_BEGIN_ALLOW_THREADS
    ret = ioctl(fd, BTRFS_IOC_QGROUP_CREATE, &cargs);
    Py_END_ALLOW_THREADS

    if (opened) close(fd);
    Py_XDECREF(path_obj);
    if (ret < 0) {
        if (path)
            return PyErr_SetFromErrnoWithFilename(PyExc_OSError, path);
        return PyErr_SetFromErrno(PyExc_OSError);
    }

    Py_RETURN_NONE;
}

/* -- qgroup_destroy(path, qgroupid) ------------------------------- */

PyDoc_STRVAR(qgroup_destroy_doc,
"qgroup_destroy(path: str | int | os.PathLike, qgroupid: int) -> None\n\n"
"Destroy an existing qgroup.\n\n"
"*path* may be a filesystem path, a path-like object, or an open\n"
"file descriptor (int).\n\n"
"The qgroup must have no child assignments. Raises ``OSError``\n"
"if the qgroup does not exist.\n\n"
"Calls BTRFS_IOC_QGROUP_CREATE with create=0.\n\n"
"Example::\n\n"
"    >>> from pybtrfs import qgroupid, qgroup_destroy\n"
"    >>> qgroup_destroy('/mnt/btrfs', qgroupid(1, 100))\n");

static PyObject *
pybtrfs_qgroup_destroy(PyObject *self, PyObject *args)
{
    PyObject *path_or_fd;
    unsigned long long qgroupid;
    if (!PyArg_ParseTuple(args, "OK:qgroup_destroy", &path_or_fd, &qgroupid))
        return NULL;

    const char *path; int fd;
    PyObject *path_obj;
    int is_fd = parse_path_or_fd(path_or_fd, &path_obj, &path, &fd);
    if (is_fd < 0)
        return NULL;

    int opened;
    fd = resolve_fd(is_fd, path, fd, &opened);
    if (fd < 0) { Py_XDECREF(path_obj); return NULL; }

    struct btrfs_ioctl_qgroup_create_args cargs = {
        .create = 0,
        .qgroupid = qgroupid,
    };

    int ret;
    Py_BEGIN_ALLOW_THREADS
    ret = ioctl(fd, BTRFS_IOC_QGROUP_CREATE, &cargs);
    Py_END_ALLOW_THREADS

    if (opened) close(fd);
    Py_XDECREF(path_obj);
    if (ret < 0) {
        if (path)
            return PyErr_SetFromErrnoWithFilename(PyExc_OSError, path);
        return PyErr_SetFromErrno(PyExc_OSError);
    }

    Py_RETURN_NONE;
}

/* -- qgroup_assign(path, src, dst) --------------------------------- */

PyDoc_STRVAR(qgroup_assign_doc,
"qgroup_assign(path: str | int | os.PathLike, src: int, dst: int) -> None\n\n"
"Assign qgroup *src* as a child of qgroup *dst*.\n\n"
"*path* may be a filesystem path, a path-like object, or an open\n"
"file descriptor (int).\n\n"
"This makes *dst* a parent qgroup that tracks the combined usage\n"
"of its children. Typically *src* is a level-0 qgroup (subvolume)\n"
"and *dst* is a higher-level qgroup.\n\n"
"Calls BTRFS_IOC_QGROUP_ASSIGN with assign=1.\n\n"
"Example::\n\n"
"    >>> from pybtrfs import qgroupid, qgroup_assign\n"
"    >>> parent = qgroupid(1, 1)    # qgroup 1/1\n"
"    >>> child = qgroupid(0, 256)   # qgroup 0/256 (subvolume)\n"
"    >>> qgroup_assign('/mnt/btrfs', child, parent)\n");

static PyObject *
pybtrfs_qgroup_assign(PyObject *self, PyObject *args)
{
    PyObject *path_or_fd;
    unsigned long long src, dst;
    if (!PyArg_ParseTuple(args, "OKK:qgroup_assign",
                          &path_or_fd, &src, &dst))
        return NULL;

    const char *path; int fd;
    PyObject *path_obj;
    int is_fd = parse_path_or_fd(path_or_fd, &path_obj, &path, &fd);
    if (is_fd < 0)
        return NULL;

    int opened;
    fd = resolve_fd(is_fd, path, fd, &opened);
    if (fd < 0) { Py_XDECREF(path_obj); return NULL; }

    struct btrfs_ioctl_qgroup_assign_args aargs = {
        .assign = 1,
        .src = src,
        .dst = dst,
    };

    int ret;
    Py_BEGIN_ALLOW_THREADS
    ret = ioctl(fd, BTRFS_IOC_QGROUP_ASSIGN, &aargs);
    Py_END_ALLOW_THREADS

    if (opened) close(fd);
    Py_XDECREF(path_obj);
    if (ret < 0) {
        if (path)
            return PyErr_SetFromErrnoWithFilename(PyExc_OSError, path);
        return PyErr_SetFromErrno(PyExc_OSError);
    }

    Py_RETURN_NONE;
}

/* -- qgroup_remove(path, src, dst) --------------------------------- */

PyDoc_STRVAR(qgroup_remove_doc,
"qgroup_remove(path: str | int | os.PathLike, src: int, dst: int) -> None\n\n"
"Remove qgroup *src* from parent qgroup *dst*.\n\n"
"*path* may be a filesystem path, a path-like object, or an open\n"
"file descriptor (int).\n\n"
"Reverses a previous :func:`qgroup_assign` call.\n\n"
"Calls BTRFS_IOC_QGROUP_ASSIGN with assign=0.\n\n"
"Example::\n\n"
"    >>> from pybtrfs import qgroupid, qgroup_remove\n"
"    >>> qgroup_remove('/mnt/btrfs', qgroupid(0, 256), qgroupid(1, 1))\n");

static PyObject *
pybtrfs_qgroup_remove(PyObject *self, PyObject *args)
{
    PyObject *path_or_fd;
    unsigned long long src, dst;
    if (!PyArg_ParseTuple(args, "OKK:qgroup_remove",
                          &path_or_fd, &src, &dst))
        return NULL;

    const char *path; int fd;
    PyObject *path_obj;
    int is_fd = parse_path_or_fd(path_or_fd, &path_obj, &path, &fd);
    if (is_fd < 0)
        return NULL;

    int opened;
    fd = resolve_fd(is_fd, path, fd, &opened);
    if (fd < 0) { Py_XDECREF(path_obj); return NULL; }

    struct btrfs_ioctl_qgroup_assign_args aargs = {
        .assign = 0,
        .src = src,
        .dst = dst,
    };

    int ret;
    Py_BEGIN_ALLOW_THREADS
    ret = ioctl(fd, BTRFS_IOC_QGROUP_ASSIGN, &aargs);
    Py_END_ALLOW_THREADS

    if (opened) close(fd);
    Py_XDECREF(path_obj);
    if (ret < 0) {
        if (path)
            return PyErr_SetFromErrnoWithFilename(PyExc_OSError, path);
        return PyErr_SetFromErrno(PyExc_OSError);
    }

    Py_RETURN_NONE;
}

/* -- qgroup_limit(path, qgroupid, max_rfer=0, max_excl=0) --------- */

PyDoc_STRVAR(qgroup_limit_doc,
"qgroup_limit(path: str | int | os.PathLike, qgroupid: int, "
"max_rfer: int = 0, max_excl: int = 0) -> None\n\n"
"Set quota limits for *qgroupid*.\n\n"
"*path* may be a filesystem path, a path-like object, or an open\n"
"file descriptor (int).\n\n"
"*max_rfer* limits the referenced bytes (total data, including shared).\n"
"*max_excl* limits the exclusive bytes (data unique to this qgroup).\n"
"A value of 0 clears the corresponding limit.\n\n"
"Calls BTRFS_IOC_QGROUP_LIMIT.\n\n"
"Example::\n\n"
"    >>> from pybtrfs import qgroupid, qgroup_limit\n"
"    >>> qgroup_limit('/mnt/btrfs', qgroupid(0, 256), max_rfer=1024**3)\n");

static PyObject *
pybtrfs_qgroup_limit(PyObject *self, PyObject *args, PyObject *kwargs)
{
    PyObject *path_or_fd;
    unsigned long long qgroupid;
    unsigned long long max_rfer = 0;
    unsigned long long max_excl = 0;

    static char *kwlist[] = {"path", "qgroupid", "max_rfer", "max_excl", NULL};

    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "OK|KK:qgroup_limit",
                                     kwlist, &path_or_fd, &qgroupid,
                                     &max_rfer, &max_excl))
        return NULL;

    const char *path; int fd;
    PyObject *path_obj;
    int is_fd = parse_path_or_fd(path_or_fd, &path_obj, &path, &fd);
    if (is_fd < 0)
        return NULL;

    int opened;
    fd = resolve_fd(is_fd, path, fd, &opened);
    if (fd < 0) { Py_XDECREF(path_obj); return NULL; }

    struct btrfs_ioctl_qgroup_limit_args largs;
    memset(&largs, 0, sizeof(largs));
    largs.qgroupid = qgroupid;

    if (max_rfer) {
        largs.lim.flags |= BTRFS_QGROUP_LIMIT_MAX_RFER;
        largs.lim.max_referenced = max_rfer;
    }
    if (max_excl) {
        largs.lim.flags |= BTRFS_QGROUP_LIMIT_MAX_EXCL;
        largs.lim.max_exclusive = max_excl;
    }

    int ret;
    Py_BEGIN_ALLOW_THREADS
    ret = ioctl(fd, BTRFS_IOC_QGROUP_LIMIT, &largs);
    Py_END_ALLOW_THREADS

    if (opened) close(fd);
    Py_XDECREF(path_obj);
    if (ret < 0) {
        if (path)
            return PyErr_SetFromErrnoWithFilename(PyExc_OSError, path);
        return PyErr_SetFromErrno(PyExc_OSError);
    }

    Py_RETURN_NONE;
}

/* -- qgroup_info(path) → list[dict] ------------------------------- */

PyDoc_STRVAR(qgroup_info_doc,
"qgroup_info(path: str | int | os.PathLike) -> list[dict]\n\n"
"Return a list of dicts describing every qgroup on the filesystem.\n\n"
"*path* may be a filesystem path, a path-like object, or an open\n"
"file descriptor (int).\n\n"
"Each dict contains:\n\n"
"- **qgroupid** (int): raw qgroup ID (``level << 48 | id``).\n"
"- **rfer** (int): referenced bytes.\n"
"- **excl** (int): exclusive bytes.\n"
"- **rfer_cmpr** (int): referenced compressed bytes.\n"
"- **excl_cmpr** (int): exclusive compressed bytes.\n"
"- **max_rfer** (int): max referenced limit (0 = unlimited).\n"
"- **max_excl** (int): max exclusive limit (0 = unlimited).\n\n"
"Uses BTRFS_IOC_TREE_SEARCH on the quota tree.\n\n"
"Example::\n\n"
"    >>> from pybtrfs import qgroupstr, qgroup_info\n"
"    >>> for qg in qgroup_info('/mnt/btrfs'):\n"
"    ...     gid = qgroupstr(qg['qgroupid'])\n"
"    ...     print(f\"{gid}: rfer={qg['rfer']}, excl={qg['excl']}\")\n"
"    0/5: rfer=16384, excl=16384\n");

static PyObject *
pybtrfs_qgroup_info(PyObject *self, PyObject *args)
{
    PyObject *path_or_fd;
    if (!PyArg_ParseTuple(args, "O:qgroup_info", &path_or_fd))
        return NULL;

    const char *path; int fd;
    PyObject *path_obj;
    int is_fd = parse_path_or_fd(path_or_fd, &path_obj, &path, &fd);
    if (is_fd < 0)
        return NULL;

    int opened;
    fd = resolve_fd(is_fd, path, fd, &opened);
    if (fd < 0) { Py_XDECREF(path_obj); return NULL; }

    /* accumulator: qgroupid -> dict */
    PyObject *accum = PyDict_New();
    if (!accum) {
        if (opened) close(fd);
        Py_XDECREF(path_obj);
        return NULL;
    }

    struct btrfs_ioctl_search_args sargs;
    struct btrfs_ioctl_search_key *sk = &sargs.key;

    memset(&sargs, 0, sizeof(sargs));
    sk->tree_id = BTRFS_QUOTA_TREE_OBJECTID;
    sk->min_objectid = 0;
    sk->max_objectid = (__u64)-1;
    sk->min_offset = 0;
    sk->max_offset = (__u64)-1;
    sk->min_transid = 0;
    sk->max_transid = (__u64)-1;
    sk->min_type = BTRFS_QGROUP_INFO_KEY;
    sk->max_type = BTRFS_QGROUP_LIMIT_KEY;
    sk->nr_items = 4096;

    while (1) {
        int ret;
        Py_BEGIN_ALLOW_THREADS
        ret = ioctl(fd, BTRFS_IOC_TREE_SEARCH, &sargs);
        Py_END_ALLOW_THREADS

        if (ret < 0) {
            if (opened) close(fd);
            Py_XDECREF(path_obj);
            Py_DECREF(accum);
            if (path)
                return PyErr_SetFromErrnoWithFilename(PyExc_OSError, path);
            return PyErr_SetFromErrno(PyExc_OSError);
        }

        if (sk->nr_items == 0)
            break;

        char *buf = sargs.buf;
        unsigned int i;
        for (i = 0; i < sk->nr_items; i++) {
            struct btrfs_ioctl_search_header *sh =
                (struct btrfs_ioctl_search_header *)buf;
            char *item = buf + sizeof(*sh);

            PyObject *key = PyLong_FromUnsignedLongLong(sh->offset);
            if (!key)
                goto error;

            /* get-or-create dict for this qgroupid */
            PyObject *entry = PyDict_GetItemWithError(accum, key);
            if (!entry) {
                if (PyErr_Occurred()) {
                    Py_DECREF(key);
                    goto error;
                }
                entry = Py_BuildValue(
                    "{s:K,s:K,s:K,s:K,s:K,s:K,s:K}",
                    "qgroupid",  (unsigned long long)sh->offset,
                    "rfer",      0ULL,
                    "excl",      0ULL,
                    "rfer_cmpr", 0ULL,
                    "excl_cmpr", 0ULL,
                    "max_rfer",  0ULL,
                    "max_excl",  0ULL);
                if (!entry) {
                    Py_DECREF(key);
                    goto error;
                }
                if (PyDict_SetItem(accum, key, entry) < 0) {
                    Py_DECREF(entry);
                    Py_DECREF(key);
                    goto error;
                }
                Py_DECREF(entry);  /* accum owns it now */
                entry = PyDict_GetItem(accum, key);
            }

            if (sh->type == BTRFS_QGROUP_INFO_KEY &&
                sh->len >= sizeof(struct btrfs_qgroup_info_item)) {
                struct btrfs_qgroup_info_item *info =
                    (struct btrfs_qgroup_info_item *)item;
                PyObject *val;

                val = PyLong_FromUnsignedLongLong(le64toh(info->rfer));
                if (!val) { Py_DECREF(key); goto error; }
                PyDict_SetItemString(entry, "rfer", val);
                Py_DECREF(val);

                val = PyLong_FromUnsignedLongLong(le64toh(info->excl));
                if (!val) { Py_DECREF(key); goto error; }
                PyDict_SetItemString(entry, "excl", val);
                Py_DECREF(val);

                val = PyLong_FromUnsignedLongLong(le64toh(info->rfer_cmpr));
                if (!val) { Py_DECREF(key); goto error; }
                PyDict_SetItemString(entry, "rfer_cmpr", val);
                Py_DECREF(val);

                val = PyLong_FromUnsignedLongLong(le64toh(info->excl_cmpr));
                if (!val) { Py_DECREF(key); goto error; }
                PyDict_SetItemString(entry, "excl_cmpr", val);
                Py_DECREF(val);
            }
            else if (sh->type == BTRFS_QGROUP_LIMIT_KEY &&
                     sh->len >= sizeof(struct btrfs_qgroup_limit_item)) {
                struct btrfs_qgroup_limit_item *lim =
                    (struct btrfs_qgroup_limit_item *)item;
                PyObject *val;

                val = PyLong_FromUnsignedLongLong(le64toh(lim->max_rfer));
                if (!val) { Py_DECREF(key); goto error; }
                PyDict_SetItemString(entry, "max_rfer", val);
                Py_DECREF(val);

                val = PyLong_FromUnsignedLongLong(le64toh(lim->max_excl));
                if (!val) { Py_DECREF(key); goto error; }
                PyDict_SetItemString(entry, "max_excl", val);
                Py_DECREF(val);
            }

            Py_DECREF(key);

            /* advance to next search result */
            buf = item + sh->len;

            /* update search key for pagination */
            sk->min_objectid = sh->objectid;
            sk->min_type = sh->type;
            sk->min_offset = sh->offset;
        }

        /* advance past last item to continue search */
        if (sk->min_offset < (__u64)-1)
            sk->min_offset++;
        else if (sk->min_type < BTRFS_QGROUP_LIMIT_KEY)
            sk->min_type++;
        else if (sk->min_objectid < (__u64)-1)
            sk->min_objectid++;
        else
            break;

        sk->nr_items = 4096;
    }

    if (opened) close(fd);
    Py_XDECREF(path_obj);

    /* convert dict-of-dicts → list-of-dicts */
    PyObject *values = PyDict_Values(accum);
    Py_DECREF(accum);
    if (!values)
        return NULL;

    PyObject *result = PySequence_List(values);
    Py_DECREF(values);
    return result;

error:
    if (opened) close(fd);
    Py_XDECREF(path_obj);
    Py_DECREF(accum);
    return NULL;
}

/* -- qgroupid(level, id) / qgroupid("level/id") ------------------- */

PyDoc_STRVAR(qgroupid_doc,
"qgroupid(level: int, id: int) -> int\n"
"qgroupid(s: str) -> int\n"
"qgroupid(qgroupid: int) -> int\n\n"
"Build a raw qgroup ID from *level* and *id*, parse a\n"
"``\"level/id\"`` string (the format used by ``btrfs qgroup show``),\n"
"or pass through a raw integer unchanged.\n\n"
"The returned value is ``(level << 48) | id`` and can be passed\n"
"directly to :func:`qgroup_create`, :func:`qgroup_assign`,\n"
":func:`qgroup_limit`, etc.\n\n"
"Raises :exc:`ValueError` on malformed input or if *level* exceeds\n"
"16 bits (0..65535) or *id* exceeds 48 bits.\n\n"
"Example::\n\n"
"    >>> from pybtrfs import qgroupid\n"
"    >>> qgroupid(1, 256)\n"
"    281474976711040\n"
"    >>> qgroupid('1/256')\n"
"    281474976711040\n"
"    >>> qgroupid('0/5')\n"
"    5\n"
"    >>> qgroupid(5)  # int passthrough\n"
"    5\n");

static PyObject *
pybtrfs_qgroupid(PyObject *self, PyObject *args)
{
    Py_ssize_t nargs = PyTuple_GET_SIZE(args);

    if (nargs == 2) {
        unsigned long long level, id;
        if (!PyArg_ParseTuple(args, "KK:qgroupid", &level, &id))
            return NULL;
        if (level > 0xFFFF) {
            PyErr_Format(PyExc_ValueError,
                         "level must fit in 16 bits (0..65535), got %llu",
                         level);
            return NULL;
        }
        if (id > ((1ULL << 48) - 1)) {
            PyErr_Format(PyExc_ValueError,
                         "id must fit in 48 bits, got %llu", id);
            return NULL;
        }
        return PyLong_FromUnsignedLongLong((level << 48) | id);
    }

    if (nargs != 1) {
        PyErr_SetString(PyExc_TypeError,
                        "qgroupid() takes 1 or 2 arguments");
        return NULL;
    }

    PyObject *arg = PyTuple_GET_ITEM(args, 0);

    if (PyUnicode_Check(arg)) {
        const char *s = PyUnicode_AsUTF8(arg);
        if (!s)
            return NULL;

        const char *slash = strchr(s, '/');
        if (!slash) {
            PyErr_Format(PyExc_ValueError,
                         "expected 'level/id' format, got '%s'", s);
            return NULL;
        }

        char *end;
        unsigned long long level = strtoull(s, &end, 10);
        if (end != slash) {
            PyErr_Format(PyExc_ValueError,
                         "invalid level in '%s'", s);
            return NULL;
        }

        unsigned long long id = strtoull(slash + 1, &end, 10);
        if (*end != '\0') {
            PyErr_Format(PyExc_ValueError,
                         "invalid id in '%s'", s);
            return NULL;
        }

        if (level > 0xFFFF) {
            PyErr_Format(PyExc_ValueError,
                         "level must fit in 16 bits (0..65535), got %llu",
                         level);
            return NULL;
        }
        if (id > ((1ULL << 48) - 1)) {
            PyErr_Format(PyExc_ValueError,
                         "id must fit in 48 bits, got %llu", id);
            return NULL;
        }

        return PyLong_FromUnsignedLongLong((level << 48) | id);
    }

    /* int passthrough */
    unsigned long long val;
    if (!PyArg_ParseTuple(args, "K:qgroupid", &val))
        return NULL;
    return PyLong_FromUnsignedLongLong(val);
}

/* -- qgroupstr(qgroupid) ------------------------------------------ */

PyDoc_STRVAR(qgroupstr_doc,
"qgroupstr(qgroupid: int) -> str\n\n"
"Convert a raw qgroup ID to the ``\"level/id\"`` string format\n"
"used by ``btrfs qgroup show``.\n\n"
"This is the inverse of :func:`qgroupid`.\n\n"
"Example::\n\n"
"    >>> from pybtrfs import qgroupid, qgroupstr\n"
"    >>> qgroupstr(qgroupid(1, 256))\n"
"    '1/256'\n"
"    >>> qgroupstr(5)\n"
"    '0/5'\n");

static PyObject *
pybtrfs_qgroupstr(PyObject *self, PyObject *args)
{
    unsigned long long val;
    if (!PyArg_ParseTuple(args, "K:qgroupstr", &val))
        return NULL;

    unsigned long long level = val >> 48;
    unsigned long long id = val & ((1ULL << 48) - 1);
    return PyUnicode_FromFormat("%llu/%llu", level, id);
}

/* -- method table -------------------------------------------------- */

static PyMethodDef quota_methods[] = {
    {"quota_enable",        (PyCFunction)pybtrfs_quota_enable,
     METH_VARARGS, quota_enable_doc},
    {"quota_enable_simple", (PyCFunction)pybtrfs_quota_enable_simple,
     METH_VARARGS, quota_enable_simple_doc},
    {"quota_disable",       (PyCFunction)pybtrfs_quota_disable,
     METH_VARARGS, quota_disable_doc},
    {"quota_rescan",        (PyCFunction)pybtrfs_quota_rescan,
     METH_VARARGS, quota_rescan_doc},
    {"quota_rescan_status", (PyCFunction)pybtrfs_quota_rescan_status,
     METH_VARARGS, quota_rescan_status_doc},
    {"quota_rescan_wait",   (PyCFunction)pybtrfs_quota_rescan_wait,
     METH_VARARGS, quota_rescan_wait_doc},
    {"qgroup_create",       (PyCFunction)pybtrfs_qgroup_create,
     METH_VARARGS, qgroup_create_doc},
    {"qgroup_destroy",      (PyCFunction)pybtrfs_qgroup_destroy,
     METH_VARARGS, qgroup_destroy_doc},
    {"qgroup_assign",       (PyCFunction)pybtrfs_qgroup_assign,
     METH_VARARGS, qgroup_assign_doc},
    {"qgroup_remove",       (PyCFunction)pybtrfs_qgroup_remove,
     METH_VARARGS, qgroup_remove_doc},
    {"qgroup_limit",        (PyCFunction)pybtrfs_qgroup_limit,
     METH_VARARGS | METH_KEYWORDS, qgroup_limit_doc},
    {"qgroup_info",         (PyCFunction)pybtrfs_qgroup_info,
     METH_VARARGS, qgroup_info_doc},
    {"qgroupid",            (PyCFunction)pybtrfs_qgroupid,
     METH_VARARGS, qgroupid_doc},
    {"qgroupstr",           (PyCFunction)pybtrfs_qgroupstr,
     METH_VARARGS, qgroupstr_doc},
    {NULL, NULL, 0, NULL},
};

/* -- module definition --------------------------------------------- */

static struct PyModuleDef quota_module = {
    PyModuleDef_HEAD_INIT,
    .m_name    = "pybtrfs.quota",
    .m_doc     = "Low-level btrfs quota / qgroup ioctl wrappers.",
    .m_size    = -1,
    .m_methods = quota_methods,
};

PyMODINIT_FUNC
PyInit_quota(void)
{
    PyObject *m = PyModule_Create(&quota_module);
    if (!m)
        return NULL;

    /* quota control commands */
    PyModule_AddIntMacro(m, BTRFS_QUOTA_CTL_ENABLE);
    PyModule_AddIntMacro(m, BTRFS_QUOTA_CTL_DISABLE);
    PyModule_AddIntMacro(m, BTRFS_QUOTA_CTL_ENABLE_SIMPLE_QUOTA);

    /* qgroup status flags */
    PyModule_AddIntMacro(m, BTRFS_QGROUP_STATUS_FLAG_ON);
    PyModule_AddIntMacro(m, BTRFS_QGROUP_STATUS_FLAG_RESCAN);
    PyModule_AddIntMacro(m, BTRFS_QGROUP_STATUS_FLAG_INCONSISTENT);
    PyModule_AddIntMacro(m, BTRFS_QGROUP_STATUS_FLAG_SIMPLE_MODE);

    /* qgroup limit flags */
    PyModule_AddIntMacro(m, BTRFS_QGROUP_LIMIT_MAX_RFER);
    PyModule_AddIntMacro(m, BTRFS_QGROUP_LIMIT_MAX_EXCL);
    PyModule_AddIntMacro(m, BTRFS_QGROUP_LIMIT_RSV_RFER);
    PyModule_AddIntMacro(m, BTRFS_QGROUP_LIMIT_RSV_EXCL);

    return m;
}
