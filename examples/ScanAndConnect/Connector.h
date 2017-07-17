//
// Created by bele on 12.07.17.
//

#ifndef GATTLIB_EXPERIMENTS_SCANNERCALLBACKPRINT_H
#define GATTLIB_EXPERIMENTS_SCANNERCALLBACKPRINT_H


#include "../Scanner/ScannerCallbackInterface.h"
#include "Connection.h"
#include "../Scanner/Scanner.h"
#include <list>
#include <thread>
#include <atomic>


class Connector : public ScannerCallbackInterface {
private:
    std::mutex scanResult_mutex;
    std::list<ScanResult *> scanResults;
    std::list<Connection *> active_connections;
    volatile std::atomic<bool> stopped;
    std::thread connector_thread;

    void printDeviceAddress(device_address *address);

    void connect_loop();

    Scanner *scanner = nullptr;
public:

    void setScanner(Scanner *scanner);

    void start();

    void stop();

    void free_connections();

    const std::list<Connection *> &getActive_connections() const;

    bool onScanReceive(ScanResult *scanResult) override;

};


#endif //GATTLIB_EXPERIMENTS_SCANNERCALLBACKPRINT_H
