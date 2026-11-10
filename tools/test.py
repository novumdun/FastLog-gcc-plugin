#!python3
import subprocess
import os
import time

import inc
from tools.fastlog_display_rec import display_flog
from tools.fastlog_make_rec import make_records
from tools.fastlog_compile import shell_cmd, mkdirs, rmfile, set_project, make_tools, prebuild, build


dir = os.path.dirname(__file__)
projPath = os.path.join(dir, '..')
projName = "test"
logName = "hello_world"

set_project(projPath, logName)
make_tools(projPath)


# prebuild files using 'fastlog'
includes = f'-I{projPath}/inc'
prebuild(projPath, "proj/test.c", includes)
prebuild(projPath, "proj/file_a.c", includes)


# build 'fastlog' lib
includes = f'-I{projPath}/inc -I{projPath}/third_party/cJSON'

srcPath = os.path.join(projPath, 'lib/shared_mutex.c')
cmd = f'gcc -c -g -o build/shared_mutex.o {srcPath} {includes}'
shell_cmd(cmd)

srcPath = os.path.join(projPath, 'lib/fastlog.c')
cmd = f'gcc -c -g -o build/fastlog.o {srcPath} {includes}'
shell_cmd(cmd)

srcPath = os.path.join(projPath, 'lib/config.c')
cmd = f'gcc -c -g -o build/config.o {srcPath}  {includes}'
shell_cmd(cmd)

srcPath = os.path.join(projPath, 'third_party/cJSON/cJSON.c')
cmd = f'gcc -c -g -o build/cJSON.o {srcPath}'
shell_cmd(cmd)


# generate records for 'fastlog'
srcPath = os.path.join(projPath, 'fastlog_out/sys/log_inc.c')
with open(srcPath, "a") as file:
    file.write("\nint FASTLOG_MODULE_SIZE_test=__COUNTER__;")
cmd = f'gcc -E -c -DLOG_IDX_FAKE_STA=0 {srcPath} > fastlog_out/sys/log_inc.i'
shell_cmd(cmd)


make_records('fastlog_out/sys/log_inc.i', projPath, projName)


# build 'log_inc.c'
srcPath = os.path.join(projPath, 'fastlog_out/sys/log_inc.c')
cmd = f'gcc -c -g {srcPath} -DLOG_IDX_FAKE_STA=1 -o build/log_inc.o'
shell_cmd(cmd)


# build files using 'fastlog'
includes += f' -I{os.path.join(projPath, "fastlog_out/sys")} '
build(projPath, 'proj/test.c', includes)
build(projPath, 'proj/file_a.c', includes)

cmd = 'gcc ' + ' -o build/test ' + \
    ' build/test.o build/file_a.o build/log_inc.o build/fastlog.o build/shared_mutex.o build/cJSON.o build/config.o'
shell_cmd(cmd)

cmd = f'./build/test {projPath}/fastlog_out/{logName}'
shell_cmd(cmd)

time.sleep(1)

display_flog(projPath, projName, logName)
