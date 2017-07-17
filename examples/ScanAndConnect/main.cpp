//
// Created by bele on 09.07.17.
//

#include "../Scanner/Scanner.h"
#include "Connection.h"
#include "Connector.h"

int main(int argc, char *argv[]) {
    Scanner scanner;
    Connector connector;

    connector.setScanner(&scanner);
    connector.start();
    scanner.scan(&connector);

    // while(scanner.isRunning()){
        //connectToAllScanResults();
    // }
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


    while (true) {
        std::string input;
        getline(std::cin, input);
        bdaddr_t bdaddr;
        if (input.compare("exit") == 0) {
            goto done;
        }
        if (str2ba(input.c_str(), &bdaddr) == -1) {
            std::cout << "invalid MAC - usage: BLE MAC Adresse in Form 00:1A:7D:DA:71:11" << std::endl;
            continue;
        }
        device_address addr;
        memset(addr.bytes, 0x0, sizeof(device_address));
        memcpy(addr.bytes, bdaddr.b, sizeof(bdaddr_t));
        bool connection_found = false;
        std::list<Connection*> active_connections = connector.getActive_connections();
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
                if (message.compare("exit") == 0) {
                    goto done;
                }
                if(message.compare("close")==0){
                    std::cout << "closing connection" << std::endl;
                    active_connections.erase(iterator);
                    connection->close();
                    delete (connection);
                    break;
                }
                if (connection->send((uint8_t *) message.c_str(), (uint16_t) message.length())) {
                    std::cout << "Send - success" << std::endl;
                } else {
                    std::cout << "Send - failure" << std::endl;
                    active_connections.erase(iterator);
                    connection->close();
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
    connector.stop();
    connector.free_connections();

    scanner.stop();
    scanner.free_scanResults();

    return 0;
}






