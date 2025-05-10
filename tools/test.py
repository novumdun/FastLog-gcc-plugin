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


dir = os.path.dirname(__file__)
projPath = os.path.join(dir, '..')
toolsPath = os.path.join(projPath, 'fastlog_out/tools')
mkdirs(toolsPath)
rmfile(os.path.join(projPath, 'proj/log_inc.c'))
rmfile(os.path.join(projPath, 'proj/log_macro.h'))
idxCnt = 0

srcPath = os.path.join(dir, 'gcc_plugin.cc')
pluginPath = os.path.join(toolsPath, 'plugin.so')
cmd = 'g++ ' + \
    f'{srcPath} -I`gcc -print-file-name=plugin`/include -fPIC -shared -o {pluginPath}'
shell_cmd(cmd)

# file_id converts the path of a file to a string which identify the file and could be
# transed into C by compiler.
srcPath = os.path.join(dir, 'file_id.c')
fileIdProcPath = os.path.join(toolsPath, 'file_id')
cmd = 'gcc ' + f'{srcPath} -o {fileIdProcPath}'
shell_cmd(cmd)

# build 'proj/test.c'
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

# build 'proj/test01.c'
srcPath = os.path.join(projPath, 'proj/test01.c')
cmd = f'{fileIdProcPath} {os.path.normpath(srcPath)}'
fileId_01 = shell_cmd(cmd)

includes = f'-I{projPath}/inc -I{projPath}/fastlog_out/proj'
cmd = f'gcc -fplugin={pluginPath} -c {srcPath} {includes} -DCOMPILE_PASS=0 -D__FILE_PATH__=\'"{
    os.path.normpath(srcPath)}"\' -D__PROJ_PATH__=\'"{os.path.normpath(projPath)}"\' -D__FILE_ID__={fileId_01}'
shell_cmd(cmd)

incPath = os.path.join(projPath, 'fastlog_out', 'proj/test01.c.inc')
incFile = open(incPath)
incLines = incFile.readlines()
incLine = incLines[-1]
idxCnt += int(incLine.split()[2])

srcPath = os.path.join(projPath, 'src/fastlog.c')
cmd = 'gcc -c -g -o build/fastlog.o ' + srcPath + ' ' + includes + ' -DCOMPILE_PASS=1'
shell_cmd(cmd)

idxCnt = (((idxCnt+1)//64)+1)*64
print(idxCnt)
srcPath = os.path.join(projPath, 'proj/fastlog_test.c')
cmd = 'gcc -c -g -o build/fastlog_test.o ' + srcPath + ' ' + includes + \
    f' -DCOMPILE_PASS=1  -DFASTLOG_MODULE_SIZE_test={idxCnt}'
shell_cmd(cmd)

srcPath = os.path.join(projPath, 'proj/test.c')
cmd = 'gcc -c -g -o build/test.o ' + srcPath + ' ' + includes + \
    ' -DCOMPILE_PASS=1 -D__FILE_ID__=' + fileId
shell_cmd(cmd)

srcPath = os.path.join(projPath, 'proj/test01.c')
cmd = 'gcc -c -g -o build/test01.o ' + srcPath + ' ' + includes + \
    ' -DCOMPILE_PASS=1 -D__FILE_ID__=' + fileId_01
shell_cmd(cmd)

srcPath = os.path.join(projPath, 'proj/log_inc.c')
cmd = 'gcc -E -c ' + srcPath + ' > ./fastlog_out/log_inc.c.i'
shell_cmd(cmd)

log_inc_fd = open('./fastlog_out/log_inc.c.i')
make_records(log_inc_fd, projPath)

srcPath = os.path.join(projPath, 'proj/log_inc.c')
cmd = 'gcc -c -g ' + srcPath + ' -D__LOG_IDX_FAKE' + ' -o build/log_inc.o'
shell_cmd(cmd)

cmd = 'gcc ' + ' -o build/test ' + ' build/test.o build/test01.o build/fastlog_test.o build/fastlog.o build/log_inc.o'
shell_cmd(cmd)

cmd = './build/test'
shell_cmd(cmd)

time.sleep(1)

display_flog()
