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

#include "LinuxUdpClientFake.h"
#include "FakeBluetoothLeSocket.h"
#include <iostream>

void LinuxUdpClientFake::loop() {
    while (!stopped) {
        fakeSocket->loop();
    }
    fakeSocket->disconnect();
    free(fakeSocket);
}

void LinuxUdpClientFake::start_loop() {
    this->fakeSocket = new FakeBluetoothLeSocket();
    if(this->fakeSocket->isDisconnected()){
        this->fakeSocket->connect(this->gw_address);
    }
    fakeSocket->setFakeClient(this);
    this->thread = std::thread(&LinuxUdpClientFake::loop, this);
    this->thread.detach();
}

void LinuxUdpClientFake::stop_loop() {
    this->stopped = true;
}

void LinuxUdpClientFake::send(uint8_t *data, uint16_t length) {
    if(length>20){
        return;
    }
    fakeSocket->send(data, (uint8_t) length);
}

void LinuxUdpClientFake::receive(uint8_t *data, uint16_t length) {
    for(int i = 0; i<length;i++){
        std::cout << data[i] << std::flush;
    }
    std::cout << std::endl;
}

void LinuxUdpClientFake::set_gw_address(device_address *address) {
    this->gw_address = address;
}


