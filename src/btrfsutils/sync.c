#include "module.h"

PyDoc_STRVAR(sync_doc,
"sync(path: str) -> None\n\n"
"Force a sync on the Btrfs filesystem at *path*.\n\n"
"Waits for all pending transactions to be committed to disk.\n"
"This is equivalent to ``btrfs filesystem sync``.\n\n"
"Example::\n\n"
"    >>> pybtrfs.sync('/mnt/btrfs')\n");

static PyObject *
mod_sync(PyObject *self, PyObject *args, PyObject *kwds)
{
    static char *kw[] = {"path", NULL};
    const char *path;
    enum btrfs_util_error err;

    if (!PyArg_ParseTupleAndKeywords(args, kwds, "s", kw, &path))
        return NULL;

    Py_BEGIN_ALLOW_THREADS
    err = btrfs_util_sync(path);
    Py_END_ALLOW_THREADS

    if (err)
        return set_error(err);
    Py_RETURN_NONE;
}

PyDoc_STRVAR(start_sync_doc,
"start_sync(path: str) -> int\n\n"
"Start a sync on the Btrfs filesystem and return the transaction ID.\n\n"
"Unlike :func:`sync`, this does not block. Use :func:`wait_sync` to\n"
"wait for the transaction to complete.\n\n"
"Example::\n\n"
"    >>> transid = pybtrfs.start_sync('/mnt/btrfs')\n"
"    >>> pybtrfs.wait_sync('/mnt/btrfs', transid)\n");

static PyObject *
mod_start_sync(PyObject *self, PyObject *args, PyObject *kwds)
{
    static char *kw[] = {"path", NULL};
    const char *path;
    uint64_t transid;
    enum btrfs_util_error err;

    if (!PyArg_ParseTupleAndKeywords(args, kwds, "s", kw, &path))
        return NULL;

    Py_BEGIN_ALLOW_THREADS
    err = btrfs_util_start_sync(path, &transid);
    Py_END_ALLOW_THREADS

    if (err)
        return set_error(err);
    return PyLong_FromUnsignedLongLong(transid);
}

PyDoc_STRVAR(wait_sync_doc,
"wait_sync(path: str, transid: int = 0) -> None\n\n"
"Wait for a transaction to sync on the Btrfs filesystem.\n\n"
"If *transid* is 0 (default), wait for the current transaction.\n"
"Releases the GIL while waiting.\n\n"
"Example::\n\n"
"    >>> transid = pybtrfs.start_sync('/mnt/btrfs')\n"
"    >>> pybtrfs.wait_sync('/mnt/btrfs', transid)\n");

static PyObject *
mod_wait_sync(PyObject *self, PyObject *args, PyObject *kwds)
{
    static char *kw[] = {"path", "transid", NULL};
    const char *path;
    uint64_t transid = 0;
    enum btrfs_util_error err;

    if (!PyArg_ParseTupleAndKeywords(args, kwds, "s|K", kw,
                                     &path, &transid))
        return NULL;

    Py_BEGIN_ALLOW_THREADS
    err = btrfs_util_wait_sync(path, transid);
    Py_END_ALLOW_THREADS

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
