# Build script for Htkis-LANProxySerCli
$ErrorActionPreference = "Stop"

$ProjectRoot = Get-Location
$EnvDir = "$ProjectRoot\env"
$CmakeBin = "$EnvDir\cmake\bin"
$MinGwBin = "$EnvDir\mingw64\bin"

# Add tools to PATH for this session
$Env:PATH = "$CmakeBin;$MinGwBin;" + $Env:PATH

Write-Host "Checking tools..."
try {
    $CmakeVer = cmake --version
    Write-Host "CMake found: $CmakeVer"
} catch {
    Write-Error "CMake not found in $CmakeBin"
}

try {
    $GppVer = g++ --version
    Write-Host "G++ found: $GppVer"
} catch {
    Write-Error "G++ not found in $MinGwBin"
}

# Create build directory
$BuildDir = "$ProjectRoot\build"
if (-not (Test-Path $BuildDir)) {
    New-Item -ItemType Directory -Force -Path $BuildDir | Out-Null
}

Set-Location $BuildDir

Write-Host "Configuring project..."
cmake -G "MinGW Makefiles" ..

Write-Host "Building project..."
cmake --build .

Write-Host "Build success!"
