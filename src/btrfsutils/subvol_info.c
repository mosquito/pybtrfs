#include "module.h"

/* ── helpers ───────────────────────────────────────────────────────── */

static PyObject *
uuid_to_bytes(const uint8_t uuid[16])
{
    return PyBytes_FromStringAndSize((const char *)uuid, 16);
}

static PyObject *
timespec_to_float(const struct timespec *ts)
{
    return PyFloat_FromDouble(
        (double)ts->tv_sec + (double)ts->tv_nsec / 1e9);
}

/* ── SubvolumeInfo type ────────────────────────────────────────────── */

typedef struct {
    PyObject_HEAD
    uint64_t id;
    uint64_t parent_id;
    uint64_t dir_id;
    uint64_t flags;
    PyObject *uuid;
    PyObject *parent_uuid;
    PyObject *received_uuid;
    uint64_t generation;
    uint64_t ctransid;
    uint64_t otransid;
    uint64_t stransid;
    uint64_t rtransid;
    PyObject *ctime;
    PyObject *otime;
    PyObject *stime;
    PyObject *rtime;
} SubvolumeInfoObject;

static void
SubvolumeInfo_dealloc(SubvolumeInfoObject *self)
{
    Py_XDECREF(self->uuid);
    Py_XDECREF(self->parent_uuid);
    Py_XDECREF(self->received_uuid);
    Py_XDECREF(self->ctime);
    Py_XDECREF(self->otime);
    Py_XDECREF(self->stime);
    Py_XDECREF(self->rtime);
    Py_TYPE(self)->tp_free((PyObject *)self);
}

static PyObject *
SubvolumeInfo_repr(SubvolumeInfoObject *self)
{
    return PyUnicode_FromFormat(
        "SubvolumeInfo(id=%llu, parent_id=%llu, generation=%llu)",
        (unsigned long long)self->id,
        (unsigned long long)self->parent_id,
        (unsigned long long)self->generation);
}

static PyMemberDef SubvolumeInfo_members[] = {
    {"id",            T_ULONGLONG, offsetof(SubvolumeInfoObject, id),            READONLY, NULL},
    {"parent_id",     T_ULONGLONG, offsetof(SubvolumeInfoObject, parent_id),     READONLY, NULL},
    {"dir_id",        T_ULONGLONG, offsetof(SubvolumeInfoObject, dir_id),        READONLY, NULL},
    {"flags",         T_ULONGLONG, offsetof(SubvolumeInfoObject, flags),         READONLY, NULL},
    {"uuid",          T_OBJECT_EX, offsetof(SubvolumeInfoObject, uuid),          READONLY, NULL},
    {"parent_uuid",   T_OBJECT_EX, offsetof(SubvolumeInfoObject, parent_uuid),   READONLY, NULL},
    {"received_uuid", T_OBJECT_EX, offsetof(SubvolumeInfoObject, received_uuid), READONLY, NULL},
    {"generation",    T_ULONGLONG, offsetof(SubvolumeInfoObject, generation),    READONLY, NULL},
    {"ctransid",      T_ULONGLONG, offsetof(SubvolumeInfoObject, ctransid),      READONLY, NULL},
    {"otransid",      T_ULONGLONG, offsetof(SubvolumeInfoObject, otransid),      READONLY, NULL},
    {"stransid",      T_ULONGLONG, offsetof(SubvolumeInfoObject, stransid),      READONLY, NULL},
    {"rtransid",      T_ULONGLONG, offsetof(SubvolumeInfoObject, rtransid),      READONLY, NULL},
    {"ctime",         T_OBJECT_EX, offsetof(SubvolumeInfoObject, ctime),         READONLY, NULL},
    {"otime",         T_OBJECT_EX, offsetof(SubvolumeInfoObject, otime),         READONLY, NULL},
    {"stime",         T_OBJECT_EX, offsetof(SubvolumeInfoObject, stime),         READONLY, NULL},
    {"rtime",         T_OBJECT_EX, offsetof(SubvolumeInfoObject, rtime),         READONLY, NULL},
    {NULL}
};

PyTypeObject SubvolumeInfoType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name      = "pybtrfs.SubvolumeInfo",
    .tp_basicsize = sizeof(SubvolumeInfoObject),
    .tp_dealloc   = (destructor)SubvolumeInfo_dealloc,
    .tp_repr      = (reprfunc)SubvolumeInfo_repr,
    .tp_flags     = Py_TPFLAGS_DEFAULT,
    .tp_doc       = "Btrfs subvolume information.",
    .tp_members   = SubvolumeInfo_members,
    .tp_new       = PyType_GenericNew,
};

PyObject *
SubvolumeInfo_from_struct(const struct btrfs_util_subvolume_info *s)
{
    SubvolumeInfoObject *self = (SubvolumeInfoObject *)
        SubvolumeInfoType.tp_alloc(&SubvolumeInfoType, 0);
    if (!self)
        return NULL;

    self->id         = s->id;
    self->parent_id  = s->parent_id;
    self->dir_id     = s->dir_id;
    self->flags      = s->flags;
    self->generation = s->generation;
    self->ctransid   = s->ctransid;
    self->otransid   = s->otransid;
    self->stransid   = s->stransid;
    self->rtransid   = s->rtransid;

    self->uuid          = uuid_to_bytes(s->uuid);
    self->parent_uuid   = uuid_to_bytes(s->parent_uuid);
    self->received_uuid = uuid_to_bytes(s->received_uuid);
    self->ctime         = timespec_to_float(&s->ctime);
    self->otime         = timespec_to_float(&s->otime);
    self->stime         = timespec_to_float(&s->stime);
    self->rtime         = timespec_to_float(&s->rtime);

    if (!self->uuid || !self->parent_uuid || !self->received_uuid ||
        !self->ctime || !self->otime || !self->stime || !self->rtime) {
        Py_DECREF(self);
        return NULL;
    }

    return (PyObject *)self;
}
