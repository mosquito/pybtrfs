#include "module.h"
#include <stdlib.h>

/* ── queries ───────────────────────────────────────────────────────── */

static PyObject *
mod_is_subvolume(PyObject *self, PyObject *args, PyObject *kwds)
{
    static char *kw[] = {"path", NULL};
    const char *path;
    enum btrfs_util_error err;

    if (!PyArg_ParseTupleAndKeywords(args, kwds, "s", kw, &path))
        return NULL;

    Py_BEGIN_ALLOW_THREADS
    err = btrfs_util_is_subvolume(path);
    Py_END_ALLOW_THREADS

    if (err == BTRFS_UTIL_OK)
        Py_RETURN_TRUE;
    if (err == BTRFS_UTIL_ERROR_NOT_BTRFS ||
        err == BTRFS_UTIL_ERROR_NOT_SUBVOLUME)
        Py_RETURN_FALSE;
    return set_error(err);
}

static PyObject *
mod_subvolume_id(PyObject *self, PyObject *args, PyObject *kwds)
{
    static char *kw[] = {"path", NULL};
    const char *path;
    uint64_t id;
    enum btrfs_util_error err;

    if (!PyArg_ParseTupleAndKeywords(args, kwds, "s", kw, &path))
        return NULL;

    Py_BEGIN_ALLOW_THREADS
    err = btrfs_util_subvolume_id(path, &id);
    Py_END_ALLOW_THREADS

    if (err)
        return set_error(err);
    return PyLong_FromUnsignedLongLong(id);
}

static PyObject *
mod_subvolume_path(PyObject *self, PyObject *args, PyObject *kwds)
{
    static char *kw[] = {"path", "id", NULL};
    const char *path;
    uint64_t id = 0;
    char *subvol_path = NULL;
    enum btrfs_util_error err;

    if (!PyArg_ParseTupleAndKeywords(args, kwds, "s|K", kw, &path, &id))
        return NULL;

    Py_BEGIN_ALLOW_THREADS
    err = btrfs_util_subvolume_path(path, id, &subvol_path);
    Py_END_ALLOW_THREADS

    if (err)
        return set_error(err);

    PyObject *result = PyUnicode_DecodeFSDefault(subvol_path);
    free(subvol_path);
    return result;
}

static PyObject *
mod_subvolume_info(PyObject *self, PyObject *args, PyObject *kwds)
{
    static char *kw[] = {"path", "id", NULL};
    const char *path;
    uint64_t id = 0;
    struct btrfs_util_subvolume_info info;
    enum btrfs_util_error err;

    if (!PyArg_ParseTupleAndKeywords(args, kwds, "s|K", kw, &path, &id))
        return NULL;

    Py_BEGIN_ALLOW_THREADS
    err = btrfs_util_subvolume_info(path, id, &info);
    Py_END_ALLOW_THREADS

    if (err)
        return set_error(err);
    return SubvolumeInfo_from_struct(&info);
}

/* ── read-only flag ────────────────────────────────────────────────── */

static PyObject *
mod_get_subvolume_read_only(PyObject *self, PyObject *args, PyObject *kwds)
{
    static char *kw[] = {"path", NULL};
    const char *path;
    bool ro;
    enum btrfs_util_error err;

    if (!PyArg_ParseTupleAndKeywords(args, kwds, "s", kw, &path))
        return NULL;

    Py_BEGIN_ALLOW_THREADS
    err = btrfs_util_get_subvolume_read_only(path, &ro);
    Py_END_ALLOW_THREADS

    if (err)
        return set_error(err);
    return PyBool_FromLong(ro);
}

static PyObject *
mod_set_subvolume_read_only(PyObject *self, PyObject *args, PyObject *kwds)
{
    static char *kw[] = {"path", "read_only", NULL};
    const char *path;
    int ro = 1;
    enum btrfs_util_error err;

    if (!PyArg_ParseTupleAndKeywords(args, kwds, "s|p", kw, &path, &ro))
        return NULL;

    Py_BEGIN_ALLOW_THREADS
    err = btrfs_util_set_subvolume_read_only(path, ro);
    Py_END_ALLOW_THREADS

    if (err)
        return set_error(err);
    Py_RETURN_NONE;
}

/* ── default subvolume ─────────────────────────────────────────────── */

static PyObject *
mod_get_default_subvolume(PyObject *self, PyObject *args, PyObject *kwds)
{
    static char *kw[] = {"path", NULL};
    const char *path;
    uint64_t id;
    enum btrfs_util_error err;

    if (!PyArg_ParseTupleAndKeywords(args, kwds, "s", kw, &path))
        return NULL;

    Py_BEGIN_ALLOW_THREADS
    err = btrfs_util_get_default_subvolume(path, &id);
    Py_END_ALLOW_THREADS

    if (err)
        return set_error(err);
    return PyLong_FromUnsignedLongLong(id);
}

static PyObject *
mod_set_default_subvolume(PyObject *self, PyObject *args, PyObject *kwds)
{
    static char *kw[] = {"path", "id", NULL};
    const char *path;
    uint64_t id = 0;
    enum btrfs_util_error err;

    if (!PyArg_ParseTupleAndKeywords(args, kwds, "s|K", kw, &path, &id))
        return NULL;

    Py_BEGIN_ALLOW_THREADS
    err = btrfs_util_set_default_subvolume(path, id);
    Py_END_ALLOW_THREADS

    if (err)
        return set_error(err);
    Py_RETURN_NONE;
}

/* ── create / snapshot / delete ────────────────────────────────────── */

static PyObject *
mod_create_subvolume(PyObject *self, PyObject *args, PyObject *kwds)
{
    static char *kw[] = {"path", "qgroup_inherit", NULL};
    const char *path;
    QgroupInheritObject *qg_obj = NULL;
    struct btrfs_util_qgroup_inherit *qg = NULL;
    enum btrfs_util_error err;

    if (!PyArg_ParseTupleAndKeywords(args, kwds, "s|O!", kw,
                                     &path, &QgroupInheritType, &qg_obj))
        return NULL;
    if (qg_obj)
        qg = qg_obj->inherit;

    Py_BEGIN_ALLOW_THREADS
    err = btrfs_util_create_subvolume(path, 0, NULL, qg);
    Py_END_ALLOW_THREADS

    if (err)
        return set_error(err);
    Py_RETURN_NONE;
}

static PyObject *
mod_create_snapshot(PyObject *self, PyObject *args, PyObject *kwds)
{
    static char *kw[] = {"source", "path", "recursive", "read_only",
                         "qgroup_inherit", NULL};
    const char *source, *path;
    int recursive = 0, read_only = 0, flags = 0;
    QgroupInheritObject *qg_obj = NULL;
    struct btrfs_util_qgroup_inherit *qg = NULL;
    enum btrfs_util_error err;

    if (!PyArg_ParseTupleAndKeywords(args, kwds, "ss|ppO!", kw,
                                     &source, &path,
                                     &recursive, &read_only,
                                     &QgroupInheritType, &qg_obj))
        return NULL;

    if (recursive)
        flags |= BTRFS_UTIL_CREATE_SNAPSHOT_RECURSIVE;
    if (read_only)
        flags |= BTRFS_UTIL_CREATE_SNAPSHOT_READ_ONLY;
    if (qg_obj)
        qg = qg_obj->inherit;

    Py_BEGIN_ALLOW_THREADS
    err = btrfs_util_create_snapshot(source, path, flags, NULL, qg);
    Py_END_ALLOW_THREADS

    if (err)
        return set_error(err);
    Py_RETURN_NONE;
}

static PyObject *
mod_delete_subvolume(PyObject *self, PyObject *args, PyObject *kwds)
{
    static char *kw[] = {"path", "recursive", NULL};
    const char *path;
    int recursive = 0, flags = 0;
    enum btrfs_util_error err;

    if (!PyArg_ParseTupleAndKeywords(args, kwds, "s|p", kw,
                                     &path, &recursive))
        return NULL;

    if (recursive)
        flags |= BTRFS_UTIL_DELETE_SUBVOLUME_RECURSIVE;

    Py_BEGIN_ALLOW_THREADS
    err = btrfs_util_delete_subvolume(path, flags);
    Py_END_ALLOW_THREADS

    if (err)
        return set_error(err);
    Py_RETURN_NONE;
}

static PyObject *
mod_deleted_subvolumes(PyObject *self, PyObject *args, PyObject *kwds)
{
    static char *kw[] = {"path", NULL};
    const char *path;
    uint64_t *ids = NULL;
    size_t n = 0;
    enum btrfs_util_error err;

    if (!PyArg_ParseTupleAndKeywords(args, kwds, "s", kw, &path))
        return NULL;

    Py_BEGIN_ALLOW_THREADS
    err = btrfs_util_deleted_subvolumes(path, &ids, &n);
    Py_END_ALLOW_THREADS

    if (err)
        return set_error(err);

    PyObject *list = PyList_New((Py_ssize_t)n);
    if (!list) { free(ids); return NULL; }

    for (size_t i = 0; i < n; i++) {
        PyObject *v = PyLong_FromUnsignedLongLong(ids[i]);
        if (!v) { Py_DECREF(list); free(ids); return NULL; }
        PyList_SET_ITEM(list, (Py_ssize_t)i, v);
    }
    free(ids);
    return list;
}

/* ── exported method table ─────────────────────────────────────────── */

PyMethodDef subvolume_methods[] = {
    {"is_subvolume", (PyCFunction)mod_is_subvolume,
     METH_VARARGS | METH_KEYWORDS,
     "is_subvolume(path: str) -> bool\n\n"
     "Return whether a path is a Btrfs subvolume."},

    {"subvolume_id", (PyCFunction)mod_subvolume_id,
     METH_VARARGS | METH_KEYWORDS,
     "subvolume_id(path: str) -> int\n\n"
     "Get the subvolume ID containing a path."},

    {"subvolume_path", (PyCFunction)mod_subvolume_path,
     METH_VARARGS | METH_KEYWORDS,
     "subvolume_path(path: str, id: int = 0) -> str\n\n"
     "Get the path of a subvolume relative to the filesystem root."},

    {"subvolume_info", (PyCFunction)mod_subvolume_info,
     METH_VARARGS | METH_KEYWORDS,
     "subvolume_info(path: str, id: int = 0) -> SubvolumeInfo\n\n"
     "Get information about a subvolume."},

    {"get_subvolume_read_only", (PyCFunction)mod_get_subvolume_read_only,
     METH_VARARGS | METH_KEYWORDS,
     "get_subvolume_read_only(path: str) -> bool\n\n"
     "Get whether a subvolume is read-only."},

    {"set_subvolume_read_only", (PyCFunction)mod_set_subvolume_read_only,
     METH_VARARGS | METH_KEYWORDS,
     "set_subvolume_read_only(path: str, read_only: bool = True) -> None\n\n"
     "Set whether a subvolume is read-only."},

    {"get_default_subvolume", (PyCFunction)mod_get_default_subvolume,
     METH_VARARGS | METH_KEYWORDS,
     "get_default_subvolume(path: str) -> int\n\n"
     "Get the default subvolume ID."},

    {"set_default_subvolume", (PyCFunction)mod_set_default_subvolume,
     METH_VARARGS | METH_KEYWORDS,
     "set_default_subvolume(path: str, id: int = 0) -> None\n\n"
     "Set the default subvolume."},

    {"create_subvolume", (PyCFunction)mod_create_subvolume,
     METH_VARARGS | METH_KEYWORDS,
     "create_subvolume(path: str, qgroup_inherit: QgroupInherit | None = None) -> None\n\n"
     "Create a new subvolume."},

    {"create_snapshot", (PyCFunction)mod_create_snapshot,
     METH_VARARGS | METH_KEYWORDS,
     "create_snapshot(source: str, path: str, recursive: bool = False, "
     "read_only: bool = False, qgroup_inherit: QgroupInherit | None = None) -> None\n\n"
     "Create a snapshot of a subvolume."},

    {"delete_subvolume", (PyCFunction)mod_delete_subvolume,
     METH_VARARGS | METH_KEYWORDS,
     "delete_subvolume(path: str, recursive: bool = False) -> None\n\n"
     "Delete a subvolume or snapshot."},

    {"deleted_subvolumes", (PyCFunction)mod_deleted_subvolumes,
     METH_VARARGS | METH_KEYWORDS,
     "deleted_subvolumes(path: str) -> list[int]\n\n"
     "Get IDs of deleted but not yet cleaned up subvolumes."},

    {NULL}
};
