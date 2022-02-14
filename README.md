# I2C LSM9DS1 RaspberryPI C++ Library

![alt tag](sparkfun_LSM9DS1.jpg)

This is a C++11 library for the LSM9DS1 on a Raspberry PI using a callback handler for the data.
The callback handler is called at the sampling rate of the gyroscope of the LSM9DS1.

It's based on the [SparkFun_LSM9DS1_Arduino_Library](https://github.com/sparkfun/SparkFun_LSM9DS1_Arduino_Library).

Included is also a PCB design to connect the IMU with a long cable via level shifters to the Raspberry PI.

## Hardware

You can use the [SparkFun 9DoF IMU Breakout board](https://www.sparkfun.com/products/13284)
or make your own PCB (see the `pcbs` folder).

  - Connect the 3.3V power (pin 1) and GND (pin 9) to the LSM9DS1.
  - Connect the I2C SDA (pin 3) & I2C SCL (pin 5) to the LSM9DS1.
  - Connect the GPIO22 (pin 15) to the INT2 output of the LSM9DS1.

## Software requirement

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

## Documentation

The relevant public sections of the header files have docstrings.

The online documentation is here: https://berndporr.github.io/LSM9DS1_RaspberryPi_CPP_Library/

This demo runs with a callback handler and it's called at the default sampling rate of 50Hz.

## PCBs

The subdirectory PCBs contains two PCBs: one hat which plugs into the
raspberry PI and the PCB which contains the IMU. The PCBs are connected
via a standard telephone cable and can be over 10m long.
