//
// Created by bele on 09.07.17.
//

#include "../Scanner/Scanner.h"
#include "Connection.h"
#include "Connector.h"

const char *scanner_mac = "00:1A:7D:DA:71:20";
const char *connector_mac = "00:1A:7D:DA:71:21";

int main(int argc, char *argv[]) {
    Scanner scanner(scanner_mac);
    Connector connector(connector_mac, &scanner);

    connector.start();
    scanner.scan(&connector);

    while (true) {
        std::string input;
        getline(std::cin, input);
        bdaddr_t bdaddr;
        if (input.compare("exit") == 0) {
            goto done;
        }
        if (str2ba(input.c_str(), &bdaddr) == 0) {
            device_address addr;
            memset(addr.bytes, 0x0, sizeof(device_address));
            memcpy(addr.bytes, bdaddr.b, sizeof(bdaddr_t));
            if (connector.isConnected(&addr)) {
                std::cout << "Connection found - input message now" << std::endl;
                std::string message;
                getline(std::cin, message);
                if (message.compare("exit") == 0) {
                    goto done;
                }
                if (message.compare("close") == 0) {
                    connector.close(&addr);
                }
                if (connector.send(&addr, (uint8_t *) message.c_str(), (uint16_t) message.length())) {
                    std::cout << "Send - success" << std::endl;
                } else {
                    std::cout << "Send - failure" << std::endl;
                    connector.close(&addr);
                }
            } else {
                std::cout << "No Connection found" << std::endl;
            }
        } else {
            std::cout << "invalid MAC - usage: BLE MAC Adresse in Form 00:1A:7D:DA:71:11" << std::endl;
            continue;
        }
    }

    done:
    connector.stop();
    connector.free_connections();

    scanner.stop();
    scanner.free_scanResults();

    return 0;
}






