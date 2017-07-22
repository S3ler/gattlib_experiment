//
// Created by bele on 22.07.17.
//


#include "BluetoothLowEnergySocket.h"

const char *scanner_mac = "00:1A:7D:DA:71:20";
const char *connector_mac = "00:1A:7D:DA:71:21";

int main(int argc, char *argv[]) {
    BluetoothLowEnergySocket bluetoothLowEnergySocket(scanner_mac, connector_mac);
    bluetoothLowEnergySocket.begin();

    while (true) {
        std::string input;
        getline(std::cin, input);
        bdaddr_t bdaddr;
        if (input.compare("exit") == 0) {
            goto done;
        }
        if (str2ba(input.c_str(), &bdaddr) == 0) {
            device_address destinaction;
            memset(destinaction.bytes, 0x0, sizeof(device_address));
            memcpy(destinaction.bytes, bdaddr.b, sizeof(bdaddr_t));
            std::cout << "input message now" << std::endl;
            std::string message;
            getline(std::cin, message);
            if (message.compare("exit") == 0) {
                goto done;
            }
            if (bluetoothLowEnergySocket.send(&destinaction, (uint8_t *) message.c_str(), (uint16_t) message.length())) {
                std::cout << "done" << std::endl;
            }
        } else {
            std::cout << "invalid MAC - usage: BLE MAC Adresse in Form 00:1A:7D:DA:71:11" << std::endl;
            bluetoothLowEnergySocket.loop();
            continue;
        }
    }

    done:

    return 0;
}