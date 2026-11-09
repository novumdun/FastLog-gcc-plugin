#!python3
import json
import os
import re

def make_records(log_inc_fd, projPath):
    print_items = {}
    while (1):
        line = log_inc_fd.readline()
        if (not line):
            break
        pattern = re.compile(
            r' ?unsigned short FASTLOG_MACRO__[^=]+_(\d+)_log_idx = (\d+); LOG_IDX_FAKE\(([^\)]+)\);')
        result = pattern.findall(line)
        if (result):
            print(result[0])
            line_num = result[0][0]
            line_rec_idx = result[0][1]
            line_rec = result[0][2]

            item = line_rec.split(",")
            args = [int(item[i]) for i in range(0x12, 0x22)]
            args_t = [int(item[i]) for i in range(2, 0x12)]
            args_s = [item[i][1:-1] for i in range(0x22, 0x32)]
            if args_s[1][0] == '"':
                _str = item[1][1:-1]
            print_items[line_rec_idx] = {'line': line_num,
                                         'argc': int(item[1]), 'args': args, 'args_t': args_t, 'args_s': args_s, 'file': item[0][1:-1]}

    fastlog_lines_file = os.path.join(
        projPath, 'fastlog_out/sys/fastlog_lines.json')
    fastlog_lines_f = open(fastlog_lines_file, mode='w')
    print_json = json.dumps(print_items)
    fastlog_lines_f.write(print_json)
    fastlog_lines_f.close()
