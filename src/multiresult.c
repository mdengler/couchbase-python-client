/**
 *     Copyright 2013 Couchbase, Inc.
 *
 *   Licensed under the Apache License, Version 2.0 (the "License");
 *   you may not use this file except in compliance with the License.
 *   You may obtain a copy of the License at
 *
 *       http://www.apache.org/licenses/LICENSE-2.0
 *
 *   Unless required by applicable law or agreed to in writing, software
 *   distributed under the License is distributed on an "AS IS" BASIS,
 *   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *   See the License for the specific language governing permissions and
 *   limitations under the License.
 **/

#include "pycbc.h"
#include "structmember.h"


static struct PyMemberDef MultiResult_TABLE_members[] = {
        { "all_ok",
                T_INT, offsetof(pycbc_MultiResult, all_ok),
                READONLY,
                PyDoc_STR("Whether all the items in this result are successful")
        },
        { NULL }
};

PyTypeObject pycbc_MultiResultType = {
        PYCBC_POBJ_HEAD_INIT(NULL)
        0
};

static PyMethodDef MultiResult_TABLE_methods[] = {
        { NULL }
};

PyTypeObject pycbc_AsyncResultType = {
        PYCBC_POBJ_HEAD_INIT(NULL)
        0
};

static struct PyMemberDef AsyncResult_TABLE_members[] = {
        { "remaining",
                T_UINT, offsetof(pycbc_AsyncResult, nops),
                READONLY,
                PyDoc_STR("Number of operations remaining for this 'AsyncResult")
        },

        { "callback",
                T_OBJECT_EX, offsetof(pycbc_AsyncResult, callback),
                0,
                PyDoc_STR("Callback to be invoked with this result")
        },

        { "errback",
                T_OBJECT_EX, offsetof(pycbc_AsyncResult, errback),
                0,
                PyDoc_STR("Callback to be invoked with any errors")
        },

        { NULL }
};

static PyObject *
AsyncResult_set_callbacks(pycbc_AsyncResult *self, PyObject *args)
{
    PyObject *errcb = NULL;
    PyObject *okcb = NULL;

    if (!PyArg_ParseTuple(args, "OO", &okcb, &errcb)) {
        PYCBC_EXCTHROW_ARGS();
        return NULL;
    }

    Py_XINCREF(errcb);
    Py_XINCREF(okcb);

    Py_XDECREF(self->callback);
    Py_XDECREF(self->errback);

    self->callback = okcb;
    self->errback = errcb;
    Py_RETURN_NONE;
}

static PyObject *
AsyncResult_set_single(pycbc_AsyncResult *self)
{
    if (self->nops != 1) {
        PYCBC_EXC_WRAP(PYCBC_EXC_ARGUMENTS, 0,
                       "Cannot set mode to single. "
                       "AsyncResult has more than one operation");
        return NULL;
    }

    self->base.mropts |= PYCBC_MRES_F_SINGLE;
    Py_RETURN_NONE;
}

static struct PyMethodDef AsyncResult_TABLE_methods[] = {
        { "set_callbacks", (PyCFunction)AsyncResult_set_callbacks,
                METH_VARARGS,
                PyDoc_STR("Set the ok and error callbacks")
        },

        { "_set_single", (PyCFunction)AsyncResult_set_single,
                METH_NOARGS,
                PyDoc_STR("Indicate that this is a 'single' result to be "
                        "wrapped")
        },

        { NULL }
};

static int
MultiResultType__init__(pycbc_MultiResult *self, PyObject *args, PyObject *kwargs)
{
    if (pycbc_multiresult_init_dict(self, args, kwargs) < 0) {
        PyErr_Print();
        abort();
        return -1;
    }

    self->all_ok = 1;
    self->exceptions = NULL;
    self->errop = NULL;
    self->mropts = 0;

    return 0;
}

static void
MultiResult_dealloc(pycbc_MultiResult *self)
{
    Py_XDECREF(self->parent);
    Py_XDECREF(self->exceptions);
    Py_XDECREF(self->errop);
    pycbc_multiresult_destroy_dict(self);
}

int
pycbc_MultiResultType_init(PyObject **ptr)
{
    PyTypeObject *p = &pycbc_MultiResultType;

    *ptr = (PyObject*)p;
    if (p->tp_name) {
        return 0;
    }

    pycbc_multiresult_set_base(p);

    p->tp_init = (initproc)MultiResultType__init__;
    p->tp_dealloc = (destructor)MultiResult_dealloc;

    p->tp_name = "MultiResult";
    p->tp_doc = PyDoc_STR(
            ":class:`dict` subclass to hold :class:`Result` objects\n"
            "\n"
            "This object also contains some of the heavy lifting, but this\n"
            "is not currently exposed in python-space\n."
            "\n"
            "An additional method is :meth:`all_ok`, which allows to see\n"
            "if all commands completed successfully\n"
            "\n");

    p->tp_basicsize = sizeof(pycbc_MultiResult);
    p->tp_members = MultiResult_TABLE_members;
    p->tp_methods = MultiResult_TABLE_methods;
    p->tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE;

    return PyType_Ready(p);
}

/**
 * AsyncResult
 */
static int
AsyncResult__init__(pycbc_AsyncResult *self, PyObject *args, PyObject *kwargs)
{
    if (MultiResultType__init__((pycbc_MultiResult *)self, args, kwargs) != 0) {
        return -1;
    }

    self->nops = 0;
    self->callback = NULL;
    self->errback = NULL;
    self->base.mropts |= PYCBC_MRES_F_ASYNC;
    return 0;
}

static void
AsyncResult_dealloc(pycbc_AsyncResult *self)
{
    Py_XDECREF(self->callback);
    Py_XDECREF(self->errback);
    MultiResult_dealloc(&self->base);
}

int
pycbc_AsyncResultType_init(PyObject **ptr)
{
    PyTypeObject *p = &pycbc_AsyncResultType;
    *ptr = (PyObject *)p;
    if (p->tp_name) {
        return 0;
    }

    p->tp_base = &pycbc_MultiResultType;
    p->tp_init = (initproc)AsyncResult__init__;
    p->tp_dealloc = (destructor)AsyncResult_dealloc;
    p->tp_members = AsyncResult_TABLE_members;
    p->tp_basicsize = sizeof(pycbc_AsyncResult);
    p->tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE;
    p->tp_methods = AsyncResult_TABLE_methods;
    p->tp_name = "AsyncResult";

    return PyType_Ready(p);
}

PyObject *
pycbc_multiresult_new(pycbc_Connection *parent)
{
    PyTypeObject *initmeth;
    pycbc_MultiResult *ret;

    if (parent->flags & PYCBC_CONN_F_ASYNC) {
        initmeth = &pycbc_AsyncResultType;

    } else {
        initmeth = &pycbc_MultiResultType;
    }

    ret = (pycbc_MultiResult*) PyObject_CallFunction((PyObject*)initmeth,
                                                     NULL,
                                                     NULL);
    if (!ret) {
        PyErr_Print();
        abort();
    }
    ret->parent = parent;
    Py_INCREF(parent);

    if (parent->pipeline_queue) {
        PyList_Append(parent->pipeline_queue, (PyObject *)ret);
    }

    return (PyObject*)ret;
}

/**
 * This function raises exceptions from the MultiResult object, as required
 */
int
pycbc_multiresult_maybe_raise(pycbc_MultiResult *self)
{
    PyObject *type = NULL, *value = NULL, *traceback = NULL;

    if (self->errop == NULL && self->exceptions == NULL) {
        return 0;
    }

    if (self->exceptions) {
        PyObject *tuple = PyList_GetItem(self->exceptions, 0);

        pycbc_assert(tuple);
        pycbc_assert(PyTuple_Size(tuple) == 3);

        type = PyTuple_GetItem(tuple, 0);
        value = PyTuple_GetItem(tuple, 1);
        traceback = PyTuple_GetItem(tuple, 2);
        PyErr_NormalizeException(&type, &value, &traceback);
        Py_XINCREF(type);
        Py_XINCREF(value);
        Py_XINCREF(traceback);

        pycbc_assert(PyObject_IsInstance(value,
                                         pycbc_helpers.default_exception));

    } else {
        pycbc_Result *res = (pycbc_Result*)self->errop;

        /** Craft an exception based on the operation */
        PYCBC_EXC_WRAP_KEY(PYCBC_EXC_LCBERR, res->rc, "Operational Error", res->key);

        /** Now we have an exception. Let's fetch it back */
        PyErr_Fetch(&type, &value, &traceback);
        PyObject_SetAttrString(value, "result", (PyObject*)res);
    }

    PyObject_SetAttrString(value, "all_results", pycbc_multiresult_wrap(self));
    PyErr_Restore(type, value, traceback);

    /**
     * This is needed since the exception object will later contain
     * a reference to ourselves. If we don't free the original exception,
     * then we'll be stuck with a circular reference
     */
    Py_XDECREF(self->exceptions);
    Py_XDECREF(self->errop);
    self->exceptions = NULL;
    self->errop = NULL;

    return 1;
}

PyObject *
pycbc_multiresult_get_result(pycbc_MultiResult *self)
{
    int rv;
    Py_ssize_t dictpos = 0;
    PyObject *key, *value;

    if (!(self->mropts & PYCBC_MRES_F_SINGLE)) {
        PyObject *res = pycbc_multiresult_wrap(self);
        Py_INCREF(res);
        return res;
    }

    rv = PyDict_Next(pycbc_multiresult_dict(self), &dictpos, &key, &value);
    if (!rv) {
        PYCBC_EXC_WRAP(PYCBC_EXC_INTERNAL, 0, "No objects in MultiResult");
        return NULL;
    }

    Py_INCREF(value);
    return value;
}

void
pycbc_asyncresult_invoke(pycbc_AsyncResult *ares)
{
    PyObject *argtuple;
    PyObject *cbmeth;

    if (!pycbc_multiresult_maybe_raise(&ares->base)) {
        /** All OK */
        PyObject *eres = pycbc_multiresult_get_result(&ares->base);
        cbmeth = ares->callback;
        argtuple = PyTuple_New(1);
        PyTuple_SET_ITEM(argtuple, 0, (PyObject *)eres);

    } else {
        PyObject *ex_value, *ex_type, *ex_tb;

        PyErr_Fetch(&ex_type, &ex_value, &ex_tb);

        if (!ex_type) {
            ex_type = Py_None; Py_INCREF(ex_type);
        }
        if (!ex_value) {
            ex_value = Py_None; Py_INCREF(ex_value);
        }
        if (!ex_tb) {
            ex_tb = Py_None; Py_INCREF(ex_tb);
        }

        cbmeth = ares->errback;
        argtuple = PyTuple_New(4);

        PyTuple_SET_ITEM(argtuple, 0, (PyObject *)ares);
        Py_INCREF(ares);

        PyTuple_SET_ITEM(argtuple, 1, ex_type);
        PyTuple_SET_ITEM(argtuple, 2, ex_value);
        PyTuple_SET_ITEM(argtuple, 3, ex_tb);
    }

    if (!cbmeth) {
        PYCBC_EXC_WRAP(PYCBC_EXC_INTERNAL, 0, "No callbacks provided");
    } else {
        PyObject *res =  PyObject_CallObject(cbmeth, argtuple);
        if (res) {
            Py_XDECREF(res);
        } else {
            PyErr_Print();
        }
    }

    Py_DECREF(argtuple);
    Py_DECREF(ares);
}
