//
// Created by bele on 12.07.17.
//

#include <cstdio>
#include <mutex>
#include <iostream>
#include "Connector.h"

bool Connector::onScanReceive(ScanResult *scanResult) {
    printf("onScanReceive: ");
    printDeviceAddress(scanResult->getDeviceAddress());

    std::lock_guard<std::mutex> scanResult_lock_guard(scanResult_mutex);
    scanResults.push_back(scanResult);
    return true;
}

void Connector::printDeviceAddress(device_address *address) {
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

void Connector::start() {
    this->connector_thread = std::thread(&Connector::connect_loop, this);
}

/*
 * 0. get Scanner's status
 * 1. stop Scanner
 * 2. try to connect
 * 2.1 ok => remove scanResult from Scanner's blacklist
 * 2.2 failure: reason == error => remove from blacklist
 * 2.3 failure: reason == missing service => do NOT remove from blacklist
 * 3. remove scanResult from local scanResults list
 * 4. Scanner's status == running => restart
 */
void Connector::connect_loop() {
    while (!stopped) {
        if (scanResults.size() > 0) {
            // 0. get Scanner's status
            bool scanner_status = scanner->isRunning();
            // 1. stop Scanner
            scanner->stop();
            // if you do not stop the scann first, we provoke a  deadlock
            std::lock_guard<std::mutex> scanResult_lock(scanResult_mutex);
            for (auto &&scanResult : scanResults) {
                Connection *connection = new Connection(scanResult);
                // 2. try to connect
                if (!connection->connect()) {
                    if (connection->getErrorStatus() == InternalBluetoothError) {
                        // something went wrong, but reconnecting helps in most cases
                        // failue == error => remove from blacklist (so it will be tried again)
                        scanResults.remove(scanResult);
                        scanner->removeScanResult(scanResult);
                        connection->close();
                        delete (connection);
                        break;
                    } else if (connection->getErrorStatus() == MissingService || connection->getErrorStatus() == ConnectionRefused) {
                        // if failure == no service => do not remove from blacklist
                        scanResults.remove(scanResult);
                        connection->close();
                        delete (connection);
                        break;
                    }
                }
                // 2.1 ok => remove scanResult from Scanner's blacklist
                scanResults.remove(scanResult);
                scanner->removeScanResult(scanResult);
                active_connections.push_front(connection);
                break;
            }

            // 4. Scanner's status == running => restart
            if(scanner_status){
                scanner->scan(this);
            }
        }
    }
}

const std::list<Connection *> &Connector::getActive_connections() const {
    return active_connections;
}

void Connector::stop() {
    this->stopped = true;
    if (connector_thread.joinable()) {
        this->connector_thread.join();
    }
}

bool deleteAll(Connection *connection) {
    delete connection;
    return true;
}

void Connector::free_connections() {
    std::lock_guard<std::mutex> scanResult_lock(scanResult_mutex);
    // disconnect from all
    for (auto &&connection : active_connections) {
        connection->close();
    }
    // free all
    active_connections.remove_if(deleteAll);
}

void Connector::setScanner(Scanner *scanner) {
    Connector::scanner = scanner;
}


