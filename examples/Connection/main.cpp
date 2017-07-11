//
// Created by bele on 09.07.17.
//

#include "../Scanner/Scanner.h"
#include "Connection.h"

void printDeviceAddress(device_address *pAddress);

static bool deleteAll(Connection *c);

int main(int argc, char *argv[]) {
    Scanner scanner;
    std::list<Connection *> active_connections;

    // scan for 10 seconds
    scanner.scan(1);
    /*
    while (true) {

        std::list<ScanResult *> scanResults = scanner.getScanResults();
        for (auto &&scanResult : scanResults) {
            printDeviceAddress(scanResult->getDeviceAddress());
        }

        std::string input;
        getline(std::cin, input);
        if (input.compare("again") == 0) {
            // scan for 10 seconds
            scanner.scan(5);
        }
        if (input.compare("ok") == 0) {
            break;
        }
        if (input.compare("exit") == 0) {
            goto done;
        }
    }
    */
    for (auto &&scanResult : scanner.getScanResults()) {
        Connection *connection = new Connection(scanResult);
        if (!connection->connect()) {
            if (connection->getErrorStatus() == InternalBluetoothError) {
                scanner.removeScanResult(scanResult);
                // something went wrong, but reconnecting helps in most cases
                delete (connection);
                continue;
            } else if (connection->getErrorStatus() == MissingService) {
                // do not remove it and try not to connect again
                delete (connection);
                continue;
            }
        }
        active_connections.push_front(connection);
    }

    while (true) {
        std::string input;
        getline(std::cin, input);
        bdaddr_t bdaddr;
        if (input.compare("exit") == 0) {
            goto done;
        }
        if (str2ba(input.c_str(), &bdaddr) == -1) {
            std::cout << "invalid MAC - usage: BLE MAC Adresse in Form FF:AA:BB:CC:99:88" << std::endl;
            continue;
        }
        device_address addr;
        memset(addr.bytes, 0x0, sizeof(device_address));
        memcpy(addr.bytes, bdaddr.b, sizeof(bdaddr_t));
        bool connection_found = false;
        if(active_connections.size() == 0){
            std::cout << "No Active Connections - exiting" << std::endl;
            goto done;
        }
        for (std::list<Connection *>::const_iterator iterator = active_connections.begin(),
                     end = active_connections.end();
             iterator != end; ++iterator) {
            Connection* connection = *iterator;
            if (connection->isDeviceAddress(&addr)) {
                connection_found = true;
                std::cout << "Connection found - input message now" << std::endl;
                std::string message;
                getline(std::cin, message);
                if (input.compare("exit") == 0) {
                    goto done;
                }
                if (connection->send((uint8_t *) message.c_str(), (uint16_t) message.length())) {
                    std::cout << "Send - success" << std::endl;
                } else {
                    std::cout << "Send - failure" << std::endl;
                    connection->close();
                    active_connections.erase(iterator);
                    delete (connection);
                }
                break;
            }
        }
        if (!connection_found) {
            std::cout << "No Connection found" << std::endl;
        }
    }


    done:
    scanner.stop();
    // disconnect from all
    for (auto &&connection : active_connections) {
        connection->close();
    }
    // free all
    active_connections.remove_if(deleteAll);

    return 0;
}


void printDeviceAddress(device_address *address) {
    for (int i = 0; i < sizeof(device_address); i++) {
        std::cout << std::hex << std::uppercase << (int) address->bytes[i]
                  << std::nouppercase << std::dec
                  << std::flush;
        if (i != sizeof(device_address) - 1) {
            std::cout << ":";
        }
    }
    std::cout << std::endl;
}

bool deleteAll(Connection *connection) {
    delete connection;
    return true;
}
