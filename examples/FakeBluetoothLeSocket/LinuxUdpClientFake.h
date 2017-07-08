//
// Created by bele on 08.07.17.
//

#ifndef GATTLIB_EXPERIMENTS_LINUXUDPCLIENTFAKE_H
#define GATTLIB_EXPERIMENTS_LINUXUDPCLIENTFAKE_H


#include <cstdint>
#include <atomic>
#include <thread>
#include "FakeSocketInterface.h"
#include "DeviceAddress.h"

class FakeSocketInterface;

class LinuxUdpClientFake {

public:
    void loop();

    void start_loop();

    void stop_loop();

    void send(uint8_t *data, uint16_t length);

    void receive(uint8_t *data, uint16_t length);

    void set_gw_address(device_address *address);


private:
    FakeSocketInterface *fakeSocket;
    std::atomic<bool> stopped{false};
    std::thread thread;
    device_address *gw_address;
};


#endif //GATTLIB_EXPERIMENTS_LINUXUDPCLIENTFAKE_H
