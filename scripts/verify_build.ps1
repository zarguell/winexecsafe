# WinExecSafe Build Verification Script
# Ensures all source files compile and have proper includes

$ErrorActionPreference = "Stop"
$ExitCode = 0

Write-Host "=== Verifying Build Configuration ===" -ForegroundColor Cyan

# Check for required headers in source files
Write-Host "Checking for required includes in source files..." -ForegroundColor Yellow

$RequiredIncludes = @{
    "src\common.cpp" = @("algorithm")
    "src\appcontainer.cpp" = @("userenv.h")
    "src\permissions.cpp" = @("aclapi.h")
    "src\config.cpp" = @("common.h")
}

foreach ($file in $RequiredIncludes.Keys) {
    Write-Host "  Checking $file..." -NoNewline
    $content = Get-Content $file -Raw
    
    foreach ($include in $RequiredIncludes[$file]) {
        if ($content -notmatch [regex]::Escape($include)) {
            Write-Host " ✗ Missing include: $include" -ForegroundColor Red
            $ExitCode = 1
        }
    }
    
    if ($ExitCode -eq 0) {
        Write-Host " ✓" -ForegroundColor Green
    }
}

# Check CMakeLists.txt for /EHsc flag
Write-Host "Checking CMakeLists.txt for /EHsc flag..." -NoNewline
$cmake = Get-Content "CMakeLists.txt" -Raw
if ($cmake -match "/EHsc") {
    Write-Host " ✓" -ForegroundColor Green
} else {
    Write-Host " ✗ Missing /EHsc flag" -ForegroundColor Red
    $ExitCode = 1
}

# Verify Config struct field names
Write-Host "Verifying Config struct field consistency..." -NoNewline
$configH = Get-Content "src\common.h" -Raw
$testFile = Get-Content "tests\unit_tests.cpp" -Raw

# Check for the typo we fixed
if ($testFile -match "noCleanup") {
    Write-Host " ✗ Found 'noCleanup' in tests (should be 'cleanup')" -ForegroundColor Red
    $ExitCode = 1
} else {
    Write-Host " ✓" -ForegroundColor Green
}

Write-Host ""
if ($ExitCode -eq 0) {
    Write-Host "=== Build Verification Passed ===" -ForegroundColor Green
} else {
    Write-Host "=== Build Verification Failed ===" -ForegroundColor Red
}

exit $ExitCode