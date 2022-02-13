# LSM9DS1 RaspberryPI C++ Library

With C++ callback handler which is called at the 80Hz sampling rate of the LSM9DS1.

Based on the [SparkFun_LSM9DS1_Arduino_Library](https://github.com/sparkfun/SparkFun_LSM9DS1_Arduino_Library)

Included is also a PCB design to connect the IMU with a long cable via level shifters to the Raspberry PI.

This library supports only I2C.

## Hardware

Use an LSM9DS1 breakout board from Sparkfun or make your own (see at the bottom of the page).

  - Connect the 3.3V power (pin 1) and GND (pin 9) to the LSM9DS1.
  - Connect the I2C SDA (pin 3) & I2C SCL (pin 5) to the LSM9DS1.
  - Connect the GPIO22 (pin 15) to the DRDY output of the LSM9DS1.

## Software requirement

* [pigpio](http://abyz.me.uk/rpi/pigpio/)

```
sudo apt-get install libpigpio-dev
```

## Install

Run `raspi-config` and enable i2c.

If you are not the `pi` user make sure that in `/etc/group` you are member of the `i2c`-group.

```
cmake .
make
sudo make install
sudo ldconfig
```

## Example / test

```
cd example
./LSM9DS1_demo
```

This demo runs with a callback handler and it's called at a sampling rate of 50Hz.

## PCBs

The subdirectory PCBs contains two PCBs: one hat which plugs into the
raspberry PI and the PCB which contains the IMU. The PCBs are connected
via a standard telephone cable and can be over 10m long.
