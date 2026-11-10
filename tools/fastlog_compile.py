#!python3
from tools.fastlog_display_rec import display_flog
from tools.fastlog_make_rec import make_records
import subprocess
import os
import time

import inc

fileIdProcPath = None
pluginPath = None


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


def set_project(projPath, logName):
    mkdirs(os.path.join(projPath, 'build'))
    mkdirs(os.path.join(projPath, 'fastlog_out/tools'))

    rmfile(os.path.join(projPath, 'fastlog_out/sys/log_inc.c'))
    rmfile(os.path.join(projPath, 'fastlog_out/sys/log_macro.h'))
    rmfile(os.path.join(projPath, f'fastlog_out/{logName}'))

    srcPath = os.path.join(projPath, 'fastlog_out/sys/log_inc.c')
    with open(srcPath, "w") as f:
        f.write("""#if LOG_IDX_FAKE_STA == 0
void LOG_IDX_FAKE(...);
#else
#define LOG_IDX_FAKE(...) 
#endif\n\n""")


def make_tools(projPath):
    global fileIdProcPath
    global pluginPath

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


def prebuild(projPath, file_rela_path, includes):
    global fileIdProcPath
    global pluginPath

    srcPath = os.path.join(projPath, file_rela_path)
    fileName = os.path.basename(srcPath).replace('c', 'o')
    cmd = f'{fileIdProcPath} {os.path.normpath(srcPath)}'
    fileId = shell_cmd(cmd)

    cmd = f'gcc -c -o build/{fileName} -fplugin={pluginPath} -c {srcPath} {includes} -DCOMPILE_PASS=0 -D__FILE_PATH__=\'"{
        os.path.normpath(srcPath)}"\' -D__PROJ_PATH__=\'"{os.path.normpath(projPath)}"\' -D__FILE_ID__={fileId}'
    shell_cmd(cmd)

    # incPath = os.path.join(projPath, 'fastlog_out', file_rela_path+".inc")
    # incFile = open(incPath)
    # incLines = incFile.readlines()
    # incLine = incLines[-1]
    # idxCnt += int(incLine.split()[2])


def build(projPath, file_rela_path, includes):
    global fileIdProcPath
    global pluginPath

    srcPath = os.path.join(projPath, file_rela_path)
    fileName = os.path.basename(srcPath).replace('c', 'o')
    cmd = f'{fileIdProcPath} {os.path.normpath(srcPath)}'
    fileId = shell_cmd(cmd)
    includes += f' -I{os.path.join(projPath, "fastlog_out/sys")} '
    cmd = f'gcc -c -g -o build/{fileName} {srcPath} {includes} -DCOMPILE_PASS=1 -D__FILE_ID__={fileId}'
    shell_cmd(cmd)
