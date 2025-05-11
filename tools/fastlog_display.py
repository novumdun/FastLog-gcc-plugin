import serial.threaded
import os

import json
import subprocess
import platform
from threading import Thread, Semaphore
from ctypes import *
import struct
import re


def frame_deal(recordItems, datas):
    global cmd_strs, cmd_strs_w, sem

    addr = str(int(datas[2]) + (int(datas[3]) << 8))
    if addr in recordItems:
        item = recordItems[addr]
        pos = 4
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
            elif 13 == arg_t:
                value = bytes(item['args_s'][idx], "utf-8")
            elif 1 == (arg_t & 0x01):
                value = int.from_bytes(
                    arg_val, byteorder='little', signed=True)
            elif 0 == (arg_t & 0x01):
                value = int.from_bytes(
                    arg_val, byteorder='little', signed=False)
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


def display_flog():
    dir = os.path.dirname(__file__)
    recordJsonFile = open(os.path.join(
        dir, '../fastlog_out/fastlog_lines.json'), mode='r')
    recordJson = recordJsonFile.read()
    recordJsonFile.close()
    recordItems = json.loads(recordJson)

    fp = open(os.path.join(
        dir, '../fastlog_out/flog'), mode='rb')
    context = fp.read()
    while (context):
        size = int(context[0]) + (int(context[1]) << 8)
        if not size:
            break
        elif (size > len(context)):
            print('Err size')
            break
        frame_deal(recordItems, context[0:size])
        context = context[size:]


if __name__ == '__main__':
    display_flog()
