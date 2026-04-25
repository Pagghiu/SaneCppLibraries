$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest
$ProgressPreference = "SilentlyContinue"
if (Get-Variable -Name PSNativeCommandUseErrorActionPreference -ErrorAction SilentlyContinue) {
    $PSNativeCommandUseErrorActionPreference = $false
}

$ScriptDir = [System.IO.Path]::GetDirectoryName($MyInvocation.MyCommand.Path)
$ScriptDir = (Resolve-Path $ScriptDir).Path
$RepositoryUrl = "https://github.com/Pagghiu/SaneCppLibraries.git"

function Fail([string]$Message) {
    Write-Error $Message
    exit 1
}

function Test-LocalLibrariesRoot([string]$Root) {
    (Test-Path (Join-Path $Root "SC.cpp") -PathType Leaf) -and
    (Test-Path (Join-Path $Root "SC.bat") -PathType Leaf) -and
    (Test-Path (Join-Path $Root "Tools\ToolsBootstrap.c") -PathType Leaf)
}

function Resolve-CanonicalDirectory([string]$Path) {
    if (-not (Test-Path $Path -PathType Container)) {
        Fail "Directory `"$Path`" does not exist"
    }
    return (Resolve-Path $Path).Path
}

function Resolve-CanonicalFile([string]$Path) {
    $Parent = Resolve-CanonicalDirectory ([System.IO.Path]::GetDirectoryName($Path))
    return (Join-Path $Parent ([System.IO.Path]::GetFileName($Path)))
}

function Resolve-ProjectRoot([string]$StartDirectory) {
    $Current = Resolve-CanonicalDirectory $StartDirectory
    while ($true) {
        if (Test-Path (Join-Path $Current "SC-build.cpp") -PathType Leaf) {
            return $Current
        }
        $Parent = [System.IO.Directory]::GetParent($Current)
        if ($null -eq $Parent) {
            break
        }
        $Current = $Parent.FullName
    }
    return $null
}

function Get-RequestedGitRef([string]$ProjectFile) {
    foreach ($Line in [System.IO.File]::ReadLines($ProjectFile)) {
        if ($Line -match '^\s*//\s*sc-build-version:\s*(.+?)\s*$') {
            return $Matches[1]
        }
    }
    return $null
}

function Get-DefaultCacheBase() {
    if ($env:SC_BUILD_CACHE_DIR) {
        return $env:SC_BUILD_CACHE_DIR
    }
    if ($env:LOCALAPPDATA) {
        return (Join-Path $env:LOCALAPPDATA "SC-build")
    }
    return (Join-Path $HOME "AppData\Local\SC-build")
}

function Ensure-GitAvailable() {
    $null = Get-Command git -ErrorAction SilentlyContinue
    if ($null -eq (Get-Command git -ErrorAction SilentlyContinue)) {
        Fail '"git" is required to resolve the shared SaneCppLibraries installation'
    }
}

function Resolve-VcVarsAllPath() {
    $VsWhere = "C:\Program Files (x86)\Microsoft Visual Studio\Installer\vswhere.exe"
    if (-not (Test-Path $VsWhere -PathType Leaf)) {
        Fail "Visual Studio Locator not found at $VsWhere"
    }
    $InstallPath = (& $VsWhere -latest -property installationPath | Select-Object -First 1).Trim()
    if ([string]::IsNullOrWhiteSpace($InstallPath)) {
        Fail "Cannot locate a Visual Studio installation"
    }
    $VcVarsAll = Join-Path $InstallPath "VC\Auxiliary\Build\vcvarsall.bat"
    if (-not (Test-Path $VcVarsAll -PathType Leaf)) {
        Fail "vcvarsall.bat not found at $VcVarsAll"
    }
    return $VcVarsAll
}

function Ensure-SharedRepositoryCheckout([string]$CacheBase) {
    Ensure-GitAvailable
    $RepositoryRoot = Join-Path $CacheBase "repository"
    [System.IO.Directory]::CreateDirectory($CacheBase) | Out-Null
    if (-not (Test-Path (Join-Path $RepositoryRoot ".git") -PathType Container)) {
        Write-Host "Cloning SaneCppLibraries into $RepositoryRoot"
        & git clone $RepositoryUrl $RepositoryRoot | Out-Null
        if ($LASTEXITCODE -ne 0) {
            Fail "Failed cloning SaneCppLibraries"
        }
    }
    else {
        & git -C $RepositoryRoot fetch --prune --tags origin | Out-Null
        if ($LASTEXITCODE -ne 0) {
            Fail "Failed fetching SaneCppLibraries updates"
        }
    }
    & git -C $RepositoryRoot remote set-head origin -a | Out-Null
    return $RepositoryRoot
}

function Resolve-RequestedCommit([string]$RepositoryRoot, [string]$RequestedRef) {
    $Candidates = @(
        "$RequestedRef`^{commit}",
        "refs/remotes/origin/$RequestedRef`^{commit}",
        "refs/tags/$RequestedRef`^{commit}"
    )

    foreach ($Candidate in $Candidates) {
        $Command = 'git -C "{0}" rev-parse --verify "{1}" 2>NUL' -f $RepositoryRoot, $Candidate
        $Commit = (& cmd.exe /d /c $Command)
        if ($LASTEXITCODE -eq 0 -and -not [string]::IsNullOrWhiteSpace($Commit)) {
            return $Commit.Trim()
        }
    }

    return $null
}

function Resolve-SharedLibrariesRoot([string]$ProjectRoot) {
    $CacheBase = Get-DefaultCacheBase
    $CacheBase = [System.IO.Path]::GetFullPath($CacheBase)
    [System.IO.Directory]::CreateDirectory($CacheBase) | Out-Null

    $RepositoryRoot = Ensure-SharedRepositoryCheckout $CacheBase
    $ProjectFile = Join-Path $ProjectRoot "SC-build.cpp"
    $RequestedRef = Get-RequestedGitRef $ProjectFile
    if ([string]::IsNullOrWhiteSpace($RequestedRef)) {
        $RequestedRef = (& git -C $RepositoryRoot symbolic-ref --quiet --short refs/remotes/origin/HEAD 2>$null)
        if ([string]::IsNullOrWhiteSpace($RequestedRef)) {
            $RequestedRef = "origin/main"
        }
        Write-Warning "Missing `"// sc-build-version: <git-ref>`" in $ProjectFile, using $RequestedRef"
    }

    $Commit = Resolve-RequestedCommit $RepositoryRoot $RequestedRef
    if ([string]::IsNullOrWhiteSpace($Commit)) {
        Fail "Cannot resolve SaneCppLibraries revision `"$RequestedRef`""
    }

    $WorktreeRoot = Join-Path (Join-Path $CacheBase "worktrees") $Commit
    if ((Test-Path $WorktreeRoot -PathType Container) -and -not (Test-Path (Join-Path $WorktreeRoot "SC.bat") -PathType Leaf)) {
        Remove-Item -Recurse -Force $WorktreeRoot
    }
    if (-not (Test-Path $WorktreeRoot -PathType Container)) {
        [System.IO.Directory]::CreateDirectory((Split-Path -Parent $WorktreeRoot)) | Out-Null
        & git -C $RepositoryRoot worktree add --detach $WorktreeRoot $Commit | Out-Null
        if ($LASTEXITCODE -ne 0) {
            Fail "Failed creating SaneCppLibraries worktree for $Commit"
        }
    }
    return $WorktreeRoot
}

function Ensure-BootstrapExecutable([string]$LibrariesRoot) {
    if (-not (Test-LocalLibrariesRoot $LibrariesRoot)) {
        Fail "`"$LibrariesRoot`" is not a valid SaneCppLibraries checkout"
    }

    $BootstrapExe = Join-Path $LibrariesRoot "_Build\_Tools\Windows\ToolsBootstrap.exe"
    [System.IO.Directory]::CreateDirectory((Split-Path -Parent $BootstrapExe)) | Out-Null
    $SourceFile = Join-Path $LibrariesRoot "Tools\ToolsBootstrap.c"
    $NeedsBuild = -not (Test-Path $BootstrapExe -PathType Leaf) -or ((Get-Item $SourceFile).LastWriteTimeUtc -gt (Get-Item $BootstrapExe).LastWriteTimeUtc)
    if ($NeedsBuild) {
        Write-Host "ToolsBootstrap.c"
        $VcVarsAll = Resolve-VcVarsAllPath
        $ObjectFile = Join-Path $LibrariesRoot "_Build\_Tools\Windows\ToolsBootstrap.obj"
        $Command = 'call "{0}" x86_amd64 >nul && cl.exe /nologo /MTd /Zi /Od /D_DEBUG=1 /Fo"{1}" /c "{2}" && link /nologo /DEBUG /OUT:"{3}" "{1}" Shell32.lib' -f $VcVarsAll, $ObjectFile, $SourceFile, $BootstrapExe
        cmd.exe /d /c $Command | Out-Null
        if ($LASTEXITCODE -ne 0) {
            Fail "Failed building ToolsBootstrap"
        }
    }
    return $BootstrapExe
}

$ProjectDirOverride = $null
$LibrariesRootOverride = $env:SC_BUILD_LIBRARIES_ROOT
$ForwardedArguments = New-Object System.Collections.Generic.List[string]

for ($Index = 0; $Index -lt $args.Count; $Index++) {
    $Argument = [string]$args[$Index]
    if ($Argument -eq "--project-dir") {
        if ($Index + 1 -ge $args.Count) {
            Fail "Missing value after --project-dir"
        }
        $ProjectDirOverride = [string]$args[$Index + 1]
        $Index++
        continue
    }
    if ($Argument.StartsWith("--project-dir=")) {
        $ProjectDirOverride = $Argument.Substring("--project-dir=".Length)
        continue
    }
    if ($Argument -eq "--libraries-root") {
        if ($Index + 1 -ge $args.Count) {
            Fail "Missing value after --libraries-root"
        }
        $LibrariesRootOverride = [string]$args[$Index + 1]
        $Index++
        continue
    }
    if ($Argument.StartsWith("--libraries-root=")) {
        $LibrariesRootOverride = $Argument.Substring("--libraries-root=".Length)
        continue
    }
    $ForwardedArguments.Add($Argument) | Out-Null
}

$ProjectRoot =
    if ($ProjectDirOverride) { Resolve-CanonicalDirectory $ProjectDirOverride }
    else { Resolve-ProjectRoot (Get-Location).Path }
if ([string]::IsNullOrWhiteSpace($ProjectRoot)) {
    Fail "Cannot find SC-build.cpp by searching upward from `"$((Get-Location).Path)`". Use --project-dir <path> to select a project."
}

$ProjectFile = Resolve-CanonicalFile (Join-Path $ProjectRoot "SC-build.cpp")
$LibrariesRoot =
    if ($LibrariesRootOverride) { Resolve-CanonicalDirectory $LibrariesRootOverride }
    elseif (Test-LocalLibrariesRoot $ScriptDir) { $ScriptDir }
    else { Resolve-SharedLibrariesRoot $ProjectRoot }

$BootstrapExe = Ensure-BootstrapExecutable $LibrariesRoot
$BuildRoot = Join-Path $ProjectRoot "_Build"
[System.IO.Directory]::CreateDirectory($BuildRoot) | Out-Null

$VcVarsAll = Resolve-VcVarsAllPath
$EscapedArguments = New-Object System.Collections.Generic.List[string]
foreach ($Argument in @($LibrariesRoot, (Join-Path $LibrariesRoot "Tools"), $BuildRoot, $ProjectRoot, $ProjectFile) + $ForwardedArguments) {
    $EscapedArguments.Add(('"{0}"' -f $Argument.Replace('"', '""'))) | Out-Null
}
$BootstrapCommand = 'call "{0}" x86_amd64 >nul && "{1}" {2}' -f $VcVarsAll, $BootstrapExe, ($EscapedArguments -join ' ')
cmd.exe /d /c $BootstrapCommand
exit $LASTEXITCODE
