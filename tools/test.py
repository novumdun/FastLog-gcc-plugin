#!python3
import subprocess
import os
import time

from fastlog_display import display_flog
from fastlog import make_records
from compile import shell_cmd, mkdirs, rmfile, make_tools, prebuild


dir = os.path.dirname(__file__)
projPath = os.path.join(dir, '..')

make_tools(projPath)

# build 'proj/test.c'
includes = f'-I{projPath}/inc -I{projPath}/fastlog_out/proj'
prebuild(projPath, "proj/test.c", includes)

# build 'proj/test01.c'
includes = f'-I{projPath}/inc -I{projPath}/fastlog_out/proj -I{projPath}/third_party/cJSON'
prebuild(projPath, "proj/test01.c", includes)

srcPath = os.path.join(projPath, 'src/fastlog.c')
cmd = f'gcc -c -g -o build/fastlog.o {srcPath} {includes}'
shell_cmd(cmd)

srcPath = os.path.join(projPath, 'src/config.c')
cmd = f'gcc -c -g -o build/config.o {srcPath}  {includes}'
shell_cmd(cmd)

fileIdProcPath = os.path.join(projPath, 'fastlog_out/tools/file_id')


srcPath = os.path.join(projPath, 'proj/test.c')
cmd = f'{fileIdProcPath} {os.path.normpath(srcPath)}'
fileId = shell_cmd(cmd)
includes += f' -I{os.path.join(projPath, "fastlog_out/sys")} '
cmd = 'gcc -c -g -o build/test.o ' + srcPath + ' ' + includes + \
    f' -DCOMPILE_PASS=1 -D__FILE_ID__=' + fileId
shell_cmd(cmd)


srcPath = os.path.join(projPath, 'proj/test01.c')
cmd = f'{fileIdProcPath} {os.path.normpath(srcPath)}'
fileId_01 = shell_cmd(cmd)
cmd = 'gcc -c -g -o build/test01.o ' + srcPath + ' ' + includes + \
    f' -DCOMPILE_PASS=1 -D__FILE_ID__=' + fileId_01
shell_cmd(cmd)

srcPath = os.path.join(projPath, 'fastlog_out/sys/log_inc.c')
with open(srcPath, "a") as file:
    file.write("\nint FASTLOG_MODULE_SIZE_test=__COUNTER__;")
cmd = 'gcc -E -c -DLOG_IDX_FAKE_STA=0 ' + \
    srcPath + ' > ./fastlog_out/sys/log_inc.c.i'
shell_cmd(cmd)

log_inc_fd = open('./fastlog_out/sys/log_inc.c.i')
make_records(log_inc_fd, projPath)

srcPath = os.path.join(projPath, 'fastlog_out/sys/log_inc.c')
cmd = 'gcc -c -g ' + srcPath + ' -DLOG_IDX_FAKE_STA=1' + ' -o build/log_inc.o'
shell_cmd(cmd)

srcPath = os.path.join(projPath, 'third_party/cJSON/cJSON.c')
cmd = 'gcc -c -g ' + srcPath + ' -o build/cJSON.o'
shell_cmd(cmd)

cmd = 'gcc ' + ' -o build/test ' + \
    ' build/test.o build/test01.o build/fastlog.o build/cJSON.o build/log_inc.o build/config.o'
shell_cmd(cmd)

cmd = './build/test'
shell_cmd(cmd)

time.sleep(1)

display_flog("fastlog_out/flog")
