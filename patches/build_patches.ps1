# PowerShell script to build patches.elf
$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$CC = "C:\Program Files\LLVM\bin\clang.exe"
$LD = "C:\Program Files\LLVM\bin\ld.lld.exe"

$CFLAGS = @(
    "-target", "mips",
    "-mips2",
    "-mabi=32",
    "-O2",
    "-G0",
    "-mno-abicalls",
    "-mno-odd-spreg",
    "-fomit-frame-pointer",
    "-ffast-math",
    "-fno-unsafe-math-optimizations",
    "-fno-builtin-memset",
    "-Wall",
    "-Wextra",
    "-Wno-incompatible-library-redeclaration",
    "-Wno-unused-parameter",
    "-Wno-unknown-pragmas",
    "-Wno-unused-variable",
    "-Wno-missing-braces",
    "-Wno-unsupported-floating-point-opt"
)

$CPPFLAGS = @(
    "-nostdinc",
    "-D_LANGUAGE_C",
    "-DMIPS",
    "-I", "dummy_headers",
    "-I", "libultra",
    "-I", "../lib/rt64/include"
)

$LDFLAGS = @(
    "-nostdlib",
    "-T", "patches.ld",
    "-T", "syms.ld",
    "-Map", "patches.map",
    "--unresolved-symbols=ignore-all",
    "--emit-relocs"
)

Push-Location $ScriptDir

Write-Host "Compiling print.c..."
& $CC @CFLAGS @CPPFLAGS "print.c" "-MMD" "-MF" "print.d" "-c" "-o" "print.o"
if ($LASTEXITCODE -ne 0) {
    Write-Host "Failed to compile print.c"
    Pop-Location
    exit 1
}

Write-Host "Linking patches.elf..."
& $LD "print.o" @LDFLAGS "-o" "patches.elf"
if ($LASTEXITCODE -ne 0) {
    Write-Host "Failed to link patches.elf"
    Pop-Location
    exit 1
}

Write-Host "Build successful!"
Pop-Location
