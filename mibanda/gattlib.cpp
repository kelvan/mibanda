// -*- mode: c++; coding: utf-8; tab-width: 4 -*-

#include <iostream>
#include <boost/thread/thread.hpp>

#include "gattlib.h"

extern "C" {
#include "lib/uuid.h"
#include "attrib/att.h"
#include "attrib/gattrib.h"
#include "attrib/gatt.h"
#include "attrib/utils.h"
}

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

void
GATTResponse::notify(uint8_t status, std::string data) {
    _status = status;
    _data = data;
    _event.set();
}

bool
GATTResponse::wait(uint16_t timeout) {
    if (not _event.wait(timeout))
		return false;

    if (_status != 0) {
		std::string msg = "Characteristic value/descriptor read failed: ";
		msg += att_ecode2str(_status);
		throw std::runtime_error(msg);
    }

    return true;
}

std::string
GATTResponse::received() {
    return _data;
}

static void
connect_cb(GIOChannel *io, GError *err, gpointer user_data) {
	if (err) {
		throw std::runtime_error(err->message);
	}
}

GATTRequester::GATTRequester(std::string address) :
    _address(address) {

	GError *gerr = NULL;
	_channel = gatt_connect
		("hci0",           // 'hciX'
		 address.c_str(),  // 'mac address'
		 "public",         // 'public' '[public | random]'
		 "low",            // 'low' '[low | medium | high]'
		 0,                // 0, int
		 0,                // 0, mtu
		 connect_cb,
		 &gerr);

	if (_channel == NULL) {
	 	g_error_free(gerr);
		throw std::runtime_error(gerr->message);
	}
}

GATTRequester::~GATTRequester() {
	if (_channel == NULL)
		return;

	GError *gerr = NULL;
	g_io_channel_shutdown(_channel, TRUE, &gerr);
	g_io_channel_unref(_channel);
}

static void
_read_by_handler_cb(guint8 status, const guint8* data,
					guint16 size, gpointer userp) {
    GATTResponse* response = (GATTResponse*)userp;
    response->notify(status, std::string((const char*)data, size));
}

void
GATTRequester::read_by_handler(uint16_t handle, GATTResponse* response) {

	GError* gerr = NULL;

	uint16_t mtu;
	uint16_t cid;
	bt_io_get(_channel, &gerr,
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

	GAttrib* attrib = g_attrib_new(_channel, mtu);
	gatt_read_char(attrib, handle, _read_by_handler_cb, (gpointer)response);
}
