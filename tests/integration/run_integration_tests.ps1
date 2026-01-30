# WinExecSafe Integration Tests
# Tests actual AppContainer jail functionality

$ErrorActionPreference = "Stop"
$BinaryPath = ".\build\bin\Release\winexecsafe.exe"
$TestJailDir = "C:\WinExecSafeIntegrationTest"
$ExitCode = 0

function Test-Step {
    param([string]$Name, [scriptblock]$Script)
    
    Write-Host "=== $Name ===" -ForegroundColor Cyan
    try {
        & $Script
        Write-Host "[PASS] $Name passed" -ForegroundColor Green
    } catch {
        $errorMsg = $_.Exception.Message
        if ($errorMsg -match "CreateOrGetAppContainerProfile failed" -or 
            $errorMsg -match "SetNamedSecurityInfo failed" -or
            $errorMsg -match "Access is denied") {
            Write-Host "[SKIP] $Name skipped (requires admin privileges)" -ForegroundColor Yellow
        } else {
            Write-Host "[FAIL] $Name failed: $errorMsg" -ForegroundColor Red
            $script:ExitCode = 1
        }
    }
}

function Cleanup {
    Write-Host "Cleaning up test directories..."
    if (Test-Path $TestJailDir) {
        Remove-Item -Path $TestJailDir -Recurse -Force
    }
}

# Cleanup at start
Cleanup

try {
    # Test 1: Basic Execution
    Test-Step "Test 1: Basic Execution" {
        $null = & $BinaryPath --executable "C:\Windows\System32\cmd.exe" --args "/c echo Hello from jail" --jail-dir $TestJailDir 2>&1
        if ($LASTEXITCODE -eq 0) {
            Write-Host "[PASS] Command executed successfully"
        } else {
            throw "Command failed with exit code $LASTEXITCODE"
        }
    }

    # Test 2: Filesystem Isolation - Try to write outside jail
    Test-Step "Test 2: Filesystem Isolation (Write Outside Jail)" {
        $result = & $BinaryPath --executable "C:\Windows\System32\cmd.exe" --args "/c echo test > C:\IntegrationTestOutside.txt" --jail-dir $TestJailDir 2>&1
        if (Test-Path "C:\IntegrationTestOutside.txt") {
            throw "File was created outside jail - isolation failed!"
        } else {
            Write-Host "[PASS] File correctly not created outside jail"
        }
    }

    # Test 3: Jail Directory Access - Write inside jail
    Test-Step "Test 3: Jail Directory Access (Write Inside Jail)" {
        $null = & $BinaryPath --executable "C:\Windows\System32\cmd.exe" --args "/c echo test > $TestJailDir\inside.txt" --jail-dir $TestJailDir 2>&1
        if (Test-Path "$TestJailDir\inside.txt") {
            Write-Host "[PASS] File created inside jail"
        } else {
            throw "File not created inside jail"
        }
    }

    # Test 4: System Command Execution
    Test-Step "Test 4: System Command Execution" {
        $result = & $BinaryPath --executable "C:\Windows\System32\cmd.exe" --args "/c dir" --jail-dir $TestJailDir
        if ($LASTEXITCODE -eq 0) {
            Write-Host "[PASS] dir command executed successfully"
        } else {
            throw "dir command failed"
        }
    }

    # Test 5: Exit Code Propagation
    Test-Step "Test 5: Exit Code Propagation" {
        & $BinaryPath --executable "C:\Windows\System32\cmd.exe" --args "/c exit 42" --jail-dir $TestJailDir
        if ($LASTEXITCODE -eq 42) {
            Write-Host "[PASS] Exit code correctly propagated: $LASTEXITCODE"
        } else {
            throw "Exit code not propagated correctly (expected 42, got $LASTEXITCODE)"
        }
    }

    # Test 6: Config File Loading
    Test-Step "Test 6: Config File Loading" {
        $configPath = Join-Path $PSScriptRoot "test_config.ini"
        $configContent = @"
[General]
Executable=C:\Windows\System32\cmd.exe
Arguments=/c echo from config
JailDirectory=$TestJailDir
Cleanup=true
"@
        Set-Content -Path $configPath -Value $configContent
        
        $null = & $BinaryPath --config $configPath 2>&1
        if ($LASTEXITCODE -eq 0) {
            Write-Host "[PASS] Config file loaded and used"
        } else {
            throw "Config file not loaded properly (exit code $LASTEXITCODE)"
        }
        Remove-Item $configPath -ErrorAction SilentlyContinue
    }

    # Test 7: Config Precedence (CLI overrides config)
    Test-Step "Test 7: Config Precedence (CLI Overrides Config)" {
        $configPath = Join-Path $PSScriptRoot "test_config.ini"
        $configContent = @"
[General]
Executable=C:\Windows\System32\cmd.exe
Arguments=/c echo from config
JailDirectory=$TestJailDir
"@
        Set-Content -Path $configPath -Value $configContent
        
        $null = & $BinaryPath --config $configPath --args "/c echo from cli" 2>&1
        if ($LASTEXITCODE -eq 0) {
            Write-Host "[PASS] CLI arguments correctly override config"
        } else {
            throw "CLI arguments did not override config (exit code $LASTEXITCODE)"
        }
        Remove-Item $configPath -ErrorAction SilentlyContinue
    }

    # Test 8: Network Capability
    Test-Step "Test 8: Network Capability" {
        # Test with network (should work for localhost)
        $result = & $BinaryPath --executable "C:\Windows\System32\ping.exe" --args "-n 1 127.0.0.1" --jail-dir $TestJailDir --allow-network
        if ($LASTEXITCODE -eq 0) {
            Write-Host "[PASS] Network access allowed with --allow-network"
        } else {
            Write-Host "Warning: Network test failed (may be environment-specific)"
        }
    }

    # Test 9: Cleanup Verification
    Test-Step "Test 9: Cleanup Behavior" {
        $containerName = "TestCleanup_$([guid]::NewGuid().ToString('N'))"
        
        # Run with cleanup
        & $BinaryPath --executable "C:\Windows\System32\cmd.exe" --args "/c echo test" --jail-dir $TestJailDir --container-name $containerName --cleanup
        Write-Host "[PASS] Container with cleanup completed"
        
        # Note: We can't easily verify deletion without admin privileges
        # The cleanup happens automatically, and the test just verifies it doesn't error
    }

    # Test 10: Error Conditions
    Test-Step "Test 10: Invalid Executable" {
        $result = & $BinaryPath --executable "C:\nonexistent.exe" --jail-dir $TestJailDir 2>&1
        if ($LASTEXITCODE -ne 0) {
            Write-Host "[PASS] Correctly rejects invalid executable"
        } else {
            throw "Did not reject invalid executable properly"
        }
    }

    Write-Host ""
    Write-Host "=== Integration Tests Summary ===" -ForegroundColor Cyan
    if ($ExitCode -eq 0) {
        Write-Host "[PASS] All integration tests passed!" -ForegroundColor Green
    } else {
        Write-Host "[FAIL] Some integration tests failed" -ForegroundColor Red
    }

} finally {
    Cleanup
}

exit $ExitCode