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


dir = os.path.dirname(__file__)
projPath = os.path.join(dir, '..')
toolsPath = os.path.join(projPath, 'fastlog_out/tools')
mkdirs(toolsPath)

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

srcPath = os.path.join(projPath, 'src/fastlog.c')
cmd = 'gcc -c -g ' + srcPath + ' ' + includes + ' -DCOMPILE_PASS=1'
shell_cmd(cmd)

srcPath = os.path.join(projPath, 'proj/fastlog_test.c')
cmd = 'gcc -c -g ' + srcPath + ' ' + includes + \
    ' -DCOMPILE_PASS=1  -DFASTLOG_MODULE_SIZE_test={0}'
shell_cmd(cmd)

srcPath = os.path.join(projPath, 'proj/test.c')
cmd = 'gcc -c -g ' + srcPath + ' ' + includes + \
    ' -DCOMPILE_PASS=1 -D__FILE_ID__=' + fileId
shell_cmd(cmd)

srcPath = os.path.join(projPath, 'proj/log_inc.c')
cmd = 'gcc -E -c -g ' + srcPath
shell_cmd(cmd)

cmd = 'gcc ' + ' -o Test ' + ' test.o fastlog_test.o fastlog.o log_inc.o'
shell_cmd(cmd)

cmd = './Test'
shell_cmd(cmd)

time.sleep(1)
