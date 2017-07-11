//
// Created by bele on 09.07.17.
//

#include <iostream>
#include "Scanner.h"
#include "ScanResult.h"
#include "../FakeBluetoothLeSocket/DeviceAddress.h"
#include <list>

int main(int argc, char *argv[]) {
    Scanner scanner;
    // scan 2 seconds
    scanner.scan(2);
    while (true) {
        std::string input;
        getline(std::cin, input);
        if (input.compare("exit") == 0) {
            break;
        }
        std::list<ScanResult *> scanResults = scanner.getScanResults();
        for (auto &&scanResult : scanResults) {
            device_address *a = scanResult->getDeviceAddress();
            for (int i = 0; i < sizeof(device_address); i++) {
                std::cout << std::hex << std::uppercase << (int) a->bytes[i]
                          << std::nouppercase << std::dec
                          << std::flush;
                if (i != sizeof(device_address) - 1) {
                    std::cout << ":";
                }
            }
            std::cout << std::endl;
        }
    }
    scanner.stop();
}