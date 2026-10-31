param([string]$BuildDirectory = (Join-Path $PSScriptRoot '..\Release'))

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
Add-Type -AssemblyName System.Windows.Forms
Add-Type -AssemblyName System.Drawing
if (-not ('ClclSmokeNative' -as [type])) {
    Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
public static class ClclSmokeNative {
    [DllImport("user32.dll", CharSet = CharSet.Unicode)] public static extern IntPtr FindWindow(string className, string title);
    [DllImport("user32.dll", SetLastError = true)] public static extern IntPtr SendMessageTimeout(IntPtr window, uint message, IntPtr parameter, IntPtr data, uint flags, uint timeout, out UIntPtr result);
    [DllImport("user32.dll")] public static extern IntPtr GetDlgItem(IntPtr window, int id);
    [DllImport("user32.dll")] public static extern int GetWindowLong(IntPtr window, int index);
    [DllImport("user32.dll")] public static extern bool IsWindowVisible(IntPtr window);
    [DllImport("user32.dll")] public static extern uint GetDpiForWindow(IntPtr window);
    [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr window, out uint process);
    [DllImport("user32.dll", SetLastError = true)] public static extern bool RegisterHotKey(IntPtr window, int id, uint modifiers, uint key);
    [DllImport("user32.dll")] public static extern bool UnregisterHotKey(IntPtr window, int id);
    [DllImport("kernel32.dll", SetLastError = true)] public static extern IntPtr OpenProcess(uint access, bool inherit, uint process);
    [DllImport("kernel32.dll", SetLastError = true)] public static extern bool ReadProcessMemory(IntPtr process, IntPtr address, byte[] buffer, int size, out IntPtr read);
    [DllImport("kernel32.dll")] public static extern bool CloseHandle(IntPtr handle);
}
'@
}

function Assert-Clcl([bool]$Condition, [string]$Message) {
    if (-not $Condition) { throw $Message }
    Write-Output "[OKAY] $Message"
}

function Send-Clcl([IntPtr]$Window, [uint32]$Message, [IntPtr]$Parameter = [IntPtr]::Zero, [IntPtr]$Data = [IntPtr]::Zero) {
    $result = [UIntPtr]::Zero
    if ([ClclSmokeNative]::SendMessageTimeout($Window, $Message, $Parameter, $Data, 2, 5000, [ref]$result) -eq [IntPtr]::Zero) {
        throw "Window message failed: $Message"
    }
    return [IntPtr][long]$result.ToUInt64()
}

function Stop-Clcl([Diagnostics.Process]$Process) {
    if ($null -eq $Process -or $Process.HasExited) { return }
    $window = [ClclSmokeNative]::FindWindow('CLCLNewMain', 'CLCL_new')
    [uint32]$windowProcess = 0
    [void][ClclSmokeNative]::GetWindowThreadProcessId($window, [ref]$windowProcess)
    if ($windowProcess -ne $Process.Id) { throw 'Refusing to close an unrelated CLCL process' }
    [void](Send-Clcl $window 16)
    if (-not $Process.WaitForExit(5000)) { throw 'CLCL did not exit normally' }
}

function Start-Clcl([string]$Directory, [string]$Executable = 'CLCL.exe') {
    $process = Start-Process -FilePath (Join-Path $Directory $Executable) -WorkingDirectory $Directory -ArgumentList '/cmshow' -PassThru
    if ($Executable -eq 'CLCL.exe' -and -not $process.WaitForInputIdle(10000)) { throw 'CLCL did not become ready' }
    return $process
}

function Get-ClclTiles {
    $main = [ClclSmokeNative]::FindWindow('CLCLNewMain', 'CLCL_new')
    [void](Send-Clcl $main 786 ([IntPtr]0x7F01))
    $viewer = [ClclSmokeNative]::FindWindow('CLCLViewer', 'ClipMaster')
    [void](Send-Clcl $viewer 33890)
    $tiles = [ClclSmokeNative]::GetDlgItem($viewer, 125)
    [uint32]$processId = 0
    [void][ClclSmokeNative]::GetWindowThreadProcessId($tiles, [ref]$processId)
    $handle = [ClclSmokeNative]::OpenProcess(16, $false, $processId)
    try {
        $state = [IntPtr][ClclSmokeNative]::GetWindowLong($tiles, -21)
        $header = New-Object byte[] 8
        $read = [IntPtr]::Zero
        if (-not [ClclSmokeNative]::ReadProcessMemory($handle, $state, $header, 8, [ref]$read)) { throw 'Cannot read x86 tile state' }
        $count = [BitConverter]::ToInt32($header, 4)
        $entries = New-Object byte[] ($count * 72)
        if ($count -gt 0 -and -not [ClclSmokeNative]::ReadProcessMemory($handle, [IntPtr][BitConverter]::ToInt32($header, 0), $entries, $entries.Length, [ref]$read)) {
            throw 'Cannot read x86 tile entries'
        }
        return @{ Count = $count; Entries = $entries; Window = $tiles; Viewer = $viewer; Main = $main }
    } finally { if ($handle -ne [IntPtr]::Zero) { [void][ClclSmokeNative]::CloseHandle($handle) } }
}

$BuildDirectory = (Resolve-Path -LiteralPath $BuildDirectory).Path
$temporary = Join-Path ([IO.Path]::GetTempPath()) ('CLCL_smoke_' + [Guid]::NewGuid().ToString('N'))
$existing = @(Get-Process CLCL -ErrorAction SilentlyContinue | Where-Object { $_.Path -eq (Join-Path $BuildDirectory 'CLCL.exe') })
$testProcess = $null
$clipboardChanged = $false
$clipboardBackup = New-Object System.Windows.Forms.DataObject
$originalClipboard = [System.Windows.Forms.Clipboard]::GetDataObject()
if ($null -ne $originalClipboard) {
    foreach ($format in $originalClipboard.GetFormats($false)) {
        $content = $originalClipboard.GetData($format, $false)
        if ($null -ne $content) { $clipboardBackup.SetData($format, $false, $content) }
    }
}

try {
    foreach ($process in $existing) { Stop-Clcl $process }
    if ([ClclSmokeNative]::FindWindow('CLCLNewMain', 'CLCL_new') -ne [IntPtr]::Zero) { throw 'Another CLCL instance is running' }
    [void][IO.Directory]::CreateDirectory($temporary)
    foreach ($file in 'CLCL.exe', 'CLCLSet.exe', 'CLCLHook.dll') { Copy-Item -LiteralPath (Join-Path $BuildDirectory $file) -Destination $temporary }
    [IO.File]::WriteAllText((Join-Path $temporary 'clcl_app.ini'), "[GENERAL]`r`nportable=1`r`n")
    [IO.File]::WriteAllText((Join-Path $temporary 'clcl.ini'), "[main]`r`nclipboard_watch=1`r`nshow_trayicon=0`r`n[config]`r`nstart_with_windows=0`r`nskip_passwords=0`r`n")
    $testProcess = Start-Clcl $temporary
    [void](Send-Clcl ([ClclSmokeNative]::FindWindow('CLCLNewMain', 'CLCL_new')) 275 ([IntPtr]1))
    $initial = Get-ClclTiles
    Assert-Clcl ((Send-Clcl $initial.Main 32870).ToInt32() -eq 1) 'Clipboard monitoring enabled'
    $clipboardChanged = $true
    [System.Windows.Forms.Clipboard]::SetText('CLCL smoke ' + [Guid]::NewGuid().ToString())
    [void](Send-Clcl $initial.Main 275 ([IntPtr]1))
    $text = Get-ClclTiles
    Assert-Clcl ($text.Count -eq $initial.Count + 1) "Text copy adds one tile ($($initial.Count) -> $($text.Count))"
    $bitmap = New-Object System.Drawing.Bitmap 137, 83
    try {
        $graphics = [System.Drawing.Graphics]::FromImage($bitmap)
        try { $graphics.Clear([System.Drawing.Color]::MediumSeaGreen) } finally { $graphics.Dispose() }
        [System.Windows.Forms.Clipboard]::SetImage($bitmap)
    } finally { $bitmap.Dispose() }
    [void](Send-Clcl $initial.Main 275 ([IntPtr]1))
    $image = Get-ClclTiles
    Assert-Clcl ($image.Count -eq $text.Count + 1) "Image copy adds one tile ($($text.Count) -> $($image.Count))"
    $dpi = [ClclSmokeNative]::GetDpiForWindow($image.Window)
    $cell = [int][Math]::Floor($dpi + 0.5)
    $gap = [int][Math]::Floor(8 * $dpi / 96.0 + 0.5)
    $margin = [int][Math]::Floor(12 * $dpi / 96.0 + 0.5)
    for ($index = 0; $index -lt $image.Count; $index++) {
        $left = [BitConverter]::ToInt32($image.Entries, $index * 72 + 36)
        $right = [BitConverter]::ToInt32($image.Entries, $index * 72 + 44)
        Assert-Clcl ((($left - $margin) % ($cell + $gap)) -eq 0 -and (($right - $left + $gap) % ($cell + $gap)) -eq 0) "Tile $index follows the grid"
    }
    foreach ($tab in 0, 1, 2, 3, 0) {
        [void](Send-Clcl $image.Viewer 33890 ([IntPtr]$tab))
        [void](Send-Clcl $image.Main 33068)
        [void](Send-Clcl $image.Viewer 33118)
        $root = Send-Clcl ([ClclSmokeNative]::GetDlgItem($image.Viewer, 100)) 4362
        if ($root -ne [IntPtr]::Zero) { [void](Send-Clcl ([ClclSmokeNative]::GetDlgItem($image.Viewer, 100)) 4363 ([IntPtr]9) $root) }
        foreach ($control in 100, 101) {
            Assert-Clcl (([ClclSmokeNative]::GetWindowLong([ClclSmokeNative]::GetDlgItem($image.Viewer, $control), -16) -band 0x10000000) -eq 0) "Legacy control $control stays hidden on tab $tab"
        }
        $visibleControl = @(125, 128, 129, 122)[$tab]
        Assert-Clcl ([ClclSmokeNative]::IsWindowVisible([ClclSmokeNative]::GetDlgItem($image.Viewer, $visibleControl))) "Tab $tab content is visible"
    }
    $spaceAvailable = [ClclSmokeNative]::RegisterHotKey([IntPtr]::Zero, 301, 1, 32)
    if ($spaceAvailable) { [void][ClclSmokeNative]::UnregisterHotKey([IntPtr]::Zero, 301) }
    Assert-Clcl (-not $spaceAvailable) 'Alt+Space is registered'
    $enterAvailable = [ClclSmokeNative]::RegisterHotKey([IntPtr]::Zero, 302, 1, 13)
    if ($enterAvailable) { [void][ClclSmokeNative]::UnregisterHotKey([IntPtr]::Zero, 302) }
    Assert-Clcl $enterAvailable 'Alt+Enter is not registered'
    Assert-Clcl ((Send-Clcl $image.Main 33071 ([IntPtr]1)).ToInt32() -ne 0) 'History saves successfully'
    Stop-Clcl $testProcess
    $testProcess = Start-Clcl $temporary
    $reloaded = Get-ClclTiles
    Assert-Clcl ($reloaded.Count -eq $image.Count) 'History survives restart'
    $launcher = Start-Clcl $temporary 'CLCLSet.exe'
    if (-not $launcher.WaitForExit(10000)) { throw 'Settings launcher did not exit' }
    [void](Send-Clcl $reloaded.Main 0)
    Assert-Clcl ((Send-Clcl ([ClclSmokeNative]::GetDlgItem($reloaded.Viewer, 120)) 4875).ToInt32() -eq 3) 'Settings launcher selects Settings'
    Write-Output '[OKAY] All isolated smoke tests passed'
} finally {
    if ($null -ne $testProcess -and -not $testProcess.HasExited) {
        try { Stop-Clcl $testProcess } catch { $testProcess.Kill(); $testProcess.WaitForExit() }
    }
    if ($clipboardChanged) { [System.Windows.Forms.Clipboard]::SetDataObject($clipboardBackup, $true) }
    if (Test-Path -LiteralPath $temporary) { Remove-Item -LiteralPath $temporary -Recurse -Force }
    if ($existing.Count -gt 0) { Start-Process -FilePath (Join-Path $BuildDirectory 'CLCL.exe') -WorkingDirectory $BuildDirectory }
}