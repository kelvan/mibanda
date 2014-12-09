# -*- mode: python; coding: utf-8 -*-

import os
import re
from commodity.os_ import SubProcess


BLE_UUID_SUFFIX = "0000-1000-8000-00805f9b34fb"
BLE_SERVICE_NAMES = {
    "00001800": "Generic Access",
    "00001801": "Generic Attribute",
}

BLE_FORMAT_TYPES = {
    "00002800": "GATT Service",
    "00002803": "GATT Characteristic",
}


def gatttool(addr, commands):
    cmd = "gatttool -b {0} {1}".format(addr, commands)
    p = SubProcess(cmd)
    p.wait()

    return p.stdout.read()


class BLEService(object):
    def __init__(self, uuid, start, end):
        self.uuid = uuid
        self.start = start
        self.end = end

        self.name = "unknown"
        if uuid[9:] == BLE_UUID_SUFFIX:
            preffix = uuid[:8]
            self.name = BLE_SERVICE_NAMES.get(
                preffix, "unknown")

        self.discover_chars()

    def discover_chars(self):
        start = int(self.start, 16)
        for i in range(start, start + self.count):
            handle = "0x{:x}".format(i)
            print handle

    @property
    def count(self):
        start = int(self.start, 16)
        end = int(self.end, 16)
        return end - start

    def __repr__(self):
        return ("<BLEService, uuid: {},\n"
                " - name: {},\n"
                " - starting handle: {},\n"
                " - ending handle: {},\n"
                " - number of chars: {}>"
                .format(self.uuid, self.name,
                        self.start, self.end,
                        self.count))


class BLEDevice(object):
    def __init__(self, addr):
        self.addr = addr
        self.services = []

    def discover_services(self):
        self.services = []
        output = gatttool(self.addr, "--primary")
        for line in output.splitlines():
            s = self.parse_service(line)
            self.services.append(s)
            break

    def parse_service(self, info):
        e = ("attr handle = (?P<start_handle>.*), "
             "end grp handle = (?P<end_handle>.*) "
             "uuid: (?P<service_uuid>.*)")
        e = re.compile(e)
        match = e.match(info)

        return BLEService(
            match.group('service_uuid'),
            match.group('start_handle'),
            match.group('end_handle'))


class BandDevice(object):
    def __init__(self, name, addr):
        self.name = name
        self.addr = addr

        self.device = BLEDevice(self.addr)
        self.device.discover_services()

    @property
    def identifier(self):
        return "unknown"

    def __repr__(self):
        return "<BandDevice, addr: {}, name: '{}'>".format(
            self.addr, self.name)


class DiscoveryService(object):
    def discover(self, timeout=3):
        output = self.get_discover_results(timeout)
        devices = self.get_unique_devices(output)
        return self.create_band_objects(devices)

    def get_discover_results(self, timeout):
        pwd = os.path.abspath(os.path.dirname(__file__))
        cmd = os.path.join(pwd, "lescan.sh")

        p = SubProcess(
            "{0} {1}".format(cmd, timeout),
            shell=True)
        p.wait()

        return p.stdout.read()

    def get_unique_devices(self, output):
        retval = {}

        for line in output.splitlines():
            addr = line[:17]
            name = line[17:].strip()
            if name == "(unknown)":
                name = None
            retval[addr] = name or retval.get(addr)

        return retval

    def create_band_objects(self, devices):
        retval = []
        for addr, name in devices.items():
            retval.append(BandDevice(name, addr))
        return retval
