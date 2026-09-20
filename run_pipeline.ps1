# ==============================================================================
# AUTOMATED NATIVE WINDOWS BUILD PIPELINE — STATIC ARCHIVE MODE
# ==============================================================================

# 0. Ensure MSYS2 UCRT64 compiler and make toolchain are in PATH
$env:PATH = "C:\msys64\ucrt64\bin;C:\msys64\usr\bin;" + $env:PATH

# 1. Force navigate to project folder and initialize build tree
Set-Location $PSScriptRoot
if (-not (Test-Path "build")) { New-Item -ItemType Directory "build" | Out-Null }
Set-Location "$PSScriptRoot\build"

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

# 4. Compile all targets (static library, test harness, runner, and tests)
Write-Host "--> Compiling decoder static library, harness, and runner binaries..." -ForegroundColor Cyan
& "C:\Program Files\CMake\bin\cmake.exe" --build . --config Release

if (-not (Test-Path ".\automotive_decoder_runner.exe") -or -not (Test-Path ".\dab_test_harness.exe")) {
    Write-Error "Fatal: Target binaries failed to build."
    exit 1
}
Write-Host "--> Static library libdab_decoder.a built successfully." -ForegroundColor Green
Write-Host "--> Runner automotive_decoder_runner.exe built successfully." -ForegroundColor Green
Write-Host "--> Test harness dab_test_harness.exe built successfully." -ForegroundColor Green

# 5. Bring in test audio stream data asset from music/ or test_streams/ folder
$streamSource = $null
if (Test-Path "..\music\stream_core24k_sbr48k.au") {
    $streamSource = "..\music\stream_core24k_sbr48k.au"
} elseif (Test-Path "..\music\stream_core48k_standalone.au") {
    $streamSource = "..\music\stream_core48k_standalone.au"
} elseif (Test-Path "..\music\stream_core16k_sbr32k.au") {
    $streamSource = "..\music\stream_core16k_sbr32k.au"
} elseif (Test-Path "..\test_streams\clean_aac_48k_stereo.au") {
    $streamSource = "..\test_streams\clean_aac_48k_stereo.au"
} elseif (Test-Path "..\english_dab_plus.au") {
    $streamSource = "..\english_dab_plus.au"
}

if ($null -eq $streamSource) {
    Write-Error "Fatal: No valid .au stream file found in music/ or test_streams/."
    exit 1
}

Write-Host "--> Using audio stream asset: $streamSource" -ForegroundColor Cyan
Copy-Item $streamSource -Destination ".\input_stream.au" -Force

# 6. Execute pipeline streaming decoder loop against the data asset
Write-Host "--> Executing stream decoder pipeline..." -ForegroundColor Cyan
.\automotive_decoder_runner.exe "input_stream.au" "decoded_output.wav"

if ($LASTEXITCODE -eq 0) {
    Write-Host "--> PIPELINE SUCCESSFUL!" -ForegroundColor Green
    Write-Host "--> Decoded audio file is ready at: build/decoded_output.wav" -ForegroundColor Green
} else {
    Write-Error "Fatal: Decoder pipeline execution failed with exit code $LASTEXITCODE."
    exit $LASTEXITCODE
}
