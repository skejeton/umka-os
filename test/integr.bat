@echo off

set files=../src/umka/src/umka_api.c ../src/umka/src/umka_common.c ../src/umka/src/umka_compiler.c ../src/umka/src/umka_const.c ../src/umka/src/umka_decl.c ../src/umka/src/umka_expr.c ../src/umka/src/umka_gen.c ../src/umka/src/umka_ident.c ../src/umka/src/umka_lexer.c ../src/umka/src/umka_runtime.c ../src/umka/src/umka_stmt.c ../src/umka/src/umka_types.c ../src/umka/src/umka_vm.c
lib /def:jmp.def /out:jmp.lib
cl /I ../src/mock_libc/ integr.c %files% /Z7 ../src/mock_libc/*.c jmp.lib user32.lib kernel32.lib /I ../src /DUMKA_BUILD /GS- /Gs90000000 /Fe:integr.exe /link /STACK:0x64000000,0x64000000 /NODEFAULTLIB /SUBSYSTEM:CONSOLE
integr.exe
rem del integr.exe
del *.obj
del *.exp
del *.lib