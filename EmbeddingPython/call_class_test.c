#include<Python.h>
#include<stdio.h>


int main(int argc, char *argv[])
{
    PyObject *pName, *pModule, *pDict, *pFunc, *pValue;

    if (argc < 3)
    {
        printf("Usage: exe_name python_source function_name\n");
        return 1;
    }

    // Initialize the Python Interpreter
    printf("Initialize filename=%s, functname=%s\n", argv[1], argv[2]);
    Py_Initialize();
    PyRun_SimpleString("import sys");
    PyRun_SimpleString("sys.path.append(\"/home/bele/mqttsngit/mqtt-sn-gateway/gattlib_experiments/EmbeddingPython\")");
    pName = PyString_FromString(argv[1]);
    printf("PyImport_Import\n");
    pModule = PyImport_Import(pName);
    if(pModule==NULL)
    {
        printf("pModule is NULL\n");
    }
    pDict = PyModule_GetDict(pModule);
    pFunc = PyDict_GetItemString(pDict, argv[2]);
    if (PyCallable_Check(pFunc))
    {
        PyObject_CallObject(pFunc, NULL);
        printf("PyObject_CallObject\n");
    }
    else
    {
        PyErr_Print();
    }
    Py_DECREF(pModule);
    Py_DECREF(pName);
    Py_Finalize();
    return 0;
}