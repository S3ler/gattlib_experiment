//
// Created by bele on 09.07.17.
//

#include "Connection.h"

Connection::Connection(const ScanResult *scanResult) {
    memcpy(this->address.bytes, scanResult->getDeviceAddress()->bytes, sizeof(device_address));
}


ConnectionErrorStatus Connection::getErrorStatus() {
    return errorStatus;
}

bool Connection::isDeviceAddress(const device_address *address) {
    return memcmp(this->address.bytes, address->bytes, sizeof(device_address)) == 0;
}

void Connection::onReceive(uint8_t *data, uint16_t length) {
    // TODO this is a placeholder function to print out the messages
    // TODO later this will be the function in the LinuxBluetoothLowEnergySocket
    for (int i = 0; i < length; i++) {
        std::cout << std::hex << std::uppercase << (int) data[i]
                  << std::nouppercase << std::dec
                  << std::flush;
        if (i != sizeof(device_address) - 1) {
            std::cout << ":";
        }
    }
}

bool Connection::connect() {
    // TODO implement me
    return true;
}

void Connection::close() {
    // TODO implement me
}

bool Connection::send(uint8_t *data, uint16_t length) {
    // TODO implement me
    return false;
}




