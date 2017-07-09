//
// Created by bele on 09.07.17.
//

#ifndef GATTLIB_EXPERIMENTS_CONNECTION_H
#define GATTLIB_EXPERIMENTS_CONNECTION_H


#include "../Scanner/ScanResult.h"
#include <cstring>
#include <iostream>

enum ConnectionErrorStatus {
    MissingService,
    InternalBluetoothError,
    ConnectionDisconnected
};


class Connection {
private:
    device_address address;
    ConnectionErrorStatus errorStatus = ConnectionDisconnected;
public:
    Connection(const ScanResult *scanResult);

    ConnectionErrorStatus getErrorStatus();

    bool isDeviceAddress(const device_address *address);

    bool connect();

    void close();

    bool send(uint8_t* data, uint16_t length);

    void onReceive(uint8_t *data, uint16_t length);


};


#endif //GATTLIB_EXPERIMENTS_CONNECTION_H
