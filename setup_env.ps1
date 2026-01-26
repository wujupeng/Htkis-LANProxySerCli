# PowerShell script to setup development environment locally
# This allows the project to be built without modifying system-wide PATH or installing global tools.

$ErrorActionPreference = "Stop"

$ProjectRoot = Get-Location
$EnvDir = "$ProjectRoot\env"
$ThirdPartyDir = "$ProjectRoot\third_party"

# Ensure directories exist
if (-not (Test-Path $EnvDir)) { New-Item -ItemType Directory -Force -Path $EnvDir | Out-Null }
if (-not (Test-Path $ThirdPartyDir)) { New-Item -ItemType Directory -Force -Path $ThirdPartyDir | Out-Null }

# --- 1. Install Asio ---
$AsioVersion = "1.28.0"
$AsioUrl = "https://github.com/chriskohlhoff/asio/archive/refs/tags/asio-1-28-0.zip"
$AsioZip = "$ThirdPartyDir\asio.zip"
$AsioExtractDir = "$ThirdPartyDir\asio-temp"
$AsioFinalDir = "$ThirdPartyDir\asio"

if (-not (Test-Path "$AsioFinalDir\include\asio.hpp")) {
    Write-Host "Downloading Asio $AsioVersion..."
    try {
        Invoke-WebRequest -Uri $AsioUrl -OutFile $AsioZip
        
        Write-Host "Extracting Asio..."
        Expand-Archive -Path $AsioZip -DestinationPath $AsioExtractDir -Force
        
        $SourceInclude = Get-ChildItem -Path "$AsioExtractDir\*\asio\include" | Select-Object -First 1
        if ($SourceInclude) {
            if (-not (Test-Path $AsioFinalDir)) { New-Item -ItemType Directory -Force -Path $AsioFinalDir | Out-Null }
            Move-Item -Path $SourceInclude.FullName -Destination "$AsioFinalDir" -Force
            Write-Host "Asio installed to $AsioFinalDir\include"
        } else {
            Write-Warning "Could not find include directory in Asio zip"
        }
    } catch {
        Write-Error "Failed to install Asio: $_"
    }
    
    # Cleanup with error suppression
    try {
        Remove-Item -Path $AsioZip -Force -ErrorAction SilentlyContinue
        Remove-Item -Path $AsioExtractDir -Recurse -Force -ErrorAction SilentlyContinue
    } catch {}
} else {
    Write-Host "Asio already installed."
}

# --- 2. Install CMake (Portable) ---
$CmakeVersion = "3.29.0"
$CmakeUrl = "https://github.com/Kitware/CMake/releases/download/v$CmakeVersion/cmake-$CmakeVersion-windows-x86_64.zip"
$CmakeZip = "$EnvDir\cmake.zip"
$CmakeDir = "$EnvDir\cmake"

if (-not (Test-Path "$CmakeDir\bin\cmake.exe")) {
    Write-Host "Downloading CMake $CmakeVersion..."
    try {
        Invoke-WebRequest -Uri $CmakeUrl -OutFile $CmakeZip
        
        Write-Host "Extracting CMake..."
        Expand-Archive -Path $CmakeZip -DestinationPath $EnvDir -Force
        
        # Rename folder to simple 'cmake'
        $ExtractedFolder = Get-ChildItem -Path $EnvDir -Filter "cmake-*-windows-x86_64" | Select-Object -First 1
        if ($ExtractedFolder) {
            # Handle if target exists
            if (Test-Path $CmakeDir) { Remove-Item -Path $CmakeDir -Recurse -Force }
            Rename-Item -Path $ExtractedFolder.FullName -NewName "cmake"
        }
        Write-Host "CMake installed to $CmakeDir"
    } catch {
        Write-Error "Failed to install CMake: $_"
    }
    
    try { Remove-Item -Path $CmakeZip -Force -ErrorAction SilentlyContinue } catch {}
} else {
    Write-Host "CMake already installed."
}

# --- 3. Install MinGW-w64 (w64devkit Portable) ---
# Using w64devkit 1.21.0 (Smaller and reliable)
$MinGwUrl = "https://github.com/skeeto/w64devkit/releases/download/v1.21.0/w64devkit-1.21.0.zip"
$MinGwZip = "$EnvDir\w64devkit.zip"
$MinGwDir = "$EnvDir\mingw64"

if (-not (Test-Path "$MinGwDir\bin\g++.exe")) {
    Write-Host "Downloading MinGW-w64 (w64devkit)..."
    try {
        # Using Start-BitsTransfer for better reliability
        Start-BitsTransfer -Source $MinGwUrl -Destination $MinGwZip
        
        Write-Host "Extracting w64devkit..."
        Expand-Archive -Path $MinGwZip -DestinationPath $EnvDir -Force
        
        # w64devkit extracts to a folder named 'w64devkit'
        $ExtractedFolder = "$EnvDir\w64devkit"
        if (Test-Path $ExtractedFolder) {
            if (Test-Path $MinGwDir) { Remove-Item -Path $MinGwDir -Recurse -Force }
            Rename-Item -Path $ExtractedFolder -NewName "mingw64"
            Write-Host "MinGW-w64 installed to $MinGwDir"
        } else {
            Write-Error "Extraction failed or unexpected folder structure."
        }
    } catch {
        Write-Error "Failed to install MinGW: $_"
    }
    
    try { Remove-Item -Path $MinGwZip -Force -ErrorAction SilentlyContinue } catch {}
} else {
    Write-Host "MinGW-w64 already installed."
}

# --- 4. Create Activation Script ---
$ActivateScript = "$ProjectRoot\activate.ps1"
$Content = @"
`$Env:PATH = "$CmakeDir\bin;$MinGwDir\bin;" + `$Env:PATH
Write-Host "Environment configured for Htkis-LANProxySerCli"
if (Get-Command cmake -ErrorAction SilentlyContinue) {
    Write-Host "CMake: $(Get-Command cmake | Select-Object -ExpandProperty Source)"
} else {
    Write-Warning "CMake not found in path"
}
if (Get-Command g++ -ErrorAction SilentlyContinue) {
    Write-Host "G++:   $(Get-Command g++ | Select-Object -ExpandProperty Source)"
} else {
    Write-Warning "G++ not found in path"
}
"@
Set-Content -Path $ActivateScript -Value $Content

Write-Host "Setup complete! Run '.\activate.ps1' to start developing."
