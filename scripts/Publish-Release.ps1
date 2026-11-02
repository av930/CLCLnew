#Requires -Version 5.1
<#
.SYNOPSIS
Builds, tests, packages, and publishes a ClipMaster Windows x64 release.

.DESCRIPTION
Creates a date-based GitHub release for av930/CLCLnew from the clean, pushed
main branch. The release ZIP contains only the six files used by the public
x64 package.

.NOTES
Requires Git, Visual Studio C++ build tools, and GitHub CLI authenticated with
"gh auth login". The default x64 tests temporarily use the clipboard and close
and restart running CLCL.exe processes. Use -SkipTests to omit those checks.

.EXAMPLE
.\scripts\Publish-Release.cmd

.EXAMPLE
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\scripts\Publish-Release.ps1 -Version 2026.11.02-2
#>
[CmdletBinding(SupportsShouldProcess = $true, ConfirmImpact = 'High')]
param(
	[ValidatePattern('^\d{4}\.\d{2}\.\d{2}(?:-\d+)?$')]
	[string]$Version = (Get-Date -Format 'yyyy.MM.dd'),
	[switch]$SkipTests
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$root = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '..')).Path
$repository = 'av930/CLCLnew'
$tag = "v$Version"
$archiveName = "ClipMaster-$Version-windows-x64.zip"
$releaseDirectory = Join-Path $root 'artifacts\releases'
$archivePath = Join-Path $releaseDirectory $archiveName
$buildDirectory = Join-Path $root 'x64\Release'
$temporaryDirectory = $null

function Write-Step([string]$Message) {
	Write-Host "==> $Message" -ForegroundColor Cyan
}

function Invoke-Git([string[]]$Arguments) {
	$output = & $gitCommand.Source -C $root @Arguments
	if ($LASTEXITCODE -ne 0) {
		throw "Git command failed: git $($Arguments -join ' ')"
	}
	return $output
}

function Assert-Amd64([string]$Path) {
	$bytes = [System.IO.File]::ReadAllBytes($Path)
	if ($bytes.Length -lt 64) {
		throw "Invalid executable: $Path"
	}
	$peOffset = [BitConverter]::ToInt32($bytes, 0x3C)
	if ($peOffset -lt 0 -or $peOffset + 6 -gt $bytes.Length -or
		[BitConverter]::ToUInt16($bytes, $peOffset + 4) -ne 0x8664) {
		throw "Expected an AMD64 binary: $Path"
	}
	Write-Host "[OKAY] AMD64: $(Split-Path -Leaf $Path)" -ForegroundColor Green
}

try {
	if ($WhatIfPreference) {
		[void]$PSCmdlet.ShouldProcess("$repository $tag", 'Build, test, package, and publish')
		return
	}

	$gitCommand = Get-Command git.exe -CommandType Application -ErrorAction SilentlyContinue
	if ($null -eq $gitCommand) {
		throw 'Git for Windows is required.'
	}
	$ghCommand = Get-Command gh.exe -CommandType Application -ErrorAction SilentlyContinue
	if ($null -eq $ghCommand) {
		throw 'GitHub CLI is required. Install GitHub CLI, then run: gh auth login'
	}

	$originUrl = (Invoke-Git @('remote', 'get-url', 'origin')).Trim()
	if ($originUrl -notmatch '(?i)github\.com[:/]av930/CLCLnew(?:\.git)?/?$') {
		throw "The origin remote must point to $repository."
	}

	& $ghCommand.Source auth status --hostname github.com *> $null
	if ($LASTEXITCODE -ne 0) {
		throw 'GitHub CLI is not authenticated. Run: gh auth login'
	}

	$branch = (Invoke-Git @('branch', '--show-current')).Trim()
	if ($branch -ne 'main') {
		throw "Release from main only. Current branch: $branch"
	}
	$changes = @(Invoke-Git @('status', '--porcelain', '--untracked-files=normal'))
	if ($changes.Count -gt 0) {
		throw 'The working tree is not clean. Commit or remove all changes before releasing.'
	}

	Write-Step 'Checking that local main matches origin/main'
	$null = Invoke-Git @('fetch', '--quiet', 'origin', '+refs/heads/main:refs/remotes/origin/main')
	$localHead = (Invoke-Git @('rev-parse', 'HEAD')).Trim()
	$remoteHead = (Invoke-Git @('rev-parse', 'refs/remotes/origin/main')).Trim()
	if ($localHead -ne $remoteHead) {
		throw 'Local main differs from origin/main. Push or pull changes before releasing.'
	}
	$localTag = @(Invoke-Git @('tag', '--list', $tag))
	$remoteTag = @(Invoke-Git @('ls-remote', '--refs', 'origin', "refs/tags/$tag"))
	if ($localTag.Count -gt 0 -or $remoteTag.Count -gt 0) {
		throw "Tag $tag already exists. Supply a new -Version, for example $Version-2."
	}
	if (Test-Path -LiteralPath $archivePath) {
		throw "The release archive already exists: $archivePath"
	}

	if (-not $PSCmdlet.ShouldProcess("$repository $tag", 'Build, test, package, and publish')) {
		return
	}

	$vsWhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
	if (-not (Test-Path -LiteralPath $vsWhere)) {
		throw 'vswhere.exe was not found. Install Visual Studio C++ build tools.'
	}
	$msBuild = & $vsWhere -latest -products '*' `
		-requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 `
		-find 'MSBuild\**\Bin\MSBuild.exe' | Select-Object -First 1
	if ([string]::IsNullOrWhiteSpace($msBuild) -or -not (Test-Path -LiteralPath $msBuild)) {
		throw 'MSBuild with the C++ x64 toolchain was not found.'
	}

	Write-Step 'Building Release|x64'
	$buildArguments = @(
		(Join-Path $root 'CLCL.sln'),
		'/nologo',
		'/v:minimal',
		'/m:1',
		'/nodeReuse:false',
		'/p:Configuration=Release',
		'/p:Platform=x64'
	)
	& $msBuild @buildArguments
	if ($LASTEXITCODE -ne 0) {
		throw "x64 build failed with exit code $LASTEXITCODE."
	}

	$binaryNames = @('CLCL.exe', 'CLCLHook.dll', 'CLCLSet.exe')
	foreach ($name in $binaryNames) {
		$binaryPath = Join-Path $buildDirectory $name
		if (-not (Test-Path -LiteralPath $binaryPath)) {
			throw "Build output is missing: $binaryPath"
		}
		Assert-Amd64 $binaryPath
	}

	$testSummary = 'Automated x64 checks were skipped.'
	if (-not $SkipTests) {
		Write-Step 'Running isolated x64 release checks'
		Write-Host 'The test temporarily uses the clipboard and closes/restarts running CLCL.exe processes.' -ForegroundColor Yellow
		$testScript = Join-Path $root 'tests\ReleaseX64Tests.ps1'
		$powerShell = Join-Path $PSHOME 'powershell.exe'
		& $powerShell -NoProfile -ExecutionPolicy Bypass -File $testScript -BuildDirectory $buildDirectory
		if ($LASTEXITCODE -ne 0) {
			throw "x64 release checks failed with exit code $LASTEXITCODE."
		}
		$testSummary = 'Isolated x64 checks passed for startup, tabs, text/image capture, image Favorites, and persistence after restart.'
	}

	Write-Step 'Creating the release ZIP'
	New-Item -Path $releaseDirectory -ItemType Directory -Force | Out-Null
	$temporaryDirectory = Join-Path ([System.IO.Path]::GetTempPath()) ('CLCL-release-' + [Guid]::NewGuid().ToString('N'))
	New-Item -Path $temporaryDirectory -ItemType Directory -Force | Out-Null
	$stageDirectory = Join-Path $temporaryDirectory 'package'
	New-Item -Path $stageDirectory -ItemType Directory -Force | Out-Null
	$packageNames = @('CLCL.exe', 'CLCLHook.dll', 'CLCLSet.exe', 'clcl_app.ini', 'LICENSE', 'read.md')
	foreach ($name in $packageNames) {
		$sourceDirectory = if ($name -in $binaryNames) { $buildDirectory } else { $root }
		$sourcePath = Join-Path $sourceDirectory $name
		if (-not (Test-Path -LiteralPath $sourcePath)) {
			throw "Release package file is missing: $sourcePath"
		}
		Copy-Item -LiteralPath $sourcePath -Destination (Join-Path $stageDirectory $name)
	}

	Add-Type -AssemblyName System.IO.Compression.FileSystem
	[System.IO.Compression.ZipFile]::CreateFromDirectory(
		$stageDirectory, $archivePath, [System.IO.Compression.CompressionLevel]::Optimal, $false)
	$zip = [System.IO.Compression.ZipFile]::OpenRead($archivePath)
	try {
		$actualNames = @($zip.Entries | ForEach-Object { $_.FullName } | Sort-Object)
		$expectedNames = @($packageNames | Sort-Object)
		if (($actualNames -join '|') -ne ($expectedNames -join '|')) {
			throw 'The release ZIP contents do not match the expected six files.'
		}
	} finally {
		$zip.Dispose()
	}
	$zipHash = (Get-FileHash -LiteralPath $archivePath -Algorithm SHA256).Hash

	$verificationNote = if ($SkipTests) {
		$testSummary
	} else {
		"$testSummary External application pasting and third-party plugins were not automated."
	}
	$notes = @"
## Download and run

Windows 10/11 x64 only. Download $archiveName from Assets below.

1. Extract the entire ZIP into a folder.
2. Run CLCL.exe. Keep the included EXE and DLL files together.
3. Copy text or an image, press Alt+Space, and choose a clip to paste.

- CapsLock+F: Favorites
- CapsLock+V: Paste formats and special characters
- Change shortcuts and history retention in Setting.

The automatic Source code downloads are not executable packages. No x86 binary is provided.

## Included files

CLCL.exe, CLCLHook.dll, CLCLSet.exe, default clcl_app.ini, LICENSE, read.md. No user settings, clipboard history, or debug files are included.

## Verification

All three binaries are AMD64. $verificationNote
Use matching x64 plugins.

Copied content may be saved in clipboard history. Disable recording before copying passwords or other sensitive information.

ClipMaster is a separate CLCL-based clone, originally based on CLCL by Ohno Tomoaki, distributed under the MIT License.

SHA-256 (ZIP): $zipHash
"@
	$notesPath = Join-Path $temporaryDirectory 'release-notes.md'
	[System.IO.File]::WriteAllText($notesPath, $notes, (New-Object System.Text.UTF8Encoding $false))

	Write-Step "Publishing $tag to $repository"
	$releaseArguments = @(
		'release', 'create', $tag, $archivePath,
		'--repo', $repository,
		'--title', "ClipMaster v$Version - Windows x64",
		'--notes-file', $notesPath,
		'--target', $localHead
	)
	& $ghCommand.Source @releaseArguments
	if ($LASTEXITCODE -ne 0) {
		throw "GitHub release creation failed with exit code $LASTEXITCODE. The ZIP is retained at $archivePath."
	}

	$releaseUrl = & $ghCommand.Source release view $tag --repo $repository --json url --jq .url
	if ($LASTEXITCODE -ne 0) {
		throw "Release was created, but its URL could not be queried. Check $repository/releases/tag/$tag"
	}
	Write-Host "[OKAY] Published: $releaseUrl" -ForegroundColor Green
	Write-Host "[OKAY] ZIP SHA-256: $zipHash" -ForegroundColor Green
} catch {
	Write-Host "[FAIL] $($_.Exception.Message)" -ForegroundColor Red
	exit 1
} finally {
	if ($null -ne $temporaryDirectory -and (Test-Path -LiteralPath $temporaryDirectory)) {
		Remove-Item -LiteralPath $temporaryDirectory -Recurse -Force
	}
}