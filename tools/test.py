from fastlog_display import display_flog
import subprocess
import os
import time
import re
import json


def shell_cmd(cmd: str):
    print(cmd)
    ret = subprocess.check_output(cmd, shell=True)
    print(ret.decode("utf-8"))
    return ret.decode("utf-8")


def mkdirs(path):
    if True != os.path.exists(path):
        os.makedirs(path)  # makedirs


def rmfile(path):
    if os.path.exists(path):
        os.remove(path)


dir = os.path.dirname(__file__)
projPath = os.path.join(dir, '..')
toolsPath = os.path.join(projPath, 'fastlog_out/tools')
mkdirs(toolsPath)
rmfile(os.path.join(projPath, 'proj/log_inc.c'))
rmfile(os.path.join(projPath, 'proj/log_macro.h'))
idxCnt = 0

srcPath = os.path.join(dir, 'gcc-plugin.cc')
pluginPath = os.path.join(toolsPath, 'plugin.so')
cmd = 'g++ ' + \
    f'{srcPath} -I`gcc -print-file-name=plugin`/include -fPIC -shared -o {pluginPath}'
shell_cmd(cmd)

srcPath = os.path.join(dir, 'file-id.c')
fileIdProcPath = os.path.join(toolsPath, 'file-id')
cmd = 'gcc ' + f'{srcPath} -o {fileIdProcPath}'
shell_cmd(cmd)

srcPath = os.path.join(projPath, 'proj/test.c')
cmd = f'{fileIdProcPath} {os.path.normpath(srcPath)}'
fileId = shell_cmd(cmd)

includes = f'-I{projPath}/inc -I{projPath}/fastlog_out/proj'
cmd = f'gcc -fplugin={pluginPath} -c {srcPath} {includes} -DCOMPILE_PASS=0 -D__FILE_PATH__=\'"{
    os.path.normpath(srcPath)}"\' -D__PROJ_PATH__=\'"{os.path.normpath(projPath)}"\' -D__FILE_ID__={fileId}'
shell_cmd(cmd)

incPath = os.path.join(projPath, 'fastlog_out', 'proj/test.c.inc')
incFile = open(incPath)
incLines = incFile.readlines()
incLine = incLines[-1]
idxCnt += int(incLine.split()[2])

srcPath = os.path.join(projPath, 'proj/test-copy.c')
cmd = f'{fileIdProcPath} {os.path.normpath(srcPath)}'
fileId_01 = shell_cmd(cmd)

includes = f'-I{projPath}/inc -I{projPath}/fastlog_out/proj'
cmd = f'gcc -fplugin={pluginPath} -c {srcPath} {includes} -DCOMPILE_PASS=0 -D__FILE_PATH__=\'"{
    os.path.normpath(srcPath)}"\' -D__PROJ_PATH__=\'"{os.path.normpath(projPath)}"\' -D__FILE_ID__={fileId_01}'
shell_cmd(cmd)

incPath = os.path.join(projPath, 'fastlog_out', 'proj/test-copy.c.inc')
incFile = open(incPath)
incLines = incFile.readlines()
incLine = incLines[-1]
idxCnt += int(incLine.split()[2])

srcPath = os.path.join(projPath, 'src/fastlog.c')
cmd = 'gcc -c -g ' + srcPath + ' ' + includes + ' -DCOMPILE_PASS=1'
shell_cmd(cmd)

idxCnt = (((idxCnt+1)//64)+1)*64
print(idxCnt)
srcPath = os.path.join(projPath, 'proj/fastlog_test.c')
cmd = 'gcc -c -g ' + srcPath + ' ' + includes + \
    f' -DCOMPILE_PASS=1  -DFASTLOG_MODULE_SIZE_test={idxCnt}'
shell_cmd(cmd)

srcPath = os.path.join(projPath, 'proj/test.c')
cmd = 'gcc -c -g ' + srcPath + ' ' + includes + \
    ' -DCOMPILE_PASS=1 -D__FILE_ID__=' + fileId
shell_cmd(cmd)

srcPath = os.path.join(projPath, 'proj/test-copy.c')
cmd = 'gcc -c -g ' + srcPath + ' ' + includes + \
    ' -DCOMPILE_PASS=1 -D__FILE_ID__=' + fileId_01
shell_cmd(cmd)

srcPath = os.path.join(projPath, 'proj/log_inc.c')
cmd = 'gcc -E -c ' + srcPath + ' > ./fastlog_out/log_inc.c.i'
shell_cmd(cmd)

log_inc_fd = open('./fastlog_out/log_inc.c.i')
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
        args = [int(item[i]) for i in range(16, 26)]
        args_t = [int(item[i]) for i in range(4, 14)]
        if item[1][0] == '"':
            _str = item[1][1:-1]
        else:
            _str = int(item[1])
        print_items[line_rec_idx] = {'line': line_num,
                                     'str': _str, 'args': args, 'args_t': args_t, 'file': item[0][1:-1]}

fastlog_lines_file = os.path.join(
    projPath, 'fastlog_out', "fastlog_lines.json")
fastlog_lines_f = open(fastlog_lines_file, mode='w')
print_json = json.dumps(print_items)
fastlog_lines_f.write(print_json)
fastlog_lines_f.close()

srcPath = os.path.join(projPath, 'proj/log_inc.c')
cmd = 'gcc -c -g ' + srcPath + ' -D__LOG_IDX_FAKE'
shell_cmd(cmd)

cmd = 'gcc ' + ' -o Test ' + ' test.o test-copy.o fastlog_test.o fastlog.o log_inc.o'
shell_cmd(cmd)

cmd = './Test'
shell_cmd(cmd)

time.sleep(1)

display_flog()
