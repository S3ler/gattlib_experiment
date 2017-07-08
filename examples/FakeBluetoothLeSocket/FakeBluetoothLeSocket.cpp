//
// Created by bele on 03.07.17.
//

#include <Python.h>
#include <sys/param.h>
#include <libgen.h>
#include "FakeBluetoothLeSocket.h"


void FakeBluetoothLeSocket::setFakeClient(LinuxUdpClientFake *fakeClient) {
    this->fakeClient = fakeClient;
}

bool FakeBluetoothLeSocket::isDisconnected() {
    return !initialized;
}

size_t FakeBluetoothLeSocket::send(const uint8_t *buf, uint8_t len) {
    /*if (isDisconnected()) {
        connect(&address);
    }*/
    if (len > 20) {
        return 0;
    }
    PyObject *pSendReturnValue = NULL;
    uint8_t buffer[20] = {0};
    memcpy(buffer, buf, len);
    {
        std::lock_guard<std::mutex> guard(pMutex);
        pSendReturnValue = PyObject_CallMethod(pNUSPeripheral, PYTHON_PERIPHERAL_SEND_METHODNAME,
                                               "cccccccccccccccccccci",
                                               buffer[0], buffer[1], buffer[2], buffer[3], buffer[4],
                                               buffer[5], buffer[6], buffer[7], buffer[8], buffer[9],
                                               buffer[10], buffer[11], buffer[12], buffer[13], buffer[14],
                                               buffer[15], buffer[16], buffer[17], buffer[18], buffer[19],
                                               len);
    }
    if (pSendReturnValue != NULL && PyInt_AsLong(pSendReturnValue) == 1) {
        Py_DECREF(pSendReturnValue);
        return len;
    }
    Py_DECREF(pSendReturnValue);

    PyErr_Print();
    // error in python or somethin else
    exit(0);
}

void FakeBluetoothLeSocket::connect(device_address *address) {
    memcpy(&this->address, &address, sizeof(device_address));
    if (isDisconnected()) {
        // init
        if (!begin()) {
            // error
            exit(0);
        }
    }
}

void FakeBluetoothLeSocket::disconnect() {

    PyObject_CallMethod(pNUSPeripheral, "join", NULL);

    // s a borrowed reference - python interpreter frees it
    // pDict

    // Clean up python
    Py_DECREF(pNUSPeripheralClass);
    Py_DECREF(pModule);
    Py_DECREF(pName);

    Py_Finalize();
}

void FakeBluetoothLeSocket::loop() {
    join_pythread();
}

bool FakeBluetoothLeSocket::begin() {
    if (initialized) {
        return initialized;
    }

    char *execution_path = getExecutionPath();
    if (execution_path == nullptr) {
        // error could not determine execution path
        return false;
    }

    char sys_path_append_string[execution_path_max_len + 25] = {0};
    sprintf(sys_path_append_string, "sys.path.append(\"%s\")", dirname(execution_path));

    // Initialize python interpreter
    Py_Initialize();

    // Add sys.path.append("execution_directory")
    // Python interpreter does not find *.py file in this directory without it
    PyRun_SimpleString("import sys");
    PyRun_SimpleString(sys_path_append_string);

    // Load Module
    pName = PyString_FromString(PYTHON_MODULE_NAME);
    pModule = PyImport_Import(pName);
    if (pModule == NULL) {
        // error python module not found - most likely the python file does not exist or is named incorrec.
        return false;
    }
    pDict = PyModule_GetDict(pModule);

    pNUSPeripheralClass = PyDict_GetItemString(pDict, PYTHON_NUSPHERIPHERAL_CLASSNAME);
    if (pNUSPeripheralClass == NULL || !PyCallable_Check(pNUSPeripheralClass)) {
        // PyErr_Print();
        // error python class not found or not callable - most like the python class does not exist or is named incorrect
        return false;
    }

    pNUSPeripheral = PyObject_CallObject(pNUSPeripheralClass, NULL);
    if (pNUSPeripheral == NULL) {
        // PyErr_Print();
        // error wrong constructor call
        return false;
    }

    PyObject *pStartReturnValue = PyObject_CallMethod(pNUSPeripheral, PYTHON_PERIPHERAL_START_METHODNAME, NULL);
    if (pStartReturnValue == NULL) {
        // PyErr_Print();
        // error wrong method call
        Py_DECREF(pStartReturnValue);
        return false;
    }
    Py_DECREF(pStartReturnValue);

    // Uncommend line when loop() is not called in a own thread
    // cpu_poll_msg_thread = std::thread(&FakeBluetoothLeSocket::join_pythread, this);

    initialized = true;
    return initialized;
}

char *FakeBluetoothLeSocket::getExecutionPath() {
    char szTmp[32];
    sprintf(szTmp, "/proc/%d/exe", getpid());
    ssize_t bytes = MIN(readlink(szTmp, execution_path, execution_path_max_len), execution_path_max_len - 1);
    if (bytes == -1 || bytes == execution_path_max_len) {
        return nullptr;
    }
    if (bytes >= 0) {
        execution_path[bytes] = '\0';
    }
    return execution_path;
}


void FakeBluetoothLeSocket::join_pythread() {
    if (!initialized) {
        return;
    }
    // !!!Iportant!!! this method must be called from time to time or else no CPU is release to Python

    std::lock_guard<std::mutex> guard(pMutex);
    PyObject *pReceiveReturnValue = NULL;

    // !!!Important!!! C thread will not release CPU to Python thread without the following call.
    pReceiveReturnValue = PyObject_CallMethod(pNUSPeripheral, "join", "(f)", 0.001);

    if (pReceiveReturnValue == NULL) {
        PyErr_Print();
        exit(0);
    }
    Py_DECREF(pReceiveReturnValue);

    // check if message arrived
    pReceiveReturnValue = PyObject_CallMethod(
            pNUSPeripheral, PYTHON_PERIPHERAL_CHECKFORMESSAGEARRIVED_METHODNAME, NULL);
    if (pReceiveReturnValue != NULL) {
        if (PyInt_AsLong(pReceiveReturnValue) == 1) {
            // new message arrived, now get content
            Py_DECREF(pReceiveReturnValue);
            pReceiveReturnValue = PyObject_CallMethod(
                    pNUSPeripheral, PYTHON_PERIPHERAL_GETMESSAGE_METHODNAME, NULL);
            if (pReceiveReturnValue != NULL) {
                uint8_t tmp_msg[21] = {0};
                //PyByteArrayObject* a = (PyByteArrayObject *) pReceiveReturnValue;
                uint16_t length = (uint16_t) PyByteArray_Size(
                        pReceiveReturnValue); // this is save because Py_ssize_t is typedef ssize_t
                if (length != -1 && length < 20) { // else ignore
                    PyByteArrayObject *pReceivedByteArray = (PyByteArrayObject *) pReceiveReturnValue;
                    char *received_bytes = pReceivedByteArray->ob_bytes;
                    memcpy(tmp_msg, received_bytes, length);
                    fakeClient->receive(tmp_msg, length);
                }
            } else {
                PyErr_Print();
                exit(0);
            }
        }
        Py_DECREF(pReceiveReturnValue);
    } else {
        PyErr_Print();
        exit(0);
    }
}


