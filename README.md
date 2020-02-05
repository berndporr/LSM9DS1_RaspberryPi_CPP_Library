# LSM9DS1 RaspberryPI C++ Library and circuit boards

For C++ with callback handler running at 50Hz sampling rate.

Based on the [SparkFun_LSM9DS1_Arduino_Library](https://github.com/sparkfun/SparkFun_LSM9DS1_Arduino_Library)

<p align="center"><img src="https://user-images.githubusercontent.com/17570265/29253393-a11ac3a6-80b6-11e7-846f-0d387fa2fbe4.jpeg" alt="LSM9DS1" width="200"/></p>

[LSM9DS1 Breakout Board (SEN-13284)](https://www.sparkfun.com/products/13284)

This library supports only I2C.

## Requirement

* [WiringPi](http://wiringpi.com/)

```
sudo apt-get install wiringpi
```

## Install

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
