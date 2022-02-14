#ifndef GPIOSYSFS
#define GPIOSYSFS

/* Copyright (c) 2011, RidgeRun
 * Copyright (c) 2014-2022, Bernd Porr
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *    This product includes software developed by the RidgeRun.
 * 4. Neither the name of the RidgeRun nor the
 *    names of its contributors may be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY RIDGERUN AND BERND PORR ''AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL RIDGERUN BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <poll.h>

/**
 * Path to the gpio sys directory
 **/
#define SYSFS_GPIO_DIR "/sys/class/gpio"
#define MAX_BUF 256

class SysGPIO {
public:
	enum Edge {rising = 0, falling = 1, both = 2};

	/**
	 * Constructor exposes the GPIO pin.
	 * \param theGPIO The GPIO pin being monitored
	 **/
	SysGPIO(unsigned int theGPIO) {
		gpio = theGPIO;

		int len;
		char buf[MAX_BUF];
		
		errorNo = open(SYSFS_GPIO_DIR "/export", O_WRONLY);
		if (errorNo < 0) {
			return;
		}
		
		len = snprintf(buf, sizeof(buf), "%d", gpio);
		errorNo = write(errorNo, buf, len);
		if (errorNo < 0) {
			return;
		}

		close(errorNo);

		errorNo = 0;
	}

	/**
	 * Destructor hides the pin in sysfs.
	 **/
	~SysGPIO() {
		int fd, len;
		char buf[MAX_BUF];

		return;
		
		fd = open(SYSFS_GPIO_DIR "/unexport", O_WRONLY);
		if (fd < 0) {
			return;
		}
		
		len = snprintf(buf, sizeof(buf), "%d", gpio);
		write(fd, buf, len);
		close(fd);
	}

	/**
	 * Setting the direction of the port.
	 * \param out_flag If true it's an output
	 * \returns Zero on success. Otherwise the error number.
	 **/
	int set_dir(bool out_flag) {
		int fd;
		char buf[MAX_BUF];
		snprintf(buf, sizeof(buf), SYSFS_GPIO_DIR  "/gpio%d/direction", gpio);
		fd = open(buf, O_WRONLY);
		if (fd < 0) {
			return fd;
		}
		if (out_flag)
			write(fd, "out", 4);
		else
			write(fd, "in", 3);
		close(fd);
		return 0;
	}

	/**
	 * Setting the value of of the GPIO pin
	 * \param value can be zero or one.
	 * \returns Zero on success. Otherwise the error number.
	 **/
	int set_value(int value) {
		int fd;
		char buf[MAX_BUF];
		
		snprintf(buf, sizeof(buf), SYSFS_GPIO_DIR "/gpio%d/value", gpio);
		fd = open(buf, O_WRONLY);
		if (fd < 0) {
			return fd;
		}		
		if (value)
			write(fd, "1", 2);
		else
			write(fd, "0", 2);
		close(fd);
		return 0;
	}

	/**
	 * Gets the value of the GPIO pin.
	 * \returns The value (0 or 1) or the negative error code.
	 **/
	int get_value() {
		int fd;
		char buf[MAX_BUF];
		char ch;
		
		snprintf(buf, sizeof(buf), SYSFS_GPIO_DIR "/gpio%d/value", gpio);
		
		fd = open(buf, O_RDONLY);
		if (fd < 0) {
			return fd;
		}
		
		read(fd, &ch, 1);

		int v;
		
		if (ch != '0') {
			v = 1;
		} else {
			v = 0;
		}
		
		close(fd);
		return v;
	}

	/**
	 * Sets the edge for IRQ handling.
	 **/
	int set_edge(Edge edge) {
		int fd;
		char buf[MAX_BUF];
		
		snprintf(buf, sizeof(buf), SYSFS_GPIO_DIR "/gpio%d/edge", gpio);
		
		fd = open(buf, O_WRONLY);
		if (fd < 0) {
			return fd;
		}
		
		char const *edge_str[]={"rising\n", "falling\n", "both\n"};
		write(fd, edge_str[edge], strlen(edge_str[edge])); 
		close(fd);
		return 0;
	}


	/**
	 * Waits for a change at the GPIO pin defined by gpio_set_edge()
	 **/
	int interrupt(int timeout) {
		int fd;
		char buf[MAX_BUF];
		snprintf(buf, sizeof(buf), SYSFS_GPIO_DIR "/gpio%d/value", gpio);
		fd = open(buf, O_RDONLY | O_NONBLOCK );
		if (fd < 0) {
			return fd;
		}
		
		struct pollfd fdset[1];
		int nfds = 1;
		int rc;
		  
		memset((void*)fdset, 0, sizeof(fdset));
		  
		fdset[0].fd = fd;
		fdset[0].events = POLLPRI;
		
		rc = poll(fdset, nfds, timeout);
		
		if (fdset[0].revents & POLLPRI) {
			// dummy read
			read(fdset[0].fd, buf, MAX_BUF);
		}
		close(fd);
		return rc;
	}

	/**
	 * Returns the error code if anything went wrong in the constructor.
	 * \returns error code (negative) or zero for success.
	 **/
	int getErrorNo() {
		return errorNo;
	}

private:

	int gpio;

	int errorNo = 0;

};

#endif
