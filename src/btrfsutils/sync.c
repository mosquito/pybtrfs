#include "module.h"

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
     METH_VARARGS | METH_KEYWORDS,
     "sync(path: str) -> None\n\nForce a sync on a Btrfs filesystem."},

    {"start_sync", (PyCFunction)mod_start_sync,
     METH_VARARGS | METH_KEYWORDS,
     "start_sync(path: str) -> int\n\n"
     "Start a sync and return the transaction ID."},

    {"wait_sync", (PyCFunction)mod_wait_sync,
     METH_VARARGS | METH_KEYWORDS,
     "wait_sync(path: str, transid: int = 0) -> None\n\n"
     "Wait for a transaction to sync."},

    {NULL}
};
