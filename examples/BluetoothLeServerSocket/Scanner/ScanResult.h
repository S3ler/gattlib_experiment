//
// Created by bele on 09.07.17.
//

#ifndef GATTLIB_EXPERIMENTS_SCANRESULT_H
#define GATTLIB_EXPERIMENTS_SCANRESULT_H


#include "DeviceAddress.h"
#include <stdint.h>

class ScanResult {
private:
    device_address address;
public:
    device_address* getDeviceAddress();

    ScanResult(const device_address *address);
};


#endif //GATTLIB_EXPERIMENTS_SCANRESULT_H
