## Tested wit
Python 2.7.12 (default, Nov 19 2016, 06:48:10)
[GCC 5.4.0 20160609] on linux2
# tested with:
# $ uname -a
# Linux MyComputerName 4.8.0-58-generic #63~16.04.1-Ubuntu SMP Mon Jun 26 18:08:51 UTC 2017 x86_64 x86_64 x86_64 GNU/Linux
# $ bluetoothd -v
# 5.37

You need to enable bluetoothd experimental
see: https://learn.adafruit.com/install-bluez-on-the-raspberry-pi/installation
In short:
The file /lib/systemd/system/bluetooth.service - has to look like this:
[Unit]
Description=Bluetooth service
Documentation=man:bluetoothd(8)
ConditionPathIsDirectory=/sys/class/bluetooth

[Service]
Type=dbus
BusName=org.bluez
ExecStart=/usr/lib/bluetooth/bluetoothd --experimental
NotifyAccess=main
#WatchdogSec=10
#Restart=on-failure
CapabilityBoundingSet=CAP_NET_ADMIN CAP_NET_BIND_SERVICE
LimitNPROC=1

[Install]
WantedBy=bluetooth.target
Alias=dbus-org.bluez.service

After editing it make:
$ sudo systemctl daemon-reload
$ sudo systemctl restart bluetooth