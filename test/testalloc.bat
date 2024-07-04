@echo off
cl /I ../src/mock_libc/ testalloc.c ../src/mock_libc/alloc.c /Fe:testalloc.exe
testalloc.exe
del testalloc.exe
del *.obj