# Copyright (c) Stefano Cristiano
# SPDX-License-Identifier: MIT
$ErrorActionPreference = 'Stop'

if (-not $env:GITHUB_ENV) {
    throw 'GITHUB_ENV must identify the environment file for subsequent CI steps.'
}
if ($env:VSCMD_VER) {
    Write-Host "MSVC environment already initialized: $env:VSCMD_VER"
    return
}

$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
$installation = (& $vswhere -latest -property installationPath | Select-Object -First 1).Trim()
if (-not $installation) {
    throw 'Cannot locate the Visual Studio installation used by SC.bat.'
}
$vcvars = Join-Path $installation 'VC\Auxiliary\Build\vcvarsall.bat'
$before = @{}
Get-ChildItem Env: | ForEach-Object { $before[$_.Name] = $_.Value }

# Match SC.bat's compiler host/target and telemetry settings. JSON preserves Unicode,
# embedded equals signs and multiline values without printing the environment.
$command = 'set __VSCMD_ARG_NO_LOGO=1&& set VSCMD_SKIP_SENDTELEMETRY=1&& set VCPKG_KEEP_ENV_VARS=VSCMD_SKIP_SENDTELEMETRY&& call "{0}" x86_amd64 >nul && python -c "import json, os; print(json.dumps(dict(os.environ)))"' -f $vcvars
$serialized = & $env:ComSpec /d /c $command
if ($LASTEXITCODE -ne 0) {
    throw "MSVC environment initialization failed: $LASTEXITCODE"
}
$after = $serialized | ConvertFrom-Json
if (-not $after.VSCMD_VER -or $after.VSCMD_ARG_HOST_ARCH -ne 'x86' -or $after.VSCMD_ARG_TGT_ARCH -ne 'x64') {
    throw 'Expected the SC.bat x86-host/x64-target MSVC environment.'
}

$lines = [System.Collections.Generic.List[string]]::new()
foreach ($property in $after.PSObject.Properties) {
    $name = $property.Name
    $value = [string]$property.Value
    if ($before.ContainsKey($name) -and $before[$name] -ceq $value) { continue }
    if ($name -match '^(GITHUB_|RUNNER_)' -or $name -eq 'NODE_OPTIONS') {
        throw "Unexpected protected environment change: $name"
    }
    $delimiter = 'SC_MSVC_' + [guid]::NewGuid().ToString('N')
    $lines.Add("$name<<$delimiter")
    $lines.Add($value)
    $lines.Add($delimiter)
}
# Persist PATH in full to retain vcvarsall's exact search order, not just new entries.
[System.IO.File]::AppendAllLines($env:GITHUB_ENV, $lines, [System.Text.UTF8Encoding]::new($false))
Write-Host "Initialized MSVC $($after.VSCMD_VER): $($after.VSCMD_ARG_HOST_ARCH) -> $($after.VSCMD_ARG_TGT_ARCH)"
