# Windows Shared Library Bundling Script for Fuego
# This script collects all shared library dependencies and creates a portable package

param(
    [string]$BuildDir = "build",
    [string]$OutputDir = "fuego-windows-bundle"
)

# Colors for output
$Red = "Red"
$Green = "Green"
$Yellow = "Yellow"
$Blue = "Blue"

# Logging functions
function Write-Info {
    param([string]$Message)
    Write-Host "[INFO] $Message" -ForegroundColor $Blue
}

function Write-Success {
    param([string]$Message)
    Write-Host "[SUCCESS] $Message" -ForegroundColor $Green
}

function Write-Warning {
    param([string]$Message)
    Write-Host "[WARNING] $Message" -ForegroundColor $Yellow
}

function Write-Error {
    param([string]$Message)
    Write-Host "[ERROR] $Message" -ForegroundColor $Red
}

# Check if required tools are available
function Test-Dependencies {
    Write-Info "Checking dependencies..."
    
    $missingDeps = @()
    
    if (-not (Get-Command "dumpbin" -ErrorAction SilentlyContinue)) {
        $missingDeps += "dumpbin"
    }
    
    if (-not (Get-Command "where" -ErrorAction SilentlyContinue)) {
        $missingDeps += "where"
    }
    
    if ($missingDeps.Count -gt 0) {
        Write-Error "Missing required tools: $($missingDeps -join ', ')"
        Write-Error "Please install Visual Studio Build Tools or Visual Studio"
        exit 1
    }
    
    Write-Success "All dependencies found"
}

# Find all executable files
function Get-Executables {
    Write-Info "Finding executable files..."
    
    $execFiles = @()
    $mainExecs = @("fuegod.exe", "fuego-wallet-cli.exe", "walletd.exe", "optimizer.exe")
    
    foreach ($exec in $mainExecs) {
        $path = Join-Path $BuildDir "src" $exec
        if (Test-Path $path) {
            $execFiles += $path
            Write-Info "Found executable: $path"
        } else {
            Write-Warning "Executable not found: $path"
        }
    }
    
    if ($execFiles.Count -eq 0) {
        Write-Error "No executables found in $BuildDir\src\"
        exit 1
    }
    
    return $execFiles
}

# Get shared library dependencies for a file
function Get-SharedLibraries {
    param([string]$FilePath)
    
    $libs = @()
    
    try {
        # Use dumpbin to get DLL dependencies
        $output = & dumpbin /dependents $FilePath 2>$null
        
        foreach ($line in $output) {
            if ($line -match "\.dll$") {
                $libName = $line.Trim()
                if ($libName -and $libName -ne "File Type: EXECUTABLE IMAGE") {
                    # Try to find the full path
                    $libPath = $null
                    
                    # Check common system directories
                    $systemDirs = @(
                        "C:\Windows\System32",
                        "C:\Windows\SysWOW64",
                        "C:\Program Files\Microsoft Visual Studio",
                        "C:\Program Files (x86)\Microsoft Visual Studio",
                        "C:\vcpkg\installed\x64-windows\bin"
                    )
                    
                    foreach ($dir in $systemDirs) {
                        $fullPath = Join-Path $dir $libName
                        if (Test-Path $fullPath) {
                            $libPath = $fullPath
                            break
                        }
                    }
                    
                    if ($libPath) {
                        $libs += $libPath
                    } else {
                        Write-Warning "Could not find: $libName"
                    }
                }
            }
        }
    } catch {
        Write-Warning "Failed to analyze dependencies for $FilePath"
    }
    
    return $libs
}

# Copy shared library to bundle directory
function Copy-SharedLibrary {
    param(
        [string]$LibPath,
        [string]$DestDir
    )
    
    if (Test-Path $LibPath) {
        $libName = Split-Path $LibPath -Leaf
        $destPath = Join-Path $DestDir $libName
        
        if (-not (Test-Path $destPath)) {
            Copy-Item $LibPath $destPath -Force
            Write-Info "Copied: $libName"
        }
    }
}

# Create a batch wrapper script for an executable
function New-WrapperScript {
    param(
        [string]$ExecPath,
        [string]$WrapperPath
    )
    
    $execName = Split-Path $ExecPath -Leaf
    
    $content = @"
@echo off
REM Wrapper script for $execName
REM This script sets up the library path and runs the executable

REM Get the directory where this script is located
set SCRIPT_DIR=%~dp0

REM Set library path to include bundled libraries
set PATH=%SCRIPT_DIR%lib;%PATH%

REM Run the actual executable
"%SCRIPT_DIR%$execName" %*
"@
    
    $content | Out-File -FilePath $WrapperPath -Encoding ASCII
    Write-Info "Created wrapper script: $(Split-Path $WrapperPath -Leaf)"
}

# Create a PowerShell wrapper script for an executable
function New-PowerShellWrapper {
    param(
        [string]$ExecPath,
        [string]$WrapperPath
    )
    
    $execName = Split-Path $ExecPath -Leaf
    
    $content = @"
# PowerShell wrapper script for $execName
# This script sets up the library path and runs the executable

# Get the directory where this script is located
`$ScriptDir = Split-Path -Parent `$MyInvocation.MyCommand.Path

# Set library path to include bundled libraries
`$env:PATH = "`$ScriptDir\lib;" + `$env:PATH

# Run the actual executable
& "`$ScriptDir\$execName" @args
"@
    
    $content | Out-File -FilePath $WrapperPath -Encoding UTF8
    Write-Info "Created PowerShell wrapper: $(Split-Path $WrapperPath -Leaf)"
}

# Main bundling function
function Bundle-Windows {
    Write-Info "Starting Windows bundling process..."
    
    # Check dependencies
    Test-Dependencies
    
    # Get script directory
    $ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
    $ProjectRoot = Split-Path -Parent $ScriptDir
    
    # Change to project root
    Set-Location $ProjectRoot
    
    # Create output directory
    Write-Info "Creating output directory: $OutputDir"
    if (Test-Path $OutputDir) {
        Remove-Item $OutputDir -Recurse -Force
    }
    New-Item -ItemType Directory -Path $OutputDir | Out-Null
    New-Item -ItemType Directory -Path (Join-Path $OutputDir "bin") | Out-Null
    New-Item -ItemType Directory -Path (Join-Path $OutputDir "lib") | Out-Null
    
    # Find all executables
    $execFiles = Get-Executables
    
    # Collect all unique shared libraries
    Write-Info "Collecting shared library dependencies..."
    $allLibs = @()
    
    foreach ($execFile in $execFiles) {
        Write-Info "Analyzing dependencies for: $(Split-Path $execFile -Leaf)"
        $libs = Get-SharedLibraries -FilePath $execFile
        
        foreach ($lib in $libs) {
            if ($lib) {
                $allLibs += $lib
            }
        }
    }
    
    # Remove duplicates
    $uniqueLibs = $allLibs | Sort-Object -Unique
    
    Write-Info "Found $($uniqueLibs.Count) unique shared libraries"
    
    # Copy shared libraries
    foreach ($lib in $uniqueLibs) {
        if (Test-Path $lib) {
            Copy-SharedLibrary -LibPath $lib -DestDir (Join-Path $OutputDir "lib")
        } else {
            Write-Warning "Library not found: $lib"
        }
    }
    
    # Copy executables and create wrapper scripts
    Write-Info "Copying executables and creating wrapper scripts..."
    foreach ($execFile in $execFiles) {
        $execName = Split-Path $execFile -Leaf
        $destExec = Join-Path $OutputDir "bin" $execName
        $batchWrapper = Join-Path $OutputDir ($execName -replace '\.exe$', '.bat')
        $psWrapper = Join-Path $OutputDir ($execName -replace '\.exe$', '.ps1')
        
        # Copy the executable
        Copy-Item $execFile $destExec -Force
        Write-Info "Copied executable: $execName"
        
        # Create wrapper scripts in root of bundle
        New-WrapperScript -ExecPath $execName -WrapperPath $batchWrapper
        New-PowerShellWrapper -ExecPath $execName -WrapperPath $psWrapper
    }
    
    # Create a README file
    New-Readme
    
    # Create a simple installation script
    New-InstallScript
    
    Write-Success "Windows bundle created successfully in: $OutputDir"
    Write-Info "Bundle contents:"
    Get-ChildItem -Path $OutputDir -Recurse -File | Sort-Object FullName
}

# Create README file
function New-Readme {
    $readmePath = Join-Path $OutputDir "README.md"
    
    $content = @"
# Fuego Windows Bundle

This is a portable Windows bundle of Fuego that includes all necessary shared libraries.

## Contents

- `bin\` - Fuego executables
- `lib\` - Shared libraries (DLLs) required by the executables
- `fuegod.bat`, `fuegod.ps1` - Batch and PowerShell wrapper scripts
- `fuego-wallet-cli.bat`, `fuego-wallet-cli.ps1` - Batch and PowerShell wrapper scripts
- `walletd.bat`, `walletd.ps1` - Batch and PowerShell wrapper scripts
- `optimizer.bat`, `optimizer.ps1` - Batch and PowerShell wrapper scripts

## Usage

### Command Prompt

Run any of the batch wrapper scripts:

```cmd
fuegod.bat --help
fuego-wallet-cli.bat --help
walletd.bat --help
optimizer.bat --help
```

### PowerShell

Run any of the PowerShell wrapper scripts:

```powershell
.\fuegod.ps1 --help
.\fuego-wallet-cli.ps1 --help
.\walletd.ps1 --help
.\optimizer.ps1 --help
```

## Installation

Run the install script as Administrator:

```cmd
install.bat
```

Or in PowerShell:

```powershell
.\install.ps1
```

## Manual Installation

If you prefer to install manually:

1. Copy the executables to a directory in your PATH (e.g., `C:\Program Files\Fuego\`):
   ```cmd
   mkdir "C:\Program Files\Fuego"
   copy bin\* "C:\Program Files\Fuego\"
   ```

2. Copy the libraries to the same directory:
   ```cmd
   copy lib\* "C:\Program Files\Fuego\"
   ```

3. Add the directory to your PATH environment variable

## Troubleshooting

If you encounter library loading issues:

1. Check that all DLLs are present in the `lib\` directory
2. Verify that the wrapper scripts are in the same directory as the executables
3. Check your PATH environment variable
4. Ensure you have the Microsoft Visual C++ Redistributable installed

## System Requirements

- Windows 10 or later
- x64 architecture
- Microsoft Visual C++ Redistributable (if not bundled)
- No additional dependencies required (all bundled)

## Security Note

Windows Defender or other antivirus software may flag the executables. This is normal for cryptocurrency software. You may need to add an exception for the Fuego directory.
"@
    
    $content | Out-File -FilePath $readmePath -Encoding UTF8
    Write-Info "Created README.md"
}

# Create installation script
function New-InstallScript {
    $batchPath = Join-Path $OutputDir "install.bat"
    $psPath = Join-Path $OutputDir "install.ps1"
    
    # Batch installation script
    $batchContent = @"
@echo off
REM Fuego Windows Bundle Installation Script

echo [INFO] Installing Fuego Windows Bundle...

REM Check if running as administrator
net session >nul 2>&1
if %errorLevel% neq 0 (
    echo [ERROR] This script must be run as administrator
    pause
    exit /b 1
)

REM Create installation directory
set INSTALL_DIR=C:\Program Files\Fuego
echo [INFO] Creating installation directory: %INSTALL_DIR%
mkdir "%INSTALL_DIR%" 2>nul

REM Install executables
echo [INFO] Installing executables...
copy "bin\*" "%INSTALL_DIR%\" /Y

REM Install libraries
echo [INFO] Installing shared libraries...
copy "lib\*" "%INSTALL_DIR%\" /Y

REM Add to PATH
echo [INFO] Adding to PATH...
setx PATH "%PATH%;%INSTALL_DIR%" /M

REM Create desktop shortcuts
echo [INFO] Creating desktop shortcuts...
set DESKTOP=%USERPROFILE%\Desktop
echo [InternetShortcut] > "%DESKTOP%\Fuego Daemon.url"
echo URL=file:///%INSTALL_DIR%\fuegod.exe >> "%DESKTOP%\Fuego Daemon.url"
echo [InternetShortcut] > "%DESKTOP%\Fuego Wallet.url"
echo URL=file:///%INSTALL_DIR%\fuego-wallet-cli.exe >> "%DESKTOP%\Fuego Wallet.url"

echo [SUCCESS] Installation completed successfully!
echo You can now run:
echo   fuegod --help
echo   fuego-wallet-cli --help
echo   walletd --help
echo   optimizer --help
pause
"@
    
    $batchContent | Out-File -FilePath $batchPath -Encoding ASCII
    
    # PowerShell installation script
    $psContent = @"
# Fuego Windows Bundle Installation Script

# Colors
`$Red = "Red"
`$Green = "Green"
`$Yellow = "Yellow"
`$Blue = "Blue"

function Write-Info {
    param([string]`$Message)
    Write-Host "[INFO] `$Message" -ForegroundColor `$Blue
}

function Write-Success {
    param([string]`$Message)
    Write-Host "[SUCCESS] `$Message" -ForegroundColor `$Green
}

function Write-Error {
    param([string]`$Message)
    Write-Host "[ERROR] `$Message" -ForegroundColor `$Red
}

# Check if running as administrator
if (-NOT ([Security.Principal.WindowsPrincipal] [Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole([Security.Principal.WindowsBuiltInRole] "Administrator")) {
    Write-Error "This script must be run as administrator"
    exit 1
}

`$ScriptDir = Split-Path -Parent `$MyInvocation.MyCommand.Path
`$InstallDir = "C:\Program Files\Fuego"

Write-Info "Installing Fuego Windows Bundle..."

# Create installation directory
Write-Info "Creating installation directory: `$InstallDir"
New-Item -ItemType Directory -Path `$InstallDir -Force | Out-Null

# Install executables
Write-Info "Installing executables..."
Copy-Item "bin\*" `$InstallDir -Force

# Install libraries
Write-Info "Installing shared libraries..."
Copy-Item "lib\*" `$InstallDir -Force

# Add to PATH
Write-Info "Adding to PATH..."
`$currentPath = [Environment]::GetEnvironmentVariable("PATH", "Machine")
`$newPath = "`$currentPath;`$InstallDir"
[Environment]::SetEnvironmentVariable("PATH", `$newPath, "Machine")

# Create desktop shortcuts
Write-Info "Creating desktop shortcuts..."
`$desktop = [Environment]::GetFolderPath("Desktop")
`$shortcut1 = Join-Path `$desktop "Fuego Daemon.url"
`$shortcut2 = Join-Path `$desktop "Fuego Wallet.url"

"@"
[InternetShortcut]
URL=file:///$InstallDir/fuegod.exe
"@ | Out-File -FilePath `$shortcut1 -Encoding ASCII

"@"
[InternetShortcut]
URL=file:///$InstallDir/fuego-wallet-cli.exe
"@ | Out-File -FilePath `$shortcut2 -Encoding ASCII

Write-Success "Installation completed successfully!"
Write-Info "You can now run:"
Write-Info "  fuegod --help"
Write-Info "  fuego-wallet-cli --help"
Write-Info "  walletd --help"
Write-Info "  optimizer --help"
"@
    
    $psContent | Out-File -FilePath $psPath -Encoding UTF8
    
    Write-Info "Created installation scripts: install.bat and install.ps1"
}

# Main execution
if ($MyInvocation.InvocationName -ne '.') {
    Bundle-Windows
}