

#include <Python.h>
#include <sys/param.h>
#include <libgen.h>
#include <stdbool.h>
#include <pthread.h>

const int len = 1024;
char pBuf[1024];
volatile bool stopped = false;

void loop(void *arg) {
    PyObject *pInstance = (PyObject *) arg;
    while (!stopped) {
        // !!!Important!!! C thread will not release CPU to Python thread without the following call.
        PyObject_CallMethod(pInstance, "join", "(f)", 0.001);
    }
}

void ble_init(void* arg){

}

int main(int argc, char *argv[]) {
    char szTmp[32];
    sprintf(szTmp, "/proc/%d/exe", getpid());
    int bytes = MIN(readlink(szTmp, pBuf, len), len - 1);
    if (bytes >= 0) {
        pBuf[bytes] = '\0';
    }

    char syspathappend[1024] = {0};
    sprintf(syspathappend, "sys.path.append(\"%s\")", dirname(pBuf));
    printf("%s\n", syspathappend);
    PyObject *pName, *pModule, *pDict, *pFunc, *pInstance, *pValue;

    // Initialize python inerpreter
    Py_Initialize();

    PyRun_SimpleString("import sys");
    PyRun_SimpleString(syspathappend);
    //PyRun_SimpleString("sys.path.append(\"/home/bele/mqttsngit/mqtt-sn-gateway/gattlib_experiments/examples/PeripheralNordicUartService\")");

    /*
    // Initialize thread support
    PyEval_InitThreads();

    // Save a pointer to the main PyThreadState object
    PyThreadState *mainThreadState = PyThreadState_Get();

    // Get a reference to the PyInterpreterState
    PyInterpreterState *mainInterpreterState = mainThreadState->interp;

    // Create a thread state object for this thread
    PyThreadState* myThreadState = PyThreadState_New(mainInterpreterState);

    // Release global lock
    PyEval_ReleaseLock();*/

    pName = PyString_FromString("py_nusperipheral");
    pModule = PyImport_Import(pName);
    if (pModule == NULL) {
        printf("pModule is NULL");
    }
    pDict = PyModule_GetDict(pModule);

    // Auslagern:
    pFunc = PyDict_GetItemString(pDict, "init2");

    if (!PyCallable_Check(pFunc)) {
        printf("PyCallable_Check for pFunc failed");
        exit(0);
    }

    pValue = PyObject_CallObject(pFunc, NULL);

    if (pValue != NULL) {
        printf("Return of call : %d\n", PyInt_AsLong(pValue));
        Py_DECREF(pValue);
    } else {
        PyErr_Print();
    }

    // here start thread with:
    /*
    while(!stopped)
    {
        // !!!Important!!! C thread will not release CPU to Python thread without the following call.
        PyObject_CallMethod(pInstance, "join", "(f)", 0.001);
    }
    */
    pFunc = PyDict_GetItemString(pDict, "send_user_input");

    if (!PyCallable_Check(pFunc)) {
        printf("PyCallable_Check for pFunc failed");
        exit(0);
    }

    pthread_t mythread;
    pthread_create(&mythread, NULL,
                   loop, NULL);

    while (true) {
        char b[1024] = {0};
        printf("input what you wand to send: ");
        scanf("%d", b);
        if (strcmp(b, "exit") == 0) {
            stopped = true;
            break;
        }
        if (strlen(b) + 1 > 20) {
            printf("Input too long");
            continue;
        }
        pValue = PyObject_CallFunction(pFunc, "cccccccccccccccccccci",
                                       b[0], b[1], b[2], b[3], b[4],
                                       b[5], b[6], b[7], b[8], b[9],
                                       b[10], b[11], b[12], b[13], b[14],
                                       b[15], b[16], b[17], b[18], b[19],
                                       strlen(b) + 1);
        if (pValue != NULL) {
            printf("Return of call : %d\n", PyInt_AsLong(pValue));
            Py_DECREF(pValue);
        } else {
            PyErr_Print();
        }
    }

    // Clean up
    Py_DECREF(pModule);
    Py_DECREF(pName);

    // Finish the Python Interpreter
    Py_Finalize();

    return 0;
}