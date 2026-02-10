#include "module.h"

static void
QgroupInherit_dealloc(QgroupInheritObject *self)
{
    if (self->inherit)
        btrfs_util_destroy_qgroup_inherit(self->inherit);
    Py_TYPE(self)->tp_free((PyObject *)self);
}

static int
QgroupInherit_init(QgroupInheritObject *self, PyObject *args, PyObject *kwds)
{
    enum btrfs_util_error err;

    if (!PyArg_ParseTuple(args, ""))
        return -1;

    err = btrfs_util_create_qgroup_inherit(0, &self->inherit);
    if (err) {
        set_error(err);
        return -1;
    }
    return 0;
}

static PyObject *
QgroupInherit_add_group(QgroupInheritObject *self, PyObject *args)
{
    uint64_t qgroupid;
    enum btrfs_util_error err;

    if (!PyArg_ParseTuple(args, "K", &qgroupid))
        return NULL;

    err = btrfs_util_qgroup_inherit_add_group(&self->inherit, qgroupid);
    if (err)
        return set_error(err);

    Py_RETURN_NONE;
}

static PyObject *
QgroupInherit_get_groups(QgroupInheritObject *self, PyObject *Py_UNUSED(a))
{
    const uint64_t *groups;
    size_t n;

    btrfs_util_qgroup_inherit_get_groups(self->inherit, &groups, &n);

    PyObject *list = PyList_New((Py_ssize_t)n);
    if (!list)
        return NULL;

    for (size_t i = 0; i < n; i++) {
        PyObject *v = PyLong_FromUnsignedLongLong(groups[i]);
        if (!v) { Py_DECREF(list); return NULL; }
        PyList_SET_ITEM(list, (Py_ssize_t)i, v);
    }
    return list;
}

static PyMethodDef QgroupInherit_methods[] = {
    {"add_group",  (PyCFunction)QgroupInherit_add_group,  METH_VARARGS,
     "add_group(qgroupid: int) -> None\n\nAdd a qgroup to inherit from."},
    {"get_groups", (PyCFunction)QgroupInherit_get_groups, METH_NOARGS,
     "get_groups() -> list[int]\n\nGet the list of qgroup IDs to inherit from."},
    {NULL}
};

PyTypeObject QgroupInheritType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name      = "pybtrfs.QgroupInherit",
    .tp_basicsize = sizeof(QgroupInheritObject),
    .tp_dealloc   = (destructor)QgroupInherit_dealloc,
    .tp_flags     = Py_TPFLAGS_DEFAULT,
    .tp_doc       = "QgroupInherit()\n\nQgroup inheritance specifier.",
    .tp_methods   = QgroupInherit_methods,
    .tp_init      = (initproc)QgroupInherit_init,
    .tp_new       = PyType_GenericNew,
};
