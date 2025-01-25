/******************************************************************************
SFE_LSM9DS1.h
SFE_LSM9DS1 Library Header File
Jim Lindblom @ SparkFun Electronics
Original Creation Date: February 27, 2015
https://github.com/sparkfun/LSM9DS1_Breakout

2020-2025, Bernd Porr, mail@berndporr.me.uk

This file prototypes the LSM9DS1 class, implemented in SFE_LSM9DS1.cpp. In
addition, it defines every register in the LSM9DS1 (both the Gyro and Accel/
Magnetometer registers).

Development environment specifics:
    Hardware Platform: Raspberry PI
    LSM9DS1

This code is beerware; if you see me (or any other SparkFun employee) at the
local, and you've found our code helpful, please buy us a round!

Distributed as-is; no warranty is given.
******************************************************************************/
#ifndef __LSM9DS1_H__
#define __LSM9DS1_H__

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <thread>
#include <gpiod.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <linux/i2c-dev.h>
#include <sys/ioctl.h>


#include "LSM9DS1_Registers.h"
#include "LSM9DS1_Types.h"

static const char could_not_open_i2c[] = "Could not open I2C.\n";

#define ISR_TIMEOUT 1000

#define LSM9DS1_AG_ADDR 0x6B
#define LSM9DS1_M_ADDR  0x1E
#define LSM9DS1_DEFAULT_I2C_BUS 1
#define LSM9DS1_DRDY_GPIO 22

/**
 * Hardware related settings
 **/
struct DeviceSettings
{
    /**
     * I2C acceleromter address
     **/
    uint8_t agAddress = LSM9DS1_AG_ADDR;

    /**
     * I2C magnetometer address
     **/
    uint8_t mAddress = LSM9DS1_M_ADDR;

    /**
     * Default I2C bus number (most likely 1)
     **/
    unsigned i2c_bus = LSM9DS1_DEFAULT_I2C_BUS;

    /**
     * Data ready pin (INT2) of the accelerometer
     **/
    unsigned drdy_gpio = LSM9DS1_DRDY_GPIO;

    /**
     * Chip number
     **/
    unsigned drdy_chip = 0;
};

/**
 * Accelerometer settings with default values
 **/
struct AccelSettings
{
    /**
     * defines all possible FSR's of the accelerometer
     **/
    enum Scale
    {
	A_SCALE_2G = 2,
	A_SCALE_16G = 16,
	A_SCALE_4G = 4,
	A_SCALE_8G = 8
    };

    /**
     * accel scale (in g) can be 2, 4, 8, or 16
     **/
    Scale scale = A_SCALE_16G;
	
    uint8_t enableX = true; //!< Enables accelerometer's X axis
    uint8_t enableY = true; //!< Enables accelerometer's Y axis
    uint8_t enableZ = true; //!< Enables accelerometer's Z axis

    /**
     * Defines all possible anti-aliasing filter rates of the accelerometer
     **/
    enum Abw
    {
	A_ABW_408 = 0,		//!< 408 Hz (0x0)
	A_ABW_211 = 1,		//!< 211 Hz (0x1)
	A_ABW_105 = 2,		//!< 105 Hz (0x2)
	A_ABW_50  = 3,		//!<  50 Hz (0x3)
	A_ABW_OFF = -1          //!< no cutoff
    };

    /**
     * Accel cutoff freqeuncy
     **/
    Abw bandwidth = A_ABW_OFF;
	
    uint8_t highResEnable = false;
    uint8_t highResBandwidth = 0;
};

/**
 * Gyroscope settings with default values
 **/
struct GyroSettings
{
    /**
     * Gyro_scale defines the possible full-scale ranges of the gyroscope
     **/
    enum Scale
    {
	G_SCALE_245DPS = 245,   //!< 245 degrees per second
	G_SCALE_500DPS = 500,	//!< 500 dps
	G_SCALE_2000DPS = 2000	//!< 2000 dps
    };

    /**
     * gyro scale can be 245, 500, or 2000
     **/
    Scale scale = G_SCALE_245DPS;

    /**
     * Gyro & Acc sampling rates.
     **/
    enum SampleRate
    {
	G_ODR_14_9 = 1,	//!< 14.9 Hz (1)
	G_ODR_59_5 = 2,	//!< 59.5 Hz (2)
	G_ODR_119 = 3,	//!< 119 Hz (3)
	G_ODR_238 = 4,	//!< 238 Hz (4)
	G_ODR_476 = 5,	//!< 476 Hz (5)
	G_ODR_952 = 6	//!< 952 Hz (6)
    };
	
    /** 
     * Gyro & Accelerometer sample rate.
     **/
    SampleRate sampleRate = G_ODR_14_9;
	
    uint8_t enableX = true; //!< X axis enabled
    uint8_t enableY = true; //!< Y axis enabled
    uint8_t enableZ = true; //!< Z axis enabled

    uint8_t bandwidth = 0;
    uint8_t lowPowerEnable = false;
    uint8_t HPFEnable = false;	
    uint8_t HPFCutoff = 0;
    uint8_t flipX = false;
    uint8_t flipY = false;
    uint8_t flipZ = false;
    uint8_t orientation = 0;
    uint8_t latchInterrupt = true;
};

/**
 * Magnetometer settings with default values
 **/
struct MagSettings
{
    uint8_t enabled = true;

    /**
     * Defines all possible FSR's of the magnetometer
     **/
    enum Scale
    {
	M_SCALE_4GS = 4, 	//!< 4Gs
	M_SCALE_8GS = 8,	//!< 8Gs
	M_SCALE_12GS = 12,	//!< 12Gs
	M_SCALE_16GS = 16,	//!< 16Gs
    };

    Scale scale = M_SCALE_16GS;
	
    /**
     * Defines all possible output data rates of the magnetometer
     **/
    enum SampleRate
    {
	M_ODR_0625,	// 0.625 Hz (0)
	M_ODR_125,	// 1.25 Hz (1)
	M_ODR_250,	// 2.5 Hz (2)
	M_ODR_5,	// 5 Hz (3)
	M_ODR_10,	// 10 Hz (4)
	M_ODR_20,	// 20 Hz (5)
	M_ODR_40,	// 40 Hz (6)
	M_ODR_80	// 80 Hz (7)
    };

    /**
     * Output data rate of the magnetometer.
     **/
    SampleRate sampleRate = M_ODR_80;
	
    uint8_t tempCompensationEnable = false;

    /**
     * magPerformance can be any value between 0-3
     * 0 = Low power mode      2 = high performance
     * 1 = medium performance  3 = ultra-high performance
     **/
    uint8_t XYPerformance = 3;
    uint8_t ZPerformance = 3;
    uint8_t lowPowerEnable = false;
};

/**
 * Temperature sensor settings
 **/
struct TemperatureSettings
{
    uint8_t enabled = true;
};

enum lsm9ds1_axis {
    X_AXIS,
    Y_AXIS,
    Z_AXIS,
    ALL_AXIS
};

/**
 * Sample from the LSM9DS1
 **/
struct LSM9DS1Sample {
    /**
     * X Acceleration in m/s^2
     **/
    float ax = 0;

    /**
     * Y Acceleration in m/s^2
     **/
    float ay = 0;

    /**
     * Z Acceleration in m/s^2
     **/
    float az = 0;

    /**
     * X Rotation in deg/s
     **/
    float gx = 0;

    /**
     * Y Rotation in deg/s
     **/
    float gy = 0;

    /**
     * Z Rotation in deg/s
     **/
    float gz = 0;

    /**
     * X Magnetic field in Gauss
     **/
    float mx = 0;

    /**
     * Y Magnetic field in Gauss
     **/
    float my = 0;

    /**
     * Z Magnetic field in Gauss
     **/
    float mz = 0;

    /**
     * Chip temperature
     **/
    float temperature = 0;
};

/**
 * Callback interface where the callback needs to be
 * implemented by the host application.
 **/
class LSM9DS1callback {
public:
    /**
     * Called after a sample has arrived.
     **/
    virtual void hasSample(LSM9DS1Sample sample) = 0;
};

/**
 * Main class for the LSM9DS1 acceleromter which manages the data acquisition
 * via pigpio and calls the main program via a callback handler.
 * The constructor and the begin() function have default settings
 * so that in the simplest case just a callback needs to be registered
 * and then begin be called. To stop the data acquistion call end().
 **/
class LSM9DS1
{
public:
    /** 
     * LSM9DS1 class constructor
     * \param deviceSettings is defined in DeviceSettings
     * The deviceSettings has default values for standard wiring.
     **/
    LSM9DS1(DeviceSettings deviceSettings = DeviceSettings());
        
    /**
     * Initializes the gyro, accelerometer, magnetometer and starts the acquistion.
     * This will set up the scale and output rate of each sensor.
     * \param accelSettings Accelerometer settings with default settings.
     * \param gyroSettings Gyroscope settings with default settings.
     * \param magSettings Magnetometer settings with default settings.
     * \param temperatureSettings Temperature sensor settings with default settings.
     **/
    void begin(GyroSettings gyroSettings = GyroSettings(),
	       AccelSettings accelSettings = AccelSettings(),
	       MagSettings magSettings = MagSettings(),
	       TemperatureSettings temperatureSettings = TemperatureSettings()
	);
	
    /**
     * Ends the data acquisition and closes all IO.
     **/
    void end();

    ~LSM9DS1() {
	end();
    }

    /**
     * Sets the callback which receives the samples at the sampling rate.
     * \param cb Callback interface.
     **/
    void setCallback(LSM9DS1callback* cb) {
	lsm9ds1Callback = cb;
    }

    /** 
     * Polls the accelerometer status register to check
     * if new data is available.
     * \returns true if data is available.
     **/
    bool accelAvailable();
    
    /** 
     * Polls the gyroscope status register to check
     * if new data is available.
     * \returns true if data is available.
     **/
    bool gyroAvailable();
    
    /** 
     * Polls the temperature status register to check
     * if new data is available.
     * \returns true if data is available.
     **/
    bool tempAvailable();
    
    /** 
     * Polls the magnetometer status register to check
     * if new data is available.
     * \param axis can be either X_AXIS, Y_AXIS, Z_AXIS, to check for new data
     *      on one specific axis. Or ALL_AXIS (default) to check for new data
     *      on all axes.
     * \returns true if data is available.
     **/
    bool magAvailable(lsm9ds1_axis axis = ALL_AXIS);
    
    /** 
     * Read a specific axis of the gyroscope.
     * \param axis can be any of X_AXIS, Y_AXIS, or Z_AXIS.
     * \returns A 16-bit signed integer with sensor data on requested axis.
     **/
    int16_t readGyro(lsm9ds1_axis axis);
    
    /**
     * Read a specific axis of the accelerometer.
     * \param axis can be any of X_AXIS, Y_AXIS, or Z_AXIS.
     * \returns A 16-bit signed integer with sensor data on requested axis.
     **/
    int16_t readAccel(lsm9ds1_axis axis);
    
    /**
     * Read a specific axis of the magnetometer.
     * \param axis can be any of X_AXIS, Y_AXIS, or Z_AXIS.
     * \returns A 16-bit signed integer with sensor data on requested axis.
     **/
    int16_t readMag(lsm9ds1_axis axis);

    /**
     * Sets the magnetometer offset.
     * \param axis can be any of X_AXIS, Y_AXIS, or Z_AXIS.
     * \param offset in raw units
     **/
    void magOffset(uint8_t axis, int16_t offset);
    
    /** 
     * Convert from RAW signed 16-bit value to degrees per second
     * This function reads in a signed 16-bit value and returns the scaled
     * DPS. This function relies on gScale and gRes being correct.
     * \param gyro A signed 16-bit raw reading from the gyroscope.
     * \returns Rotation in deg/s.
     **/
    float calcGyro(int16_t gyro);
    
    /** 
     * Convert from RAW signed 16-bit value to gravity (g's).
     * This function reads in a signed 16-bit value and returns the scaled
     * g's. This function relies on aScale and aRes being correct.
     * \param accel A signed 16-bit raw reading from the accelerometer.
     * \returns Acceleration in m/s^2.
     **/
    float calcAccel(int16_t accel);
    
    /** 
     * Convert from RAW signed 16-bit value to Gauss (Gs)
     * This function reads in a signed 16-bit value and returns the scaled
     * Gs. This function relies on mScale and mRes being correct.
     * \param mag A signed 16-bit raw reading from the magnetometer.
     * \returns Magnetic field strength in Gauss.
     **/
    float calcMag(int16_t mag);
    
    /** 
     * Set the full-scale range of the gyroscope.
     * This function can be called to set the scale of the gyroscope to 
     * 245, 500, or 200 degrees per second.
     * \param gScl The desired gyroscope scale.
     **/
    void setGyroScale(GyroSettings::Scale gScl);

    /**
     * Set the full-scale range of the accelerometer.
     * \param The desired accelerometer scale.
     **/
    void setAccelScale(AccelSettings::Scale aScl);
    
    /** 
     * Set the full-scale range of the magnetometer.
     * The desired magnetometer scale.
     **/
    void setMagScale(MagSettings::Scale mScl);
    
    /** 
     * Get contents of Gyroscope interrupt source register
     **/
    uint8_t getGyroIntSrc();
    
    /**
     * Get contents of accelerometer interrupt source register
     **/
    uint8_t getAccelIntSrc();
    
    /**
     * Get contents of magnetometer interrupt source register
     **/
    uint8_t getMagIntSrc();
    
    /** 
     * Get status of inactivity interrupt
     **/
    uint8_t getInactivity();
    
    /**
     * Get number of FIFO samples
     **/
    uint8_t getFIFOSamples();
        

private:
    // gRes, aRes, and mRes store the current resolution for each sensor. 
    // Units of these values would be DPS (or g's or Gs's) per ADC tick.
    // This value is calculated as (sensor scale) / (2^15).
    float gRes, aRes, mRes;
    
    // initGyro() -- Sets up the gyroscope to begin reading.
    // This function steps through all five gyroscope control registers.
    // Upon exit, the following parameters will be set:
    //    - CTRL_REG1_G = 0x0F: Normal operation mode, all axes enabled. 
    //        95 Hz ODR, 12.5 Hz cutoff frequency.
    //    - CTRL_REG2_G = 0x00: HPF set to normal mode, cutoff frequency
    //        set to 7.2 Hz (depends on ODR).
    //    - CTRL_REG3_G = 0x88: Interrupt enabled on INT_G (set to push-pull and
    //        active high). Data-ready output enabled on DRDY_G.
    //    - CTRL_REG4_G = 0x00: Continuous update mode. Data LSB stored in lower
    //        address. Scale set to 245 DPS.
    //    - CTRL_REG5_G = 0x00: FIFO disabled. HPF disabled.
    void initGyro();
    
    // readGyro() -- Read the gyroscope output registers.
    // This function will read all six gyroscope output registers.
    // The readings are stored in the class' gx, gy, and gz variables. Read
    // those _after_ calling readGyro().
    void readGyro();
    
    // initAccel() -- Sets up the accelerometer to begin reading.
    // This function steps through all accelerometer related control registers.
    // Upon exit these registers will be set as:
    //    - CTRL_REG0_XM = 0x00: FIFO disabled. HPF bypassed. Normal mode.
    //    - CTRL_REG1_XM = 0x57: 100 Hz data rate. Continuous update.
    //        all axes enabled.
    //    - CTRL_REG2_XM = 0x00:  2g scale. 773 Hz anti-alias filter BW.
    //    - CTRL_REG3_XM = 0x04: Accel data ready signal on INT1_XM pin.
    void initAccel();
    
    // readAccel() -- Read the accelerometer output registers.
    // This function will read all six accelerometer output registers.
    // The readings are stored in the class' ax, ay, and az variables. Read
    // those _after_ calling readAccel().
    void readAccel();
    
    // initMag() -- Sets up the magnetometer to begin reading.
    // This function steps through all magnetometer-related control registers.
    // Upon exit these registers will be set as:
    //    - CTRL_REG4_XM = 0x04: Mag data ready signal on INT2_XM pin.
    //    - CTRL_REG5_XM = 0x14: 100 Hz update rate. Low resolution. Interrupt
    //        requests don't latch. Temperature sensor disabled.
    //    - CTRL_REG6_XM = 0x00:  2 Gs scale.
    //    - CTRL_REG7_XM = 0x00: Continuous conversion mode. Normal HPF mode.
    //    - INT_CTRL_REG_M = 0x09: Interrupt active-high. Enable interrupts.
    void initMag();
    
    // readMag() -- Read the magnetometer output registers.
    // This function will read all six magnetometer output registers.
    // The readings are stored in the class' mx, my, and mz variables. Read
    // those _after_ calling readMag().
    void readMag();
    
    // readTemp() -- Read the temperature output register.
    // This function will read two temperature output registers.
    // The combined readings are stored in the class' temperature variables. Read
    // those _after_ calling readTemp().
    void readTemp();
    
    // gReadByte() -- Reads a byte from a specified gyroscope register.
    // Input:
    //     - subAddress = Register to be read from.
    // Output:
    //     - An 8-bit value read from the requested address.
    uint8_t mReadByte(uint8_t subAddress);
    
    // gReadBytes() -- Reads a number of bytes -- beginning at an address
    // and incrementing from there -- from the gyroscope.
    // Input:
    //     - subAddress = Register to be read from.
    //     - * dest = A pointer to an array of uint8_t's. Values read will be
    //        stored in here on return.
    //    - count = The number of bytes to be read.
    // Output: No value is returned, but the `dest` array will store
    //     the data read upon exit.
    void mReadBytes(uint8_t subAddress, uint8_t * dest, uint8_t count);
    
    // gWriteByte() -- Write a byte to a register in the gyroscope.
    // Input:
    //    - subAddress = Register to be written to.
    //    - data = data to be written to the register.
    void mWriteByte(uint8_t subAddress, uint8_t data);
    
    // xmReadByte() -- Read a byte from a register in the accel/mag sensor
    // Input:
    //    - subAddress = Register to be read from.
    // Output:
    //    - An 8-bit value read from the requested register.
    uint8_t xgReadByte(uint8_t subAddress);
    
    // xmReadBytes() -- Reads a number of bytes -- beginning at an address
    // and incrementing from there -- from the accelerometer/magnetometer.
    // Input:
    //     - subAddress = Register to be read from.
    //     - * dest = A pointer to an array of uint8_t's. Values read will be
    //        stored in here on return.
    //    - count = The number of bytes to be read.
    // Output: No value is returned, but the `dest` array will store
    //     the data read upon exit.
    void xgReadBytes(uint8_t subAddress, uint8_t * dest, uint8_t count);
    
    // xmWriteByte() -- Write a byte to a register in the accel/mag sensor.
    // Input:
    //    - subAddress = Register to be written to.
    //    - data = data to be written to the register.
    void xgWriteByte(uint8_t subAddress, uint8_t data);
    
    // calcgRes() -- Calculate the resolution of the gyroscope.
    // This function will set the value of the gRes variable. gScale must
    // be set prior to calling this function.
    void calcgRes();
    
    // calcmRes() -- Calculate the resolution of the magnetometer.
    // This function will set the value of the mRes variable. mScale must
    // be set prior to calling this function.
    void calcmRes();
    
    // calcaRes() -- Calculate the resolution of the accelerometer.
    // This function will set the value of the aRes variable. aScale must
    // be set prior to calling this function.
    void calcaRes();

    // Opens the i2c dev
    int i2cOpen(int bus, int address) {
	char gpioFilename[20];
	snprintf(gpioFilename, 19, "/dev/i2c-%d", bus);
	const int fd_i2c = open(gpioFilename, O_RDWR);
	if (fd_i2c < 0) {
	    return fd_i2c;
	}
	const int r = ioctl(fd_i2c, I2C_SLAVE, address);
	if (r < 0) {
	    return -1;
	}
	return fd_i2c;
    }
    
    // I2CwriteByte() -- Write a byte out of I2C to a register in the device
    // Input:
    //    - address = The 7-bit I2C address of the slave device.
    //    - subAddress = The register to be written to.
    //    - data = Byte to be written to the register.
    void I2CwriteByte(uint8_t address, uint8_t subAddress, uint8_t data);
    
    // I2CreadByte() -- Read a single byte from a register over I2C.
    // Input:
    //    - address = The 7-bit I2C address of the slave device.
    //    - subAddress = The register to be read from.
    // Output:
    //    - The byte read from the requested address.
    uint8_t I2CreadByte(uint8_t address, uint8_t subAddress);
    
    // I2CreadBytes() -- Read a series of bytes, starting at a register via SPI
    // Input:
    //    - address = The 7-bit I2C address of the slave device.
    //    - subAddress = The register to begin reading.
    //     - * dest = Pointer to an array where we'll store the readings.
    //    - count = Number of registers to be read.
    // Output: No value is returned by the function, but the registers read are
    //         all stored in the *dest array given.
    uint8_t I2CreadBytes(uint8_t address, uint8_t subAddress, uint8_t * dest, uint8_t count);

    // Callback interface which is registered with the main program.
    LSM9DS1callback* lsm9ds1Callback = nullptr;

    // Callback function in this class when the data ready pin has been triggered
    void dataReady();

    // thread woken up by data!
    void worker()
	{
	    running = true;
	    while (running) {
		const struct timespec ts = { 1, 0 };
		int r = gpiod_line_event_wait(pinDRDY, &ts);
		if (1 == r) {
		    struct gpiod_line_event event;
		    gpiod_line_event_read(pinDRDY, &event);
		    dataReady();
		} else {
		    running = false;
		}
	    }
	}
	
    // setGyroODR() -- Set the output data rate and bandwidth of the gyroscope
    // Input:
    //    - gRate = The desired output rate and cutoff frequency of the gyro.
    void setGyroODR(GyroSettings::SampleRate gRate);
    
    // setAccelODR() -- Set the output data rate of the accelerometer
    // Input:
    //    - aRate = The desired output rate of the accel.
    void setAccelODR(uint8_t aRate);     
    
    // setMagODR() -- Set the output data rate of the magnetometer
    // Input:
    //    - mRate = The desired output rate of the mag.
    void setMagODR(MagSettings::SampleRate mRate);
    
    // configInactivity() -- Configure inactivity interrupt parameters
    // Input:
    //    - duration = Inactivity duration - actual value depends on gyro ODR
    //    - threshold = Activity Threshold
    //    - sleepOn = Gyroscope operating mode during inactivity.
    //      true: gyroscope in sleep mode
    //      false: gyroscope in power-down
    void configInactivity(uint8_t duration, uint8_t threshold, bool sleepOn);
    
    // configAccelInt() -- Configure Accelerometer Interrupt Generator
    // Input:
    //    - generator = Interrupt axis/high-low events
    //      Any OR'd combination of ZHIE_XL, ZLIE_XL, YHIE_XL, YLIE_XL, XHIE_XL, XLIE_XL
    //    - andInterrupts = AND/OR combination of interrupt events
    //      true: AND combination
    //      false: OR combination
    void configAccelInt(uint8_t generator, bool andInterrupts = false);
    
    // configAccelThs() -- Configure the threshold of an accelereomter axis
    // Input:
    //    - threshold = Interrupt threshold. Possible values: 0-255.
    //      Multiply by 128 to get the actual raw accel value.
    //    - axis = Axis to be configured. Either X_AXIS, Y_AXIS, or Z_AXIS
    //    - duration = Duration value must be above or below threshold to trigger interrupt
    //    - wait = Wait function on duration counter
    //      true: Wait for duration samples before exiting interrupt
    //      false: Wait function off
    void configAccelThs(uint8_t threshold, lsm9ds1_axis axis, uint8_t duration = 0, bool wait = 0);
    
    // configGyroInt() -- Configure Gyroscope Interrupt Generator
    // Input:
    //    - generator = Interrupt axis/high-low events
    //      Any OR'd combination of ZHIE_G, ZLIE_G, YHIE_G, YLIE_G, XHIE_G, XLIE_G
    //    - aoi = AND/OR combination of interrupt events
    //      true: AND combination
    //      false: OR combination
    //    - latch: latch gyroscope interrupt request.
    void configGyroInt(uint8_t generator, bool aoi, bool latch);
    
    // configGyroThs() -- Configure the threshold of a gyroscope axis
    // Input:
    //    - threshold = Interrupt threshold. Possible values: 0-0x7FF.
    //      Value is equivalent to raw gyroscope value.
    //    - axis = Axis to be configured. Either X_AXIS, Y_AXIS, or Z_AXIS
    //    - duration = Duration value must be above or below threshold to trigger interrupt
    //    - wait = Wait function on duration counter
    //      true: Wait for duration samples before exiting interrupt
    //      false: Wait function off
    void configGyroThs(int16_t threshold, lsm9ds1_axis axis, uint8_t duration, bool wait);
    
    // configInt() -- Configure INT1 or INT2 (Gyro and Accel Interrupts only)
    // Input:
    //    - interrupt = Select INT1 or INT2
    //      Possible values: XG_INT1 or XG_INT2
    //    - generator = Or'd combination of interrupt generators.
    //      Possible values: INT_DRDY_XL, INT_DRDY_G, INT1_BOOT (INT1 only), INT2_DRDY_TEMP (INT2 only)
    //      INT_FTH, INT_OVR, INT_FSS5, INT_IG_XL (INT1 only), INT1_IG_G (INT1 only), INT2_INACT (INT2 only)
    //    - activeLow = Interrupt active configuration
    //      Can be either INT_ACTIVE_HIGH or INT_ACTIVE_LOW
    //    - pushPull =  Push-pull or open drain interrupt configuration
    //      Can be either INT_PUSH_PULL or INT_OPEN_DRAIN
    void configInt(interrupt_select interupt, uint8_t generator,
		   h_lactive activeLow = INT_ACTIVE_LOW, pp_od pushPull = INT_PUSH_PULL);
                   
    // configMagInt() -- Configure Magnetometer Interrupt Generator
    // Input:
    //    - generator = Interrupt axis/high-low events
    //      Any OR'd combination of ZIEN, YIEN, XIEN
    //    - activeLow = Interrupt active configuration
    //      Can be either INT_ACTIVE_HIGH or INT_ACTIVE_LOW
    //    - latch: latch gyroscope interrupt request.
    void configMagInt(uint8_t generator, h_lactive activeLow, bool latch = true);
    
    // configMagThs() -- Configure the threshold of a gyroscope axis
    // Input:
    //    - threshold = Interrupt threshold. Possible values: 0-0x7FF.
    //      Value is equivalent to raw magnetometer value.
    void configMagThs(uint16_t threshold);
    
    // sleepGyro() -- Sleep or wake the gyroscope
    // Input:
    //    - enable: True = sleep gyro. False = wake gyro.
    void sleepGyro(bool enable = true);
    
    // enableFIFO() - Enable or disable the FIFO
    // Input:
    //    - enable: true = enable, false = disable.
    void enableFIFO(bool enable = true);
    
    // setFIFO() - Configure FIFO mode and Threshold
    // Input:
    //    - fifoMode: Set FIFO mode to off, FIFO (stop when full), continuous, bypass
    //      Possible inputs: FIFO_OFF, FIFO_THS, FIFO_CONT_TRIGGER, FIFO_OFF_TRIGGER, FIFO_CONT
    //    - fifoThs: FIFO threshold level setting
    //      Any value from 0-0x1F is acceptable.
    void setFIFO(fifoMode_type fifoMode, uint8_t fifoThs);
    
    DeviceSettings device;
    MagSettings mag;
    GyroSettings gyro;
    AccelSettings accel;
    TemperatureSettings temp;

    // We'll store the gyro, accel, and magnetometer readings in a series of
    // public class variables. Each sensor gets three variables -- one for each
    // axis. Call readGyro(), readAccel(), and readMag() first, before using
    // these variables!
    // These values are the RAW signed 16-bit readings from the sensors.
    int16_t gx = 0, gy = 0, gz = 0; // x, y, and z axis readings of the gyroscope
    int16_t ax = 0, ay = 0, az = 0; // x, y, and z axis readings of the accelerometer
    int16_t mx = 0, my = 0, mz = 0; // x, y, and z axis readings of the magnetometer
    int16_t temperature = 0; // Chip temperature

    struct gpiod_chip *chipDRDY = nullptr;
    struct gpiod_line *pinDRDY = nullptr;

    bool running = false;

    std::thread thr;
};

#endif // SFE_LSM9DS1_H //
