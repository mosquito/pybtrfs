#include "module.h"

/* defined in error.c */
extern PyObject *BtrfsUtilError_type_new(void);

/* ── merge sentinel-terminated method tables ───────────────────────── */

static int
count_methods(PyMethodDef *m)
{
    int n = 0;
    while (m->ml_name) { n++; m++; }
    return n;
}

static PyMethodDef *
merge_methods(void)
{
    int ns = count_methods(sync_methods);
    int nv = count_methods(subvolume_methods);
    int total = ns + nv;

    PyMethodDef *all = PyMem_Calloc((size_t)(total + 1), sizeof(PyMethodDef));
    if (!all)
        return NULL;

    memcpy(all, sync_methods, (size_t)ns * sizeof(PyMethodDef));
    memcpy(all + ns, subvolume_methods, (size_t)nv * sizeof(PyMethodDef));
    /* last entry is already zeroed by Calloc */
    return all;
}

/* ── module definition ─────────────────────────────────────────────── */

static struct PyModuleDef module_def = {
    PyModuleDef_HEAD_INIT,
    .m_name    = "btrfsutils",
    .m_doc     = "Python bindings for libbtrfsutil.",
    .m_size    = -1,
    .m_methods = NULL,  /* filled in PyInit */
};

PyMODINIT_FUNC
PyInit_btrfsutils(void)
{
    /* types */
    if (PyType_Ready(&SubvolumeInfoType) < 0)
        return NULL;
    if (PyType_Ready(&SubvolumeIteratorType) < 0)
        return NULL;
    if (PyType_Ready(&QgroupInheritType) < 0)
        return NULL;

    /* merge method tables */
    PyMethodDef *methods = merge_methods();
    if (!methods)
        return PyErr_NoMemory();
    module_def.m_methods = methods;

    PyObject *m = PyModule_Create(&module_def);
    if (!m) {
        PyMem_Free(methods);
        return NULL;
    }

    /* exception */
    BtrfsUtilError = BtrfsUtilError_type_new();
    if (!BtrfsUtilError) { Py_DECREF(m); return NULL; }
    if (PyModule_AddObject(m, "BtrfsUtilError", BtrfsUtilError) < 0) {
        Py_DECREF(BtrfsUtilError);
        Py_DECREF(m);
        return NULL;
    }

    /* types */
    Py_INCREF(&SubvolumeInfoType);
    if (PyModule_AddObject(m, "SubvolumeInfo",
                           (PyObject *)&SubvolumeInfoType) < 0)
        goto fail;

    Py_INCREF(&SubvolumeIteratorType);
    if (PyModule_AddObject(m, "SubvolumeIterator",
                           (PyObject *)&SubvolumeIteratorType) < 0)
        goto fail;

    Py_INCREF(&QgroupInheritType);
    if (PyModule_AddObject(m, "QgroupInherit",
                           (PyObject *)&QgroupInheritType) < 0)
        goto fail;

    /* __annotations__ for BtrfsUtilError (heap type) */
    {
        PyObject *ann = PyDict_New();
        if (!ann) goto fail;
        PyDict_SetItemString(ann, "btrfsutil_errno", (PyObject *)&PyLong_Type);
        PyObject_SetAttrString(BtrfsUtilError, "__annotations__", ann);
        Py_DECREF(ann);
    }

    /* error-code constants */
    PyModule_AddIntConstant(m, "ERROR_OK",
                            BTRFS_UTIL_OK);
    PyModule_AddIntConstant(m, "ERROR_STOP_ITERATION",
                            BTRFS_UTIL_ERROR_STOP_ITERATION);
    PyModule_AddIntConstant(m, "ERROR_NO_MEMORY",
                            BTRFS_UTIL_ERROR_NO_MEMORY);
    PyModule_AddIntConstant(m, "ERROR_INVALID_ARGUMENT",
                            BTRFS_UTIL_ERROR_INVALID_ARGUMENT);
    PyModule_AddIntConstant(m, "ERROR_NOT_BTRFS",
                            BTRFS_UTIL_ERROR_NOT_BTRFS);
    PyModule_AddIntConstant(m, "ERROR_NOT_SUBVOLUME",
                            BTRFS_UTIL_ERROR_NOT_SUBVOLUME);
    PyModule_AddIntConstant(m, "ERROR_SUBVOLUME_NOT_FOUND",
                            BTRFS_UTIL_ERROR_SUBVOLUME_NOT_FOUND);

    /* flag constants */
    PyModule_AddIntMacro(m, BTRFS_UTIL_CREATE_SNAPSHOT_RECURSIVE);
    PyModule_AddIntMacro(m, BTRFS_UTIL_CREATE_SNAPSHOT_READ_ONLY);
    PyModule_AddIntMacro(m, BTRFS_UTIL_DELETE_SUBVOLUME_RECURSIVE);
    PyModule_AddIntMacro(m, BTRFS_UTIL_SUBVOLUME_ITERATOR_POST_ORDER);

    return m;

fail:
    Py_DECREF(m);
    return NULL;
}
