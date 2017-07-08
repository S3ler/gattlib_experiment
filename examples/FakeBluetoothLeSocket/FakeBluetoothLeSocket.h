//
// Created by bele on 03.07.17.
//

#ifndef TEST_MQTT_SN_GATEWAY_FAKEBLUETOOTHLESOCKET_H
#define TEST_MQTT_SN_GATEWAY_FAKEBLUETOOTHLESOCKET_H

#include <Python.h>
#include <stdint.h>
#include <thread>
#include <mutex>
#include "FakeSocketInterface.h"

#define PYTHON_MODULE_NAME "py_nusperipheral"
#define PYTHON_NUSPHERIPHERAL_CLASSNAME "MyThread"
#define PYTHON_PERIPHERAL_START_METHODNAME "start"
#define PYTHON_PERIPHERAL_SEND_METHODNAME "send_user_input"
#define PYTHON_PERIPHERAL_CHECKFORMESSAGEARRIVED_METHODNAME "is_new_message_arrived"
#define PYTHON_PERIPHERAL_GETMESSAGE_METHODNAME "get_new_message"

class FakeBluetoothLeSocket : public FakeSocketInterface {
private:
    LinuxUdpClientFake *fakeClient;

    // class state:
    bool initialized = false;

    PyObject *pName, *pModule, *pDict;
    PyObject *pNUSPeripheralClass, *pNUSPeripheral;

#define MAX_EXECUTION_PATH_LENGTH 1024
    const int execution_path_max_len = MAX_EXECUTION_PATH_LENGTH;
    char execution_path[MAX_EXECUTION_PATH_LENGTH] = {0};

    // poll thread state
    // std::thread cpu_poll_msg_thread;
    std::mutex pMutex;

    device_address address;
public:

    virtual void setFakeClient(LinuxUdpClientFake *fakeClient);

    virtual bool isDisconnected();

    virtual size_t send(const uint8_t *buffer, uint8_t len);

    virtual void connect(device_address *address);

    virtual void disconnect();

    virtual void loop();



private:
    bool begin();

    char *getExecutionPath();

    void join_pythread();

};


#endif //TEST_MQTT_SN_GATEWAY_FAKEBLUETOOTHLESOCKET_H
