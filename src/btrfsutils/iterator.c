#include "module.h"
#include <stdlib.h>

typedef struct {
    PyObject_HEAD
    struct btrfs_util_subvolume_iterator *iter;
    int info_flag;
} SubvolumeIteratorObject;

static void
SubvolumeIterator_dealloc(SubvolumeIteratorObject *self)
{
    if (self->iter)
        btrfs_util_destroy_subvolume_iterator(self->iter);
    Py_TYPE(self)->tp_free((PyObject *)self);
}

static int
SubvolumeIterator_init(SubvolumeIteratorObject *self,
                       PyObject *args, PyObject *kwds)
{
    static char *kw[] = {"path", "top", "post_order", "info", NULL};
    const char *path;
    uint64_t top = 0;
    int post_order = 0, info = 0, flags = 0;
    enum btrfs_util_error err;

    if (!PyArg_ParseTupleAndKeywords(args, kwds, "s|Kpp", kw,
                                     &path, &top, &post_order, &info))
        return -1;

    if (post_order)
        flags |= BTRFS_UTIL_SUBVOLUME_ITERATOR_POST_ORDER;

    self->info_flag = info;

    Py_BEGIN_ALLOW_THREADS
    err = btrfs_util_create_subvolume_iterator(path, top, flags, &self->iter);
    Py_END_ALLOW_THREADS

    if (err) {
        set_error(err);
        return -1;
    }
    return 0;
}

static PyObject *
SubvolumeIterator_next(SubvolumeIteratorObject *self)
{
    enum btrfs_util_error err;
    char *path = NULL;

    if (!self->iter) {
        PyErr_SetString(PyExc_ValueError, "iterator is closed");
        return NULL;
    }

    if (self->info_flag) {
        struct btrfs_util_subvolume_info info;

        Py_BEGIN_ALLOW_THREADS
        err = btrfs_util_subvolume_iterator_next_info(
            self->iter, &path, &info);
        Py_END_ALLOW_THREADS

        if (err == BTRFS_UTIL_ERROR_STOP_ITERATION)
            return NULL;                 /* sets StopIteration */
        if (err)
            return set_error(err);

        PyObject *p = PyUnicode_DecodeFSDefault(path);
        free(path);
        if (!p)
            return NULL;

        PyObject *i = SubvolumeInfo_from_struct(&info);
        if (!i) { Py_DECREF(p); return NULL; }

        PyObject *t = PyTuple_Pack(2, p, i);
        Py_DECREF(p);
        Py_DECREF(i);
        return t;
    }

    uint64_t id;

    Py_BEGIN_ALLOW_THREADS
    err = btrfs_util_subvolume_iterator_next(self->iter, &path, &id);
    Py_END_ALLOW_THREADS

    if (err == BTRFS_UTIL_ERROR_STOP_ITERATION)
        return NULL;
    if (err)
        return set_error(err);

    PyObject *p = PyUnicode_DecodeFSDefault(path);
    free(path);
    if (!p)
        return NULL;

    PyObject *id_obj = PyLong_FromUnsignedLongLong(id);
    if (!id_obj) { Py_DECREF(p); return NULL; }

    PyObject *t = PyTuple_Pack(2, p, id_obj);
    Py_DECREF(p);
    Py_DECREF(id_obj);
    return t;
}

/* close / context-manager */

static PyObject *
SubvolumeIterator_close(SubvolumeIteratorObject *self, PyObject *Py_UNUSED(a))
{
    if (self->iter) {
        btrfs_util_destroy_subvolume_iterator(self->iter);
        self->iter = NULL;
    }
    Py_RETURN_NONE;
}

static PyObject *
SubvolumeIterator_enter(SubvolumeIteratorObject *self, PyObject *Py_UNUSED(a))
{
    return Py_NewRef(self);
}

static PyObject *
SubvolumeIterator_exit(SubvolumeIteratorObject *self, PyObject *args)
{
    return SubvolumeIterator_close(self, NULL);
}

static PyObject *
SubvolumeIterator_get_fd(SubvolumeIteratorObject *self, void *closure)
{
    if (!self->iter) {
        PyErr_SetString(PyExc_ValueError, "iterator is closed");
        return NULL;
    }
    return PyLong_FromLong(btrfs_util_subvolume_iterator_fd(self->iter));
}

/* ── type tables ───────────────────────────────────────────────────── */

static PyMethodDef SubvolumeIterator_methods[] = {
    {"close",     (PyCFunction)SubvolumeIterator_close, METH_NOARGS,
     "close() -> None\n\nClose the iterator and release resources."},
    {"__enter__", (PyCFunction)SubvolumeIterator_enter, METH_NOARGS,
     "__enter__() -> SubvolumeIterator\n\nEnter the context manager."},
    {"__exit__",  (PyCFunction)SubvolumeIterator_exit,  METH_VARARGS,
     "__exit__(*args) -> None\n\nExit the context manager and close the iterator."},
    {NULL}
};

static PyGetSetDef SubvolumeIterator_getset[] = {
    {"fd", (getter)SubvolumeIterator_get_fd, NULL,
     "File descriptor for the iterator.", NULL},
    {NULL}
};

PyTypeObject SubvolumeIteratorType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name      = "pybtrfs.SubvolumeIterator",
    .tp_basicsize = sizeof(SubvolumeIteratorObject),
    .tp_dealloc   = (destructor)SubvolumeIterator_dealloc,
    .tp_flags     = Py_TPFLAGS_DEFAULT,
    .tp_doc       = "SubvolumeIterator(path: str, top: int = 0, post_order: bool = False, info: bool = False)\n\n"
                    "Iterator over Btrfs subvolumes.",
    .tp_iter      = PyObject_SelfIter,
    .tp_iternext  = (iternextfunc)SubvolumeIterator_next,
    .tp_methods   = SubvolumeIterator_methods,
    .tp_getset    = SubvolumeIterator_getset,
    .tp_init      = (initproc)SubvolumeIterator_init,
    .tp_new       = PyType_GenericNew,
};
