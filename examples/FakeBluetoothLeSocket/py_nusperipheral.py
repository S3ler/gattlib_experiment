#!/usr/bin/python

#
#  py_nusperipheral - modified file test/example-gatt-client from bluez-5.37 sources
#
#  Copyright (C) 2017 - Gabriel Nikol and all bluez authors
#
#  This program is free software; you can redistribute it and/or modify
#  it under the terms of the GNU General Public License as published by
#  the Free Software Foundation; either version 2 of the License, or
#  (at your option) any later version.
#
#  This program is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
#
#  You should have received a copy of the GNU General Public License
#  along with this program; if not, write to the Free Software
#  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
#



import dbus
import dbus.exceptions
import dbus.mainloop.glib
import dbus.service

from gi.repository import GObject as gobject

import threading

BATTERY_SERVICE_UUID = '0001'

mainloop = None
nus_service = None

BLUEZ_SERVICE_NAME = 'org.bluez'
LE_ADAPTER = '/org/bluez/hci0'
LE_ADVERTISING_MANAGER_IFACE = 'org.bluez.LEAdvertisingManager1'
DBUS_OM_IFACE = 'org.freedesktop.DBus.ObjectManager'
DBUS_PROP_IFACE = 'org.freedesktop.DBus.Properties'

LE_ADVERTISEMENT_IFACE = 'org.bluez.LEAdvertisement1'
GATT_MANAGER_IFACE = 'org.bluez.GattManager1'

GATT_SERVICE_IFACE = 'org.bluez.GattService1'
GATT_CHRC_IFACE = 'org.bluez.GattCharacteristic1'
GATT_DESC_IFACE = 'org.bluez.GattDescriptor1'


class InvalidArgsException(dbus.exceptions.DBusException):
    _dbus_error_name = 'org.freedesktop.DBus.Error.InvalidArgs'


class NotSupportedException(dbus.exceptions.DBusException):
    _dbus_error_name = 'org.bluez.Error.NotSupported'


class NotPermittedException(dbus.exceptions.DBusException):
    _dbus_error_name = 'org.bluez.Error.NotPermitted'


class InvalidValueLengthException(dbus.exceptions.DBusException):
    _dbus_error_name = 'org.bluez.Error.InvalidValueLength'


class FailedException(dbus.exceptions.DBusException):
    _dbus_error_name = 'org.bluez.Error.Failed'


# ADVERTISMENT
class Advertisement(dbus.service.Object):
    PATH_BASE = '/org/bluez/example/advertisement'

    def __init__(self, bus, index, advertising_type):
        self.path = self.PATH_BASE + str(index)
        self.bus = bus
        self.ad_type = advertising_type
        self.service_uuids = None
        self.manufacturer_data = None
        self.solicit_uuids = None
        self.service_data = None
        self.include_tx_power = None
        dbus.service.Object.__init__(self, bus, self.path)

    def get_properties(self):
        properties = dict()
        properties['Type'] = self.ad_type
        if self.service_uuids is not None:
            properties['ServiceUUIDs'] = dbus.Array(self.service_uuids,
                                                    signature='s')
        if self.solicit_uuids is not None:
            properties['SolicitUUIDs'] = dbus.Array(self.solicit_uuids,
                                                    signature='s')
        if self.manufacturer_data is not None:
            properties['ManufacturerData'] = dbus.Dictionary(
                self.manufacturer_data, signature='qay')
        if self.service_data is not None:
            properties['ServiceData'] = dbus.Dictionary(self.service_data,
                                                        signature='say')
        if self.include_tx_power is not None:
            properties['IncludeTxPower'] = dbus.Boolean(self.include_tx_power)
        return {LE_ADVERTISEMENT_IFACE: properties}

    def get_path(self):
        return dbus.ObjectPath(self.path)

    def add_service_uuid(self, uuid):
        if not self.service_uuids:
            self.service_uuids = []
        self.service_uuids.append(uuid)

    def add_solicit_uuid(self, uuid):
        if not self.solicit_uuids:
            self.solicit_uuids = []
        self.solicit_uuids.append(uuid)

    def add_manufacturer_data(self, manuf_code, data):
        if not self.manufacturer_data:
            self.manufacturer_data = dict()
        self.manufacturer_data[manuf_code] = data

    def add_service_data(self, uuid, data):
        if not self.service_data:
            self.service_data = dict()
        self.service_data[uuid] = data

    @dbus.service.method(DBUS_PROP_IFACE,
                         in_signature='s',
                         out_signature='a{sv}')
    def GetAll(self, interface):
        if interface != LE_ADVERTISEMENT_IFACE:
            raise InvalidArgsException()
        return self.get_properties()[LE_ADVERTISEMENT_IFACE]

    @dbus.service.method(LE_ADVERTISEMENT_IFACE,
                         in_signature='',
                         out_signature='')
    def Release(self):
        print '%s: Released!' % self.path


class NUSAdvertisment(Advertisement):
    def __init__(self, bus, index):
        Advertisement.__init__(self, bus, index, 'peripheral')
        self.add_service_uuid(BATTERY_SERVICE_UUID)
        self.add_manufacturer_data(0xffff, [0x00, 0x01, 0x02, 0x03, 0x04])
        self.add_service_data('9999', [0x00, 0x01, 0x02, 0x03, 0x04])
        self.include_tx_power = True


def register_ad_cb():
    # print 'Advertisement registered'
    return


def register_ad_error_cb(error):
    print 'Failed to register advertisement: ' + str(error)
    mainloop.quit()


class Service(dbus.service.Object):
    PATH_BASE = '/org/bluez/example/service'

    def __init__(self, bus, index, uuid, primary):
        self.path = self.PATH_BASE + str(index)
        self.bus = bus
        self.uuid = uuid
        self.primary = primary
        self.characteristics = []
        dbus.service.Object.__init__(self, bus, self.path)

    def get_properties(self):
        return {
            GATT_SERVICE_IFACE: {
                'UUID': self.uuid,
                'Primary': self.primary,
                'Characteristics': dbus.Array(
                    self.get_characteristic_paths(),
                    signature='o')
            }
        }

    def get_path(self):
        return dbus.ObjectPath(self.path)

    def add_characteristic(self, characteristic):
        self.characteristics.append(characteristic)

    def get_characteristic_paths(self):
        result = []
        for chrc in self.characteristics:
            result.append(chrc.get_path())
        return result

    def get_characteristics(self):
        return self.characteristics

    @dbus.service.method(DBUS_PROP_IFACE,
                         in_signature='s',
                         out_signature='a{sv}')
    def GetAll(self, interface):
        if interface != GATT_SERVICE_IFACE:
            raise InvalidArgsException()

        return self.get_properties[GATT_SERVICE_IFACE]

    @dbus.service.method(DBUS_OM_IFACE, out_signature='a{oa{sa{sv}}}')
    def GetManagedObjects(self):
        response = {}

        response[self.get_path()] = self.get_properties()
        chrcs = self.get_characteristics()
        for chrc in chrcs:
            response[chrc.get_path()] = chrc.get_properties()
            descs = chrc.get_descriptors()
            for desc in descs:
                response[desc.get_path()] = desc.get_properties()

        return response


class Characteristic(dbus.service.Object):
    def __init__(self, bus, index, uuid, flags, service):
        self.path = service.path + '/char' + str(index)
        self.bus = bus
        self.uuid = uuid
        self.service = service
        self.flags = flags
        self.descriptors = []
        self.value = []
        dbus.service.Object.__init__(self, bus, self.path)

    def get_properties(self):
        return {
            GATT_CHRC_IFACE: {
                'Service': self.service.get_path(),
                'UUID': self.uuid,
                'Flags': self.flags,
                'Value': dbus.Array(self.value, signature='ay'),
                'Descriptors': dbus.Array(
                    self.get_descriptor_paths(),
                    signature='o')
            }
        }

    def get_path(self):
        return dbus.ObjectPath(self.path)

    def add_descriptor(self, descriptor):
        self.descriptors.append(descriptor)

    def get_descriptor_paths(self):
        result = []
        for desc in self.descriptors:
            result.append(desc.get_path())
        return result

    def get_descriptors(self):
        return self.descriptors

    @dbus.service.method(DBUS_PROP_IFACE,
                         in_signature='s',
                         out_signature='a{sv}')
    def GetAll(self, interface):
        if interface != GATT_CHRC_IFACE:
            raise InvalidArgsException()

        return self.get_properties[GATT_CHRC_IFACE]

    @dbus.service.method(GATT_CHRC_IFACE, out_signature='ay')
    def ReadValue(self):
        print('Default ReadValue called, returning error')
        raise NotSupportedException()

    @dbus.service.method(GATT_CHRC_IFACE, in_signature='ay')
    def WriteValue(self, value):
        print('Default WriteValue called, returning error')
        raise NotSupportedException()

    @dbus.service.method(GATT_CHRC_IFACE)
    def StartNotify(self):
        print('Default StartNotify called, returning error')
        raise NotSupportedException()

    @dbus.service.method(GATT_CHRC_IFACE)
    def StopNotify(self):
        print('Default StopNotify called, returning error')
        raise NotSupportedException()

    @dbus.service.signal(DBUS_PROP_IFACE,
                         signature='sa{sv}as')
    def PropertiesChanged(self, interface, changed, invalidated):
        pass


class Descriptor(dbus.service.Object):
    def __init__(self, bus, index, uuid, flags, characteristic):
        self.path = characteristic.path + '/desc' + str(index)
        self.bus = bus
        self.uuid = uuid
        self.flags = flags
        self.chrc = characteristic
        dbus.service.Object.__init__(self, bus, self.path)

    def get_properties(self):
        return {
            GATT_DESC_IFACE: {
                'Characteristic': self.chrc.get_path(),
                'UUID': self.uuid,
                'Flags': self.flags,
            }
        }

    def get_path(self):
        return dbus.ObjectPath(self.path)

    @dbus.service.method(DBUS_PROP_IFACE,
                         in_signature='s',
                         out_signature='a{sv}')
    def GetAll(self, interface):
        if interface != GATT_DESC_IFACE:
            raise InvalidArgsException()

        return self.get_properties[GATT_CHRC_IFACE]

    @dbus.service.method(GATT_DESC_IFACE, out_signature='ay')
    def ReadValue(self):
        print ('Default ReadValue called, returning error')
        raise NotSupportedException()

    @dbus.service.method(GATT_DESC_IFACE, in_signature='ay')
    def WriteValue(self, value):
        print('Default WriteValue called, returning error')
        raise NotSupportedException()


class NUSService(Service):
    """
    TODO: fill description
    https://devzone.nordicsemi.com/documentation/nrf51/6.0.0/s110/html/a00066.html
    """

    # NUS Service UUID
    TEST_SVC_UUID = '6E400001-B5A3-F393-E0A9-E50E24DCCA9E'

    def __init__(self, bus, index):
        Service.__init__(self, bus, index, self.TEST_SVC_UUID, False)
        self.add_characteristic(RXCharacteristic(bus, 0, self))
        self.tx_characteristc = TXCharacteristic(bus, 1, self)
        self.add_characteristic(self.tx_characteristc)

    def send_bytes(self, bytes):
        return self.tx_characteristc.send_tx_bytes(bytes)


class TXCharacteristic(Characteristic):
    """
    TODO: fill description
    """

    TX_CHRC_UUID = '6E400002-B5A3-F393-E0A9-E50E24DCCA9E'

    def __init__(self, bus, index, service):
        Characteristic.__init__(
            self, bus, index,
            self.TX_CHRC_UUID,
            ['read', 'notify'],
            service)
        self.notifying = False
        self.tx_bytes = 0
        # gobject.timeout_add(5000, self.send_hello)

    def notify_tx_bytes(self):
        if not self.notifying:
            return
        self.PropertiesChanged(
            GATT_CHRC_IFACE,
            {'Value': dbus.ByteArray(self.tx_bytes)}, [])

    def send_hello(self):
        self.tx_bytes = "Hello"
        print('Sending: ' + repr(self.tx_bytes))
        self.notify_tx_bytes()
        return True

    def send_tx_bytes(self, tx_bytes):
        if len(tx_bytes) > 20:
            return False

        self.tx_bytes = tx_bytes
        self.notify_tx_bytes()
        return True

    def ReadValue(self):
        # print('TX Bytes read: ' + repr(self.tx_bytes))
        return dbus.ByteArray(self.tx_bytes)

    def StartNotify(self):
        if self.notifying:
            print('Already notifying, nothing to do')
            return

        self.notifying = True
        self.notify_tx_bytes()

    def StopNotify(self):
        if not self.notifying:
            print('Not notifying, nothing to do')
            return

        self.notifying = False


class RXCharacteristic(Characteristic):
    """
    TODO: fill description
    """

    # TX Characteristic (UUID: 6E400002-B5A3-F393-E0A9-E50E24DCCA9E):
    # RX Characteristic (UUID: 6E400003-B5A3-F393-E0A9-E50E24DCCA9E):
    RX_CHRC_UUID = '6E400003-B5A3-F393-E0A9-E50E24DCCA9E'

    def __init__(self, bus, index, service):
        Characteristic.__init__(
            self, bus, index,
            self.RX_CHRC_UUID,
            ['write', 'writable-auxiliaries'],
            service)
        self.rx_bytes = None

    def ReadValue(self):
        print('RX Bytes read: ' + repr(self.rx_bytes))
        return self.value

    def WriteValue(self, value):
        global newMessageArrived
        global newMessage
        newMessageArrived = True
        newMessage = bytearray(value)
        self.value = value


def register_service_cb():
    # print('GATT service registered')
    return


def register_service_error_cb(error):
    print('Failed to register service: ' + str(error))
    mainloop.quit()


def find_adapter(bus, adapter):
    remote_om = dbus.Interface(bus.get_object(BLUEZ_SERVICE_NAME, '/'),
                               DBUS_OM_IFACE)
    objects = remote_om.GetManagedObjects()

    for o, props in objects.iteritems():
        if (str(o) == str(adapter)) and props.has_key(GATT_MANAGER_IFACE):
            return o

    return None


def init():
    global mainloop
    global nus_service

    dbus.mainloop.glib.DBusGMainLoop(set_as_default=True)

    bus = dbus.SystemBus()

    adapter = find_adapter(bus, LE_ADAPTER)
    if not adapter:
        print('GattManager1 interface not found')
        return

    service_manager = dbus.Interface(
        bus.get_object(BLUEZ_SERVICE_NAME, adapter),
        GATT_MANAGER_IFACE)

    nus_service = NUSService(bus, 0)

    mainloop = gobject.MainLoop()

    service_manager.RegisterService(nus_service.get_path(), {},
                                    reply_handler=register_service_cb,
                                    error_handler=register_service_error_cb)

    adapter_props = dbus.Interface(bus.get_object(BLUEZ_SERVICE_NAME, adapter),
                                   "org.freedesktop.DBus.Properties");

    adapter_props.Set("org.bluez.Adapter1", "Powered", dbus.Boolean(1))

    ad_manager = dbus.Interface(bus.get_object(BLUEZ_SERVICE_NAME, adapter),
                                LE_ADVERTISING_MANAGER_IFACE)

    nus_advertisment = NUSAdvertisment(bus, 1)

    mainloop = gobject.MainLoop()

    ad_manager.RegisterAdvertisement(nus_advertisment.get_path(), {},
                                     reply_handler=register_ad_cb,
                                     error_handler=register_ad_error_cb)
    mainloop.run()


newMessageArrived = False
newMessage = [0] * 20


def is_new_message_arrived():
    global newMessageArrived
    return newMessageArrived


class NUSPeripheral(threading.Thread):
    def __init__(self):
        threading.Thread.__init__(self)

    def run(self):
        init()

    def send_user_input(self, c0, c1, c2, c3, c4, c5, c6, c7, c8, c9, c10, c11, c12, c13, c14, c15, c16, c17, c18, c19,
                        length):
        global nus_service
        to_send_buffer = bytearray(
            [c0, c1, c2, c3, c4, c5, c6, c7, c8, c9, c10, c11, c12, c13, c14, c15, c16, c17, c18, c19])
        to_send_length = length
        to_send_buffer = to_send_buffer[0:to_send_length]
        return nus_service.send_bytes(to_send_buffer);

    def is_new_message_arrived(self):
        global newMessageArrived
        return newMessageArrived

    def get_new_message(self):
        global newMessageArrived
        newMessageArrived = False;
        global newMessage
        return bytearray(newMessage)
