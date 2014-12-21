// -*- mode: c++; coding: utf-8; tab-width: 4 -*-

// Copyright (C) 2014, Oscar Acena <oscar.acena@uclm.es>
// This software is under the terms of GPLv3 or later.

#include <iostream>
#include <boost/thread/thread.hpp>
#include <boost/python.hpp>

#include "gattlib.h"

void
IOService::start() {
	boost::thread iothread(*this);
}

void
IOService::operator()() {
	GMainLoop *event_loop = g_main_loop_new(NULL, FALSE);

	g_main_loop_run(event_loop);
	g_main_loop_unref(event_loop);
}

GATTResponse::GATTResponse() :
	_status(0) {
}

void
GATTResponse::add_result(std::string data) {
    _data.append(data);
}

void
GATTResponse::notify(uint8_t status) {
	_status = status;
	_event.set();
}

bool
GATTResponse::wait(uint16_t timeout) {
    if (not _event.wait(timeout))
		return false;

    if (_status != 0) {
		std::string msg = "Characteristic value/descriptor operation failed: ";
		msg += att_ecode2str(_status);
		throw std::runtime_error(msg);
    }

    return true;
}

boost::python::list
GATTResponse::received() {
    return _data;
}

void
connect_cb(GIOChannel* channel, GError* err, gpointer userp) {
	if (err) {
		std::string msg(err->message);
		g_error_free(err);
		throw std::runtime_error(msg);
	}

	GError *gerr = NULL;
	uint16_t mtu;
	uint16_t cid;
	bt_io_get(channel, &gerr,
			  BT_IO_OPT_IMTU, &mtu,
			  BT_IO_OPT_CID, &cid,
			  BT_IO_OPT_INVALID);

	// Can't detect MTU, using default
	if (gerr) {
		g_error_free(gerr);
	 	mtu = ATT_DEFAULT_LE_MTU;
	}

	if (cid == ATT_CID)
	 	mtu = ATT_DEFAULT_LE_MTU;

	GATTRequester* request = (GATTRequester*)userp;
	request->_attrib = g_attrib_new(channel, mtu);
}

GATTRequester::GATTRequester(std::string address) :
    _address(address),
	_channel(NULL),
	_attrib(NULL) {

	GError *gerr = NULL;
	_channel = gatt_connect
		("hci0",           // 'hciX'
		 address.c_str(),  // 'mac address'
		 "public",         // 'public' '[public | random]'
		 "low",            // 'low' '[low | medium | high]'
		 0,                // 0, int
		 0,                // 0, mtu
		 connect_cb,
		 &gerr,
		 (gpointer)this);

	if (_channel == NULL) {
	 	g_error_free(gerr);
		throw std::runtime_error(gerr->message);
	}
}

GATTRequester::~GATTRequester() {
	if (_channel != NULL) {
		g_io_channel_shutdown(_channel, TRUE, NULL);
		g_io_channel_unref(_channel);
	}

	if (_attrib != NULL) {
		g_attrib_unref(_attrib);
	}
}

static void
read_by_handler_cb(guint8 status, const guint8* data, guint16 size, gpointer userp) {

	// Note: first byte is the payload size, remove it
    GATTResponse* response = (GATTResponse*)userp;
    response->add_result(std::string((const char*)data + 1, size - 1));
	response->notify(status);
}

void
GATTRequester::read_by_handle_async(uint16_t handle, GATTResponse* response) {
	check_channel();
    gatt_read_char(_attrib, handle, read_by_handler_cb, (gpointer)response);
}

boost::python::list
GATTRequester::read_by_handle(uint16_t handle) {
	GATTResponse response;
	read_by_handle_async(handle, &response);

	if (not response.wait(MAX_WAIT_FOR_PACKET))
		// FIXME: now, response is deleted, but is still registered on
		// GLIB as callback!!
		throw std::runtime_error("Device is not responding!");

	return response.received();
}

static void
read_by_uuid_cb(guint8 status, const guint8* data, guint16 size, gpointer userp) {

	struct att_data_list* list;
	list = dec_read_by_type_resp(data, size);
	if (list == NULL)
	 	return;

    GATTResponse* response = (GATTResponse*)userp;
	for (int i=0; i<list->num; i++) {
		uint8_t* item = list->data[i];

		// Remove handle addr
		item += 2;

		std::string value((const char*)item, list->len - 2);
		response->add_result(value);
	}

	att_data_list_free(list);
	response->notify(status);
}

void
GATTRequester::read_by_uuid_async(std::string uuid, GATTResponse* response) {
	uint16_t start = 0x0001;
	uint16_t end = 0xffff;
	bt_uuid_t btuuid;

	check_channel();
	if (bt_string_to_uuid(&btuuid, uuid.c_str()) < 0)
		throw std::runtime_error("Invalid UUID\n");

	gatt_read_char_by_uuid(_attrib, start, end, &btuuid, read_by_uuid_cb,
						   (gpointer)response);

}

boost::python::list
GATTRequester::read_by_uuid(std::string uuid) {
	GATTResponse response;
	read_by_uuid_async(uuid, &response);

	if (not response.wait(MAX_WAIT_FOR_PACKET))
		// FIXME: now, response is deleted, but is still registered on
		// GLIB as callback!!
		throw std::runtime_error("Device is not responding!");

	return response.received();
}

static void
write_by_handle_cb(guint8 status, const guint8* data, guint16 size, gpointer userp) {
	std::cout << "Response recived from write request, size: "
			  << size << " bytes" << std::endl;

	GATTResponse* response = (GATTResponse*)userp;
	response->notify(status);
}

void
GATTRequester::write_by_handle_async(uint16_t handle, std::string data,
									 GATTResponse* response) {
	check_channel();
	gatt_write_char(_attrib, handle, (const uint8_t*)data.data(), data.size(),
					write_by_handle_cb, (gpointer)response);
}

void
GATTRequester::write_by_handle(uint16_t handle, std::string data) {
	GATTResponse response;
	write_by_handle_async(handle, data, &response);

	if (not response.wait(MAX_WAIT_FOR_PACKET))
		// FIXME: now, response is deleted, but is still registered on
		// GLIB as callback!!
		throw std::runtime_error("Device is not responding!");
}

void
GATTRequester::check_channel() {
	time_t ts = time(NULL);
	while (_channel == NULL || _attrib == NULL) {
		usleep(1000);
		if (time(NULL) - ts > MAX_WAIT_FOR_PACKET)
			throw std::runtime_error("Channel or attrib not ready");
	}
}

using namespace boost::python;
BOOST_PYTHON_MODULE(gattlib) {

	class_<GATTRequester>("GATTRequester", init<std::string>())
		.def("read_by_handle", &GATTRequester::read_by_handle)
		.def("read_by_handle_async", &GATTRequester::read_by_handle_async)
		.def("read_by_uuid", &GATTRequester::read_by_uuid)
		.def("read_by_uuid_async", &GATTRequester::read_by_uuid_async)
		.def("write_by_handle", &GATTRequester::write_by_handle)
		.def("write_by_handle_async", &GATTRequester::write_by_handle_async)
    ;

	register_ptr_to_python<GATTResponse*>();

	class_<GATTResponse, boost::noncopyable>("GATTResponse")
		.def("wait", &GATTResponse::wait)
		.def("received", &GATTResponse::received)
	;
}
