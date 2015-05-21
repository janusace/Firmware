/****************************************************************************
 *
 *   Copyright (c) 2015 PX4 Development Team. All rights reserved.
 *   Author: Mohammed Kabir <mhkabir98@gmail.com>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 * 3. Neither the name PX4 nor the names of its contributors may be
 *    used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 ****************************************************************************/

/**
 * @file camera_trigger.cpp
 *
 * External camera-IMU synchronisation and triggering via FMU auxillary pins.
 *
 * @author Mohammed Kabir <mhkabir98@gmail.com>
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <stdbool.h>
#include <nuttx/clock.h>
#include <nuttx/arch.h>
#include <systemlib/systemlib.h>
#include <systemlib/err.h>
#include <systemlib/param/param.h>
#include <uORB/uORB.h>
#include <uORB/topics/camera_trigger.h>
#include <uORB/topics/sensor_combined.h>
#include <uORB/topics/vehicle_command.h>
#include <poll.h>
#include <drivers/drv_gpio.h>
#include <drivers/drv_hrt.h>
#include <mavlink/mavlink_log.h>

extern "C" __EXPORT int camera_trigger_main(int argc, char *argv[]);

class CameraTrigger
{
public:
	/**
	 * Constructor
	 */
	CameraTrigger();

	/**
	 * Destructor, also kills task.
	 */
	~CameraTrigger();

	/**
	 * Start the task.
	 */
	void		start();

	/**
	 * Stop the task.
	 */
	void		stop();

	/**
	 * Display info.
	 */
	void		info();
		
	int 		pin;
	
private:
	
	struct hrt_call		_firecall;
	
	int 			_gpio_fd;
	
	bool	 		_trigger_enabled;

	/**
	 * Fires trigger
	 */
	static void	engage(void *arg);


};

namespace camera_trigger
{

CameraTrigger	*g_camera_trigger;
}

CameraTrigger::CameraTrigger() :
	pin(1),
	_gpio_fd(-1),
	_trigger_enabled(true),
	_trigger{}
{
	memset(&_firecall, 0, sizeof(_firecall));
}

CameraTrigger::~CameraTrigger()
{
	camera_trigger::g_camera_trigger = nullptr;
}

void
CameraTrigger::start()
{

	_gpio_fd = open(PX4FMU_DEVICE_PATH, 0);

	if (_gpio_fd < 0) {
		
		warnx("GPIO device open fail");	
		stop();
	}
	else
	{
		warnx("GPIO device opened");
	}


	ioctl(_gpio_fd, GPIO_SET_OUTPUT, pin);

	hrt_call_every(&_firecall, 0, 2000000, (hrt_callout)&CameraTrigger::engage, this);
}

void
CameraTrigger::stop()
{
	hrt_cancel(&_firecall);
	delete camera_trigger::g_camera_trigger;
}

void
CameraTrigger::engage(void *arg)
{

	CameraTrigger *trig = reinterpret_cast<CameraTrigger *>(arg);
	
	if(trig->_trigger_enabled){
		trig->_trigger_enabled = false; 
		ioctl(trig->_gpio_fd, GPIO_CLEAR, trig->pin);    
	}
	else if(!trig->_trigger_enabled){
		trig->_trigger_enabled = true; 
		ioctl(trig->_gpio_fd, GPIO_SET, trig->pin);    
	}
}

void
CameraTrigger::info()
{
	warnx("Trigger state : %s", _trigger_enabled ? "enabled" : "disabled");
}

static void usage()
{
	errx(1, "usage: camera_trigger {start|stop|info} [-p <n>]\n"
		     "\t-p <n>\tUse specified AUX OUT pin number (default: 1)"
		    );
}

int camera_trigger_main(int argc, char *argv[])
{
	if (argc < 2) {
		usage();
	}

	if (!strcmp(argv[1], "start")) {

		if (camera_trigger::g_camera_trigger != nullptr) {
			errx(0, "already running");
		}
			
		camera_trigger::g_camera_trigger = new CameraTrigger;

		if (camera_trigger::g_camera_trigger == nullptr) {
			errx(1, "alloc failed");
		}
		
		if (argc > 3) {
	
			camera_trigger::g_camera_trigger->pin = (int)argv[3];
			if (atoi(argv[3]) > 0 && atoi(argv[3]) < 6) {	
				warnx("starting trigger on pin : %li ", atoi(argv[3]));	
				camera_trigger::g_camera_trigger->pin = atoi(argv[3]);
			}
			else
			{
				usage(); 
			}
		}
		camera_trigger::g_camera_trigger->start();

		return 0;
	}

	if (camera_trigger::g_camera_trigger == nullptr) {
		errx(1, "not running");
	}

	else if (!strcmp(argv[1], "stop")) {
		camera_trigger::g_camera_trigger->stop(); 

	}
	else if (!strcmp(argv[1], "info")) {
		camera_trigger::g_camera_trigger->info(); 

	} else {
		usage();
	}

	return 0;
}

