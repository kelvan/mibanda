// -*- mode: c++; coding: utf-8; tab-width: 4 -*-

// Copyright (C) 2014, Oscar Acena <oscar.acena@uclm.es>
// This software is under the terms of GPLv3 or later.

#include <bluetooth/bluetooth.h>
#include <bluetooth/hci.h>
#include <bluetooth/hci_lib.h>

#include <boost/python.hpp>
#include <exception>
#include <map>

#include "devices.h"
#include "gattlib.h"

#define EIR_NAME_SHORT     0x08  /* shortened local name */
#define EIR_NAME_COMPLETE  0x09  /* complete local name */

typedef std::pair<std::string, std::string> StringPair;
typedef std::map<std::string, std::string> StringDict;

class DiscoveryService {
public:
	DiscoveryService(std::string device, int timeout) :
		_device(device),
		_device_desc(-1),
		_timeout(timeout) {

		int dev_id = hci_devid(device.c_str());
		if (dev_id < 0)
			throw std::runtime_error("Invalid device!");

		_device_desc = hci_open_dev(dev_id);
		if (_device_desc < 0)
			throw std::runtime_error("Could not open device!");
	}

	~DiscoveryService() {
		if (_device_desc != -1)
			hci_close_dev(_device_desc);
	}

	void enable_scan_mode() {
		int result;
		uint8_t scan_type = 0x01;
		uint16_t interval = htobs(0x0010);
		uint16_t window = htobs(0x0010);
		uint8_t own_type = 0x00;
		uint8_t filter_policy = 0x00;

		result = hci_le_set_scan_parameters
			(_device_desc, scan_type, interval, window,
			 own_type, filter_policy, 10000);

		if (result < 0)
			throw std::runtime_error
				("Set scan parameters failed (are you root?)");

		result = hci_le_set_scan_enable(_device_desc, 0x01, 1, 10000);
		if (result < 0)
			throw std::runtime_error("Enable scan failed");
	}

	StringDict get_advertisements() {
		struct hci_filter old_options;
	 	socklen_t slen = sizeof(old_options);
	 	if (getsockopt(_device_desc, SOL_HCI, HCI_FILTER,
					   &old_options, &slen) < 0)
			throw std::runtime_error("Could not get socket options");

		struct hci_filter new_options;
		hci_filter_clear(&new_options);
	 	hci_filter_set_ptype(HCI_EVENT_PKT, &new_options);
		hci_filter_set_event(EVT_LE_META_EVENT, &new_options);

		if (setsockopt(_device_desc, SOL_HCI, HCI_FILTER,
					   &new_options, sizeof(new_options)) < 0)
			throw std::runtime_error("Could not set socket options\n");

	 	int len;
		unsigned char buffer[HCI_MAX_EVENT_SIZE];
		struct timeval wait;
		fd_set read_set;
		wait.tv_sec = _timeout;
		int ts = time(NULL);

		StringDict retval;
	 	while(1) {
			FD_ZERO(&read_set);
			FD_SET(_device_desc, &read_set);

			int ret = select(FD_SETSIZE, &read_set, NULL, NULL, &wait);
			if (ret <= 0)
				break;

			len = read(_device_desc, buffer, sizeof(buffer));
			StringPair info = process_input(buffer, len);
			if (not retval.count(info.first) and not info.second.empty())
				retval.insert(info);

			int elapsed = time(NULL) - ts;
			if (elapsed >= _timeout)
				break;

			wait.tv_sec = _timeout - elapsed;
		}

	 	setsockopt(_device_desc, SOL_HCI, HCI_FILTER,
				   &old_options, sizeof(old_options));
		return retval;
	}

	StringPair process_input(unsigned char* buffer, int size) {
		unsigned char* ptr = buffer + HCI_EVENT_HDR_SIZE + 1;
		evt_le_meta_event* meta = (evt_le_meta_event*) ptr;
		if (meta->subevent != 0x02)
			return StringPair();

		le_advertising_info* info;
		info = (le_advertising_info*) (meta->data + 1);

		char addr[18];
		ba2str(&info->bdaddr, addr);

		std::string name = parse_name(info->data, info->length);
		return StringPair(addr, name);
	}

	std::string parse_name(uint8_t* data, size_t size) {
		size_t offset = 0;
		std::string unknown = "";

		while (offset < size) {
			uint8_t field_len = data[0];
			size_t name_len;

			if (field_len == 0 || offset + field_len > size)
				return unknown;

			switch (data[1]) {
			case EIR_NAME_SHORT:
			case EIR_NAME_COMPLETE:
				name_len = field_len - 1;
				if (name_len > size)
					return unknown;

				char name[name_len + 1];
				memcpy(name, data + 2, name_len);
				name[name_len] = 0;
				return name;
			}

			offset += field_len + 1;
			data += field_len + 1;
		}

		return unknown;
	}

	void disable_scan_mode() {
		if (_device_desc == -1)
			throw std::runtime_error("Could not disable scan, not enabled yet");

		int result = hci_le_set_scan_enable(_device_desc, 0x00, 1, 10000);
		if (result < 0)
			throw std::runtime_error("Disable scan failed");
	}

	BandDeviceList discover() {
		enable_scan_mode();
		StringDict devices = get_advertisements();
		disable_scan_mode();

		BandDeviceList retval;
		for (StringDict::iterator i=devices.begin(); i!=devices.end(); i++) {
			BandDevicePtr dev = BandDevicePtr(new BandDevice(i->first, i->second));
			retval.push_back(dev);
		}

		return retval;
	}

private:
	std::string _device;
	int _device_desc;
	int _timeout;
};

using namespace boost::python;
BOOST_PYTHON_MODULE(services) {

	class_<DiscoveryService>("DiscoveryService",
							 init<std::string, int>())
		.def("discover", &DiscoveryService::discover)
	;

	class_<IOService>("IOService")
		.def("start", &IOService::start)
	;
}
