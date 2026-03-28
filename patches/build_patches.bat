@echo off
REM Build patches.elf for Windows (replacement for Makefile)
setlocal enabledelayedexpansion
pushd "%~dp0"

REM Set compiler and linker with absolute paths
set CC="C:\Program Files\LLVM\bin\clang.exe"
set LD="C:\Program Files\LLVM\bin\ld.lld.exe"

REM Set flags
set CFLAGS=-target mips -mips2 -mabi=32 -O2 -G0 -mno-abicalls -mno-odd-spreg -mno-check-zero-division -fomit-frame-pointer -ffast-math -fno-unsafe-math-optimizations -fno-builtin-memset -Wall -Wextra -Wno-incompatible-library-redeclaration -Wno-unused-parameter -Wno-unknown-pragmas -Wno-unused-variable -Wno-missing-braces -Wno-unsupported-floating-point-opt
set CPPFLAGS=-nostdinc -D_LANGUAGE_C -DMIPS -I dummy_headers -I libultra -I../lib/rt64/include
set LDFLAGS=-nostdlib -T patches.ld -T syms.ld -Map patches.map --unresolved-symbols=ignore-all --emit-relocs

REM Compile print.c to print.o
echo Compiling print.c...
%CC% %CFLAGS% %CPPFLAGS% print.c -MMD -MF print.d -c -o print.o
if errorlevel 1 (
    echo Failed to compile print.c
    popd
    exit /b 1
)

REM Link to create patches.elf
echo Linking patches.elf...
%LD% print.o %LDFLAGS% -o patches.elf
if errorlevel 1 (
    echo Failed to link patches.elf
    popd
    exit /b 1
)

echo Build successful!
popd
endlocal
