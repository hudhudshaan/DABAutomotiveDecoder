# ==============================================================================
# AUTOMATED NATIVE WINDOWS BUILD PIPELINE — STATIC ARCHIVE MODE
# ==============================================================================

# 0. Ensure MSYS2 UCRT64 compiler and make toolchain are in PATH
$env:PATH = "C:\msys64\ucrt64\bin;C:\msys64\usr\bin;" + $env:PATH

# 1. Force navigate to project folder and initialize build tree
cd C:\PersonalData\Shaan\Projects\dab_automotive_decoder
if (-not (Test-Path "build")) { New-Item -ItemType Directory "build" | Out-Null }
cd build

# 2. Wipe old cache attempts completely to ensure a clean slate
if (Test-Path "CMakeCache.txt") { Remove-Item -Force "CMakeCache.txt" }
if (Test-Path "CMakeFiles") { Remove-Item -Recurse -Force "CMakeFiles" }

Write-Host "--> Configuring CMake with MSYS2 UCRT64 MinGW toolchain..." -ForegroundColor Cyan

# 3. Run CMake with MSYS2 UCRT64 toolchain
& "C:\Program Files\CMake\bin\cmake.exe" .. `
  -G "MinGW Makefiles" `
  -DCMAKE_C_COMPILER="C:\msys64\ucrt64\bin\gcc.exe" `
  -DCMAKE_CXX_COMPILER="C:\msys64\ucrt64\bin\g++.exe" `
  -DCMAKE_MAKE_PROGRAM="C:\msys64\ucrt64\bin\mingw32-make.exe" `
  -DCMAKE_BUILD_TYPE=Release

# 4. Compile the dedicated runner binary target
Write-Host "--> Compiling target binary..." -ForegroundColor Cyan
& "C:\Program Files\CMake\bin\cmake.exe" --build . --target automotive_decoder_runner

# 5. Bring in test audio stream data asset from root or test_streams folder if needed
if (-not (Test-Path "english_dab_plus.au")) {
    if (Test-Path "..\english_dab_plus.au") {
        Copy-Item "..\english_dab_plus.au" -Destination "."
    } elseif (Test-Path "..\test_streams\english_dab_plus.au") {
        Copy-Item "..\test_streams\english_dab_plus.au" -Destination "."
    }
}

# 6. Execute pipeline streaming decoder loop against the data asset
Write-Host "--> Executing stream decoder pipeline..." -ForegroundColor Cyan
if (Test-Path ".\automotive_decoder_runner.exe") {
    .\automotive_decoder_runner.exe "english_dab_plus.au" "decoded_output.wav"
    Write-Host "--> PIPELINE SUCCESSFUL!" -ForegroundColor Green
    Write-Host "--> Decoded audio file is ready at: C:\PersonalData\Shaan\Projects\dab_automotive_decoder\build\decoded_output.wav" -ForegroundColor Green
} else {
    Write-Error "Fatal: Application binary build target missing."
}
