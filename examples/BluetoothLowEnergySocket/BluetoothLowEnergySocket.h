//
// Created by bele on 22.07.17.
//

#ifndef GATTLIB_EXPERIMENTS_BLUETOOTHLOWENERGYSOCKET_H
#define GATTLIB_EXPERIMENTS_BLUETOOTHLOWENERGYSOCKET_H

#include "../ScanAndConnect/Connector.h"
#include "../ScanAndConnect/Connection.h"
#include <list>
#include "GattlibExperimentsSocketInterface.h"
#include "BluetoothLowEnergyMessage.h"
#include "Queue.h"


class BluetoothLowEnergySocket : public GattlibExperimentsSocketInterface, public ReceiverInterface {
private:

    char scanner_mac[18] = {0};
    Scanner *scanner = nullptr;

    char connector_mac[18] = {0};
    Connector *connector = nullptr;

    device_address broadcast_address;
    device_address own_address;

    Queue<std::shared_ptr<BluetoothLowEnergyMessage>> receiver_queue;


public:
    BluetoothLowEnergySocket(const char *scanner_mac, const char *connector_mac);

    bool begin() override;

    device_address *getBroadcastAddress() override;

    device_address *getAddress() override;

    uint8_t getMaximumMessageLength() override;

    void onReceive(const device_address *address, const uint8_t *payload, const uint16_t payload_length) override;

    bool send(device_address *destination, uint8_t *bytes, uint16_t bytes_len) override;

    bool send(device_address *destination, uint8_t *bytes, uint16_t bytes_len, uint8_t signal_strength) override;

    bool loop() override;
};


#endif //GATTLIB_EXPERIMENTS_BLUETOOTHLOWENERGYSOCKET_H
