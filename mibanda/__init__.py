# -*- mode: python; coding: utf-8 -*-

# Copyright (C) 2014, Oscar Acena <oscar.acena@uclm.es>
# This software is under the terms of GPLv3 or later.

from datetime import datetime
import struct
import gattlib


UUID_DEVICE_INFO = "0000ff01-0000-1000-8000-00805f9b34fb"
UUID_DEVICE_NAME = "0000ff02-0000-1000-8000-00805f9b34fb"
UUID_USER_INFO   = "0000ff04-0000-1000-8000-00805f9b34fb"
UUID_STEPS       = "0000ff06-0000-1000-8000-00805f9b34fb"
UUID_LE_PARAMS   = "0000ff09-0000-1000-8000-00805f9b34fb"
UUID_BATTERY     = "0000ff0c-0000-1000-8000-00805f9b34fb"

HANDLE_TEST          = 0x2e
HANDLE_USER_INFO     = 0x19
HANDLE_CONTROL_POINT = 0x1b
HANDLE_PAIR          = 0x33


class BatteryInfo(object):
    def __init__(self, data):
        fields = map(ord, data)
        self.level = fields[0]
        self.last_charged = datetime(
            fields[1] + 2000,
            fields[2] + 1,
            *fields[3:6])
        self.charge_counter = fields[7] + (fields[8] << 8)

        status_names = {1: 'low', 2: 'medium', 3: 'full', 4: 'not charging'}
        self.status = status_names.get(fields[9], "unknown")


class LEParams(object):
    def __init__(self, data):
        fields = map(ord, data)
        self.minimum_connection_interval = fields[0] + (fields[1] << 8)
        self.maximum_connection_interval = fields[2] + (fields[3] << 8)
        self.latency = fields[4] + (fields[5] << 8)
        self.timeout = fields[6] + (fields[7] << 8)
        self.connection_interval = fields[8] + (fields[9] << 8)
        self.advertisement_interval = fields[10] + (fields[11] << 8)


class DeviceInfo(object):
    def __init__(self, data):
        fields = map(ord, data)
        self.firmware_version = "{}.{}.{}.{}".format(*reversed(fields[-4:]))


class BandDevice(object):
    def __init__(self, address, name):
        self.address = address
        self.name = name

        self.requester = gattlib.GATTRequester(address, False)

    def connect(self):
        self.requester.connect(True)

    def getAddress(self):
        return self.address

    def getName(self, cached=True):
        if cached:
            return self.name

        data = self.requester.read_by_uuid(UUID_DEVICE_NAME)
        return data[0]

    def getBatteryInfo(self):
        data = self.requester.read_by_uuid(UUID_BATTERY)
        return BatteryInfo(data[0])

    def getDeviceInfo(self):
        data = self.requester.read_by_uuid(UUID_DEVICE_INFO)[0]
        return DeviceInfo(data)

    def getSteps(self):
        data = self.requester.read_by_uuid(UUID_STEPS)[0]
        return ord(data[0]) + (ord(data[1]) << 8)

    def getLEParams(self):
        data = self.requester.read_by_uuid(UUID_LE_PARAMS)[0]
        return LEParams(data)

    def selfTest(self):
        self.requester.write_by_handle(HANDLE_TEST, str(bytearray([2])))

    def pair(self):
        # raise NotImplementedError(
        # "Sorry, this is not yet available, I'm working on it :)")
        self.requester.write_by_handle(HANDLE_PAIR, str(bytearray([2])))

    def setUserInfo(self, uid, gender, age, height, weight, type_, alias):
        seq = bytearray(20)

        seq[:4] = [ord(i) for i in struct.pack("<I", uid)]
        seq[4] = bool(gender)
        seq[5] = age & 0xff
        seq[6] = height & 0xff
        seq[7] = weight & 0xff
        seq[8] = type_ & 0xff

        assert len(alias) == 10, "Alias size must be 10"
        seq[9:19] = alias

        addr = self.getAddress()
        crc = self._getCRC8(seq[:19])
        crc = (crc ^ int(addr[-2:], 16)) & 0xff
        seq[19] = crc

        self.requester.write_by_handle(HANDLE_USER_INFO, str(seq))

    def flashLeds(self, r, g, b):
        """ levels range from 1 (min bright) to 6 (max bright) """
        self.requester.write_by_handle(
            HANDLE_CONTROL_POINT, str(bytearray([0x0e, r, g, b, 0x01])))

    def locate(self):
        self.requester.write_by_handle(
            HANDLE_CONTROL_POINT, str(bytearray([0x08, 0x00])))

    def _getCRC8(self, data):
        crc = 0
        for i in range(0, len(data)):
            crc = crc ^ (data[i] & 0xff)

            for j in range(8):
                if crc & 0x01:
                    crc = (crc >> 1) ^ 0x8c
                else:
                    crc >>= 1
        return crc


class DiscoveryService(object):
    def __init__(self, device="hci0"):
        self.service = gattlib.DiscoveryService(device)

    def discover(self, timeout=3):
        bands = []
        for addr, name in self.service.discover(timeout).items():
            band = BandDevice(addr, name)
            bands.append(band)
        return bands
