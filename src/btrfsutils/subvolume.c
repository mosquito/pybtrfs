#include "module.h"
#include <stdlib.h>

/* -- queries --------------------------------------------------------- */

PyDoc_STRVAR(is_subvolume_doc,
"is_subvolume(path: str) -> bool\n\n"
"Return whether *path* is a Btrfs subvolume.\n\n"
"Returns ``False`` for paths that are not on a Btrfs filesystem\n"
"or are not subvolumes.\n\n"
"Example::\n\n"
"    >>> import pybtrfs\n"
"    >>> pybtrfs.is_subvolume('/mnt/btrfs')\n"
"    True\n"
"    >>> pybtrfs.is_subvolume('/tmp')\n"
"    False\n");

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

PyDoc_STRVAR(subvolume_id_doc,
"subvolume_id(path: str) -> int\n\n"
"Get the subvolume ID of the subvolume containing *path*.\n\n"
"Example::\n\n"
"    >>> pybtrfs.subvolume_id('/mnt/btrfs')\n"
"    5\n"
"    >>> pybtrfs.subvolume_id('/mnt/btrfs/my_subvol')\n"
"    256\n");

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

PyDoc_STRVAR(subvolume_path_doc,
"subvolume_path(path: str, id: int = 0) -> str\n\n"
"Get the path of a subvolume relative to the filesystem root.\n\n"
"If *id* is 0 (default), the subvolume containing *path* is used.\n\n"
"Example::\n\n"
"    >>> pybtrfs.subvolume_path('/mnt/btrfs/my_subvol')\n"
"    'my_subvol'\n"
"    >>> pybtrfs.subvolume_path('/mnt/btrfs', id=256)\n"
"    'my_subvol'\n");

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

PyDoc_STRVAR(subvolume_info_doc,
"subvolume_info(path: str, id: int = 0) -> SubvolumeInfo\n\n"
"Get information about a subvolume.\n\n"
"If *id* is 0 (default), the subvolume containing *path* is used.\n"
"Returns a :class:`SubvolumeInfo` object with attributes such as\n"
"``id``, ``parent_id``, ``generation``, ``uuid``, etc.\n\n"
"Example::\n\n"
"    >>> info = pybtrfs.subvolume_info('/mnt/btrfs/my_subvol')\n"
"    >>> info.id\n"
"    256\n"
"    >>> info.parent_id\n"
"    5\n");

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

/* -- read-only flag -------------------------------------------------- */

PyDoc_STRVAR(get_subvolume_read_only_doc,
"get_subvolume_read_only(path: str) -> bool\n\n"
"Get whether the subvolume at *path* is read-only.\n\n"
"Example::\n\n"
"    >>> pybtrfs.get_subvolume_read_only('/mnt/btrfs/snap')\n"
"    True\n");

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

PyDoc_STRVAR(set_subvolume_read_only_doc,
"set_subvolume_read_only(path: str, read_only: bool = True) -> None\n\n"
"Set whether the subvolume at *path* is read-only.\n\n"
"Example::\n\n"
"    >>> pybtrfs.set_subvolume_read_only('/mnt/btrfs/snap')\n"
"    >>> pybtrfs.get_subvolume_read_only('/mnt/btrfs/snap')\n"
"    True\n"
"    >>> pybtrfs.set_subvolume_read_only('/mnt/btrfs/snap', False)\n");

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

/* -- default subvolume ----------------------------------------------- */

PyDoc_STRVAR(get_default_subvolume_doc,
"get_default_subvolume(path: str) -> int\n\n"
"Get the default subvolume ID for the filesystem at *path*.\n\n"
"Example::\n\n"
"    >>> pybtrfs.get_default_subvolume('/mnt/btrfs')\n"
"    5\n");

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

PyDoc_STRVAR(set_default_subvolume_doc,
"set_default_subvolume(path: str, id: int = 0) -> None\n\n"
"Set the default subvolume for the filesystem at *path*.\n\n"
"If *id* is 0 (default), the subvolume containing *path* is used.\n\n"
"Example::\n\n"
"    >>> pybtrfs.set_default_subvolume('/mnt/btrfs', 256)\n");

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

/* -- create / snapshot / delete -------------------------------------- */

PyDoc_STRVAR(create_subvolume_doc,
"create_subvolume(path: str, qgroup_inherit: QgroupInherit | None = None) -> None\n\n"
"Create a new subvolume at *path*.\n\n"
"Optionally pass a :class:`QgroupInherit` to set up qgroup inheritance\n"
"for the new subvolume.\n\n"
"Example::\n\n"
"    >>> pybtrfs.create_subvolume('/mnt/btrfs/new_subvol')\n"
"    >>> pybtrfs.is_subvolume('/mnt/btrfs/new_subvol')\n"
"    True\n");

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

PyDoc_STRVAR(create_snapshot_doc,
"create_snapshot(source: str, path: str, recursive: bool = False, "
"read_only: bool = False, qgroup_inherit: QgroupInherit | None = None) -> None\n\n"
"Create a snapshot of the subvolume at *source*, placing it at *path*.\n\n"
"Set *recursive* to ``True`` to also snapshot child subvolumes.\n"
"Set *read_only* to ``True`` to make the snapshot read-only.\n\n"
"Example::\n\n"
"    >>> pybtrfs.create_snapshot('/mnt/btrfs/data', '/mnt/btrfs/snap')\n"
"    >>> pybtrfs.create_snapshot(\n"
"    ...     '/mnt/btrfs/data', '/mnt/btrfs/ro_snap', read_only=True\n"
"    ... )\n");

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

PyDoc_STRVAR(delete_subvolume_doc,
"delete_subvolume(path: str, recursive: bool = False) -> None\n\n"
"Delete the subvolume or snapshot at *path*.\n\n"
"Set *recursive* to ``True`` to also delete child subvolumes.\n\n"
"Example::\n\n"
"    >>> pybtrfs.delete_subvolume('/mnt/btrfs/old_snap')\n"
"    >>> pybtrfs.delete_subvolume('/mnt/btrfs/nested', recursive=True)\n");

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

PyDoc_STRVAR(deleted_subvolumes_doc,
"deleted_subvolumes(path: str) -> list[int]\n\n"
"Get IDs of subvolumes that have been deleted but not yet cleaned up.\n\n"
"These are subvolumes awaiting background reclamation by the kernel.\n\n"
"Example::\n\n"
"    >>> pybtrfs.deleted_subvolumes('/mnt/btrfs')\n"
"    [258, 261]\n");

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

/* -- exported method table ------------------------------------------- */

PyMethodDef subvolume_methods[] = {
    {"is_subvolume", (PyCFunction)mod_is_subvolume,
     METH_VARARGS | METH_KEYWORDS, is_subvolume_doc},

    {"subvolume_id", (PyCFunction)mod_subvolume_id,
     METH_VARARGS | METH_KEYWORDS, subvolume_id_doc},

    {"subvolume_path", (PyCFunction)mod_subvolume_path,
     METH_VARARGS | METH_KEYWORDS, subvolume_path_doc},

    {"subvolume_info", (PyCFunction)mod_subvolume_info,
     METH_VARARGS | METH_KEYWORDS, subvolume_info_doc},

    {"get_subvolume_read_only", (PyCFunction)mod_get_subvolume_read_only,
     METH_VARARGS | METH_KEYWORDS, get_subvolume_read_only_doc},

    {"set_subvolume_read_only", (PyCFunction)mod_set_subvolume_read_only,
     METH_VARARGS | METH_KEYWORDS, set_subvolume_read_only_doc},

    {"get_default_subvolume", (PyCFunction)mod_get_default_subvolume,
     METH_VARARGS | METH_KEYWORDS, get_default_subvolume_doc},

    {"set_default_subvolume", (PyCFunction)mod_set_default_subvolume,
     METH_VARARGS | METH_KEYWORDS, set_default_subvolume_doc},

    {"create_subvolume", (PyCFunction)mod_create_subvolume,
     METH_VARARGS | METH_KEYWORDS, create_subvolume_doc},

    {"create_snapshot", (PyCFunction)mod_create_snapshot,
     METH_VARARGS | METH_KEYWORDS, create_snapshot_doc},

    {"delete_subvolume", (PyCFunction)mod_delete_subvolume,
     METH_VARARGS | METH_KEYWORDS, delete_subvolume_doc},

    {"deleted_subvolumes", (PyCFunction)mod_deleted_subvolumes,
     METH_VARARGS | METH_KEYWORDS, deleted_subvolumes_doc},

    {NULL}
};
