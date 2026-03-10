#define PY_SSIZE_T_CLEAN
#include <Python.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <endian.h>

#include "kernel-shared/uapi/btrfs.h"
#include "kernel-shared/uapi/btrfs_tree.h"

/* -- helper -------------------------------------------------------- */

static int
open_path(const char *path)
{
    int fd = open(path, O_RDONLY);
    if (fd < 0)
        PyErr_SetFromErrnoWithFilename(PyExc_OSError, path);
    return fd;
}

/* -- quota_enable(path) -------------------------------------------- */

PyDoc_STRVAR(quota_enable_doc,
"quota_enable(path: str) -> None\n\n"
"Enable btrfs quotas on the filesystem at *path*.\n\n"
"Calls BTRFS_IOC_QUOTA_CTL with BTRFS_QUOTA_CTL_ENABLE.");

static PyObject *
pybtrfs_quota_enable(PyObject *self, PyObject *args)
{
    const char *path;
    if (!PyArg_ParseTuple(args, "s:quota_enable", &path))
        return NULL;

    int fd = open_path(path);
    if (fd < 0)
        return NULL;

    struct btrfs_ioctl_quota_ctl_args qargs = {
        .cmd = BTRFS_QUOTA_CTL_ENABLE,
    };

    int ret;
    Py_BEGIN_ALLOW_THREADS
    ret = ioctl(fd, BTRFS_IOC_QUOTA_CTL, &qargs);
    Py_END_ALLOW_THREADS

    close(fd);
    if (ret < 0)
        return PyErr_SetFromErrnoWithFilename(PyExc_OSError, path);

    Py_RETURN_NONE;
}

/* -- quota_enable_simple(path) ------------------------------------- */

PyDoc_STRVAR(quota_enable_simple_doc,
"quota_enable_simple(path: str) -> None\n\n"
"Enable simple quotas (squota) on the filesystem at *path*.\n\n"
"Calls BTRFS_IOC_QUOTA_CTL with BTRFS_QUOTA_CTL_ENABLE_SIMPLE_QUOTA.");

static PyObject *
pybtrfs_quota_enable_simple(PyObject *self, PyObject *args)
{
    const char *path;
    if (!PyArg_ParseTuple(args, "s:quota_enable_simple", &path))
        return NULL;

    int fd = open_path(path);
    if (fd < 0)
        return NULL;

    struct btrfs_ioctl_quota_ctl_args qargs = {
        .cmd = BTRFS_QUOTA_CTL_ENABLE_SIMPLE_QUOTA,
    };

    int ret;
    Py_BEGIN_ALLOW_THREADS
    ret = ioctl(fd, BTRFS_IOC_QUOTA_CTL, &qargs);
    Py_END_ALLOW_THREADS

    close(fd);
    if (ret < 0)
        return PyErr_SetFromErrnoWithFilename(PyExc_OSError, path);

    Py_RETURN_NONE;
}

/* -- quota_disable(path) ------------------------------------------- */

PyDoc_STRVAR(quota_disable_doc,
"quota_disable(path: str) -> None\n\n"
"Disable btrfs quotas on the filesystem at *path*.\n\n"
"Calls BTRFS_IOC_QUOTA_CTL with BTRFS_QUOTA_CTL_DISABLE.");

static PyObject *
pybtrfs_quota_disable(PyObject *self, PyObject *args)
{
    const char *path;
    if (!PyArg_ParseTuple(args, "s:quota_disable", &path))
        return NULL;

    int fd = open_path(path);
    if (fd < 0)
        return NULL;

    struct btrfs_ioctl_quota_ctl_args qargs = {
        .cmd = BTRFS_QUOTA_CTL_DISABLE,
    };

    int ret;
    Py_BEGIN_ALLOW_THREADS
    ret = ioctl(fd, BTRFS_IOC_QUOTA_CTL, &qargs);
    Py_END_ALLOW_THREADS

    close(fd);
    if (ret < 0)
        return PyErr_SetFromErrnoWithFilename(PyExc_OSError, path);

    Py_RETURN_NONE;
}

/* -- quota_rescan(path) -------------------------------------------- */

PyDoc_STRVAR(quota_rescan_doc,
"quota_rescan(path: str) -> None\n\n"
"Start a quota rescan on the filesystem at *path*.\n\n"
"Calls BTRFS_IOC_QUOTA_RESCAN.");

static PyObject *
pybtrfs_quota_rescan(PyObject *self, PyObject *args)
{
    const char *path;
    if (!PyArg_ParseTuple(args, "s:quota_rescan", &path))
        return NULL;

    int fd = open_path(path);
    if (fd < 0)
        return NULL;

    struct btrfs_ioctl_quota_rescan_args rargs;
    memset(&rargs, 0, sizeof(rargs));

    int ret;
    Py_BEGIN_ALLOW_THREADS
    ret = ioctl(fd, BTRFS_IOC_QUOTA_RESCAN, &rargs);
    Py_END_ALLOW_THREADS

    close(fd);
    if (ret < 0)
        return PyErr_SetFromErrnoWithFilename(PyExc_OSError, path);

    Py_RETURN_NONE;
}

/* -- quota_rescan_status(path) ------------------------------------- */

PyDoc_STRVAR(quota_rescan_status_doc,
"quota_rescan_status(path: str) -> dict\n\n"
"Return the current quota rescan status as ``{\"flags\": int, \"progress\": int}``.\n\n"
"Calls BTRFS_IOC_QUOTA_RESCAN_STATUS.");

static PyObject *
pybtrfs_quota_rescan_status(PyObject *self, PyObject *args)
{
    const char *path;
    if (!PyArg_ParseTuple(args, "s:quota_rescan_status", &path))
        return NULL;

    int fd = open_path(path);
    if (fd < 0)
        return NULL;

    struct btrfs_ioctl_quota_rescan_args rargs;
    memset(&rargs, 0, sizeof(rargs));

    int ret;
    Py_BEGIN_ALLOW_THREADS
    ret = ioctl(fd, BTRFS_IOC_QUOTA_RESCAN_STATUS, &rargs);
    Py_END_ALLOW_THREADS

    close(fd);
    if (ret < 0)
        return PyErr_SetFromErrnoWithFilename(PyExc_OSError, path);

    return Py_BuildValue("{s:K,s:K}",
                         "flags",    (unsigned long long)rargs.flags,
                         "progress", (unsigned long long)rargs.progress);
}

/* -- quota_rescan_wait(path) --------------------------------------- */

PyDoc_STRVAR(quota_rescan_wait_doc,
"quota_rescan_wait(path: str) -> None\n\n"
"Block until the current quota rescan completes.\n\n"
"Calls BTRFS_IOC_QUOTA_RESCAN_WAIT (releases the GIL).");

static PyObject *
pybtrfs_quota_rescan_wait(PyObject *self, PyObject *args)
{
    const char *path;
    if (!PyArg_ParseTuple(args, "s:quota_rescan_wait", &path))
        return NULL;

    int fd = open_path(path);
    if (fd < 0)
        return NULL;

    int ret;
    Py_BEGIN_ALLOW_THREADS
    ret = ioctl(fd, BTRFS_IOC_QUOTA_RESCAN_WAIT, NULL);
    Py_END_ALLOW_THREADS

    close(fd);
    if (ret < 0)
        return PyErr_SetFromErrnoWithFilename(PyExc_OSError, path);

    Py_RETURN_NONE;
}

/* -- qgroup_create(path, qgroupid) -------------------------------- */

PyDoc_STRVAR(qgroup_create_doc,
"qgroup_create(path: str, qgroupid: int) -> None\n\n"
"Create a new qgroup.\n\n"
"Calls BTRFS_IOC_QGROUP_CREATE with create=1.");

static PyObject *
pybtrfs_qgroup_create(PyObject *self, PyObject *args)
{
    const char *path;
    unsigned long long qgroupid;
    if (!PyArg_ParseTuple(args, "sK:qgroup_create", &path, &qgroupid))
        return NULL;

    int fd = open_path(path);
    if (fd < 0)
        return NULL;

    struct btrfs_ioctl_qgroup_create_args cargs = {
        .create = 1,
        .qgroupid = qgroupid,
    };

    int ret;
    Py_BEGIN_ALLOW_THREADS
    ret = ioctl(fd, BTRFS_IOC_QGROUP_CREATE, &cargs);
    Py_END_ALLOW_THREADS

    close(fd);
    if (ret < 0)
        return PyErr_SetFromErrnoWithFilename(PyExc_OSError, path);

    Py_RETURN_NONE;
}

/* -- qgroup_destroy(path, qgroupid) ------------------------------- */

PyDoc_STRVAR(qgroup_destroy_doc,
"qgroup_destroy(path: str, qgroupid: int) -> None\n\n"
"Destroy an existing qgroup.\n\n"
"Calls BTRFS_IOC_QGROUP_CREATE with create=0.");

static PyObject *
pybtrfs_qgroup_destroy(PyObject *self, PyObject *args)
{
    const char *path;
    unsigned long long qgroupid;
    if (!PyArg_ParseTuple(args, "sK:qgroup_destroy", &path, &qgroupid))
        return NULL;

    int fd = open_path(path);
    if (fd < 0)
        return NULL;

    struct btrfs_ioctl_qgroup_create_args cargs = {
        .create = 0,
        .qgroupid = qgroupid,
    };

    int ret;
    Py_BEGIN_ALLOW_THREADS
    ret = ioctl(fd, BTRFS_IOC_QGROUP_CREATE, &cargs);
    Py_END_ALLOW_THREADS

    close(fd);
    if (ret < 0)
        return PyErr_SetFromErrnoWithFilename(PyExc_OSError, path);

    Py_RETURN_NONE;
}

/* -- qgroup_assign(path, src, dst) --------------------------------- */

PyDoc_STRVAR(qgroup_assign_doc,
"qgroup_assign(path: str, src: int, dst: int) -> None\n\n"
"Assign qgroup *src* as a child of qgroup *dst*.\n\n"
"Calls BTRFS_IOC_QGROUP_ASSIGN with assign=1.");

static PyObject *
pybtrfs_qgroup_assign(PyObject *self, PyObject *args)
{
    const char *path;
    unsigned long long src, dst;
    if (!PyArg_ParseTuple(args, "sKK:qgroup_assign", &path, &src, &dst))
        return NULL;

    int fd = open_path(path);
    if (fd < 0)
        return NULL;

    struct btrfs_ioctl_qgroup_assign_args aargs = {
        .assign = 1,
        .src = src,
        .dst = dst,
    };

    int ret;
    Py_BEGIN_ALLOW_THREADS
    ret = ioctl(fd, BTRFS_IOC_QGROUP_ASSIGN, &aargs);
    Py_END_ALLOW_THREADS

    close(fd);
    if (ret < 0)
        return PyErr_SetFromErrnoWithFilename(PyExc_OSError, path);

    Py_RETURN_NONE;
}

/* -- qgroup_remove(path, src, dst) --------------------------------- */

PyDoc_STRVAR(qgroup_remove_doc,
"qgroup_remove(path: str, src: int, dst: int) -> None\n\n"
"Remove qgroup *src* from parent qgroup *dst*.\n\n"
"Calls BTRFS_IOC_QGROUP_ASSIGN with assign=0.");

static PyObject *
pybtrfs_qgroup_remove(PyObject *self, PyObject *args)
{
    const char *path;
    unsigned long long src, dst;
    if (!PyArg_ParseTuple(args, "sKK:qgroup_remove", &path, &src, &dst))
        return NULL;

    int fd = open_path(path);
    if (fd < 0)
        return NULL;

    struct btrfs_ioctl_qgroup_assign_args aargs = {
        .assign = 0,
        .src = src,
        .dst = dst,
    };

    int ret;
    Py_BEGIN_ALLOW_THREADS
    ret = ioctl(fd, BTRFS_IOC_QGROUP_ASSIGN, &aargs);
    Py_END_ALLOW_THREADS

    close(fd);
    if (ret < 0)
        return PyErr_SetFromErrnoWithFilename(PyExc_OSError, path);

    Py_RETURN_NONE;
}

/* -- qgroup_limit(path, qgroupid, max_rfer=0, max_excl=0) --------- */

PyDoc_STRVAR(qgroup_limit_doc,
"qgroup_limit(path: str, qgroupid: int, max_rfer: int = 0, max_excl: int = 0) -> None\n\n"
"Set quota limits for *qgroupid*. A value of 0 clears the limit.\n\n"
"Calls BTRFS_IOC_QGROUP_LIMIT.");

static PyObject *
pybtrfs_qgroup_limit(PyObject *self, PyObject *args, PyObject *kwargs)
{
    const char *path;
    unsigned long long qgroupid;
    unsigned long long max_rfer = 0;
    unsigned long long max_excl = 0;

    static char *kwlist[] = {"path", "qgroupid", "max_rfer", "max_excl", NULL};

    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "sK|KK:qgroup_limit",
                                     kwlist, &path, &qgroupid,
                                     &max_rfer, &max_excl))
        return NULL;

    int fd = open_path(path);
    if (fd < 0)
        return NULL;

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

    close(fd);
    if (ret < 0)
        return PyErr_SetFromErrnoWithFilename(PyExc_OSError, path);

    Py_RETURN_NONE;
}

/* -- qgroup_info(path) → list[dict] ------------------------------- */

PyDoc_STRVAR(qgroup_info_doc,
"qgroup_info(path: str) -> list[dict]\n\n"
"Return a list of dicts describing every qgroup on the filesystem.\n\n"
"Each dict contains: qgroupid, rfer, excl, rfer_cmpr, excl_cmpr,\n"
"max_rfer, max_excl.\n\n"
"Uses BTRFS_IOC_TREE_SEARCH on the quota tree.");

static PyObject *
pybtrfs_qgroup_info(PyObject *self, PyObject *args)
{
    const char *path;
    if (!PyArg_ParseTuple(args, "s:qgroup_info", &path))
        return NULL;

    int fd = open_path(path);
    if (fd < 0)
        return NULL;

    /* accumulator: qgroupid -> dict */
    PyObject *accum = PyDict_New();
    if (!accum) {
        close(fd);
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
            close(fd);
            Py_DECREF(accum);
            return PyErr_SetFromErrnoWithFilename(PyExc_OSError, path);
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

    close(fd);

    /* convert dict-of-dicts → list-of-dicts */
    PyObject *values = PyDict_Values(accum);
    Py_DECREF(accum);
    if (!values)
        return NULL;

    PyObject *result = PySequence_List(values);
    Py_DECREF(values);
    return result;

error:
    close(fd);
    Py_DECREF(accum);
    return NULL;
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
