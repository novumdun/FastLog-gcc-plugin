# FastLog-gcc-plugin

```shell
g++ gcc-plugin.cc  -I`gcc -print-file-name=plugin`/include -fPIC -shared -o plugin.so
```

gcc  -fplugin=./plugin.so -c test.c -DCOMPILE_PASS=0 -D__FILE_ID__=_home_dengbo_workspace_cabin_FastLog__gcc__plugin

gcc -c test.c -DCOMPILE_PASS=1 -D__FILE_ID__=_home_dengbo_workspace_cabin_FastLog__gcc__plugin -DFASTLOG_FILE_ADDR=0

gcc -c fastlog_test.c -DCOMPILE_PASS=1 -D__FILE_ID__=_home_dengbo_workspace_cabin_FastLog__gcc__plugin -DFASTLOG_FILE_ADDR=0 -DFASTLOG_MODULE_SIZE_test=64