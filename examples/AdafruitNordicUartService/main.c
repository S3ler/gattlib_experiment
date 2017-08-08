/*
*
*  Pentral Nordic Uart Service - CNUS
*
*  Copyright (C) 2017  S3ler
*
*
+  This program is free software; you can redistribute it and/or modify
*  it under the terms of the GNU General Public License as published by
*  the Free Software Foundation; either version 2 of the License, or
*  (at your option) any later version.
*
*  This program is distributed in the hope that it will be useful,
*  but WITHOUT ANY WARRANTY; without even the implied warranty of
*  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*  GNU General Public License for more details.
*
*  You should have received a copy of the GNU General Public License
*  along with this program; if not, write to the Free Software
*  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*
*/

#include <Python.h>
#include <sys/param.h>
#include <libgen.h>
#include <stdbool.h>
#include <pthread.h>

volatile bool stopped = false;
volatile bool await = false;
volatile bool awaiting = false;

#define Sleep(x) sleep(x/1000)

void *join_pythread(void *arg) {
    PyObject *pValue;
    PyObject *pInstance = (PyObject *) arg;
    while (!stopped) {
        // !!!Important!!! C thread will not release CPU to Python thread without the following call.
        pValue = PyObject_CallMethod(pInstance, "join", "(f)", 0.001);
        if (pValue == NULL) {
            PyErr_Print();
        }
        Py_DECREF(pValue);
        while (await) {
            awaiting = true;
            Sleep(1);
        }
        awaiting = false;
        Sleep(10);
        /**/
        // check if message arrived
        pValue = PyObject_CallMethod(pInstance, "is_new_message_arrived", NULL);
        if (pValue != NULL) {
            // printf("Return of call : %d\n", PyInt_AsLong(pValue));
            if (PyInt_AsLong(pValue) == 1) {
                printf("is_new_message_arrived: %d\n", PyInt_AsLong(pValue));
                Py_DECREF(pValue);
                pValue = PyObject_CallMethod(pInstance, "get_new_message", NULL);
                if (pValue != NULL) {
                    printf("is_new_message_arrived: %s\n", PyByteArray_AsString(pValue));
                } else {
                    PyErr_Print();
                }
            }
            Py_DECREF(pValue);
        } else {
            PyErr_Print();
        }
        /**/
    }
    return NULL;
}


int main(int argc, char *argv[]) {

    const int len = 1024;
    char pBuf[1024];
    char szTmp[32];
    sprintf(szTmp, "/proc/%d/exe", getpid());
    int bytes = MIN(readlink(szTmp, pBuf, len), len - 1);
    if (bytes >= 0) {
        pBuf[bytes] = '\0';
    }

    char syspathappend[1024] = {0};
    sprintf(syspathappend, "sys.path.append(\"%s\")", dirname(pBuf));
    printf("%s\n", syspathappend);
    PyObject *pName, *pModule, *pDict, *pClass, *pInstance, *pValue, *pFunc;

    // Initialize python inerpreter
    Py_Initialize();

    PyRun_SimpleString("import sys");
    //PyRun_SimpleString(syspathappend);
    PyRun_SimpleString(
            "sys.path.append(\"/home/bele/mqttsngit/mqtt-sn-gateway/gattlib_experiments/examples/PeripheralNordicUartService\")");


    pName = PyString_FromString("py_nusperipheral");
    pModule = PyImport_Import(pName);
    if (pModule == NULL) {
        printf("pModule is NULL");
        return 1;
    }
    pDict = PyModule_GetDict(pModule);

    // Auslagern:
    pClass = PyDict_GetItemString(pDict, "MyThread");
    if (pClass == NULL || !PyCallable_Check(pClass)) {
        PyErr_Print();
        fprintf(stderr, "The class \"%s\" is not callable\n", "MyThread");
        return 1;
    }

    // Create instance
    pInstance = PyObject_CallObject(pClass, NULL);
    if (pInstance == NULL) {
        PyErr_Print();
        fprintf(stderr, "Failed to create the class instance \"%s\"\n", "MyThread");
        return 1;
    }

    pValue = PyObject_CallMethod(pInstance, "start", NULL);
    if (pValue == NULL) {
        PyErr_Print();
        return 1;
    }

    pthread_t mythread;
    pthread_create(&mythread, NULL, join_pythread, pInstance);


    while (true) {
        size_t characters = -1;
        size_t buffer_size = 1024;
        char buffer[1024] = {0};
        printf("input what you want to send: ");
        char *b = buffer;
        characters = getline(&b, &buffer_size, stdin);
        if (strcmp(buffer, "exit") == 0) {
            stopped = true;
            break;
        }
        if (strlen(buffer) + 1 > 20) {
            printf("Input too long");
            continue;
        }
        await = true;
        while (!awaiting) {}

        pValue = PyObject_CallMethod(pInstance, "send_user_input",
                                     "cccccccccccccccccccci",
                                     buffer[0], buffer[1], buffer[2], buffer[3], buffer[4],
                                     buffer[5], buffer[6], buffer[7], buffer[8], buffer[9],
                                     buffer[10], buffer[11], buffer[12], buffer[13], buffer[14],
                                     buffer[15], buffer[16], buffer[17], buffer[18], buffer[19],
                                     (strlen(buffer) + 1));

        await = false;
        if (pValue != NULL) {
            printf("Return of call : %d\n", PyInt_AsLong(pValue));
            Py_DECREF(pValue);
        } else {
            PyErr_Print();
            return 1;
        }
    }


    printf("C thread join and wait for Python thread to complete...\n");
    PyObject_CallMethod(pInstance, "join", NULL);

    // Clean up
    Py_DECREF(pModule);
    Py_DECREF(pName);

    // Finish the Python Interpreter
    Py_Finalize();

    return 0;
}