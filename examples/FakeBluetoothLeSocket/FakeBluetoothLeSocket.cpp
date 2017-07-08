//
// Created by bele on 03.07.17.
//

#include <stdint.h>
#include <clocale>
#include "FakeBluetoothLeSocket.h"


bool FakeBluetoothLeSocket::isDisconnected() {
    return false;
}

ssize_t FakeBluetoothLeSocket::send(const uint8_t *buf, uint8_t len) {
    return NULL;
}

void FakeBluetoothLeSocket::connect(device_address *address) {
    //TODO init
}

void FakeBluetoothLeSocket::disconnect() {

}

void FakeBluetoothLeSocket::loop() {

}
