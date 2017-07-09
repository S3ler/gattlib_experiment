/*
*
*  Fake Bluetooth LE Socket
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

#ifndef TEST_MQTT_SN_GATEWAY_FAKESOCKETINTERFACE_H
#define TEST_MQTT_SN_GATEWAY_FAKESOCKETINTERFACE_H


#include <stdint.h>
#include <cstdio>
#include "LinuxUdpClientFake.h"
#include "DeviceAddress.h"

class LinuxUdpClientFake;



class FakeSocketInterface {
public:
    virtual void setFakeClient(LinuxUdpClientFake *fakeClient)=0;

    virtual bool isDisconnected() =0;

    virtual size_t send(const uint8_t *buf, uint8_t len)=0;

    virtual void connect(device_address *address)=0;

    virtual void disconnect()=0;

    virtual void loop()=0;
};

#endif //TEST_MQTT_SN_GATEWAY_FAKESOCKETINTERFACE_H
