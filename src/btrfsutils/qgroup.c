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

PyDoc_STRVAR(QgroupInherit_add_group_doc,
"add_group(qgroupid: int) -> None\n\n"
"Add a qgroup ID to the inheritance list.\n\n"
"The new subvolume will be automatically assigned to the specified\n"
"qgroup when created with this :class:`QgroupInherit`.\n\n"
"Example::\n\n"
"    >>> from pybtrfs import qgroupid, QgroupInherit\n"
"    >>> qg = QgroupInherit()\n"
"    >>> qg.add_group(qgroupid(1, 1))\n"
"    >>> qg.get_groups()\n"
"    [281474976710657]\n");

PyDoc_STRVAR(QgroupInherit_get_groups_doc,
"get_groups() -> list[int]\n\n"
"Get the list of qgroup IDs to inherit from.\n\n"
"Returns the raw uint64 qgroup IDs previously added with\n"
":meth:`add_group`.\n\n"
"Example::\n\n"
"    >>> qg = pybtrfs.QgroupInherit()\n"
"    >>> qg.get_groups()\n"
"    []\n"
"    >>> qg.add_group(0)\n"
"    >>> qg.get_groups()\n"
"    [0]\n");

static PyMethodDef QgroupInherit_methods[] = {
    {"add_group",  (PyCFunction)QgroupInherit_add_group,  METH_VARARGS,
     QgroupInherit_add_group_doc},
    {"get_groups", (PyCFunction)QgroupInherit_get_groups, METH_NOARGS,
     QgroupInherit_get_groups_doc},
    {NULL}
};

PyTypeObject QgroupInheritType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name      = "pybtrfs.QgroupInherit",
    .tp_basicsize = sizeof(QgroupInheritObject),
    .tp_dealloc   = (destructor)QgroupInherit_dealloc,
    .tp_flags     = Py_TPFLAGS_DEFAULT,
    .tp_doc       = "QgroupInherit()\n\n"
                    "Qgroup inheritance specifier for subvolume creation.\n\n"
                    "Build a list of qgroups that a new subvolume should inherit\n"
                    "from, then pass the object to :func:`create_subvolume` or\n"
                    ":func:`create_snapshot`.\n\n"
                    "Example::\n\n"
                    "    >>> from pybtrfs import qgroupid, QgroupInherit, create_subvolume\n"
                    "    >>> qg = QgroupInherit()\n"
                    "    >>> qg.add_group(qgroupid(1, 1))\n"
                    "    >>> create_subvolume('/mnt/btrfs/vol', qgroup_inherit=qg)\n",
    .tp_methods   = QgroupInherit_methods,
    .tp_init      = (initproc)QgroupInherit_init,
    .tp_new       = PyType_GenericNew,
};
