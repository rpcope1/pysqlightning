/* cache .c - a LRU cache
 *
 * Copyright (C) 2004 Gerhard H�ring <gh@ghaering.de>
 *
 * This file is part of pysqlite.
 *
 * This software is provided 'as-is', without any express or implied
 * warranty.  In no event will the authors be held liable for any damages
 * arising from the use of this software.
 *
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute it
 * freely, subject to the following restrictions:
 *
 * 1. The origin of this software must not be misrepresented; you must not
 *    claim that you wrote the original software. If you use this software
 *    in a product, an acknowledgment in the product documentation would be
 *    appreciated but is not required.
 * 2. Altered source versions must be plainly marked as such, and must not be
 *    misrepresented as being the original software.
 * 3. This notice may not be removed or altered from any source distribution.
 */

#include "cache.h"

/* only used internally */
Node* new_node(PyObject* key, PyObject* data)
{
    Node* node;

    node = (Node*) (NodeType.tp_alloc(&NodeType, 0));
    /*node = PyObject_New(Node, &NodeType);*/
    if (!node) {
        return NULL;
    }

    Py_INCREF(key);
    node->key = key;

    Py_INCREF(data);
    node->data = data;

    node->prev = NULL;
    node->next = NULL;

    return node;
}

void node_dealloc(Node* self)
{
    Py_DECREF(self->key);
    Py_DECREF(self->data);
    self->ob_type->tp_free((PyObject*)self);
}

int cache_init(Cache* self, PyObject* args, PyObject* kwargs)
{
    PyObject* factory;
    int size = 10;

    if (!PyArg_ParseTuple(args, "O|i", &factory, &size))
    {
        return -1; 
    }

    if (size < 5) {
        size = 5;
    }
    self->size = size;
    self->first = NULL;
    self->last = NULL;
    self->mapping = PyDict_New();
    Py_INCREF(factory);
    self->factory = factory;

    return 0;
}

void cache_dealloc(Cache* self)
{
    Node* node;
    Node* delete_node;

    node = self->first;
    while (node) {
        delete_node = node;
        node = node->next;
        /*node_dealloc(delete_node);*/
        Py_DECREF(delete_node);
    }

    Py_DECREF(self->factory);
    Py_DECREF(self->mapping);
    self->ob_type->tp_free((PyObject*)self);
}

PyObject* cache_get(Cache* self, PyObject* args)
{
    PyObject* key;
    Node* node;
    Node* ptr;
    PyObject* data;

    if (!PyArg_ParseTuple(args, "O", &key))
    {
        return NULL; 
    }

    node = (Node*)PyDict_GetItem(self->mapping, key);
    if (node) {
        node->count++;
        if (node->prev && node->count > node->prev->count) {
            ptr = node->prev;

            while (ptr->prev && node->count > ptr->prev->count) {
                ptr = ptr->prev;
            }

            if (node->next) {
                node->next->prev = node->prev;
            } else {
                self->last = node->prev;
            }
            if (node->prev) {
                node->prev->next = node->next;
            }
            if (ptr->prev) {
                ptr->prev->next = node;
            } else {
                self->first = node;
            }

            node->next = ptr;
            node->prev = ptr->prev;
            if (!node->prev) {
                self->first = node;
            }
            ptr->prev = node;
        }
    } else {
        if (PyDict_Size(self->mapping) == self->size) {
            if (self->last) {
                node = self->last;
                PyDict_DelItem(self->mapping, self->last->key);
                if (node->prev) {
                    node->prev->next = NULL;
                }
                self->last = node->prev;
                node->prev = NULL;
            }
        }

        data = PyObject_CallFunction(self->factory, "O", key);

        if (!data) {
            return NULL;
        }

        node = new_node(key, data);
        node->prev = self->last;

        Py_DECREF(data);

        if (self->last) {
            self->last->next = node;
        } else {
            self->first = node;
        }
        self->last = node;
        PyDict_SetItem(self->mapping, key, (PyObject*)node);
    }

    Py_INCREF(node->data);
    return node->data;
}

PyObject* cache_display(Cache* self, PyObject* args)
{
    Node* ptr;
    PyObject* prevkey;
    PyObject* nextkey;
    PyObject* fmt_args;
    PyObject* template;
    PyObject* display_str;

    ptr = self->first;

    while (ptr) {
        if (ptr->prev) {
            prevkey = ptr->prev->key;
        } else {
            prevkey = Py_None;
        }
        Py_INCREF(prevkey);

        if (ptr->next) {
            nextkey = ptr->next->key;
        } else {
            nextkey = Py_None;
        }
        Py_INCREF(nextkey);

        fmt_args = Py_BuildValue("OOO", prevkey, ptr->key, nextkey);
        template = PyString_FromString("%s <- %s ->%s\n");
        display_str = PyString_Format(template, fmt_args);
        Py_DECREF(template);
        Py_DECREF(fmt_args);
        PyObject_Print(display_str, stdout, Py_PRINT_RAW);
        Py_DECREF(display_str);

        Py_DECREF(prevkey);
        Py_DECREF(nextkey);

        ptr = ptr->next;
    }

    Py_INCREF(Py_None);
    return Py_None;
}

static PyMethodDef cache_methods[] = {
    {"get", (PyCFunction)cache_get, METH_VARARGS,
        PyDoc_STR("Gets an entry from the cache.")},
    {"display", (PyCFunction)cache_display, METH_NOARGS,
        PyDoc_STR("For debugging only.")},
    {NULL, NULL}
};

PyTypeObject NodeType = {
        PyObject_HEAD_INIT(NULL)
        0,                                              /* ob_size */
        "sqlite.Node",                                  /* tp_name */
        sizeof(Node),                                   /* tp_basicsize */
        0,                                              /* tp_itemsize */
        (destructor)node_dealloc,                       /* tp_dealloc */
        0,                                              /* tp_print */
        0,                                              /* tp_getattr */
        0,                                              /* tp_setattr */
        0,                                              /* tp_compare */
        0,                                              /* tp_repr */
        0,                                              /* tp_as_number */
        0,                                              /* tp_as_sequence */
        0,                                              /* tp_as_mapping */
        0,                                              /* tp_hash */
        0,                                              /* tp_call */
        0,                                              /* tp_str */
        PyObject_GenericGetAttr,                        /* tp_getattro */
        0,                                              /* tp_setattro */
        0,                                              /* tp_as_buffer */
        Py_TPFLAGS_DEFAULT|Py_TPFLAGS_BASETYPE,         /* tp_flags */
        0,                                              /* tp_doc */
        0,                                              /* tp_traverse */
        0,                                              /* tp_clear */
        0,                                              /* tp_richcompare */
        0,                                              /* tp_weaklistoffset */
        0,                                              /* tp_iter */
        0,                                              /* tp_iternext */
        0,                                              /* tp_methods */
        0,                                              /* tp_members */
        0,                                              /* tp_getset */
        0,                                              /* tp_base */
        0,                                              /* tp_dict */
        0,                                              /* tp_descr_get */
        0,                                              /* tp_descr_set */
        0,                                              /* tp_dictoffset */
        (initproc)0,                                    /* tp_init */
        0,                                              /* tp_alloc */
        0,                                              /* tp_new */
        0                                               /* tp_free */
};

PyTypeObject CacheType = {
        PyObject_HEAD_INIT(NULL)
        0,                                              /* ob_size */
        "sqlite.Cache",                                 /* tp_name */
        sizeof(Cache),                                  /* tp_basicsize */
        0,                                              /* tp_itemsize */
        (destructor)cache_dealloc,                      /* tp_dealloc */
        0,                                              /* tp_print */
        0,                                              /* tp_getattr */
        0,                                              /* tp_setattr */
        0,                                              /* tp_compare */
        0,                                              /* tp_repr */
        0,                                              /* tp_as_number */
        0,                                              /* tp_as_sequence */
        0,                                              /* tp_as_mapping */
        0,                                              /* tp_hash */
        0,                                              /* tp_call */
        0,                                              /* tp_str */
        PyObject_GenericGetAttr,                        /* tp_getattro */
        0,                                              /* tp_setattro */
        0,                                              /* tp_as_buffer */
        Py_TPFLAGS_DEFAULT|Py_TPFLAGS_BASETYPE,         /* tp_flags */
        0,                                              /* tp_doc */
        0,                                              /* tp_traverse */
        0,                                              /* tp_clear */
        0,                                              /* tp_richcompare */
        0,                                              /* tp_weaklistoffset */
        0,                                              /* tp_iter */
        0,                                              /* tp_iternext */
        cache_methods,                                  /* tp_methods */
        0,                                              /* tp_members */
        0,                                              /* tp_getset */
        0,                                              /* tp_base */
        0,                                              /* tp_dict */
        0,                                              /* tp_descr_get */
        0,                                              /* tp_descr_set */
        0,                                              /* tp_dictoffset */
        (initproc)cache_init,                           /* tp_init */
        0,                                              /* tp_alloc */
        0,                                              /* tp_new */
        0                                               /* tp_free */
};