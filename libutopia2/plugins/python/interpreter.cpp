/*****************************************************************************
 *  
 *   This file is part of the Utopia Documents application.
 *       Copyright (c) 2008-2014 Lost Island Labs
 *   
 *   Utopia Documents is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU GENERAL PUBLIC LICENSE VERSION 3 as
 *   published by the Free Software Foundation.
 *   
 *   Utopia Documents is distributed in the hope that it will be useful, but
 *   WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General
 *   Public License for more details.
 *   
 *   In addition, as a special exception, the copyright holders give
 *   permission to link the code of portions of this program with the OpenSSL
 *   library under certain conditions as described in each individual source
 *   file, and distribute linked combinations including the two.
 *   
 *   You must obey the GNU General Public License in all respects for all of
 *   the code used other than OpenSSL. If you modify file(s) with this
 *   exception, you may extend this exception to your version of the file(s),
 *   but you are not obligated to do so. If you do not wish to do so, delete
 *   this exception statement from your version.
 *   
 *   You should have received a copy of the GNU General Public License
 *   along with Utopia Documents. If not, see <http://www.gnu.org/licenses/>
 *  
 *****************************************************************************/

#include "interpreter.h"

#include <utopia2/global.h>
#include <boost/python.hpp>
#include <iostream>
#include <stdlib.h>
#include <datetime.h>

#ifdef _WIN32
#include <windows.h>
#endif

#ifndef REMAP_BUNDLED_PYTHON
#include <dlfcn.h>
#endif


namespace python = boost::python;


namespace
{

    QString joinPath(const QString& s1_, const QString& s2_, const QString& s3_ = "", const QString& s4_ = "", const QString& s5_ = "", const QString& s6_ = "")
    {
#ifdef _WIN32
        QString sep = "\\";
#else
        QString sep = "/";
#endif
        QString path = s1_ + sep + s2_;
        if (!s3_.isEmpty()) {
            path += sep + s3_;
            if (!s4_.isEmpty()) {
                path += sep + s4_;
                if (!s5_.isEmpty()) {
                    path += sep + s5_;
                    if (!s6_.isEmpty()) {
                        path += sep + s6_;
                    }
                }
            }
        }

        return path;
    }

}

#define PYTHON_STEP ("python" PYTHON_VERSION)

PythonInterpreter::PythonInterpreter()
{
    // Initialise python interpreter
#ifdef REMAP_BUNDLED_PYTHON
#ifdef _WIN32
        putenv("PYTHONHOME=utopia/python");
        SetEnvironmentVariable("PYTHONHOME", joinPath(Utopia::private_library_path(), "python").toUtf8().constData());
#else
    ::setenv("PYTHONHOME", joinPath(Utopia::private_library_path(), "python").toUtf8().constData(), 1);
    ::setenv("PYTHONPATH", joinPath(Utopia::private_library_path(), "python", "lib", PYTHON_STEP).toUtf8().constData(), 1);
#endif
#else
    //::dlopen("libpython" PYTHON_VERSION ".so.1.0", RTLD_LAZY | RTLD_GLOBAL);
#endif

    Py_Initialize();
    PyEval_InitThreads();

    // Import sys module
    PyObject* sysName = PyString_FromString("sys");
    PyObject* sys = PyImport_Import(sysName);
    Py_DECREF(sysName);
    PyObject* path = PyObject_GetAttrString(sys, "path");

    // Modify search path to point to installed python library
#ifdef REMAP_BUNDLED_PYTHON
#ifdef _WIN32
    PyObject* newSysPath = Py_BuildValue("[sssssss]",
                                         joinPath(Utopia::private_library_path(), "python", "lib", PYTHON_STEP, "site-packages", "coda_network").toUtf8().constData(),
                                         joinPath(Utopia::plugin_path(), "python").toUtf8().constData(),
                                         joinPath(Utopia::private_library_path(), "python", "dlls").toUtf8().constData(),
                                         joinPath(Utopia::private_library_path(), "python", "lib", PYTHON_STEP).toUtf8().constData(),
                                         joinPath(Utopia::private_library_path(), "python", "lib", PYTHON_STEP, "plat-win").toUtf8().constData(),
                                         joinPath(Utopia::private_library_path(), "python", "lib", PYTHON_STEP, "lib-tk").toUtf8().constData(),
                                         joinPath(Utopia::private_library_path(), "python", "lib", PYTHON_STEP, "site-packages").toUtf8().constData());
#else

    PyObject* newSysPath = Py_BuildValue("[ssssssss]",
                                         joinPath(Utopia::private_library_path(), "python", "lib", PYTHON_STEP, "site-packages", "coda_network").toUtf8().constData(),
                                         joinPath(Utopia::plugin_path(), "python").toUtf8().constData(),
                                         joinPath(Utopia::private_library_path(), "python", "lib", PYTHON_STEP).toUtf8().constData(),
                                         joinPath(Utopia::private_library_path(), "python", "lib", PYTHON_STEP, "encodings").toUtf8().constData(),
                                         joinPath(Utopia::private_library_path(), "python", "lib", PYTHON_STEP, "lib-dynload").toUtf8().constData(),
                                         joinPath(Utopia::private_library_path(), "python", "lib", PYTHON_STEP, "plat-darwin").toUtf8().constData(),
                                         joinPath(Utopia::private_library_path(), "python", "lib", PYTHON_STEP, "plat-mac").toUtf8().constData(),
                                         joinPath(Utopia::private_library_path(), "python", "lib", PYTHON_STEP, "site-packages").toUtf8().constData());
#endif
    PySequence_DelSlice(path, 0, PySequence_Size(path));
#else
    PyObject* newSysPath = Py_BuildValue("[sss]",
                                         joinPath(Utopia::private_library_path(), "python", "lib", PYTHON_STEP, "site-packages", "coda_network").toUtf8().constData(),
                                         joinPath(Utopia::plugin_path(), "python").toUtf8().constData(),
                                         joinPath(Utopia::private_library_path(), "python", "lib", PYTHON_STEP, "site-packages").toUtf8().constData());
#endif

    PySequence_SetSlice(path, 0, 0, newSysPath);
    Py_DECREF(newSysPath);

    // Reset search path
    Py_DECREF(path);
    Py_DECREF(sys);

    // Release GIL
    pyThreadState = PyThreadState_Swap(NULL);
    PyEval_ReleaseLock();
}

PythonInterpreter::~PythonInterpreter()
{
    // Synchronise with GIL
    PyEval_AcquireLock();
    PyThreadState_Swap(pyThreadState);
    PyEval_ReleaseLock();


#ifndef _WIN32 // FIXME horrid hack to stop this hanging on Windows
    Py_Finalize();

#endif
}

PyObject * PythonInterpreter::evalUtf8(const std::string & utf8)
{
    PyObject * ret = 0;
    PyObject * unicode = PyUnicode_DecodeUTF8(utf8.c_str(), utf8.size(), 0);
    if (unicode) {

        Py_XDECREF(unicode);
    }
    return ret;
}

void PythonInterpreter::evalUtf8AndDiscard(const std::string & utf8)
{
    PyObject * ret = evalUtf8(utf8);
    Py_XDECREF(ret);
}

PythonInterpreter & PythonInterpreter::instance()
{
    static PythonInterpreter interpreter;
    return interpreter;
}

std::set< std::string > PythonInterpreter::getTypeNames(const std::string & api)
{
    std::set< std::string > extensionClasses;


    if (PyObject * main = PyImport_AddModule("__main__"))
    {
        PyObject * dict = PyModule_GetDict(main);
        std::string cmd(api + ".typeNames()");

        if (PyObject * extensionClassTuple = PyRun_String(cmd.c_str(), Py_eval_input, dict, dict))
        {

            if (PySequence_Check(extensionClassTuple))
            {
                int rows = PySequence_Size(extensionClassTuple);
                for (int j = 0; j < rows; ++j)
                {
                    PyObject * extensionClass = PySequence_GetItem(extensionClassTuple, j);
                    extensionClasses.insert(PyString_AsString(extensionClass));
                }
            }

            Py_DECREF(extensionClassTuple);
        }
        else
        {
            PyErr_Print();
        }
    }

    return extensionClasses;
}
