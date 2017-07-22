//
// Created by bele on 22.07.17.
//

#ifndef GATTLIB_EXPERIMENTS_SOCKETINTERFACE_H
#define GATTLIB_EXPERIMENTS_SOCKETINTERFACE_H

#include <stdbool.h>
#include <stdint.h>
#include "../Scanner/DeviceAddress.h"

class GattlibExperimentsSocketInterface {

public:
    virtual ~GattlibExperimentsSocketInterface() {}

    virtual bool begin() =0;

    virtual device_address*  getBroadcastAddress() = 0;

    virtual device_address*  getAddress() = 0;

    virtual uint8_t getMaximumMessageLength() = 0;

    virtual bool send(device_address* destination, uint8_t* bytes, uint16_t bytes_len) = 0;

    virtual bool send(device_address* destination, uint8_t* bytes, uint16_t bytes_len, uint8_t signal_strength) = 0;

    virtual bool loop() = 0;

};

#endif //GATTLIB_EXPERIMENTS_SOCKETINTERFACE_H
