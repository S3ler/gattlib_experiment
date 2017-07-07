//
// Created by bele on 03.07.17.
//

#ifndef TEST_MQTT_SN_GATEWAY_FAKEBLUETOOTHLESOCKET_H
#define TEST_MQTT_SN_GATEWAY_FAKEBLUETOOTHLESOCKET_H


#include <monetary.h>
#include <stdint.h>
#include "FakeSocketInterface.h"

class FakeBluetoothLeSocket : public FakeSocketInterface {
public:
    //virtual void setFakeClient(LinuxUdpClientFake *fakeClient);

    virtual bool isDisconnected();

    virtual ssize_t send(const uint8_t *buf, uint8_t len);

    virtual void connect(device_address *address);

    virtual void disconnect();

    virtual void loop();
};


#endif //TEST_MQTT_SN_GATEWAY_FAKEBLUETOOTHLESOCKET_H
