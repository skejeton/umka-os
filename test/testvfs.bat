@echo off
cl /I ../src/mock_libc/ testvfs.c ../src/mock_libc/vfs.c /Fe:testvfs.exe
testvfs.exe
del testvfs.exe
del *.obj