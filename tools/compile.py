#!python3
from fastlog_display import display_flog
from fastlog import make_records
import subprocess
import os
import time


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


fileIdProcPath = None
pluginPath = None


def make_tools(projPath):
    global fileIdProcPath
    global pluginPath

    mkdirs(os.path.join(projPath, 'fastlog_out/tools'))
    rmfile(os.path.join(projPath, 'fastlog_out/sys/log_inc.c'))
    rmfile(os.path.join(projPath, 'fastlog_out/sys/log_macro.h'))

    srcPath = os.path.join(projPath, 'tools/gcc_plugin.cc')
    pluginPath = os.path.join(projPath, 'fastlog_out/tools/plugin.so')
    cmd = 'g++ ' + \
        f'{srcPath} -I`gcc -print-file-name=plugin`/include -fPIC -shared -o {pluginPath}'
    shell_cmd(cmd)

    # file_id converts the path of a file to a string which identify the file and could be
    # transed into C by compiler.
    srcPath = os.path.join(projPath, 'tools/file_id.c')
    fileIdProcPath = os.path.join(projPath, 'fastlog_out/tools/file_id')
    cmd = 'gcc ' + f'{srcPath} -o {fileIdProcPath}'
    shell_cmd(cmd)


def prebuild(projPath, file_path, includes):
    global fileIdProcPath
    global pluginPath

    srcPath = os.path.join(projPath, file_path)
    cmd = f'{fileIdProcPath} {os.path.normpath(srcPath)}'
    fileId = shell_cmd(cmd)

    cmd = f'gcc -fplugin={pluginPath} -c {srcPath} {includes} -DCOMPILE_PASS=0 -D__FILE_PATH__=\'"{
        os.path.normpath(srcPath)}"\' -D__PROJ_PATH__=\'"{os.path.normpath(projPath)}"\' -D__FILE_ID__={fileId}'
    shell_cmd(cmd)

    # incPath = os.path.join(projPath, 'fastlog_out', file_path+".inc")
    # incFile = open(incPath)
    # incLines = incFile.readlines()
    # incLine = incLines[-1]
    # idxCnt += int(incLine.split()[2])
