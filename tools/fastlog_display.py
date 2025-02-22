import serial.threaded
import os

import json
import subprocess
import platform
from threading import Thread, Semaphore
from ctypes import *
import struct


def frame_deal(recordItems, datas):
    global cmd_strs, cmd_strs_w, sem

    addr = str(int(datas[2]) + (int(datas[3]) << 8))
    if addr in recordItems:
        item = recordItems[addr]

        args_p = 4
        args_c = 0
        args_array = [0, 0, 0, 0, 0, 0, 0, 0, 0, 0]
        for idx, arg_len in enumerate(item['args']):
            if arg_len:
                arg_t = item['args_t'][idx]
                arg = datas[args_p: (args_p + arg_len)]
                if 7 == arg_t:
                    value = c_double(struct.unpack('f', arg)[0])
                elif 8 == arg_t:
                    value = c_double(struct.unpack('d', arg)[0])
                elif 1 == (arg_t & 0x01):
                    value = int.from_bytes(
                        arg, byteorder='little', signed=True)
                elif 0 == (arg_t & 0x01):
                    value = int.from_bytes(
                        arg, byteorder='little', signed=False)

                args_array[args_c] = value
                args_c += 1
                args_p += arg_len
        # print(args_array)

        str_array = create_string_buffer(1000)  #

        sys = platform.system()
        if sys == "Windows":
            msvcrt = cdll.msvcrt
            msvcrt.sprintf(str_array, bytes(item['str'] + '\r\n', encoding="utf-8"), args_array[0],
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
            libc.sprintf(str_array, bytes(item['str'] + '\r\n', encoding="utf-8"), args_array[0],
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
        if size > len(context):
            print('Err size')
            break
        frame_deal(recordItems, context[0:size])
        context = context[size:]

if __name__ == '__main__':
    display_flog()