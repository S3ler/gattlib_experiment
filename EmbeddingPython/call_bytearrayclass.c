// A sample of python embedding (calling python classes from within C++ code)
//
// To run:
// 1) setenv PYTHONPATH ${PYTHONPATH}:./
// 2) call_class py_source Multiply multiply 
// 3) call_class py_source Multiply multiply 9 8 
//

#include <Python.h>
#include <bytearrayobject.h>

// arguments: py_bytearrayclass ByteArrayClass setByteArray

int main(int argc, char *argv[]) {
    PyObject *pName, *pModule, *pDict, *pClass, *pInstance, *pValue;

    if (argc < 4) {
        fprintf(stderr, "Usage: call python_filename class_name function_name\n");
        fprintf(stderr, "Example: py_bytearrayclass ByteArrayClass setByteArray\n");
        return 1;
    }

    printf("Initialize python_filename=%s, class_name=%s, function_name=%s\n", argv[1], argv[2], argv[3]);


    Py_Initialize();

    PyRun_SimpleString("import sys");
    PyRun_SimpleString("sys.path.append(\"/home/bele/mqttsngit/mqtt-sn-gateway/gattlib_experiments/EmbeddingPython\")");

    pName = PyString_FromString(argv[1]);
    pModule = PyImport_Import(pName);
    if (pModule == NULL) {
        printf("Error: Check your python_filename");
    }
    pDict = PyModule_GetDict(pModule);

    // Build the name of a callable class 
    pClass = PyDict_GetItemString(pDict, argv[2]);

    // Create an instance of the class
    if (PyCallable_Check(pClass)) {
        pInstance = PyObject_CallObject(pClass, NULL);
    }

    // Build parameter list
    // Call a method of the class with two parameters
    // https://www2.informatik.hu-berlin.de/Themen/manuals/python/python-ext/section2_2_8.html
    uint8_t to_write[6] = {0x00, 0x01, 0x02, 0x03, 0x04, 0x05};
    uint8_t to_write_length = 6;
    /* was a try -> Segmentation fault
    PyByteArrayObject to_write_pArray;
    to_write_pArray.ob_bytes = (char *) to_write;
    to_write_pArray.ob_size = 6;
    pValue = PyObject_CallMethodObjArgs(pInstance, argv[3], (PyObject*) &to_write_pArray);
    */
    pValue = PyObject_CallMethod(pInstance, argv[3], "(cccccci)",
                                 to_write[0], to_write[1], to_write[2],
                                 to_write[3], to_write[4], to_write[5],
                                 to_write_length);

    if (pValue != NULL) {
        if(PyByteArray_Check(pValue)){
#define TO_RECEIVE_SIZE 6
            uint8_t to_receive[TO_RECEIVE_SIZE]={0};

            PyByteArrayObject* pArray = (PyByteArrayObject *) pValue;
            Py_ssize_t pArraySize = pArray->ob_size;

            if (pArraySize <= TO_RECEIVE_SIZE) {
                memcpy(to_receive, pArray->ob_bytes, (size_t) pArraySize);
            }

            printf("Return of call type: %s\n", pValue->ob_type->tp_name);
            printf("Return of call size: %zd\n", pArraySize);
            printf("Return of call bytes: ");
            printf("%02X:%02X:%02X:%02X:%02X:%02X\n",
                   pArray->ob_bytes[0], pArray->ob_bytes[1],
                   pArray->ob_bytes[2], pArray->ob_bytes[3],
                   pArray->ob_bytes[4], pArray->ob_bytes[5]);


        } else{
            printf("Return of call was no bytearray");
        }
        Py_DECREF(pValue);
    } else {
        PyErr_Print();
    }

    // Clean up
    Py_DECREF(pModule);
    Py_DECREF(pName);
    Py_Finalize();

    return 0;
}
