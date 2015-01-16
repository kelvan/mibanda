#!/usr/bin/python -u
# -*- mode: python; coding: utf-8 -*-

import sys
import pyshark

GREEN  = '\033[32;5m'
BLUE   = '\033[34;5m'
RED    = '\033[31;5m'
YELLOW = '\033[33;1m'
ENDC   = '\033[0m'

def g(msg): return GREEN + msg + ENDC
def r(msg): return RED + msg + ENDC
def b(msg): return BLUE + msg + ENDC
def y(msg): return YELLOW + msg + ENDC


CMD_CREATE_CONNECTION = "0x200d"
CMD_DISCONNECT = "0x0406"

OPCODE_ERROR_RESPONSE = "0x01"
OPCODE_FIND_INFORMATION_REQUEST = "0x04"
OPCODE_FIND_INFORMATION_RESPONSE = "0x05"
OPCODE_READ_BY_TYPE_REQUEST = "0x08"
OPCODE_READ_BY_TYPE_RESPONSE = "0x09"
OPCODE_READ_REQUEST = "0x0a"
OPCODE_READ_RESPONSE = "0x0b"
OPCODE_READ_BY_GROUP_TYPE_REQUEST = "0x10"
OPCODE_READ_BY_GROUP_TYPE_RESPONSE = "0x11"
OPCODE_WRITE_REQUEST = "0x12"
OPCODE_WRITE_RESPONSE = "0x13"
OPCODE_HANDLE_VALUE_NOTIFICATION = "0x1b"

HANDLE_DEVICE_INFO = "0x0012"
HANDLE_NOTIFICATION = "0x0016"
HANDLE_CCC_USER_INFO = "0x0017"
HANDLE_USER_INFO = "0x0019"
HANDLE_CONTROL_POINT = "0x001b"
HANDLE_REALTIME_STEPS = "0x001d"
HANDLE_CCC_ACTIVITY_DATA = "0x001e"
HANDLE_ACTIVITY_DATA = "0x0020"
HANDLE_CCC_FIRMWARE_DATA = "0x0021"
HANDLE_LE_PARAMS = "0x0025"
HANDLE_DATE_TIME = "0x0027"
HANDLE_STATISTICS = "0x0029"
HANDLE_BATTERY = "0x002b"
HANDLE_CCC_TEST = "0x002c"
HANDLE_CCC_PAIR = "0x0031"


def handle_name(handle):
    names = dict([(v, k) for k, v in globals().items() if k.startswith("HANDLE_")])
    return names.get(handle, handle)


if __name__ == '__main__':
    if len(sys.argv) < 2:
        print "Usage: {} <capture_file>".format(sys.argv[0])
        sys.exit(-1)

    cap = pyshark.FileCapture(sys.argv[1])

    for i, pkt in enumerate(cap):
        print ("\r" * 4) + "{: 4}".format(i),

        cmd = getattr(pkt, "bthci_cmd", None)
        if cmd is not None:
            if cmd.opcode == CMD_CREATE_CONNECTION:
                print y("#### Create Connection ######")
                continue

            if cmd.opcode == CMD_DISCONNECT:
                print y("#### End Connection ######")
                continue

            continue

        evt = getattr(pkt, "bthci_evt", None)
        if evt is not None:
            # print "EVT:", evt.get_field("code").showname
            continue

        att = getattr(pkt, "btatt", None)
        if att is not None:
            if att.opcode == OPCODE_ERROR_RESPONSE:
                continue

            if att.opcode == OPCODE_FIND_INFORMATION_REQUEST:
                continue

            if att.opcode == OPCODE_FIND_INFORMATION_RESPONSE:
                continue

            if att.opcode == OPCODE_READ_BY_TYPE_REQUEST:
                continue

            if att.opcode == OPCODE_READ_BY_TYPE_RESPONSE:
                continue

            if att.opcode == OPCODE_READ_REQUEST:
                name = handle_name(att.handle)
                print g("r {}".format(name))
                continue

            if att.opcode == OPCODE_READ_RESPONSE:
                print "  -> {}".format(att.value)
                continue

            if att.opcode == OPCODE_READ_BY_GROUP_TYPE_REQUEST:
                continue

            if att.opcode == OPCODE_READ_BY_GROUP_TYPE_RESPONSE:
                continue

            if att.opcode == OPCODE_WRITE_REQUEST:
                name = handle_name(att.handle)
                space = " " * (24 - len(name))
                print r("w {} -> {}".format(name + space, att.value))
                continue

            if att.opcode == OPCODE_WRITE_RESPONSE:
                continue

            if att.opcode == OPCODE_HANDLE_VALUE_NOTIFICATION:
                name = handle_name(att.handle)
                space = " " * (24 - len(name))
                print b("n {} -> {}".format(name + space, att.value))
                if name == att.handle:
                    break
                continue

            print "Unknown operation code"
            break

        print "Unknown packet type"
        break
