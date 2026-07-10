param(
    [string]$SamplesDir = "D:\samples\Harmony\list",
    [string]$TargetId = "23E0223A12011100",
    [string]$Keystore = "D:\ark-key\arks_debug.p12",
    [string]$KeyAlias = "arks_debug",
    [string]$Profile = "D:\ark-key\arks_debug_26Debug.p7b",
    [string]$Certificate = "D:\ark-key\ark_debug_26.cer",
    [string]$Output = "Testing\install-samples-smoke-report.json",
    [string]$SamplesFile = "",
    [string]$FromReport = "",
    [string[]]$Status = @(),
    [string[]]$StripPermission = @(),
    [string[]]$ForceDeviceType = @(),
    [int]$ForceCompatibleApi = 0,
    [int]$ForceTargetApi = 0,
    [int]$Limit = 0,
    [string]$Configuration = "Release"
)

$ErrorActionPreference = "Stop"

function Convert-SecureStringToPlain {
    param([securestring]$Value)

    if ($null -eq $Value) {
        return ""
    }

    $bstr = [Runtime.InteropServices.Marshal]::SecureStringToBSTR($Value)
    try {
        return [Runtime.InteropServices.Marshal]::PtrToStringBSTR($bstr)
    } finally {
        if ($bstr -ne [IntPtr]::Zero) {
            [Runtime.InteropServices.Marshal]::ZeroFreeBSTR($bstr)
        }
    }
}

$repoRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$exe = Join-Path $repoRoot "build\$Configuration\reark_install_samples_smoke_test.exe"
if (-not (Test-Path -LiteralPath $exe)) {
    throw "Smoke test executable not found: $exe. Build target reark_install_samples_smoke_test first."
}

function Get-ConfiguredSecret {
    param([string]$Name)

    $value = [Environment]::GetEnvironmentVariable($Name, "Process")
    if (-not [string]::IsNullOrEmpty($value)) {
        return $value
    }

    $value = [Environment]::GetEnvironmentVariable($Name, "User")
    if (-not [string]::IsNullOrEmpty($value)) {
        return $value
    }

    $value = [Environment]::GetEnvironmentVariable($Name, "Machine")
    if (-not [string]::IsNullOrEmpty($value)) {
        return $value
    }

    return ""
}

$generatedSamplesFile = $null
if (-not [string]::IsNullOrWhiteSpace($FromReport)) {
    if (-not (Test-Path -LiteralPath $FromReport)) {
        throw "Report not found: $FromReport"
    }

    $report = Get-Content -LiteralPath $FromReport -Raw | ConvertFrom-Json
    $selected = @($report.results)
    if ($Status.Count -gt 0) {
        $wanted = @{}
        foreach ($item in $Status) {
            if (-not [string]::IsNullOrWhiteSpace($item)) {
                $wanted[$item] = $true
            }
        }
        $selected = @($selected | Where-Object { $wanted.ContainsKey([string]$_.status) })
    } else {
        $selected = @($selected | Where-Object { [string]$_.status -notmatch 'ok$' })
    }

    $generatedSamplesFile = Join-Path ([IO.Path]::GetTempPath()) ("reark-install-smoke-samples-{0}.txt" -f ([Guid]::NewGuid().ToString("N")))
    $selected | ForEach-Object { $_.path } | Set-Content -LiteralPath $generatedSamplesFile -Encoding UTF8
    $SamplesFile = $generatedSamplesFile

    Write-Host ("Selected {0} sample(s) from {1}" -f $selected.Count, $FromReport)
    if ($Status.Count -gt 0) {
        Write-Host ("Status filter: {0}" -f ($Status -join ", "))
    } else {
        Write-Host "Status filter: non-ok statuses"
    }
}

$keystorePassword = Get-ConfiguredSecret "REARK_HARMONY_KEYSTORE_PASSWORD"
if ([string]::IsNullOrEmpty($keystorePassword)) {
    $keystorePasswordSecure = Read-Host "Harmony keystore password" -AsSecureString
    $keystorePassword = Convert-SecureStringToPlain $keystorePasswordSecure
}

$keyPassword = Get-ConfiguredSecret "REARK_HARMONY_KEY_PASSWORD"
if ([string]::IsNullOrEmpty($keyPassword)) {
    $keyPasswordSecure = Read-Host "Harmony key password (press Enter to reuse keystore password)" -AsSecureString
    $keyPassword = Convert-SecureStringToPlain $keyPasswordSecure
}
if ([string]::IsNullOrEmpty($keyPassword)) {
    $keyPassword = $keystorePassword
}

$oldStore = $env:REARK_HARMONY_KEYSTORE_PASSWORD
$oldKey = $env:REARK_HARMONY_KEY_PASSWORD
try {
    $env:REARK_HARMONY_KEYSTORE_PASSWORD = $keystorePassword
    $env:REARK_HARMONY_KEY_PASSWORD = $keyPassword

    $args = @(
        "--target", $TargetId,
        "--keystore", $Keystore,
        "--keystore-password-env", "REARK_HARMONY_KEYSTORE_PASSWORD",
        "--key-alias", $KeyAlias,
        "--key-password-env", "REARK_HARMONY_KEY_PASSWORD",
        "--profile", $Profile,
        "--certificate", $Certificate,
        "--output", $Output
    )
    if (-not [string]::IsNullOrWhiteSpace($SamplesFile)) {
        $args += @("--samples-file", $SamplesFile)
    } else {
        $args += @("--samples-dir", $SamplesDir)
    }
    if ($Limit -gt 0) {
        $args += @("--limit", [string]$Limit)
    }
    foreach ($permission in $StripPermission) {
        if (-not [string]::IsNullOrWhiteSpace($permission)) {
            $args += @("--strip-request-permission", $permission)
        }
    }
    foreach ($deviceType in $ForceDeviceType) {
        if (-not [string]::IsNullOrWhiteSpace($deviceType)) {
            $args += @("--force-device-type", $deviceType)
        }
    }
    if ($ForceCompatibleApi -gt 0) {
        $args += @("--force-compatible-api", [string]$ForceCompatibleApi)
    }
    if ($ForceTargetApi -gt 0) {
        $args += @("--force-target-api", [string]$ForceTargetApi)
    }

    & $exe @args
    if ($LASTEXITCODE -ne 0) {
        throw "Smoke test failed with exit code $LASTEXITCODE."
    }
} finally {
    $env:REARK_HARMONY_KEYSTORE_PASSWORD = $oldStore
    $env:REARK_HARMONY_KEY_PASSWORD = $oldKey
    if ($generatedSamplesFile -and (Test-Path -LiteralPath $generatedSamplesFile)) {
        Remove-Item -LiteralPath $generatedSamplesFile -Force
    }
}
