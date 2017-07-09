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
