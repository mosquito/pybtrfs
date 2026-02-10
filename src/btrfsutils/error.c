#include "module.h"
#include <errno.h>

/* ── exception type ────────────────────────────────────────────────── */

PyObject *BtrfsUtilError = NULL;

typedef struct {
    PyOSErrorObject base;
    int btrfsutil_code;
} BtrfsUtilErrorObject;

static PyObject *
BtrfsUtilError_str(PyObject *self)
{
    int code = ((BtrfsUtilErrorObject *)self)->btrfsutil_code;
    const char *btrfs_msg = btrfs_util_strerror((enum btrfs_util_error)code);

    PyObject *os_str = PyObject_CallMethod(
        (PyObject *)&PyBaseObject_Type, "__str__", "O", self);
    if (!os_str)
        return NULL;

    PyObject *result = PyUnicode_FromFormat("%U: %s", os_str, btrfs_msg);
    Py_DECREF(os_str);
    return result;
}

static PyMemberDef BtrfsUtilError_members[] = {
    {"btrfsutil_errno", T_INT,
     offsetof(BtrfsUtilErrorObject, btrfsutil_code), READONLY,
     "libbtrfsutil error code"},
    {NULL}
};

static PyType_Slot BtrfsUtilError_slots[] = {
    {Py_tp_str,     BtrfsUtilError_str},
    {Py_tp_members, BtrfsUtilError_members},
    {0, NULL}
};

PyType_Spec BtrfsUtilError_spec = {
    .name      = "pybtrfs.BtrfsUtilError",
    .basicsize = sizeof(BtrfsUtilErrorObject),
    .flags     = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,
    .slots     = BtrfsUtilError_slots,
};

/* ── helper: raise BtrfsUtilError from an error code ───────────────── */

PyObject *
set_error(enum btrfs_util_error err)
{
    int saved_errno = errno;
    const char *msg = btrfs_util_strerror(err);

    PyObject *exc_args = Py_BuildValue("(is)", saved_errno, msg);
    if (!exc_args)
        return NULL;

    PyObject *exc = PyObject_Call(BtrfsUtilError, exc_args, NULL);
    Py_DECREF(exc_args);
    if (!exc)
        return NULL;

    ((BtrfsUtilErrorObject *)exc)->btrfsutil_code = (int)err;

    PyErr_SetObject(BtrfsUtilError, exc);
    Py_DECREF(exc);
    return NULL;
}

/* called from module init */
PyObject *
BtrfsUtilError_type_new(void)
{
    PyObject *bases = PyTuple_Pack(1, PyExc_OSError);
    if (!bases)
        return NULL;
    PyObject *type = PyType_FromSpecWithBases(&BtrfsUtilError_spec, bases);
    Py_DECREF(bases);
    return type;
}
