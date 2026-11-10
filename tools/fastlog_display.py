#!python3
import serial.threaded
import os

import json
import subprocess
import platform
from threading import Thread, Semaphore
from ctypes import *
import struct
import re
import bisect
from collections import OrderedDict


def find_largest_smaller(sorted_list, target):
    # bisect_left finds the index of the first element >= target
    idx = bisect.bisect_left(sorted_list, target)

    # If idx is 0, every element in the list is >= target
    if idx == 0:
        return None

    # The item right before the insertion point is the largest one < target
    return sorted_list[idx - 1]


def get_base(addr, base_list):
    addr = (addr // 0x4000) * 0x4000
    if addr in base_list:
        return addr
    else:
        ret = find_largest_smaller(base_list, addr)
        if ret:
            return ret
        else:
            exit()


def frame_deal(recordItems, datas, base_dict, base_list):
    global cmd_strs, cmd_strs_w, sem

    addr = int(datas[2]) + (int(datas[3]) << 8) + \
        (int(datas[4]) << 16) + (int(datas[5]) << 24)
    base = get_base(addr, base_list)
    addr = addr - base
    addr = str(addr)
    if addr in recordItems:
        item = recordItems[addr]
        pos = 2 + 4
        # item_str = item_str.replace("%s", "%p")
        args_array = [0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0]
        for idx in range(0, item['argc']):
            arg_size = item['args'][idx]
            arg_t = item['args_t'][idx]
            arg_val = datas[pos: (pos + arg_size)]
            if 9 == arg_t:
                value = c_double(struct.unpack('f', arg_val)[0])
            elif 10 == arg_t:
                value = c_double(struct.unpack('d', arg_val)[0])
            elif 12 == arg_t:
                arg_size = 0
                while (int(datas[pos+arg_size]) != 0):
                    arg_size += 1
                arg_size += 1
                value = datas[pos:pos + arg_size]
            elif arg_t in [13, 14, 15, 16, 17, 18, 19, 20, 21]:
                value = bytes(item['args_s'][idx], "utf-8")
            elif 1 == (arg_t & 0x01):
                value = int.from_bytes(
                    arg_val, byteorder='little', signed=True)
            elif 0 == (arg_t & 0x01):
                value = int.from_bytes(
                    arg_val, byteorder='little', signed=False)
            else:
                print(arg_t)
            pos += arg_size
            args_array[idx] = value

        str_array = create_string_buffer(1000)  #

        sys = platform.system()
        if sys == "Windows":
            msvcrt = cdll.msvcrt
            msvcrt.sprintf(str_array, bytes(args_array[0] + '\r\n', encoding="utf-8"),
                           args_array[1],
                           args_array[2],
                           args_array[3],
                           args_array[4],
                           args_array[5],
                           args_array[6],
                           args_array[7],
                           args_array[8],
                           args_array[9])
        else:
            cdll.LoadLibrary("libc.so.6")
            libc = CDLL("libc.so.6")
            libc.sprintf(str_array, args_array[0],
                         args_array[1],
                         args_array[2],
                         args_array[3],
                         args_array[4],
                         args_array[5],
                         args_array[6],
                         args_array[7],
                         args_array[8],
                         args_array[9])
        print(str_array.value.decode('utf-8'))


def display_flog(projPath, projName, logName):
    projIdxRecPath = os.path.join(
        projPath, f'fastlog_out/sys/{projName}_lines.json')
    recordJsonFile = open(projIdxRecPath, mode='r')
    recordJson = recordJsonFile.read()
    recordJsonFile.close()
    recordItems = json.loads(recordJson)

    globalIdxRecPath = os.path.join(
        projPath, f'fastlog_out/{logName}_idx.json')
    globalRecordJsonFile = open(globalIdxRecPath, mode='r')
    globalRecordJson = globalRecordJsonFile.read()
    globalRecordJsonFile.close()
    globalRecordItems = json.loads(globalRecordJson)

    base_dict = {}
    base_list = []
    for name, processes in globalRecordItems['clients'].items():
        for pid, ranges in processes.items():
            for range in ranges:
                start = range[0]
                end = range[1]
                base_dict[start] = [name, pid, end]
                base_list.append(start)
    base_list = sorted(base_list)

    flog_path = os.path.join(projPath, f'fastlog_out/{logName}')
    fp = open(flog_path, mode='rb')
    context = fp.read()
    while (context):
        size = int(context[0]) + (int(context[1]) << 8)
        if not size:
            break
        elif (size > len(context)):
            print('Err size')
            break
        frame_deal(recordItems, context[0:size], base_dict, base_list)
        context = context[size:]


if __name__ == '__main__':
    dir = os.path.dirname(__file__)
    projPath = os.path.join(dir, '..')
    projName = "test"
    logName = "hello_world"
    display_flog(projPath, projName, logName)
