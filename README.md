rpi-midi-ble
============
git
This project allows the [Raspberry Pi 3](https://www.raspberrypi.org/products/raspberry-pi-3-model-b/) to be used as an USB-MIDI over BLE-MIDI ([Bluetooth Low Energy MIDI](https://developer.apple.com/bluetooth/Apple-Bluetooth-Low-Energy-MIDI-Specification.pdf)) device.

![rpi-midi-ble](https://raw.githubusercontent.com/oxesoft/rpi-midi-ble/master/rpi-midi-ble.jpg)

It has two components:

[btmidi-server](https://github.com/oxesoft/bluez/blob/midi/tools/btmidi-server.c)
-------------
This tool creates an ALSA sequencer port everytime some central BLE app
(eg.: [Korg Module](http://www.korg.com/us/products/software/korg_module/), [GarageBand](http://www.apple.com/ios/garageband/), ...) is connected with the rpi-midi-ble peripheral.

[alsa-seq-autoconnect](https://github.com/oxesoft/rpi-midi-ble/blob/master/alsa-seq-autoconnect/main.c)
--------------------
This tool was created to be used with the tool [btmidi-server](https://github.com/oxesoft/bluez/blob/midi/tools/btmidi-server.c)
and is intended to perform the same use case as [Mourino](https://github.com/oxesoft/mourino) when used in a [Raspberry Pi 3](https://www.raspberrypi.org/products/raspberry-pi-3-model-b/).
Everytime some MIDI hardware is connected to the rpi-midi-ble USB port it automatically connects the respective ALSA sequencer port to the "BLE MIDI Device" port.
