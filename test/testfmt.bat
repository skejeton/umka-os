@echo off
cl /I ../src/mock_libc/ testfmt.c ../src/mock_libc/fmt.c ../src/mock_libc/vfs.c ../src/mock_libc/alloc.c /Fe:testfmt.exe
testfmt.exe
del testfmt.exe
del *.obj