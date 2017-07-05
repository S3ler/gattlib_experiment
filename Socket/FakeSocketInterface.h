//
// Created by bele on 03.07.17.
//

#ifndef TEST_MQTT_SN_GATEWAY_FAKESOCKETINTERFACE_H
#define TEST_MQTT_SN_GATEWAY_FAKESOCKETINTERFACE_H

#include <LinuxUdpClientFake.h>

class LinuxUdpClientFake;

class FakeSocketInterface {
public:
    virtual void setFakeClient(LinuxUdpClientFake *fakeClient)=0;

    virtual bool isDisconnected() =0;

    virtual ssize_t send(const uint8_t *buf, uint8_t len)=0;

    virtual void connect(device_address *address)=0;

    virtual void disconnect()=0;

    virtual void loop()=0;
};

#endif //TEST_MQTT_SN_GATEWAY_FAKESOCKETINTERFACE_H
