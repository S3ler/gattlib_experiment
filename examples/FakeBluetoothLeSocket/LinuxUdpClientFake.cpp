//
// Created by bele on 08.07.17.
//

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


