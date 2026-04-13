#include "module.h"

PyDoc_STRVAR(sync_doc,
"sync(path: str | int | os.PathLike) -> None\n\n"
"Force a sync on the Btrfs filesystem.\n\n"
"*path* may be a filesystem path, a path-like object, or an open\n"
"file descriptor (int).\n\n"
"Waits for all pending transactions to be committed to disk.\n"
"This is equivalent to ``btrfs filesystem sync``.\n\n"
"Example::\n\n"
"    >>> pybtrfs.sync('/mnt/btrfs')\n");

static PyObject *
mod_sync(PyObject *self, PyObject *args, PyObject *kwds)
{
    static char *kw[] = {"path", NULL};
    PyObject *path_or_fd;
    if (!PyArg_ParseTupleAndKeywords(args, kwds, "O", kw, &path_or_fd))
        return NULL;

    const char *path; int fd;
    PyObject *path_obj;
    int is_fd = parse_path_or_fd(path_or_fd, &path_obj, &path, &fd);
    if (is_fd < 0)
        return NULL;

    enum btrfs_util_error err;
    Py_BEGIN_ALLOW_THREADS
    err = is_fd ? btrfs_util_sync_fd(fd) : btrfs_util_sync(path);
    Py_END_ALLOW_THREADS

    Py_XDECREF(path_obj);
    if (err)
        return set_error(err);
    Py_RETURN_NONE;
}

PyDoc_STRVAR(start_sync_doc,
"start_sync(path: str | int | os.PathLike) -> int\n\n"
"Start a sync on the Btrfs filesystem and return the transaction ID.\n\n"
"*path* may be a filesystem path, a path-like object, or an open\n"
"file descriptor (int).\n\n"
"Unlike :func:`sync`, this does not block. Use :func:`wait_sync` to\n"
"wait for the transaction to complete.\n\n"
"Example::\n\n"
"    >>> transid = pybtrfs.start_sync('/mnt/btrfs')\n"
"    >>> pybtrfs.wait_sync('/mnt/btrfs', transid)\n");

static PyObject *
mod_start_sync(PyObject *self, PyObject *args, PyObject *kwds)
{
    static char *kw[] = {"path", NULL};
    PyObject *path_or_fd;
    uint64_t transid;
    if (!PyArg_ParseTupleAndKeywords(args, kwds, "O", kw, &path_or_fd))
        return NULL;

    const char *path; int fd;
    PyObject *path_obj;
    int is_fd = parse_path_or_fd(path_or_fd, &path_obj, &path, &fd);
    if (is_fd < 0)
        return NULL;

    enum btrfs_util_error err;
    Py_BEGIN_ALLOW_THREADS
    err = is_fd ? btrfs_util_start_sync_fd(fd, &transid)
                : btrfs_util_start_sync(path, &transid);
    Py_END_ALLOW_THREADS

    Py_XDECREF(path_obj);
    if (err)
        return set_error(err);
    return PyLong_FromUnsignedLongLong(transid);
}

PyDoc_STRVAR(wait_sync_doc,
"wait_sync(path: str | int | os.PathLike, transid: int = 0) -> None\n\n"
"Wait for a transaction to sync on the Btrfs filesystem.\n\n"
"*path* may be a filesystem path, a path-like object, or an open\n"
"file descriptor (int).\n\n"
"If *transid* is 0 (default), wait for the current transaction.\n"
"Releases the GIL while waiting.\n\n"
"Example::\n\n"
"    >>> transid = pybtrfs.start_sync('/mnt/btrfs')\n"
"    >>> pybtrfs.wait_sync('/mnt/btrfs', transid)\n");

static PyObject *
mod_wait_sync(PyObject *self, PyObject *args, PyObject *kwds)
{
    static char *kw[] = {"path", "transid", NULL};
    PyObject *path_or_fd;
    uint64_t transid = 0;
    if (!PyArg_ParseTupleAndKeywords(args, kwds, "O|K", kw,
                                     &path_or_fd, &transid))
        return NULL;

    const char *path; int fd;
    PyObject *path_obj;
    int is_fd = parse_path_or_fd(path_or_fd, &path_obj, &path, &fd);
    if (is_fd < 0)
        return NULL;

    enum btrfs_util_error err;
    Py_BEGIN_ALLOW_THREADS
    err = is_fd ? btrfs_util_wait_sync_fd(fd, transid)
                : btrfs_util_wait_sync(path, transid);
    Py_END_ALLOW_THREADS

    Py_XDECREF(path_obj);
    if (err)
        return set_error(err);
    Py_RETURN_NONE;
}

PyMethodDef sync_methods[] = {
    {"sync", (PyCFunction)mod_sync,
     METH_VARARGS | METH_KEYWORDS, sync_doc},

    {"start_sync", (PyCFunction)mod_start_sync,
     METH_VARARGS | METH_KEYWORDS, start_sync_doc},

    {"wait_sync", (PyCFunction)mod_wait_sync,
     METH_VARARGS | METH_KEYWORDS, wait_sync_doc},

    {NULL}
};
