param([string]$BuildDirectory = (Join-Path $PSScriptRoot '..\Release'), [switch]$LayoutOnly, [switch]$SettingsOnly, [switch]$CategoryOnly, [switch]$SampleOnly)

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
    [DllImport("user32.dll")] public static extern bool GetClientRect(IntPtr window, [Out] int[] rectangle);
    [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr window, out uint process);
    [DllImport("user32.dll", SetLastError = true)] public static extern bool RegisterHotKey(IntPtr window, int id, uint modifiers, uint key);
    [DllImport("user32.dll")] public static extern bool UnregisterHotKey(IntPtr window, int id);
    [DllImport("kernel32.dll", SetLastError = true)] public static extern IntPtr OpenProcess(uint access, bool inherit, uint process);
    [DllImport("kernel32.dll", SetLastError = true)] public static extern bool ReadProcessMemory(IntPtr process, IntPtr address, byte[] buffer, int size, out IntPtr read);
    [DllImport("kernel32.dll", SetLastError = true)] public static extern bool WriteProcessMemory(IntPtr process, IntPtr address, byte[] buffer, int size, out IntPtr written);
    [DllImport("kernel32.dll")] public static extern bool CloseHandle(IntPtr handle);
}
'@
}

if (-not ('ClclEditorSmokeNative' -as [type])) {
    Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
public static class ClclEditorSmokeNative {
    [DllImport("user32.dll", CharSet = CharSet.Unicode)] public static extern bool PostMessage(IntPtr window, uint message, IntPtr parameter, IntPtr data);
}
'@
}

if (-not ('ClclTabSmokeNative' -as [type])) {
    Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
public static class ClclTabSmokeNative {
    [DllImport("user32.dll")] public static extern bool PrintWindow(IntPtr window, IntPtr context, uint flags);
    [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr window, [Out] int[] rectangle);
    [DllImport("user32.dll")] public static extern bool MoveWindow(IntPtr window, int x, int y, int width, int height, bool repaint);
    [DllImport("user32.dll")] public static extern bool SystemParametersInfo(uint action, uint parameter, [Out] int[] rectangle, uint flags);
    [DllImport("dwmapi.dll")] public static extern int DwmFlush();
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

function Get-ClclText([IntPtr]$Window) {
    $buffer = [Runtime.InteropServices.Marshal]::AllocHGlobal(512)
    try {
        [void](Send-Clcl $Window 13 ([IntPtr]256) $buffer)
        return [Runtime.InteropServices.Marshal]::PtrToStringAnsi($buffer)
    } finally { [Runtime.InteropServices.Marshal]::FreeHGlobal($buffer) }
}

function Set-ClclText([IntPtr]$Window, [string]$Text) {
    $buffer = [Runtime.InteropServices.Marshal]::StringToHGlobalAnsi($Text)
    try { [void](Send-Clcl $Window 12 ([IntPtr]::Zero) $buffer) }
    finally { [Runtime.InteropServices.Marshal]::FreeHGlobal($buffer) }
}

function Set-ClclImage([Drawing.Bitmap]$Bitmap) {
    $data = New-Object System.Windows.Forms.DataObject
    $data.SetImage($Bitmap)
    [System.Windows.Forms.Clipboard]::SetDataObject($data, $true, 10, 100)
}

function Paste-ClclEditorImage([IntPtr]$Editor, [int]$Width, [int]$Height) {
    $preview = [ClclSmokeNative]::GetDlgItem($Editor, 307)
    [uint32]$processId = 0
    [void][ClclSmokeNative]::GetWindowThreadProcessId($preview, [ref]$processId)
    $handle = [ClclSmokeNative]::OpenProcess(16, $false, $processId)
    $timer = [Diagnostics.Stopwatch]::StartNew()
    try {
        do {
            [void](Send-Clcl $Editor 273 ([IntPtr]302))
            $header = New-Object byte[] 8
            $entry = New-Object byte[] 72
            $read = [IntPtr]::Zero
            $state = [IntPtr][ClclSmokeNative]::GetWindowLong($preview, -21)
            $matched = [ClclSmokeNative]::ReadProcessMemory($handle, $state, $header, 8, [ref]$read) -and
                [BitConverter]::ToInt32($header, 4) -gt 0 -and
                [ClclSmokeNative]::ReadProcessMemory($handle, [IntPtr][BitConverter]::ToInt32($header, 0), $entry, 72, [ref]$read) -and
                [BitConverter]::ToInt32($entry, 12) -eq $Width -and [BitConverter]::ToInt32($entry, 16) -eq $Height
        } while (-not $matched -and $timer.ElapsedMilliseconds -lt 3000)
        Assert-Clcl $matched "Editor receives image $Width x $Height before saving"
    } finally { [void][ClclSmokeNative]::CloseHandle($handle) }
}

function Start-Clcl([string]$Directory, [string]$Executable = 'CLCL.exe') {
    $process = Start-Process -FilePath (Join-Path $Directory $Executable) -WorkingDirectory $Directory -ArgumentList '/cmshow' -PassThru
    if ($Executable -eq 'CLCL.exe' -and -not $process.WaitForInputIdle(10000)) { throw 'CLCL did not become ready' }
    return $process
}

function Open-ClclEditor([IntPtr]$Viewer, [int]$Kind, [IntPtr]$Original = [IntPtr]::Zero) {
    $message = if ($Original -eq [IntPtr]::Zero) { 33911 } else { 33912 }
    Assert-Clcl ([ClclEditorSmokeNative]::PostMessage($Viewer, $message, [IntPtr]$Kind, $Original)) 'Open favorite editor'
    $timer = [Diagnostics.Stopwatch]::StartNew()
    do {
        $editor = [ClclSmokeNative]::FindWindow('ClipMasterEditor', 'Favorite Clip')
    } while (($editor -eq [IntPtr]::Zero -or -not [ClclSmokeNative]::IsWindowVisible($editor)) -and $timer.ElapsedMilliseconds -lt 3000)
    Assert-Clcl ($editor -ne [IntPtr]::Zero) 'Favorite editor is ready'
    return $editor
}

function Get-ClclTiles([int]$Identifier = 125, [switch]$NoRefresh) {
    $main = [ClclSmokeNative]::FindWindow('CLCLNewMain', 'CLCL_new')
    if (-not $NoRefresh) { [void](Send-Clcl $main 786 ([IntPtr]0x7F01)) }
    $viewer = [ClclSmokeNative]::FindWindow('CLCLViewer', 'ClipMaster')
    if (-not $NoRefresh) { [void](Send-Clcl $viewer 33890 ([IntPtr][int]$(switch ($Identifier) { 128 { 1 } 131 { 3 } default { 0 } }))) }
    $tiles = [ClclSmokeNative]::GetDlgItem($viewer, $Identifier)
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
        $layout = New-Object byte[] 44
        if (-not [ClclSmokeNative]::ReadProcessMemory($handle, [IntPtr]($state.ToInt64() + 544), $layout, $layout.Length, [ref]$read)) {
            throw 'Cannot read x86 layout cache'
        }
        $positionCount = [BitConverter]::ToInt32($layout, 4)
        $positions = New-Object byte[] ($positionCount * 56)
        if ($positionCount -gt 0 -and -not [ClclSmokeNative]::ReadProcessMemory($handle, [IntPtr][BitConverter]::ToInt32($layout, 0), $positions, $positions.Length, [ref]$read)) {
            throw 'Cannot read chronological positions'
        }
        $order = New-Object byte[] ($count * 4)
        if ($count -gt 0 -and -not [ClclSmokeNative]::ReadProcessMemory($handle, [IntPtr][BitConverter]::ToInt32($layout, 40), $order, $order.Length, [ref]$read)) {
            throw 'Cannot read paint order'
        }
        return @{ Count = $count; Entries = $entries; Positions = $positions; Order = $order; Window = $tiles; Viewer = $viewer; Main = $main }
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
    [IO.File]::WriteAllText((Join-Path $temporary 'clcl.ini'), "[main]`r`nclipboard_watch=1`r`nshow_trayicon=0`r`n[config]`r`nstart_with_windows=0`r`n")
    if ($CategoryOnly) { [IO.File]::AppendAllText((Join-Path $temporary 'clcl.ini'), "exclude_audio=0`r`n") }
    $testProcess = Start-Clcl $temporary
    $clipboardChanged = $true
    [System.Windows.Forms.Clipboard]::SetDataObject('CLCL test baseline ' + [Guid]::NewGuid().ToString(), $true, 10, 100)
    [void](Send-Clcl ([ClclSmokeNative]::FindWindow('CLCLNewMain', 'CLCL_new')) 275 ([IntPtr]1))
    $initial = Get-ClclTiles
    Assert-Clcl ((Send-Clcl $initial.Main 32870).ToInt32() -eq 1) 'Clipboard monitoring enabled'
    $clipboardChanged = $true
    [System.Windows.Forms.Clipboard]::SetDataObject('CLCL smoke password passwd secret token ' + [Guid]::NewGuid().ToString(), $true, 10, 100)
    $captureTimer = [Diagnostics.Stopwatch]::StartNew()
    do {
        [void](Send-Clcl $initial.Main 275 ([IntPtr]1))
        $text = Get-ClclTiles
    } while ($text.Count -eq $initial.Count -and $captureTimer.ElapsedMilliseconds -lt 3000)
    Assert-Clcl ($text.Count -eq $initial.Count + 1) "Text copy adds one tile ($($initial.Count) -> $($text.Count))"
    if ($CategoryOnly) {
        $montage = New-Object Drawing.Bitmap 1296, 448
        $montageGraphics = [Drawing.Graphics]::FromImage($montage)
        $montageGraphics.Clear([Drawing.Color]::White)
        $masks = New-Object 'Collections.Generic.HashSet[string]'
        $categoryIndex = 0
        try {
            foreach ($letter in 'T', 'R', 'I', 'F', 'A', 'D') {
                $before = Get-ClclTiles
                $data = New-Object Windows.Forms.DataObject
                $imageFixture = $null
                switch ($letter) {
                    'T' { $data.SetText('Text category') }
                    'R' { $data.SetText('RichText / HTML category'); $data.SetData('Rich Text Format', $false, '{\rtf1 RichText category}') }
                    'I' {
                        $imageFixture = New-Object Drawing.Bitmap 83, 137
                        $graphics = [Drawing.Graphics]::FromImage($imageFixture)
                        try { $graphics.Clear([Drawing.Color]::MediumSeaGreen) } finally { $graphics.Dispose() }
                        $data.SetImage($imageFixture)
                    }
                    'F' {
                        $paths = New-Object Collections.Specialized.StringCollection
                        [void]$paths.Add((Join-Path $temporary 'CLCL.exe'))
                        $data.SetFileDropList($paths)
                        $data.SetText('File category')
                    }
                    'A' { $data.SetText('Audio category'); $data.SetData('audio/wav', $false, 'Audio payload') }
                    'D' { $data.SetText('Device category') }
                }
                try { [Windows.Forms.Clipboard]::SetDataObject($data, $true, 10, 100) }
                finally { if ($imageFixture) { $imageFixture.Dispose() } }
                $timer = [Diagnostics.Stopwatch]::StartNew()
                do {
                    [void](Send-Clcl $before.Main 275 ([IntPtr]1))
                    $tiles = Get-ClclTiles
                } while ($tiles.Count -eq $before.Count -and $timer.ElapsedMilliseconds -lt 3000)
                Assert-Clcl ($tiles.Count -eq $before.Count + 1) "Category $letter fixture is captured"
                if ($letter -eq 'D') {
                    $handle = [ClclSmokeNative]::OpenProcess(56, $false, $testProcess.Id)
                    try {
                        $metadata = New-Object byte[] 100
                        $read = [IntPtr]::Zero
                        Assert-Clcl ([ClclSmokeNative]::ReadProcessMemory($handle, [IntPtr][BitConverter]::ToInt32($tiles.Entries, 0), $metadata, 100, [ref]$read)) 'Read isolated device fixture'
                        $address = [BitConverter]::ToInt32($metadata, 92)
                        while ($address -ne 0) {
                            Assert-Clcl ([ClclSmokeNative]::ReadProcessMemory($handle, [IntPtr]$address, $metadata, 100, [ref]$read)) 'Read isolated device format'
                            if ([BitConverter]::ToInt32($metadata, 20) -in 1, 7, 13) {
                                Assert-Clcl ([ClclSmokeNative]::WriteProcessMemory($handle, [IntPtr]($address + 20), [BitConverter]::GetBytes([int]129), 4, [ref]$read)) 'Simulate CF_DSPTEXT device format'
                            }
                            $address = [BitConverter]::ToInt32($metadata, 96)
                        }
                    } finally { [void][ClclSmokeNative]::CloseHandle($handle) }
                    $tiles = Get-ClclTiles
                }
                $favoritesBefore = Get-ClclTiles 128
                $tiles = Get-ClclTiles
                Assert-Clcl ([ClclEditorSmokeNative]::PostMessage($tiles.Viewer, 33909, [IntPtr]::Zero,
                    [IntPtr][BitConverter]::ToInt32($tiles.Entries, 0))) "Add category $letter to Favorites"
                $dialogTimer = [Diagnostics.Stopwatch]::StartNew()
                do {
                    $dialog = [ClclSmokeNative]::FindWindow('#32770', 'ClipMaster')
                } while (($dialog -eq [IntPtr]::Zero -or -not [ClclSmokeNative]::IsWindowVisible($dialog)) -and $dialogTimer.ElapsedMilliseconds -lt 3000)
                [uint32]$dialogProcess = 0
                [void][ClclSmokeNative]::GetWindowThreadProcessId($dialog, [ref]$dialogProcess)
                Assert-Clcl ($dialog -ne [IntPtr]::Zero -and $dialogProcess -eq $testProcess.Id) 'Isolated favorite confirmation is shown'
                [void](Send-Clcl $dialog 273 ([IntPtr]1))
                [void](Send-Clcl $tiles.Viewer 0)
                Assert-Clcl ((Get-ClclTiles 128).Count -eq $favoritesBefore.Count + 1) "Category $letter is saved in Favorites"
                foreach ($identifier in 125, 128) {
                    $tiles = Get-ClclTiles $identifier
                    [void](Send-Clcl $tiles.Window 277 ([IntPtr]6))
                    $client = New-Object int[] 4
                    [void][ClclSmokeNative]::GetClientRect($tiles.Window, $client)
                    $bitmap = New-Object Drawing.Bitmap $client[2], $client[3]
                    $graphics = [Drawing.Graphics]::FromImage($bitmap)
                    try {
                        $screenBounds = New-Object int[] 4
                        [void][ClclTabSmokeNative]::GetWindowRect($tiles.Window, $screenBounds)
                        [void](Send-Clcl $tiles.Window 15)
                        [void][ClclTabSmokeNative]::DwmFlush()
                        $graphics.CopyFromScreen($screenBounds[0], $screenBounds[1], 0, 0, $bitmap.Size)
                    } finally { $graphics.Dispose() }
                    try {
                        $left = [BitConverter]::ToInt32($tiles.Entries, 36)
                        $top = [BitConverter]::ToInt32($tiles.Entries, 40)
                        $right = [BitConverter]::ToInt32($tiles.Entries, 44)
                        $bottom = [BitConverter]::ToInt32($tiles.Entries, 48)
                        $dpi = [ClclSmokeNative]::GetDpiForWindow($tiles.Window)
                        $inset = [int][Math]::Floor(3 * $dpi / 96.0 + 0.5)
                        $size = [int][Math]::Floor(23 * $dpi / 96.0 + 0.5) - $inset
                        $level = if ($letter -in 'T', 'R', 'I') { 160 } else { 88 }
                        $fill = [Drawing.Color]::FromArgb($level, $level, $level).ToArgb()
                        $mask = New-Object Text.StringBuilder
                        $white = 0
                        $gray = 0
                        for ($pixelY = $bottom - $inset - $size + 4; $pixelY -lt $bottom - $inset - 4; $pixelY++) {
                            for ($pixelX = $right - $inset - $size + 4; $pixelX -lt $right - $inset - 4; $pixelX++) {
                                $color = $bitmap.GetPixel($pixelX, $pixelY)
                                $pixel = $color.ToArgb()
                                $isWhite = ($color.R + $color.G + $color.B) / 3 -gt $level + 32
                                [void]$mask.Append([int]$isWhite)
                                if ($isWhite) { $white++ }
                                if ($pixel -eq $fill) { $gray++ }
                            }
                        }
                        Assert-Clcl ($gray -gt 20 -and $white -gt 5) "Category $letter in view $identifier uses gray $level and a white letter (gray=$gray, glyph=$white)"
                        $centerY = $bottom - $inset - [int]($size / 2)
                        Assert-Clcl ($bitmap.GetPixel($right - $inset - $size - 1, $centerY).ToArgb() -ne $fill -and
                            $bitmap.GetPixel($right - $inset - $size + 1, $centerY).ToArgb() -eq $fill) "Category $letter in view $identifier uses the smaller 20px circle"
                        if ($identifier -eq 125) {
                            $historyMask = $mask.ToString()
                            Assert-Clcl ($masks.Add($historyMask)) "Category $letter has a distinct letter"
                        } else {
                            Assert-Clcl ($mask.ToString() -eq $historyMask) "Favorite category $letter matches the history badge"
                        }
                        $clip = $bitmap.Clone([Drawing.Rectangle]::FromLTRB($left, $top, $right, $bottom), $bitmap.PixelFormat)
                        $columnOffset = if ($identifier -eq 128) { 648 } else { 0 }
                        try { $montageGraphics.DrawImageUnscaled($clip, ($categoryIndex % 3) * 216 + $columnOffset, [int][Math]::Floor($categoryIndex / 3) * 224) }
                        finally { $clip.Dispose() }
                    } finally { $bitmap.Dispose() }
                }
                $categoryIndex++
            }
            $montage.Save((Join-Path ([IO.Path]::GetTempPath()) 'CLCL_categories.png'), [Drawing.Imaging.ImageFormat]::Png)
            Write-Output '[OKAY] All isolated category badge tests passed'
        } finally { $montageGraphics.Dispose(); $montage.Dispose() }
        return
    }
    if ($SampleOnly) {
        $tabs = [ClclSmokeNative]::GetDlgItem($text.Viewer, 120)
        Assert-Clcl ((Send-Clcl $tabs 4868).ToInt32() -eq 5) 'Viewer has five tabs'
        $historyBefore = Get-ClclTiles
        $samples = Get-ClclTiles 131
        Assert-Clcl ([ClclSmokeNative]::IsWindowVisible($samples.Window)) 'ClipSample tab is shown before Setting'
        Assert-Clcl ((Send-Clcl $tabs 4875).ToInt32() -eq 3) 'ClipSample is tab index 3'
        Assert-Clcl ($samples.Count -eq 12) "ClipSample creates 12 sample panels (actual: $($samples.Count))"
        $samplePath = Join-Path $temporary 'samples\Lorem Ipsum.txt'
        Assert-Clcl ((Test-Path -LiteralPath $samplePath) -and ([IO.File]::ReadAllText($samplePath) -eq 'Lorem Ipsum')) 'Lorem Ipsum sample file is created'
        $dpi = [ClclSmokeNative]::GetDpiForWindow($samples.Window)
        $cell = [int][Math]::Floor($dpi + 0.5)
        $gap = [int][Math]::Floor(8 * $dpi / 96.0 + 0.5)
        $small = [int][Math]::Floor(48 * $dpi / 96.0 + 0.5)
        $expectedKinds = 1, 2, 3, 1, 1, 1, 1, 1, 2, 3, 4, 5
        $expectedCategories = 'T', 'T', 'T', 'R', 'F', 'A', 'D', 'I', 'I', 'I', 'I', 'I'
        $colors = @(@(214, 69, 65), @(236, 150, 34), @(46, 160, 90), @(52, 120, 205), @(130, 80, 190))
        [void](Send-Clcl $samples.Window 277 ([IntPtr]6))
        $client = New-Object int[] 4
        [void][ClclSmokeNative]::GetClientRect($samples.Window, $client)
        $bitmap = New-Object Drawing.Bitmap $client[2], $client[3]
        $graphics = [Drawing.Graphics]::FromImage($bitmap)
        try {
            $screenBounds = New-Object int[] 4
            [void][ClclTabSmokeNative]::GetWindowRect($samples.Window, $screenBounds)
            [void][ClclTabSmokeNative]::DwmFlush()
            $graphics.CopyFromScreen($screenBounds[0], $screenBounds[1], 0, 0, $bitmap.Size)
        } finally { $graphics.Dispose() }
        try {
            for ($index = 0; $index -lt 12; $index++) {
                $kind = $expectedKinds[$index]
                $left = [BitConverter]::ToInt32($samples.Entries, $index * 72 + 36)
                $top = [BitConverter]::ToInt32($samples.Entries, $index * 72 + 40)
                $right = [BitConverter]::ToInt32($samples.Entries, $index * 72 + 44)
                $bottom = [BitConverter]::ToInt32($samples.Entries, $index * 72 + 48)
                $columns = switch ($kind) { 3 { 4 } 5 { 1 } default { 2 } }
                $height = switch ($kind) { 1 { $small } 2 { $cell } 3 { $cell } default { 2 * $cell + $gap } }
                Assert-Clcl ([BitConverter]::ToInt32($samples.Entries, $index * 72 + 52) -eq $kind -and ($right - $left) -eq $columns * ($cell + $gap) - $gap -and ($bottom - $top) -eq $height) "Sample $index ($($expectedCategories[$index])) uses panel kind $kind"
                $level = if ($expectedCategories[$index] -in 'F', 'A', 'D') { 88 } else { 160 }
                $badge = $bitmap.GetPixel($right - [int](13 * $dpi / 96) , $bottom - [int](21 * $dpi / 96))
                Assert-Clcl ($badge.R -eq $level -and $badge.G -eq $level -and $badge.B -eq $level) "Sample $index shows the $($expectedCategories[$index]) category badge color"
                if ($index -ge 7) {
                    $color = $bitmap.GetPixel([int](($left + $right) / 2), [int](($top + $bottom) / 2) - [int](12 * $dpi / 96))
                    $expected = $colors[$index - 7]
                    Assert-Clcl ($color.R -eq $expected[0] -and $color.G -eq $expected[1] -and $color.B -eq $expected[2]) "Image sample $($index - 6) uses a unique solid color"
                }
                for ($other = 0; $other -lt $index; $other++) {
                    $otherLeft = [BitConverter]::ToInt32($samples.Entries, $other * 72 + 36)
                    $otherTop = [BitConverter]::ToInt32($samples.Entries, $other * 72 + 40)
                    $otherRight = [BitConverter]::ToInt32($samples.Entries, $other * 72 + 44)
                    $otherBottom = [BitConverter]::ToInt32($samples.Entries, $other * 72 + 48)
                    if (-not ($right -le $otherLeft -or $left -ge $otherRight -or $bottom -le $otherTop -or $top -ge $otherBottom)) { throw "Samples $index and $other overlap" }
                }
            }
            Write-Output '[OKAY] Sample panels do not overlap'
            $bitmap.Save((Join-Path ([IO.Path]::GetTempPath()) 'CLCL_samples.png'), [Drawing.Imaging.ImageFormat]::Png)
        } finally { $bitmap.Dispose() }
        for ($index = 0; $index -lt 12; $index++) {
            $before = Get-ClclTiles
            $samples = Get-ClclTiles 131
            $centerX = [int](([BitConverter]::ToInt32($samples.Entries, $index * 72 + 36) + [BitConverter]::ToInt32($samples.Entries, $index * 72 + 44)) / 2)
            $centerY = [int](([BitConverter]::ToInt32($samples.Entries, $index * 72 + 40) + [BitConverter]::ToInt32($samples.Entries, $index * 72 + 48)) / 2)
            [void](Send-Clcl $samples.Window 514 ([IntPtr]::Zero) ([IntPtr](($centerY -shl 16) -bor $centerX)))
            $after = Get-ClclTiles
            Assert-Clcl ($after.Count -eq $before.Count + 1) "Clicking sample $index adds a ClipMaster clip"
            Assert-Clcl ([BitConverter]::ToInt32($after.Entries, 52) -eq $expectedKinds[$index] -and ([BitConverter]::ToInt32($after.Entries, 8) -ne 0) -eq ($index -ge 7)) "Clicked sample $index keeps its size and preview in ClipMaster"
        }
        Assert-Clcl ((Send-Clcl $historyBefore.Main 33071 ([IntPtr]1)).ToInt32() -ne 0) 'Clicked samples are saved in history'
        Stop-Clcl $testProcess
        $testProcess = Start-Clcl $temporary
        $restored = Get-ClclTiles
        Assert-Clcl ($restored.Count -eq $historyBefore.Count + 12) 'Clicked samples survive restart'
        Write-Output '[OKAY] All isolated sample tab tests passed'
        return
    }
    $panel = [ClclSmokeNative]::GetDlgItem($text.Viewer, 122)
    foreach ($control in 5420..5422) {
        Assert-Clcl ([ClclSmokeNative]::GetDlgItem($panel, $control) -eq [IntPtr]::Zero) "Initial alignment control $control is removed"
    }
    $unicodeOption = [ClclSmokeNative]::GetDlgItem($panel, 5116)
    Assert-Clcl ((Get-ClclText $unicodeOption).StartsWith('Ansi2Unicode text')) 'Unicode option uses the new name and original control ID'
    [void](Send-Clcl $text.Viewer 33890 ([IntPtr]4))
    $viewerBounds = New-Object int[] 4
    [void][ClclTabSmokeNative]::GetWindowRect($text.Viewer, $viewerBounds)
    $workArea = New-Object int[] 4
    Assert-Clcl ([ClclTabSmokeNative]::SystemParametersInfo(48, 0, $workArea, 0)) 'Read desktop work area'
    Assert-Clcl ([Math]::Abs($viewerBounds[0] + $viewerBounds[2] - $workArea[0] - $workArea[2]) -le 2 -and
        [Math]::Abs($viewerBounds[1] + $viewerBounds[3] - $workArea[1] - $workArea[3]) -le 2) 'Default startup window is centered horizontally and vertically'
    foreach ($width in @(($viewerBounds[2] - $viewerBounds[0]), 520)) {
        [void][ClclTabSmokeNative]::MoveWindow($text.Viewer, $viewerBounds[0], $viewerBounds[1], $width, $viewerBounds[3] - $viewerBounds[1], $true)
        [void](Send-Clcl $text.Viewer 0)
        $optionBounds = New-Object int[] 4
        $handlerBounds = New-Object int[] 4
        $excludeBounds = New-Object int[] 4
        $firstHandler = New-Object int[] 4
        [void][ClclTabSmokeNative]::GetWindowRect($unicodeOption, $optionBounds)
        [void][ClclTabSmokeNative]::GetWindowRect([ClclSmokeNative]::GetDlgItem($panel, 6004), $handlerBounds)
        [void][ClclTabSmokeNative]::GetWindowRect([ClclSmokeNative]::GetDlgItem($panel, 6003), $excludeBounds)
        [void][ClclTabSmokeNative]::GetWindowRect([ClclSmokeNative]::GetDlgItem($panel, 5440), $firstHandler)
        Assert-Clcl ($optionBounds[0] -gt $handlerBounds[0] -and $optionBounds[1] -gt $handlerBounds[1] -and
            $optionBounds[2] -lt $handlerBounds[2] -and $optionBounds[3] -lt $handlerBounds[3]) "Unicode option is inside the paste handler group at width $width"
        Assert-Clcl ($optionBounds[1] -ge $excludeBounds[3] -and $optionBounds[3] -le $firstHandler[1]) "Unicode option does not overlap excluded categories or handler fields at width $width"
    }
    [void][ClclTabSmokeNative]::MoveWindow($text.Viewer, $viewerBounds[0], $viewerBounds[1], $viewerBounds[2] - $viewerBounds[0], $viewerBounds[3] - $viewerBounds[1], $true)
    if ($SettingsOnly) {
        $panelBounds = New-Object int[] 4
        [void][ClclTabSmokeNative]::GetWindowRect($text.Viewer, $panelBounds)
        [void][ClclTabSmokeNative]::GetWindowRect([ClclSmokeNative]::GetDlgItem($panel, 6004), $handlerBounds)
        $bitmap = New-Object Drawing.Bitmap ($panelBounds[2] - $panelBounds[0]), ($panelBounds[3] - $panelBounds[1])
        $graphics = [Drawing.Graphics]::FromImage($bitmap)
        $context = $graphics.GetHdc()
        try { Assert-Clcl ([ClclTabSmokeNative]::PrintWindow($text.Viewer, $context, 0)) 'Paste handler settings render to a bitmap' }
        finally { $graphics.ReleaseHdc($context); $graphics.Dispose() }
        try {
            $crop = [Drawing.Rectangle]::FromLTRB($handlerBounds[0] - $panelBounds[0], $handlerBounds[1] - $panelBounds[1],
                $handlerBounds[2] - $panelBounds[0], $handlerBounds[3] - $panelBounds[1])
            $preview = $bitmap.Clone($crop, $bitmap.PixelFormat)
            try { $preview.Save((Join-Path ([IO.Path]::GetTempPath()) 'CLCL_unicode_handler.png'), [Drawing.Imaging.ImageFormat]::Png) }
            finally { $preview.Dispose() }
        } finally { $bitmap.Dispose() }
        foreach ($legacyAlignment in 0, 2) {
            Stop-Clcl $testProcess
            $testProcess = $null
            [IO.File]::WriteAllText((Join-Path $temporary 'clcl.ini'), "[main]`r`nclipboard_watch=0`r`nshow_trayicon=0`r`n[config]`r`nstart_with_windows=0`r`nremember_layout=0`r`nsaved_geometry=0`r`nalignment=$legacyAlignment`r`n")
            $testProcess = Start-Clcl $temporary
            $startup = Get-ClclTiles
            $bounds = New-Object int[] 4
            [void][ClclTabSmokeNative]::GetWindowRect($startup.Viewer, $bounds)
            Assert-Clcl ([Math]::Abs($bounds[0] + $bounds[2] - $workArea[0] - $workArea[2]) -le 2 -and
                [Math]::Abs($bounds[1] + $bounds[3] - $workArea[1] - $workArea[3]) -le 2) "Startup ignores legacy alignment=$legacyAlignment and stays centered"
        }
        Write-Output '[OKAY] All isolated settings layout tests passed'
        return
    }
    [void](Send-Clcl $text.Viewer 33890)
    $favoriteLimit = [ClclSmokeNative]::GetDlgItem($panel, 5107)
    $limitText = Get-ClclText $favoriteLimit
    Assert-Clcl ($limitText -eq '50') "Favorite count defaults to 50 (actual: $limitText)"
    Set-ClclText $favoriteLimit '100'
    Assert-Clcl ((Get-ClclText $favoriteLimit) -eq '100') 'Favorite count input changes to 100'
    [void](Send-Clcl $text.Viewer 273 ([IntPtr]123))
    $saveStatus = Get-ClclText ([ClclSmokeNative]::GetDlgItem($panel, 124))
    Assert-Clcl ((Get-Content -LiteralPath (Join-Path $temporary 'clcl.ini') -Raw) -match '(?m)^favorite_max=100\r?$') "Favorite count accepts and persists 100 (status: $saveStatus)"
    Set-ClclText $favoriteLimit '101'
    [void](Send-Clcl $text.Viewer 273 ([IntPtr]123))
    Assert-Clcl ((Get-Content -LiteralPath (Join-Path $temporary 'clcl.ini') -Raw) -match '(?m)^favorite_max=100\r?$') 'Favorite count rejects values above 100'
    Set-ClclText $favoriteLimit '50'
    [void](Send-Clcl $text.Viewer 273 ([IntPtr]123))
    Assert-Clcl ([ClclSmokeNative]::GetDlgItem($panel, 5109) -eq [IntPtr]::Zero) 'Password keyword setting is removed'
    foreach ($field in 5110..5119) {
        Assert-Clcl ([ClclSmokeNative]::GetDlgItem($panel, $field) -ne [IntPtr]::Zero) "Remaining setting $field keeps its control ID"
    }
    foreach ($marker in 'ExcludeClipboardContentFromMonitorProcessing', 'application/x-keepassxc-confidential', 'Clipboard Viewer Ignore') {
        $confidential = New-Object System.Windows.Forms.DataObject
        $confidential.SetText('Confidential marker regression ' + [Guid]::NewGuid().ToString())
        $confidential.SetData($marker, $false, [byte[]]@(1))
        [System.Windows.Forms.Clipboard]::SetDataObject($confidential, $true)
        [void](Send-Clcl $initial.Main 275 ([IntPtr]1))
        Assert-Clcl ((Get-ClclTiles).Count -eq $text.Count) "Explicit confidential marker is respected: $marker"
    }
    Assert-Clcl ([ClclEditorSmokeNative]::PostMessage($text.Viewer, 33911, [IntPtr]5, [IntPtr]::Zero)) 'Open portrait favorite editor'
    $editorTimer = [Diagnostics.Stopwatch]::StartNew()
    do {
        $editor = [ClclSmokeNative]::FindWindow('ClipMasterEditor', 'Favorite Clip')
    } while (($editor -eq [IntPtr]::Zero -or -not [ClclSmokeNative]::IsWindowVisible($editor)) -and $editorTimer.ElapsedMilliseconds -lt 3000)
    Assert-Clcl ($editor -ne [IntPtr]::Zero) 'Portrait favorite editor is ready'
    $sizes = [ClclSmokeNative]::GetDlgItem($editor, 303)
    Assert-Clcl ((Send-Clcl $sizes 326).ToInt32() -eq 6 -and (Send-Clcl $sizes 327).ToInt32() -eq 4) 'Fifth portrait panel size is selected with automatic sizing available'
    [void](Send-Clcl ([ClclSmokeNative]::GetDlgItem($editor, 301)) 258 ([IntPtr]65))
    [void](Send-Clcl $editor 273 ([IntPtr]1))
    Assert-Clcl ([ClclSmokeNative]::FindWindow('ClipMasterEditor', 'Favorite Clip') -eq $editor) 'Portrait panel rejects text'
    foreach ($dimensions in @(@(240, 80), @(160, 160), @(80, 240))) {
        $bitmap = New-Object System.Drawing.Bitmap $dimensions[0], $dimensions[1]
        try {
            $graphics = [System.Drawing.Graphics]::FromImage($bitmap)
            try { $graphics.Clear([System.Drawing.Color]::DarkCyan) } finally { $graphics.Dispose() }
            Set-ClclImage $bitmap
        } finally { $bitmap.Dispose() }
        Paste-ClclEditorImage $editor $dimensions[0] $dimensions[1]
        [void](Send-Clcl $editor 273 ([IntPtr]1))
        $remainingEditor = [ClclSmokeNative]::FindWindow('ClipMasterEditor', 'Favorite Clip')
        if ($dimensions[1] -gt $dimensions[0]) {
            $status = if ($remainingEditor -eq [IntPtr]::Zero) { 'saved' } else { Get-ClclText ([ClclSmokeNative]::GetDlgItem($editor, 304)) }
            Assert-Clcl ($remainingEditor -eq [IntPtr]::Zero) "Portrait panel accepts a tall image (status: $status)"
        } else {
            Assert-Clcl ($remainingEditor -eq $editor) "Portrait panel rejects image $($dimensions[0]) x $($dimensions[1])"
        }
    }
    [void](Send-Clcl $initial.Main 275 ([IntPtr]1))
    foreach ($kind in 1..5 + @(0)) {
        $before = Get-ClclTiles 128
        $editor = @(Open-ClclEditor $before.Viewer $kind)[-1]
        if ($kind -eq 4 -or $kind -eq 5) {
            $textControl = [ClclSmokeNative]::GetDlgItem($editor, 301)
            Assert-Clcl (([ClclSmokeNative]::GetWindowLong($textControl, -16) -band 0x08000000) -ne 0) "Image-only panel $kind disables text input"
            Assert-Clcl (([ClclSmokeNative]::GetWindowLong([ClclSmokeNative]::GetDlgItem($editor, 308), -16) -band 0x08000000) -ne 0) "Image-only panel $kind disables text mode"
            Set-ClclText $textControl 'Text must not be saved in this panel'
            [void](Send-Clcl $editor 273 ([IntPtr]1))
            Assert-Clcl ([ClclSmokeNative]::FindWindow('ClipMasterEditor', 'Favorite Clip') -eq $editor) "Image-only panel $kind rejects text on save"
        }
        if ($kind -le 3 -and $kind -gt 0) {
            [void](Send-Clcl ([ClclSmokeNative]::GetDlgItem($editor, 301)) 258 ([IntPtr](65 + $kind)))
        } else {
            $bitmap = if ($kind -eq 5) { New-Object System.Drawing.Bitmap 80, 240 } else { New-Object System.Drawing.Bitmap 400, 80 }
            try {
                $graphics = [System.Drawing.Graphics]::FromImage($bitmap)
                try { $graphics.Clear([System.Drawing.Color]::OrangeRed) } finally { $graphics.Dispose() }
                Set-ClclImage $bitmap
            } finally { $bitmap.Dispose() }
            Paste-ClclEditorImage $editor $(if ($kind -eq 5) { 80 } else { 400 }) $(if ($kind -eq 5) { 240 } else { 80 })
            if ($kind -eq 4 -or $kind -eq 5) {
                [void](Send-Clcl $editor 273 ([IntPtr]308))
                Assert-Clcl ([ClclSmokeNative]::IsWindowVisible([ClclSmokeNative]::GetDlgItem($editor, 307))) "Image-only panel $kind cannot switch its image to text"
            }
        }
        [void](Send-Clcl $editor 273 ([IntPtr]1))
        Assert-Clcl ([ClclSmokeNative]::FindWindow('ClipMasterEditor', 'Favorite Clip') -eq [IntPtr]::Zero) "Favorite panel kind $kind saves"
        $favorite = Get-ClclTiles 128
        Assert-Clcl ($favorite.Count -eq $before.Count + 1) "Favorite panel kind $kind appears in Favorites"
        Assert-Clcl ([BitConverter]::ToInt32($favorite.Entries, 52) -eq $kind) "Favorite panel kind $kind preserves its size metadata"
        $dpi = [ClclSmokeNative]::GetDpiForWindow($favorite.Window)
        $cell = [int][Math]::Floor($dpi + 0.5)
        $gap = [int][Math]::Floor(8 * $dpi / 96.0 + 0.5)
        $columns = if ($kind -eq 5) { 1 } elseif ($kind -eq 3 -or $kind -eq 0) { 4 } else { 2 }
        $height = if ($kind -eq 1) { [int][Math]::Floor(48 * $dpi / 96.0 + 0.5) } elseif ($kind -eq 4 -or $kind -eq 5) { 2 * $cell + $gap } else { $cell }
        Assert-Clcl (([BitConverter]::ToInt32($favorite.Entries, 44) - [BitConverter]::ToInt32($favorite.Entries, 36)) -eq $columns * ($cell + $gap) - $gap -and
            ([BitConverter]::ToInt32($favorite.Entries, 48) - [BitConverter]::ToInt32($favorite.Entries, 40)) -eq $height) "Favorite panel kind $kind displays at the correct size"
    }
    $automaticItem = [IntPtr][BitConverter]::ToInt32($favorite.Entries, 0)
    $editor = @(Open-ClclEditor $favorite.Viewer 0 $automaticItem)[-1]
    Assert-Clcl ((Send-Clcl ([ClclSmokeNative]::GetDlgItem($editor, 303)) 327).ToInt32() -eq 5) 'Editing an automatic favorite retains automatic sizing'
    [void](Send-Clcl $editor 273 ([IntPtr]1))
    $favoritesSaved = Get-ClclTiles 128
    Assert-Clcl ([Convert]::ToBase64String($favorite.Entries, 36, 16) -eq [Convert]::ToBase64String($favoritesSaved.Entries, 36, 16)) 'Saving an automatic favorite does not shrink its panel'
    $client = New-Object int[] 4
    [void][ClclSmokeNative]::GetClientRect($favoritesSaved.Window, $client)
    $placeholders = New-Object System.Drawing.Bitmap $client[2], $client[3]
    $graphics = [System.Drawing.Graphics]::FromImage($placeholders)
    $context = $graphics.GetHdc()
    try { Assert-Clcl ([ClclTabSmokeNative]::PrintWindow($favoritesSaved.Window, $context, 0)) 'Image-only placeholders render' }
    finally { $graphics.ReleaseHdc($context); $graphics.Dispose() }
    try {
        $crop = [Drawing.Rectangle]::Empty
        foreach ($index in ($favoritesSaved.Count - 2)..($favoritesSaved.Count - 1)) {
            $rectangle = [Drawing.Rectangle]::FromLTRB([BitConverter]::ToInt32($favoritesSaved.Entries, $index * 72 + 36),
                [BitConverter]::ToInt32($favoritesSaved.Entries, $index * 72 + 40),
                [BitConverter]::ToInt32($favoritesSaved.Entries, $index * 72 + 44),
                [BitConverter]::ToInt32($favoritesSaved.Entries, $index * 72 + 48))
            $crop = if ($crop.IsEmpty) { $rectangle } else { [Drawing.Rectangle]::Union($crop, $rectangle) }
            $grayPixels = 0
            for ($pixelY = $rectangle.Top + [int]($rectangle.Height / 2) + 10; $pixelY -lt $rectangle.Bottom - 8; $pixelY++) {
                for ($pixelX = $rectangle.Left + 8; $pixelX -lt $rectangle.Right - 8; $pixelX++) {
                    if ($placeholders.GetPixel($pixelX, $pixelY).ToArgb() -eq [Drawing.Color]::FromArgb(128, 128, 128).ToArgb()) { $grayPixels++ }
                }
            }
            Assert-Clcl ($grayPixels -gt 5) "Image-only placeholder $index displays its gray label"
        }
        $preview = $placeholders.Clone($crop, $placeholders.PixelFormat)
        try { $preview.Save((Join-Path ([IO.Path]::GetTempPath()) 'CLCL_image_only.png'), [Drawing.Imaging.ImageFormat]::Png) }
        finally { $preview.Dispose() }
    } finally { $placeholders.Dispose() }
    [void](Send-Clcl $initial.Main 275 ([IntPtr]1))
    foreach ($dimensions in @(@(80, 240, 96, 200), @(96, 192, 96, 200), @(160, 160, 200, 200), @(240, 80, 200, 96), @(400, 80, 408, 96))) {
        $before = Get-ClclTiles
        $bitmap = New-Object System.Drawing.Bitmap $dimensions[0], $dimensions[1]
        try {
            $graphics = [System.Drawing.Graphics]::FromImage($bitmap)
            try { $graphics.Clear([System.Drawing.Color]::CornflowerBlue) } finally { $graphics.Dispose() }
            Set-ClclImage $bitmap
        } finally { $bitmap.Dispose() }
        $captureTimer = [Diagnostics.Stopwatch]::StartNew()
        do {
            [void](Send-Clcl $initial.Main 275 ([IntPtr]1))
            $text = Get-ClclTiles
        } while ($text.Count -eq $before.Count -and $captureTimer.ElapsedMilliseconds -lt 3000)
        Assert-Clcl ($text.Count -eq $before.Count + 1) "Image $($dimensions[0]) x $($dimensions[1]) is captured"
        $dpi = [ClclSmokeNative]::GetDpiForWindow($text.Window)
        $width = [BitConverter]::ToInt32($text.Entries, 44) - [BitConverter]::ToInt32($text.Entries, 36)
        $height = [BitConverter]::ToInt32($text.Entries, 48) - [BitConverter]::ToInt32($text.Entries, 40)
        $cell = [int][Math]::Floor($dpi + 0.5)
        $gap = [int][Math]::Floor(8 * $dpi / 96.0 + 0.5)
        $columns = [int](($dimensions[2] + 8) / 104)
        $rows = [int](($dimensions[3] + 8) / 104)
        Assert-Clcl ($width -eq $columns * ($cell + $gap) - $gap -and $height -eq $rows * ($cell + $gap) - $gap) "Image $($dimensions[0]) x $($dimensions[1]) uses the correct panel"
        Assert-Clcl ([Convert]::ToBase64String($before.Positions) -eq [Convert]::ToBase64String($text.Positions, 0, $before.Positions.Length)) 'Image shape preserves older cached positions'
        $favoritesBeforeCopy = Get-ClclTiles 128
        $historyPanel = Get-ClclTiles
        [void](Send-Clcl $historyPanel.Window 277 ([IntPtr]6))
        $clickX = [BitConverter]::ToInt32($historyPanel.Entries, 36) + 10
        $clickY = [BitConverter]::ToInt32($historyPanel.Entries, 40) + 10
        $click = [IntPtr](($clickY -shl 16) -bor ($clickX -band 65535))
        Assert-Clcl ([ClclEditorSmokeNative]::PostMessage($historyPanel.Window, 517, [IntPtr]::Zero, $click)) 'Right-click history image to add it to Favorites'
        $dialogTimer = [Diagnostics.Stopwatch]::StartNew()
        do {
            $dialog = [ClclSmokeNative]::FindWindow('#32770', 'ClipMaster')
        } while (($dialog -eq [IntPtr]::Zero -or -not [ClclSmokeNative]::IsWindowVisible($dialog)) -and $dialogTimer.ElapsedMilliseconds -lt 3000)
        [uint32]$dialogProcess = 0
        [void][ClclSmokeNative]::GetWindowThreadProcessId($dialog, [ref]$dialogProcess)
        Assert-Clcl ($dialog -ne [IntPtr]::Zero -and $dialogProcess -eq $testProcess.Id) 'Isolated favorite confirmation is shown'
        [void](Send-Clcl $dialog 273 ([IntPtr]1))
        [void](Send-Clcl $historyPanel.Viewer 0)
        $favoritesAfterCopy = Get-ClclTiles 128 -NoRefresh
        Assert-Clcl ($favoritesAfterCopy.Count -eq $favoritesBeforeCopy.Count + 1) "History image $($dimensions[0]) x $($dimensions[1]) immediately appears in Favorites without tab refresh"
        $favoriteWidth = [BitConverter]::ToInt32($favoritesAfterCopy.Entries, 44) - [BitConverter]::ToInt32($favoritesAfterCopy.Entries, 36)
        $favoriteHeight = [BitConverter]::ToInt32($favoritesAfterCopy.Entries, 48) - [BitConverter]::ToInt32($favoritesAfterCopy.Entries, 40)
        Assert-Clcl ($favoriteWidth -eq $width -and $favoriteHeight -eq $height -and
            [BitConverter]::ToInt32($favoritesAfterCopy.Entries, 8) -ne 0) 'Copied history image keeps its panel size and image preview'
    }
    for ($capture = 0; $capture -lt 18; $capture++) {
        $before = Get-ClclTiles
        [System.Windows.Forms.Clipboard]::SetText(('Layout regression ' + [Guid]::NewGuid().ToString() + ' ') * (1 + $capture % 4))
        $captureTimer = [Diagnostics.Stopwatch]::StartNew()
        do {
            [void](Send-Clcl $initial.Main 275 ([IntPtr]1))
            $text = Get-ClclTiles
        } while ($text.Count -eq $before.Count -and $captureTimer.ElapsedMilliseconds -lt 3000)
        Assert-Clcl ($text.Count -eq $before.Count + 1) "Layout capture $capture adds one panel"
        Assert-Clcl ([Convert]::ToBase64String($before.Positions) -eq [Convert]::ToBase64String($text.Positions, 0, $before.Positions.Length)) "Layout capture $capture preserves all older cached positions"
    }
    $bitmap = New-Object System.Drawing.Bitmap 137, 83
    try {
        $graphics = [System.Drawing.Graphics]::FromImage($bitmap)
        try { $graphics.Clear([System.Drawing.Color]::MediumSeaGreen) } finally { $graphics.Dispose() }
        Set-ClclImage $bitmap
    } finally { $bitmap.Dispose() }
    $captureTimer = [Diagnostics.Stopwatch]::StartNew()
    do {
        [void](Send-Clcl $initial.Main 275 ([IntPtr]1))
        $image = Get-ClclTiles
    } while ($image.Count -eq $text.Count -and $captureTimer.ElapsedMilliseconds -lt 3000)
    Assert-Clcl ($image.Count -eq $text.Count + 1) "Image copy adds one tile ($($text.Count) -> $($image.Count))"
    Assert-Clcl ([Convert]::ToBase64String($text.Positions) -eq [Convert]::ToBase64String($image.Positions, 0, $text.Positions.Length)) 'Image capture preserves older cached positions'
    $dpi = [ClclSmokeNative]::GetDpiForWindow($image.Window)
    $cell = [int][Math]::Floor($dpi + 0.5)
    $gap = [int][Math]::Floor(8 * $dpi / 96.0 + 0.5)
    $margin = [int][Math]::Floor(12 * $dpi / 96.0 + 0.5)
    $client = New-Object int[] 4
    [void][ClclSmokeNative]::GetClientRect($image.Window, $client)
    for ($index = 0; $index -lt $image.Count; $index++) {
        $left = [BitConverter]::ToInt32($image.Entries, $index * 72 + 36)
        $right = [BitConverter]::ToInt32($image.Entries, $index * 72 + 44)
        Assert-Clcl ((($client[2] - $right - $margin) % ($cell + $gap)) -eq 0 -and (($right - $left + $gap) % ($cell + $gap)) -eq 0) "Tile $index follows the right-anchored grid"
        $top = [BitConverter]::ToInt32($image.Entries, $index * 72 + 40)
        $bottom = [BitConverter]::ToInt32($image.Entries, $index * 72 + 48)
        for ($other = 0; $other -lt $index; $other++) {
            $otherLeft = [BitConverter]::ToInt32($image.Entries, $other * 72 + 36)
            $otherTop = [BitConverter]::ToInt32($image.Entries, $other * 72 + 40)
            $otherRight = [BitConverter]::ToInt32($image.Entries, $other * 72 + 44)
            $otherBottom = [BitConverter]::ToInt32($image.Entries, $other * 72 + 48)
            Assert-Clcl ($right -le $otherLeft -or $left -ge $otherRight -or $bottom -le $otherTop -or $top -ge $otherBottom) "Tiles $index and $other do not overlap"
        }
        if ($index -gt 0) {
            $previousIndex = [BitConverter]::ToInt32($image.Order, ($index - 1) * 4)
            $paintIndex = [BitConverter]::ToInt32($image.Order, $index * 4)
            $previousTop = [BitConverter]::ToInt32($image.Entries, $previousIndex * 72 + 40)
            $paintTop = [BitConverter]::ToInt32($image.Entries, $paintIndex * 72 + 40)
            $previousLeft = [BitConverter]::ToInt32($image.Entries, $previousIndex * 72 + 36)
            $paintLeft = [BitConverter]::ToInt32($image.Entries, $paintIndex * 72 + 36)
            Assert-Clcl ($previousTop -lt $paintTop -or ($previousTop -eq $paintTop -and $previousLeft -le $paintLeft)) "Paint step $index proceeds top-to-bottom and left-to-right"
        }
    }
    foreach ($tab in 0, 1, 2, 3, 4, 0) {
        [void](Send-Clcl $image.Viewer 33890 ([IntPtr]$tab))
        [void](Send-Clcl $image.Main 33068)
        [void](Send-Clcl $image.Viewer 33118)
        $root = Send-Clcl ([ClclSmokeNative]::GetDlgItem($image.Viewer, 100)) 4362
        if ($root -ne [IntPtr]::Zero) { [void](Send-Clcl ([ClclSmokeNative]::GetDlgItem($image.Viewer, 100)) 4363 ([IntPtr]9) $root) }
        foreach ($control in 100, 101) {
            Assert-Clcl (([ClclSmokeNative]::GetWindowLong([ClclSmokeNative]::GetDlgItem($image.Viewer, $control), -16) -band 0x10000000) -eq 0) "Legacy control $control stays hidden on tab $tab"
        }
        $visibleControl = @(125, 128, 129, 131, 122)[$tab]
        Assert-Clcl ([ClclSmokeNative]::IsWindowVisible([ClclSmokeNative]::GetDlgItem($image.Viewer, $visibleControl))) "Tab $tab content is visible"
    }
    $spaceAvailable = [ClclSmokeNative]::RegisterHotKey([IntPtr]::Zero, 301, 1, 32)
    if ($spaceAvailable) { [void][ClclSmokeNative]::UnregisterHotKey([IntPtr]::Zero, 301) }
    Assert-Clcl (-not $spaceAvailable) 'Alt+Space is registered'
    $enterAvailable = [ClclSmokeNative]::RegisterHotKey([IntPtr]::Zero, 302, 1, 13)
    if ($enterAvailable) { [void][ClclSmokeNative]::UnregisterHotKey([IntPtr]::Zero, 302) }
    Assert-Clcl $enterAvailable 'Alt+Enter is not registered'
    Assert-Clcl ((Send-Clcl $image.Main 33071 ([IntPtr]1)).ToInt32() -ne 0) 'History saves successfully'
    $favoritesSaved = Get-ClclTiles 128
    Stop-Clcl $testProcess
    $testProcess = Start-Clcl $temporary
    $reloaded = Get-ClclTiles
    Assert-Clcl ($reloaded.Count -eq $image.Count) 'History survives restart'
    for ($index = 0; $index -lt $image.Count; $index++) {
        Assert-Clcl ([Convert]::ToBase64String($image.Entries, $index * 72 + 36, 16) -eq
            [Convert]::ToBase64String($reloaded.Entries, $index * 72 + 36, 16)) "Tile $index keeps its layout after restart"
    }
    $favoritesReloaded = Get-ClclTiles 128
    Assert-Clcl ($favoritesReloaded.Count -eq $favoritesSaved.Count) 'All favorite panel types survive restart'
    for ($index = 0; $index -lt $favoritesSaved.Count; $index++) {
        Assert-Clcl ([Convert]::ToBase64String($favoritesSaved.Entries, $index * 72 + 36, 20) -eq
            [Convert]::ToBase64String($favoritesReloaded.Entries, $index * 72 + 36, 20)) "Favorite panel $index preserves its kind and layout after restart"
    }
    $tabs = [ClclSmokeNative]::GetDlgItem($reloaded.Viewer, 120)
    $tabClient = New-Object int[] 4
    [void][ClclSmokeNative]::GetClientRect($tabs, $tabClient)
    $tabImage = New-Object System.Drawing.Bitmap $tabClient[2], $tabClient[3]
    $tabGraphics = [System.Drawing.Graphics]::FromImage($tabImage)
    $tabContext = $tabGraphics.GetHdc()
    try { Assert-Clcl ([ClclTabSmokeNative]::PrintWindow($tabs, $tabContext, 0)) 'Colored tabs render to a bitmap' }
    finally { $tabGraphics.ReleaseHdc($tabContext); $tabGraphics.Dispose() }
    try {
        $tabColors = @([Drawing.Color]::FromArgb(219, 235, 252), [Drawing.Color]::FromArgb(219, 242, 226),
            [Drawing.Color]::FromArgb(255, 234, 210), [Drawing.Color]::FromArgb(250, 222, 230))
        foreach ($color in $tabColors) {
            $pixels = 0
            for ($pixelY = 0; $pixelY -lt $tabImage.Height; $pixelY++) {
                for ($pixelX = 0; $pixelX -lt $tabImage.Width; $pixelX++) {
                    if ($tabImage.GetPixel($pixelX, $pixelY).ToArgb() -eq $color.ToArgb()) { $pixels++ }
                }
            }
            Assert-Clcl ($pixels -gt 20) "Tab color $($color.Name) is visible"
        }
        $tabScreenshot = Join-Path ([IO.Path]::GetTempPath()) 'CLCL_tab_colors.png'
        $tabImage.Save($tabScreenshot, [Drawing.Imaging.ImageFormat]::Png)
        Write-Output "[OKAY] Tab screenshot: $tabScreenshot"
    } finally { $tabImage.Dispose() }
    Stop-Clcl $testProcess
    $testProcess = $null
    $iniFile = Join-Path $temporary 'clcl.ini'
    $iniText = Get-Content -LiteralPath $iniFile -Raw
    [IO.File]::WriteAllText($iniFile, [regex]::Replace($iniText, '(?m)^favorite_max=\d+', 'favorite_max=200'))
    $testProcess = Start-Clcl $temporary
    $limited = Get-ClclTiles
    $limitText = Get-ClclText ([ClclSmokeNative]::GetDlgItem([ClclSmokeNative]::GetDlgItem($limited.Viewer, 122), 5107))
    Assert-Clcl ($limitText -eq '100') 'Persisted favorite count above 100 is clamped on load'
    $reloaded = $limited
    if ($LayoutOnly) {
        Write-Output '[OKAY] All isolated layout tests passed'
        return
    }
    $launcher = Start-Clcl $temporary 'CLCLSet.exe'
    if (-not $launcher.WaitForExit(10000)) { throw 'Settings launcher did not exit' }
    [void](Send-Clcl $reloaded.Main 0)
    Assert-Clcl ((Send-Clcl ([ClclSmokeNative]::GetDlgItem($reloaded.Viewer, 120)) 4875).ToInt32() -eq 4) 'Settings launcher selects Settings'
    [void](Send-Clcl $reloaded.Main 32871)
    $imageItem = [IntPtr][BitConverter]::ToInt32($reloaded.Entries, 0)
    Assert-Clcl ((Send-Clcl $reloaded.Main 33168 ([IntPtr]1) $imageItem).ToInt32() -eq 1) 'Saved image copies after restart'
    Assert-Clcl ([System.Windows.Forms.Clipboard]::ContainsImage()) 'Saved image is available on the clipboard'
    $handle = [ClclSmokeNative]::OpenProcess(56, $false, $testProcess.Id)
    try {
        $metadata = New-Object byte[] 100
        $transferred = [IntPtr]::Zero
        Assert-Clcl ([ClclSmokeNative]::ReadProcessMemory($handle, $imageItem, $metadata, 100, [ref]$transferred)) 'Read isolated image metadata'
        $formatAddress = [BitConverter]::ToInt32($metadata, 92)
        while ($formatAddress -ne 0) {
            Assert-Clcl ([ClclSmokeNative]::ReadProcessMemory($handle, [IntPtr]$formatAddress, $metadata, 100, [ref]$transferred)) 'Read isolated format metadata'
            if ([BitConverter]::ToInt32($metadata, 20) -eq 8) { break }
            $formatAddress = [BitConverter]::ToInt32($metadata, 96)
        }
        Assert-Clcl ($formatAddress -ne 0) 'Image includes a DIB format for legacy regression'
        $zero = New-Object byte[] 4
        Assert-Clcl ([ClclSmokeNative]::WriteProcessMemory($handle, [IntPtr]($formatAddress + 12), $zero, 4, [ref]$transferred)) 'Simulate legacy unnamed format'
        Assert-Clcl ([ClclSmokeNative]::WriteProcessMemory($handle, [IntPtr]($formatAddress + 20), $zero, 4, [ref]$transferred)) 'Simulate legacy zero format ID'
    } finally { if ($handle -ne [IntPtr]::Zero) { [void][ClclSmokeNative]::CloseHandle($handle) } }
    Assert-Clcl ((Send-Clcl $reloaded.Main 33168 ([IntPtr]1) $imageItem).ToInt32() -eq 1) 'Image with legacy unnamed format copies without an error'
    Assert-Clcl ([System.Windows.Forms.Clipboard]::ContainsImage()) 'Legacy image retains a usable clipboard image'
    [void](Send-Clcl $reloaded.Main 275 ([IntPtr]1))
    Write-Output '[OKAY] All isolated smoke tests passed'
} finally {
    if ($null -ne $testProcess -and -not $testProcess.HasExited) {
        try { Stop-Clcl $testProcess } catch { $testProcess.Kill(); $testProcess.WaitForExit() }
    }
    if ($clipboardChanged) { [System.Windows.Forms.Clipboard]::SetDataObject($clipboardBackup, $true) }
    if (Test-Path -LiteralPath $temporary) { Remove-Item -LiteralPath $temporary -Recurse -Force }
    if ($existing.Count -gt 0) { Start-Process -FilePath (Join-Path $BuildDirectory 'CLCL.exe') -WorkingDirectory $BuildDirectory }
}