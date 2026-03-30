Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

function Resolve-ToolPath {
    param(
        [string[]]$EnvVarNames,
        [string[]]$CommandNames,
        [string[]]$CandidatePaths,
        [string]$DisplayName
    )

    foreach ($envVarName in $EnvVarNames) {
        $requested = [Environment]::GetEnvironmentVariable($envVarName)
        if ([string]::IsNullOrWhiteSpace($requested)) {
            continue
        }

        if (Test-Path -LiteralPath $requested) {
            return (Resolve-Path -LiteralPath $requested).Path
        }

        $resolvedCommand = Get-Command $requested -ErrorAction SilentlyContinue
        if ($resolvedCommand) {
            return $resolvedCommand.Source
        }

        throw "Unable to resolve $DisplayName from environment variable $envVarName='$requested'."
    }

    foreach ($commandName in $CommandNames) {
        $resolvedCommand = Get-Command $commandName -ErrorAction SilentlyContinue
        if ($resolvedCommand) {
            return $resolvedCommand.Source
        }
    }

    foreach ($candidatePath in $CandidatePaths) {
        if (Test-Path -LiteralPath $candidatePath) {
            return (Resolve-Path -LiteralPath $candidatePath).Path
        }
    }

    throw "Unable to find $DisplayName. Set one of the environment variables [$($EnvVarNames -join ', ')] or install it in a standard location."
}

function Invoke-PatchesCompile {
    param(
        [string]$CompilerPath,
        [string[]]$CompileFlags,
        [string[]]$PreprocessorFlags
    )

    $logDir = Join-Path ([System.IO.Path]::GetTempPath()) ("mm64-patches-compile-" + [Guid]::NewGuid().ToString("N"))
    New-Item -ItemType Directory -Path $logDir | Out-Null
    $stdoutPath = Join-Path $logDir "stdout.log"
    $stderrPath = Join-Path $logDir "stderr.log"
    $arguments = @($CompileFlags + $PreprocessorFlags + @("print.c", "-MMD", "-MF", "print.d", "-c", "-o", "print.o"))
    $exitCode = 1
    $outputText = ""

    try {
        $process = Start-Process -FilePath $CompilerPath -ArgumentList $arguments -NoNewWindow -Wait -PassThru -RedirectStandardOutput $stdoutPath -RedirectStandardError $stderrPath
        $exitCode = $process.ExitCode

        $stdout = ""
        $stderr = ""
        if (Test-Path -LiteralPath $stdoutPath) {
            $stdout = Get-Content -Raw -LiteralPath $stdoutPath
        }
        if (Test-Path -LiteralPath $stderrPath) {
            $stderr = Get-Content -Raw -LiteralPath $stderrPath
        }
        $outputText = ($stdout + $stderr).TrimEnd()
        if (-not [string]::IsNullOrWhiteSpace($outputText)) {
            Write-Host $outputText
        }
    }
    finally {
        if (Test-Path -LiteralPath $logDir) {
            Remove-Item -LiteralPath $logDir -Recurse -Force -ErrorAction SilentlyContinue
        }
    }

    return @{
        ExitCode = $exitCode
        Output = $outputText
    }
}

# PowerShell script to build patches.elf
$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$CC = Resolve-ToolPath `
    -EnvVarNames @("MM64_PATCHES_CC", "PATCHES_C_COMPILER") `
    -CommandNames @("clang", "clang.exe") `
    -CandidatePaths @(
        "C:\Program Files\LLVM\bin\clang.exe",
        "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Tools\Llvm\x64\bin\clang.exe",
        "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Tools\Llvm\x64\bin\clang.exe"
    ) `
    -DisplayName "clang"
$LD = Resolve-ToolPath `
    -EnvVarNames @("MM64_PATCHES_LD", "PATCHES_LD") `
    -CommandNames @("ld.lld", "ld.lld.exe", "lld-link", "lld-link.exe") `
    -CandidatePaths @(
        "C:\Program Files\LLVM\bin\ld.lld.exe",
        "C:\Program Files\LLVM\bin\lld-link.exe",
        "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Tools\Llvm\x64\bin\ld.lld.exe",
        "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Tools\Llvm\x64\bin\lld-link.exe",
        "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Tools\Llvm\x64\bin\ld.lld.exe",
        "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Tools\Llvm\x64\bin\lld-link.exe"
    ) `
    -DisplayName "ld.lld"

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

Write-Host "Using clang: $CC"
Write-Host "Using linker: $LD"
Write-Host "Compiling print.c..."
$compileResult = Invoke-PatchesCompile -CompilerPath $CC -CompileFlags $CFLAGS -PreprocessorFlags $CPPFLAGS
$effectiveCFlags = $CFLAGS

# Some LLVM builds reject the legacy -G0 flow by emitting an internal
# -mips-ssection-threshold argument error. Retry once without -G0.
if ($compileResult.ExitCode -ne 0 -and $compileResult.Output -match "mips-ssection-threshold=0") {
    $fallbackCFlags = $effectiveCFlags | Where-Object { $_ -ne "-G0" }
    if ($fallbackCFlags.Count -ne $effectiveCFlags.Count) {
        Write-Host "Retrying compile without -G0 for LLVM compatibility..."
        $compileResult = Invoke-PatchesCompile -CompilerPath $CC -CompileFlags $fallbackCFlags -PreprocessorFlags $CPPFLAGS
        $effectiveCFlags = $fallbackCFlags
    }
}

# Some LLVM distributions reject -mno-abicalls by surfacing an unsupported
# internal -mgpopt option. Retry once without -mno-abicalls.
if ($compileResult.ExitCode -ne 0 -and $compileResult.Output -match "Unknown command line argument '-mgpopt'") {
    $fallbackCFlags = $effectiveCFlags | Where-Object { $_ -ne "-mno-abicalls" }
    if ($fallbackCFlags.Count -ne $effectiveCFlags.Count) {
        Write-Host "Retrying compile without -mno-abicalls for LLVM compatibility..."
        $compileResult = Invoke-PatchesCompile -CompilerPath $CC -CompileFlags $fallbackCFlags -PreprocessorFlags $CPPFLAGS
        $effectiveCFlags = $fallbackCFlags
    }
}

if ($compileResult.ExitCode -ne 0) {
    if ($compileResult.Output -match 'No available targets are compatible with triple "mips"') {
        Write-Host "Failed to compile print.c. This clang build does not include the MIPS backend required for patch overlays."
    }
    elseif ($compileResult.Output -match "Unknown command line argument '-mgpopt'" -or $compileResult.Output -match "mips-ssection-threshold=0") {
        Write-Host "Failed to compile print.c. This clang build does not support the legacy MIPS options required by patch overlays."
    }
    else {
        Write-Host "Failed to compile print.c. Ensure clang supports the MIPS target used by patch overlays."
    }
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
