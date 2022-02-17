/******************************************************************************
SFE_LSM9DS1.cpp
SFE_LSM9DS1 Library Source File
Jim Lindblom @ SparkFun Electronics
Original Creation Date: February 27, 2015
https://github.com/sparkfun/LSM9DS1_Breakout

2020-2022, Bernd Porr, mail@berndporr.me.uk

This file implements all functions of the LSM9DS1 class. Functions here range
from higher level stuff, like reading/writing LSM9DS1 registers to low-level,
hardware reads and writes. Both SPI and I2C handler functions can be found
towards the bottom of this file.

Development environment specifics:
    Hardware Platform: Raspberry PI
    LSM9DS1 Breakout Version: 1.0

This code is beerware; if you see me (or any other SparkFun employee) at the
local, and you've found our code helpful, please buy us a round!

Distributed as-is; no warranty is given.
******************************************************************************/

#include <time.h>
#include <unistd.h>
#include <assert.h>
#include <math.h>
#include "LSM9DS1.h"
#include "LSM9DS1_Registers.h"
#include "LSM9DS1_Types.h"

float magSensitivity[4] = {0.00014, 0.00029, 0.00043, 0.00058};

#ifndef NDEBUG
#define DEBUG
#endif

LSM9DS1::LSM9DS1(DeviceSettings deviceSettings) {
	device = deviceSettings;
#ifdef DEBUG
	fprintf(stderr,"LSM9DS1: bus=%02x, agAddr=%02x, mAddr=%02x\n",
		device.i2c_bus,device.agAddress,device.mAddress);
#endif
}

void LSM9DS1::begin(GyroSettings gyroSettings,
			AccelSettings accelSettings,
			MagSettings magSettings,
			TemperatureSettings temperatureSettings)
{
	accel = accelSettings;
	gyro = gyroSettings;
	mag = magSettings;
	temp = temperatureSettings;

	if (device.initPIGPIO) {
		int cfg = gpioCfgGetInternals();
		cfg |= PI_CFG_NOSIGHANDLER;
		gpioCfgSetInternals(cfg);
		int r = gpioInitialise();
		if (r < 0) {
			char msg[] = "Cannot init pigpio.";
#ifdef DEBUG
			fprintf(stderr,"%s\n",msg);
#endif
			throw msg;
		}
	}

	// Once we have the scale values, we can calculate the resolution
	// of each sensor. That's what these functions are for. One for each sensor
	calcgRes(); // Calculate DPS / ADC tick, stored in gRes variable
	calcmRes(); // Calculate Gs / ADC tick, stored in mRes variable
	calcaRes(); // Calculate g / ADC tick, stored in aRes variable

	// To verify communication, we can read from the WHO_AM_I register of
	// each device. Store those in a variable so we can return them.
	uint8_t mTest = mReadByte(WHO_AM_I_M);        // Read the gyro WHO_AM_I
	uint8_t xgTest = xgReadByte(WHO_AM_I_XG);    // Read the accel/mag WHO_AM_I
	uint16_t whoAmICombined = (xgTest << 8) | mTest;

	if (whoAmICombined != ((WHO_AM_I_AG_RSP << 8) | WHO_AM_I_M_RSP)) {
		throw "WhoIAm returns wrong result.";
	}

	// Gyro initialization stuff:
	initGyro();    // This will "turn on" the gyro. Setting up interrupts, etc.

	// Accelerometer initialization stuff:
	initAccel(); // "Turn on" all axes of the accel. Set up interrupts, etc.

	// Magnetometer initialization stuff:
	initMag(); // "Turn on" all axes of the mag. Set up interrupts, etc.

	gpioSetMode(device.drdy_gpio,PI_INPUT);
	gpioSetISRFuncEx(device.drdy_gpio,RISING_EDGE,ISR_TIMEOUT,gpioISR,(void*)this);
}

void LSM9DS1::dataReady() {
	if (!lsm9ds1Callback) return;
	
	readGyro();
	readAccel();
	readMag();
	readTemp();
	
	LSM9DS1Sample sample;
	sample.gx = calcGyro(gx);
	sample.gy = calcGyro(gy);
	sample.gz = calcGyro(gz);
	sample.ax = calcAccel(ax);
	sample.ay = calcAccel(ay);
	sample.az = calcAccel(az);
	sample.mx = calcMag(mx);
	sample.my = calcMag(my);
	sample.mz = calcMag(mz);
	sample.temperature = round(((float)temperature/16.0f + 25.0f)*10.0f)/10.0f;
	lsm9ds1Callback->hasSample(sample);
}

void LSM9DS1::end() {
	gpioSetISRFuncEx(device.drdy_gpio,RISING_EDGE,-1,NULL,(void*)this);
	if (device.initPIGPIO) {
		gpioTerminate();
	}
}

void LSM9DS1::initGyro()
{
	uint8_t tempRegValue = 0;

	// CTRL_REG1_G (Default value: 0x00)
	// [ODR_G2][ODR_G1][ODR_G0][FS_G1][FS_G0][0][BW_G1][BW_G0]
	// ODR_G[2:0] - Output data rate selection
	// FS_G[1:0] - Gyroscope full-scale selection
	// BW_G[1:0] - Gyroscope bandwidth selection

	tempRegValue = (gyro.sampleRate & 0x07) << 5;
	switch (gyro.scale) {
        case 500:
		tempRegValue |= (0x1 << 3);
		break;
        case 2000:
		tempRegValue |= (0x3 << 3);
		break;
		// Otherwise we'll set it to 245 dps (0x0 << 4)
	}
	tempRegValue |= (gyro.bandwidth & 0x3);
	xgWriteByte(CTRL_REG1_G, tempRegValue);

	// CTRL_REG2_G (Default value: 0x00)
	// [0][0][0][0][INT_SEL1][INT_SEL0][OUT_SEL1][OUT_SEL0]
	// INT_SEL[1:0] - INT selection configuration
	// OUT_SEL[1:0] - Out selection configuration
	xgWriteByte(CTRL_REG2_G, 0x00);

	// CTRL_REG3_G (Default value: 0x00)
	// [LP_mode][HP_EN][0][0][HPCF3_G][HPCF2_G][HPCF1_G][HPCF0_G]
	// LP_mode - Low-power mode enable (0: disabled, 1: enabled)
	// HP_EN - HPF enable (0:disabled, 1: enabled)
	// HPCF_G[3:0] - HPF cutoff frequency
	tempRegValue = gyro.lowPowerEnable ? (1<<7) : 0;
	if (gyro.HPFEnable) {
		tempRegValue |= (1<<6) | (gyro.HPFCutoff & 0x0F);
	}
	xgWriteByte(CTRL_REG3_G, tempRegValue);

	// CTRL_REG4 (Default value: 0x38)
	// [0][0][Zen_G][Yen_G][Xen_G][0][LIR_XL1][4D_XL1]
	// Zen_G - Z-axis output enable (0:disable, 1:enable)
	// Yen_G - Y-axis output enable (0:disable, 1:enable)
	// Xen_G - X-axis output enable (0:disable, 1:enable)
	// LIR_XL1 - Latched interrupt (0:not latched, 1:latched)
	// 4D_XL1 - 4D option on interrupt (0:6D used, 1:4D used)
	tempRegValue = 0;
	if (gyro.enableZ) tempRegValue |= (1<<5);
	if (gyro.enableY) tempRegValue |= (1<<4);
	if (gyro.enableX) tempRegValue |= (1<<3);
	if (gyro.latchInterrupt) tempRegValue |= (1<<1);
	xgWriteByte(CTRL_REG4, tempRegValue);

	// ORIENT_CFG_G (Default value: 0x00)
	// [0][0][SignX_G][SignY_G][SignZ_G][Orient_2][Orient_1][Orient_0]
	// SignX_G - Pitch axis (X) angular rate sign (0: positive, 1: negative)
	// Orient [2:0] - Directional user orientation selection
	tempRegValue = 0;
	if (gyro.flipX) tempRegValue |= (1<<5);
	if (gyro.flipY) tempRegValue |= (1<<4);
	if (gyro.flipZ) tempRegValue |= (1<<3);
	xgWriteByte(ORIENT_CFG_G, tempRegValue);

	// INT2_CTRL (0Dd)
	// Enable data ready on the INT2 pin: INT2_DRDY_XL = 2
	xgWriteByte(INT2_CTRL, 2);
}

void LSM9DS1::initAccel()
{
	uint8_t tempRegValue = 0;

	//    CTRL_REG5_XL (0x1F) (Default value: 0x38)
	//    [DEC_1][DEC_0][Zen_XL][Yen_XL][Zen_XL][0][0][0]
	//    DEC[0:1] - Decimation of accel data on OUT REG and FIFO.
	//        00: None, 01: 2 samples, 10: 4 samples 11: 8 samples
	//    Zen_XL - Z-axis output enabled
	//    Yen_XL - Y-axis output enabled
	//    Xen_XL - X-axis output enabled
	if (accel.enableZ) tempRegValue |= (1<<5);
	if (accel.enableY) tempRegValue |= (1<<4);
	if (accel.enableX) tempRegValue |= (1<<3);

	xgWriteByte(CTRL_REG5_XL, tempRegValue);

	// CTRL_REG6_XL (0x20) (Default value: 0x00)
	// [ODR_XL2][ODR_XL1][ODR_XL0][FS1_XL][FS0_XL][BW_SCAL_ODR][BW_XL1][BW_XL0]
	// ODR_XL[2:0] - Output data rate & power mode selection
	// FS_XL[1:0] - Full-scale selection
	// BW_SCAL_ODR - Bandwidth selection
	// BW_XL[1:0] - Anti-aliasing filter bandwidth selection
	tempRegValue = 0;
	const int sampleRate = 1; // dummy
	tempRegValue |= (sampleRate & 0x07) << 5;
	switch (accel.scale)
		{
		case 4:
			tempRegValue |= (0x2 << 3);
			break;
		case 8:
			tempRegValue |= (0x3 << 3);
			break;
		case 16:
			tempRegValue |= (0x1 << 3);
			break;
			// Otherwise it'll be set to 2g (0x0 << 3)
		}
	if (accel.bandwidth >= 0)
		{
			tempRegValue |= (1<<2); // Set BW_SCAL_ODR
			tempRegValue |= (accel.bandwidth & 0x03);
		}
	xgWriteByte(CTRL_REG6_XL, tempRegValue);

	// CTRL_REG7_XL (0x21) (Default value: 0x00)
	// [HR][DCF1][DCF0][0][0][FDS][0][HPIS1]
	// HR - High resolution mode (0: disable, 1: enable)
	// DCF[1:0] - Digital filter cutoff frequency
	// FDS - Filtered data selection
	// HPIS1 - HPF enabled for interrupt function
	tempRegValue = 0;
	if (accel.highResEnable)
		{
			tempRegValue |= (1<<7); // Set HR bit
			tempRegValue |= (accel.highResBandwidth & 0x3) << 5;
		}
	xgWriteByte(CTRL_REG7_XL, tempRegValue);
}

void LSM9DS1::magOffset(uint8_t axis, int16_t offset)
{
	if (axis > 2)
		return;
	uint8_t msb, lsb;
	msb = (offset & 0xFF00) >> 8;
	lsb = offset & 0x00FF;
	mWriteByte(OFFSET_X_REG_L_M + (2 * axis), lsb);
	mWriteByte(OFFSET_X_REG_H_M + (2 * axis), msb);
}

void LSM9DS1::initMag()
{
	uint8_t tempRegValue = 0;

	// CTRL_REG1_M (Default value: 0x10)
	// [TEMP_COMP][OM1][OM0][DO2][DO1][DO0][0][ST]
	// TEMP_COMP - Temperature compensation
	// OM[1:0] - X & Y axes op mode selection
	//    00:low-power, 01:medium performance
	//    10: high performance, 11:ultra-high performance
	// DO[2:0] - Output data rate selection
	// ST - Self-test enable
	if (mag.tempCompensationEnable) tempRegValue |= (1<<7);
	tempRegValue |= (mag.XYPerformance & 0x3) << 5;
	tempRegValue |= (mag.sampleRate & 0x7) << 2;
	mWriteByte(CTRL_REG1_M, tempRegValue);

	// CTRL_REG2_M (Default value 0x00)
	// [0][FS1][FS0][0][REBOOT][SOFT_RST][0][0]
	// FS[1:0] - Full-scale configuration
	// REBOOT - Reboot memory content (0:normal, 1:reboot)
	// SOFT_RST - Reset config and user registers (0:default, 1:reset)
	tempRegValue = 0;
	switch (mag.scale)
		{
		case 8:
			tempRegValue |= (0x1 << 5);
			break;
		case 12:
			tempRegValue |= (0x2 << 5);
			break;
		case 16:
			tempRegValue |= (0x3 << 5);
			break;
			// Otherwise we'll default to 4 gauss (00)
		}
	mWriteByte(CTRL_REG2_M, tempRegValue); // +/-4Gauss

	// CTRL_REG3_M (Default value: 0x03)
	// [I2C_DISABLE][0][LP][0][0][SIM][MD1][MD0]
	// I2C_DISABLE - Disable I2C interace (0:enable, 1:disable)
	// LP - Low-power mode cofiguration (1:enable)
	// SIM - SPI mode selection (0:write-only, 1:read/write enable)
	// MD[1:0] - Operating mode
	//    00:continuous conversion, 01:single-conversion,
	//  10,11: Power-down
	tempRegValue = 0;
	if (mag.lowPowerEnable) tempRegValue |= (1<<5);
	const int operatingMode = 0;
	tempRegValue |= (operatingMode & 0x3);
	mWriteByte(CTRL_REG3_M, tempRegValue); // Continuous conversion mode

	// CTRL_REG4_M (Default value: 0x00)
	// [0][0][0][0][OMZ1][OMZ0][BLE][0]
	// OMZ[1:0] - Z-axis operative mode selection
	//    00:low-power mode, 01:medium performance
	//    10:high performance, 10:ultra-high performance
	// BLE - Big/little endian data
	tempRegValue = 0;
	tempRegValue = (mag.ZPerformance & 0x3) << 2;
	mWriteByte(CTRL_REG4_M, tempRegValue);

	// CTRL_REG5_M (Default value: 0x00)
	// [0][BDU][0][0][0][0][0][0]
	// BDU - Block data update for magnetic data
	//    0:continuous, 1:not updated until MSB/LSB are read
	tempRegValue = 0;
	mWriteByte(CTRL_REG5_M, tempRegValue);
}

bool LSM9DS1::accelAvailable()
{
	assert(nullptr == lsm9ds1Callback);
	const uint8_t status = xgReadByte(STATUS_REG_1);
	return (status & (1<<0)) > 0;
}

bool LSM9DS1::gyroAvailable()
{
	assert(nullptr == lsm9ds1Callback);
	const uint8_t status = xgReadByte(STATUS_REG_1);
	return (status & (1<<1)) > 0;
}

bool LSM9DS1::tempAvailable()
{
	assert(nullptr == lsm9ds1Callback);
	const uint8_t status = xgReadByte(STATUS_REG_1);
	return (status & (1<<2)) > 0;
}

bool LSM9DS1::magAvailable(lsm9ds1_axis axis)
{
	assert(nullptr == lsm9ds1Callback);
	const uint8_t status = mReadByte(STATUS_REG_M);
	return (status & (1<<axis)) > 0;
}

void LSM9DS1::readAccel()
{
	uint8_t temp[32];
	try {
		xgReadBytes(OUT_X_L_XL, temp, 6); // Read 6 bytes, beginning at OUT_X_L_XL
		ax = (temp[1] << 8) | temp[0]; // Store x-axis values into ax
		ay = (temp[3] << 8) | temp[2]; // Store y-axis values into ay
		az = (temp[5] << 8) | temp[4]; // Store z-axis values into az
	} catch(int fError) {
		ax = ay = az = 999;
		return;
	}
}

int16_t LSM9DS1::readAccel(lsm9ds1_axis axis)
{
	assert(nullptr == lsm9ds1Callback);
	uint8_t temp[2] = {0, 0};
	int16_t value;
	xgReadBytes(OUT_X_L_XL + (2 * axis), temp, 2);
	value = (temp[1] << 8) | temp[0];
	return value;
}

void LSM9DS1::readMag()
{
	if (!mag.enabled) return;
	uint8_t temp[32];
	mReadBytes(OUT_X_L_M, temp, 6); // Read 6 bytes, beginning at OUT_X_L_M
	mx = (temp[1] << 8) | temp[0]; // Store x-axis values into mx
	my = (temp[3] << 8) | temp[2]; // Store y-axis values into my
	mz = (temp[5] << 8) | temp[4]; // Store z-axis values into mz
}

int16_t LSM9DS1::readMag(lsm9ds1_axis axis)
{
	assert(nullptr == lsm9ds1Callback);
	uint8_t temp[2] = {0,0};
	mReadBytes(OUT_X_L_M + (2 * axis), temp, 2);
	return (temp[1] << 8) | temp[0];
}

void LSM9DS1::readTemp()
{
	if (!temp.enabled) return;
	uint8_t temp[2] = {0,0}; // We'll read two bytes from the temperature sensor into temp
	xgReadBytes(OUT_TEMP_L, temp, 2); // Read 2 bytes, beginning at OUT_TEMP_L
	temperature = ((int16_t)temp[1] << 8) | temp[0];
}

void LSM9DS1::readGyro()
{
	uint8_t temp[32];
	try {
		xgReadBytes(OUT_X_L_G, temp, 6); // Read 6 bytes, beginning at OUT_X_L_G
		gx = (temp[1] << 8) | temp[0]; // Store x-axis values into gx
		gy = (temp[3] << 8) | temp[2]; // Store y-axis values into gy
		gz = (temp[5] << 8) | temp[4]; // Store z-axis values into gz
	} catch(int fError) {
		gx = gy = gz = 9999;
		return;
	}
}

int16_t LSM9DS1::readGyro(lsm9ds1_axis axis)
{
	assert(nullptr == lsm9ds1Callback);
	uint8_t temp[2] = {0,0};
	int16_t value;

	xgReadBytes(OUT_X_L_G + (2 * axis), temp, 2);

	value = (temp[1] << 8) | temp[0];
	return value;
}

float LSM9DS1::calcGyro(int16_t gyro)
{
	// Return the gyro raw reading times our pre-calculated DPS / (ADC tick):
	return gRes * gyro;
}

float LSM9DS1::calcAccel(int16_t accel)
{
	// Return the accel raw reading times our pre-calculated g's / (ADC tick):
	return aRes * accel;
}

float LSM9DS1::calcMag(int16_t mag)
{
	// Return the mag raw reading times our pre-calculated Gs / (ADC tick):
	return mRes * mag;
}

void LSM9DS1::setGyroScale(GyroSettings::Scale gScl)
{
	// Read current value of CTRL_REG1_G:
	uint8_t ctrl1RegValue = xgReadByte(CTRL_REG1_G);
	// Mask out scale bits (3 & 4):
	ctrl1RegValue &= 0xE7;
	switch (gScl)
		{
		case GyroSettings::G_SCALE_500DPS:
			ctrl1RegValue |= (0x1 << 3);
			gyro.scale = gScl;
			break;
		case GyroSettings::G_SCALE_2000DPS:
			ctrl1RegValue |= (0x3 << 3);
			gyro.scale = gScl;
			break;
		default: // Otherwise we'll set it to 245 dps (0x0 << 4)
			gyro.scale = GyroSettings::G_SCALE_245DPS;
			break;
		}
	xgWriteByte(CTRL_REG1_G, ctrl1RegValue);

	calcgRes();
}

void LSM9DS1::setAccelScale(AccelSettings::Scale aScl)
{
	// We need to preserve the other bytes in CTRL_REG6_XL. So, first read it:
	uint8_t tempRegValue = xgReadByte(CTRL_REG6_XL);
	// Mask out accel scale bits:
	tempRegValue &= 0xE7;

	switch (aScl)
		{
		case AccelSettings::A_SCALE_4G:
			tempRegValue |= (0x2 << 3);
			accel.scale = aScl;
			break;
		case AccelSettings::A_SCALE_8G:
			tempRegValue |= (0x3 << 3);
			accel.scale = aScl;
			break;
		case AccelSettings::A_SCALE_16G:
			tempRegValue |= (0x1 << 3);
			accel.scale = aScl;
			break;
		default: // Otherwise it'll be set to 2g (0x0 << 3)
			accel.scale = AccelSettings::A_SCALE_2G;
			break;
		}
	xgWriteByte(CTRL_REG6_XL, tempRegValue);

	// Then calculate a new aRes, which relies on aScale being set correctly:
	calcaRes();
}

void LSM9DS1::setMagScale(MagSettings::Scale mScl)
{
	// We need to preserve the other bytes in CTRL_REG6_XM. So, first read it:
	uint8_t temp = mReadByte(CTRL_REG2_M);
	// Then mask out the mag scale bits:
	temp &= 0xFF^(0x3 << 5);

	switch (mScl)
		{
		case MagSettings::M_SCALE_8GS:
			temp |= (0x1 << 5);
			mag.scale = mScl;
			break;
		case MagSettings::M_SCALE_12GS:
			temp |= (0x2 << 5);
			mag.scale = mScl;
			break;
		case MagSettings::M_SCALE_16GS:
			temp |= (0x3 << 5);
			mag.scale = mScl;
			break;
		default: // Otherwise we'll default to 4 gauss (00)
			mag.scale = MagSettings::M_SCALE_4GS;
			break;
		}

	// And write the new register value back into CTRL_REG6_XM:
	mWriteByte(CTRL_REG2_M, temp);

	// We've updated the sensor, but we also need to update our class variables
	// First update mScale:
	//mScale = mScl;
	// Then calculate a new mRes, which relies on mScale being set correctly:
	calcmRes();
}

void LSM9DS1::setGyroODR(GyroSettings::SampleRate gRate)
{
	// Only do this if gRate is not 0 (which would disable the gyro)
	if ((gRate & 0x07) != 0)
		{
			// We need to preserve the other bytes in CTRL_REG1_G.
			// So, first read it:
			uint8_t temp = xgReadByte(CTRL_REG1_G);
			// Then mask out the gyro ODR bits:
			temp &= 0xFF^(0x7 << 5);
			temp |= (gRate & 0x07) << 5;
			// Update our settings struct
			gyro.sampleRate = gRate;
			// And write the new register value back into CTRL_REG1_G:
			xgWriteByte(CTRL_REG1_G, temp);
		}
}

void LSM9DS1::setAccelODR(uint8_t aRate)
{
	// Only do this if aRate is not 0 (which would disable the accel)
	if ((aRate & 0x07) != 0)
		{
			// We need to preserve the other bytes in CTRL_REG1_XM. So, first read it:
			uint8_t temp = xgReadByte(CTRL_REG6_XL);
			// Then mask out the accel ODR bits:
			temp &= 0x1F;
			// Then shift in our new ODR bits:
			temp |= ((aRate & 0x07) << 5);
			// And write the new register value back into CTRL_REG1_XM:
			xgWriteByte(CTRL_REG6_XL, temp);
		}
}

void LSM9DS1::setMagODR(MagSettings::SampleRate mRate)
{
	// We need to preserve the other bytes in CTRL_REG5_XM. So, first read it:
	uint8_t temp = mReadByte(CTRL_REG1_M);
	// Then mask out the mag ODR bits:
	temp &= 0xFF^(0x7 << 2);
	// Then shift in our new ODR bits:
	temp |= ((mRate & 0x07) << 2);
	mag.sampleRate = mRate;
	// And write the new register value back into CTRL_REG5_XM:
	mWriteByte(CTRL_REG1_M, temp);
}

void LSM9DS1::calcgRes()
{
	gRes = ((float) gyro.scale) / 32768.0;
}

void LSM9DS1::calcaRes()
{
	aRes = ((float) accel.scale) / 32768.0;
}

void LSM9DS1::calcmRes()
{
	//mRes = ((float) mag.scale) / 32768.0;
	switch (mag.scale)
		{
		case 4:
			mRes = magSensitivity[0];
			break;
		case 8:
			mRes = magSensitivity[1];
			break;
		case 12:
			mRes = magSensitivity[2];
			break;
		case 16:
			mRes = magSensitivity[3];
			break;
		}

}

void LSM9DS1::configInt(interrupt_select interrupt, uint8_t generator,
			h_lactive activeLow, pp_od pushPull)
{
	// Write to INT1_CTRL or INT2_CTRL. [interupt] should already be one of
	// those two values.
	// [generator] should be an OR'd list of values from the interrupt_generators enum
	xgWriteByte(interrupt, generator);

	// Configure CTRL_REG8
	uint8_t temp;
	temp = xgReadByte(CTRL_REG8);

	if (activeLow) temp |= (1<<5);
	else temp &= ~(1<<5);

	if (pushPull) temp &= ~(1<<4);
	else temp |= (1<<4);

	xgWriteByte(CTRL_REG8, temp);
}

void LSM9DS1::configInactivity(uint8_t duration, uint8_t threshold, bool sleepOn)
{
	uint8_t temp = 0;

	temp = threshold & 0x7F;
	if (sleepOn) temp |= (1<<7);
	xgWriteByte(ACT_THS, temp);

	xgWriteByte(ACT_DUR, duration);
}

uint8_t LSM9DS1::getInactivity()
{
	uint8_t temp = xgReadByte(STATUS_REG_0);
	temp &= (0x10);
	return temp;
}

void LSM9DS1::configAccelInt(uint8_t generator, bool andInterrupts)
{
	// Use variables from accel_interrupt_generator, OR'd together to create
	// the [generator]value.
	uint8_t temp = generator;
	if (andInterrupts) temp |= 0x80;
	xgWriteByte(INT_GEN_CFG_XL, temp);
}

void LSM9DS1::configAccelThs(uint8_t threshold, lsm9ds1_axis axis, uint8_t duration, bool wait)
{
	// Write threshold value to INT_GEN_THS_?_XL.
	// axis will be 0, 1, or 2 (x, y, z respectively)
	xgWriteByte(INT_GEN_THS_X_XL + axis, threshold);

	// Write duration and wait to INT_GEN_DUR_XL
	uint8_t temp;
	temp = (duration & 0x7F);
	if (wait) temp |= 0x80;
	xgWriteByte(INT_GEN_DUR_XL, temp);
}

uint8_t LSM9DS1::getAccelIntSrc()
{
	uint8_t intSrc = xgReadByte(INT_GEN_SRC_XL);

	// Check if the IA_XL (interrupt active) bit is set
	if (intSrc & (1<<6))
		{
			return (intSrc & 0x3F);
		}

	return 0;
}

void LSM9DS1::configGyroInt(uint8_t generator, bool aoi, bool latch)
{
	// Use variables from accel_interrupt_generator, OR'd together to create
	// the [generator]value.
	uint8_t temp = generator;
	if (aoi) temp |= 0x80;
	if (latch) temp |= 0x40;
	xgWriteByte(INT_GEN_CFG_G, temp);
}

void LSM9DS1::configGyroThs(int16_t threshold, lsm9ds1_axis axis, uint8_t duration, bool wait)
{
	uint8_t buffer[2];
	buffer[0] = (threshold & 0x7F00) >> 8;
	buffer[1] = (threshold & 0x00FF);
	// Write threshold value to INT_GEN_THS_?H_G and  INT_GEN_THS_?L_G.
	// axis will be 0, 1, or 2 (x, y, z respectively)
	xgWriteByte(INT_GEN_THS_XH_G + (axis * 2), buffer[0]);
	xgWriteByte(INT_GEN_THS_XH_G + 1 + (axis * 2), buffer[1]);

	// Write duration and wait to INT_GEN_DUR_XL
	uint8_t temp;
	temp = (duration & 0x7F);
	if (wait) temp |= 0x80;
	xgWriteByte(INT_GEN_DUR_G, temp);
}

uint8_t LSM9DS1::getGyroIntSrc()
{
	uint8_t intSrc = xgReadByte(INT_GEN_SRC_G);

	// Check if the IA_G (interrupt active) bit is set
	if (intSrc & (1<<6)) {
		return (intSrc & 0x3F);
	}

	return 0;
}

void LSM9DS1::configMagInt(uint8_t generator, h_lactive activeLow, bool latch)
{
	// Mask out non-generator bits (0-4)
	uint8_t config = (generator & 0xE0);
	// IEA bit is 0 for active-low, 1 for active-high.
	if (activeLow == INT_ACTIVE_HIGH) config |= (1<<2);
	// IEL bit is 0 for latched, 1 for not-latched
	if (!latch) config |= (1<<1);
	// As long as we have at least 1 generator, enable the interrupt
	if (generator != 0) config |= (1<<0);

	mWriteByte(INT_CFG_M, config);
}

void LSM9DS1::configMagThs(uint16_t threshold)
{
	// Write high eight bits of [threshold] to INT_THS_H_M
	mWriteByte(INT_THS_H_M, uint8_t((threshold & 0x7F00) >> 8));
	// Write low eight bits of [threshold] to INT_THS_L_M
	mWriteByte(INT_THS_L_M, uint8_t(threshold & 0x00FF));
}

uint8_t LSM9DS1::getMagIntSrc()
{
	uint8_t intSrc = mReadByte(INT_SRC_M);

	// Check if the INT (interrupt active) bit is set
	if (intSrc & (1<<0))
		return (intSrc & 0xFE);

	return 0;
}

void LSM9DS1::sleepGyro(bool enable)
{
	uint8_t temp = xgReadByte(CTRL_REG9);
	if (enable) temp |= (1<<6);
	else temp &= ~(1<<6);
	xgWriteByte(CTRL_REG9, temp);
}

void LSM9DS1::enableFIFO(bool enable)
{
	uint8_t temp = xgReadByte(CTRL_REG9);
	if (enable) temp |= (1<<1);
	else temp &= ~(1<<1);
	xgWriteByte(CTRL_REG9, temp);
}

void LSM9DS1::setFIFO(fifoMode_type fifoMode, uint8_t fifoThs)
{
	// Limit threshold - 0x1F (31) is the maximum. If more than that was asked
	// limit it to the maximum.
	uint8_t threshold = fifoThs <= 0x1F ? fifoThs : 0x1F;
	xgWriteByte(FIFO_CTRL, ((fifoMode & 0x7) << 5) | (threshold & 0x1F));
}

uint8_t LSM9DS1::getFIFOSamples()
{
	return (xgReadByte(FIFO_SRC) & 0x3F);
}

void LSM9DS1::xgWriteByte(uint8_t subAddress, uint8_t data)
{
	I2CwriteByte(device.agAddress, subAddress, data);
	return ;
}

void LSM9DS1::mWriteByte(uint8_t subAddress, uint8_t data)
{
	return I2CwriteByte(device.mAddress, subAddress, data);
}

uint8_t LSM9DS1::xgReadByte(uint8_t subAddress)
{
	return I2CreadByte(device.agAddress, subAddress);
}

void LSM9DS1::xgReadBytes(uint8_t subAddress, uint8_t *dest, uint8_t count)
{
	I2CreadBytes(device.agAddress, subAddress, dest, count);
}

uint8_t LSM9DS1::mReadByte(uint8_t subAddress)
{
	return I2CreadByte(device.mAddress, subAddress);
}

void LSM9DS1::mReadBytes(uint8_t subAddress, uint8_t *dest, uint8_t count)
{
	I2CreadBytes(device.mAddress, subAddress, dest, count);
}



// i2c read and write protocols
void LSM9DS1::I2CwriteByte(uint8_t address, uint8_t subAddress, uint8_t data)
{
	int fd = i2cOpen(device.i2c_bus, address, 0);
	if (fd < 0) {
#ifdef DEBUG
		fprintf(stderr,"Could not write %02x to %02x,%02x,%02x\n",data,device.i2c_bus,address,subAddress);
#endif
		throw could_not_open_i2c;
	}
	i2cWriteByteData(fd, subAddress, data);
	i2cClose(fd);
}

uint8_t LSM9DS1::I2CreadByte(uint8_t address, uint8_t subAddress)
{
	int fd = i2cOpen(device.i2c_bus, address, 0);
	if (fd < 0) {
#ifdef DEBUG
		fprintf(stderr,"Could not read byte from %02x,%02x,%02x\n",device.i2c_bus,address,subAddress);
#endif
		throw could_not_open_i2c;
	}
	int data; // `data` will store the register data
	data = i2cReadByteData(fd, subAddress);
	if (data < 0) {
#ifdef DEBUG
		fprintf(stderr,"Could not read byte from %02x,%02x,%02x. ret=%d.\n",device.i2c_bus,address,subAddress,data);
#endif
		throw "Could not read from i2c.";
	}
	i2cClose(fd);
	return data;           
}

uint8_t LSM9DS1::I2CreadBytes(uint8_t address, uint8_t subAddress, uint8_t * dest, uint8_t count)
{
	int fd = i2cOpen(device.i2c_bus, address, 0);
	if (fd < 0) {
#ifdef DEBUG
		fprintf(stderr,"Could not read %n byte(s) from %02x,%02x,%02x\n",count,device.i2c_bus,address,subAddress);
#endif
		throw could_not_open_i2c;
	}
	int ret = i2cReadI2CBlockData(fd,subAddress,(char*)dest,count);
	i2cClose(fd);
	if (ret != count) {
		throw "Block read didn't work";
	}
	return ret;
}
