// -*- mode: c++; coding: utf-8; tab-width: 4 -*-

// Copyright (C) 2014, Oscar Acena <oscar.acena@uclm.es>
// This software is under the terms of GPLv3 or later.

#ifndef _MIBANDA_GATTLIB_H_
#define _MIBANDA_GATTLIB_H_

#define MAX_WAIT_FOR_PACKET 15 // seconds

#include <boost/python/list.hpp>
#include <string>
#include <stdint.h>
#include <glib.h>

extern "C" {
#include "lib/uuid.h"
#include "attrib/att.h"
#include "attrib/gattrib.h"
#include "attrib/gatt.h"
#include "attrib/utils.h"
}

#include "event.hpp"

class IOService {
public:
	void start();
	void operator()();
};

class GATTResponse {
public:
	GATTResponse();

	void add_result(std::string data);
	boost::python::list received();
	bool wait(uint16_t timeout);
	void notify(uint8_t status);

private:
	uint8_t _status;
	boost::python::list _data;
	Event _event;
};

void connect_cb(GIOChannel* channel, GError* err, gpointer user_data);

class GATTRequester {
public:
	GATTRequester(std::string address);
	~GATTRequester();

	void read_by_handle_async(uint16_t handle, GATTResponse* response);
	boost::python::list read_by_handle(uint16_t handle);
	void read_by_uuid_async(std::string uuid, GATTResponse* response);
	boost::python::list read_by_uuid(std::string uuid);
	void write_by_handle_async(uint16_t handle, std::string data, GATTResponse* response);
	void write_by_handle(uint16_t handle, std::string data);

	friend void connect_cb(GIOChannel*, GError*, gpointer);

private:
	void check_channel();

	std::string _address;
	GIOChannel* _channel;
	GAttrib* _attrib;
};

#endif // _MIBANDA_GATTLIB_H_
