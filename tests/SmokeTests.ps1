param([string]$BuildDirectory = (Join-Path $PSScriptRoot '..\Release'), [switch]$LayoutOnly, [switch]$SettingsOnly, [switch]$CategoryOnly, [switch]$SampleOnly)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
Add-Type -AssemblyName System.Windows.Forms
Add-Type -AssemblyName System.Drawing
if (-not ('CpcpSmokeNative' -as [type])) {
    Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
public static class CpcpSmokeNative {
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

if (-not ('CpcpEditorSmokeNative' -as [type])) {
    Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
public static class CpcpEditorSmokeNative {
    [DllImport("user32.dll", CharSet = CharSet.Unicode)] public static extern bool PostMessage(IntPtr window, uint message, IntPtr parameter, IntPtr data);
}
'@
}

if (-not ('CpcpTabSmokeNative' -as [type])) {
    Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
public static class CpcpTabSmokeNative {
    [DllImport("user32.dll")] public static extern bool PrintWindow(IntPtr window, IntPtr context, uint flags);
    [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr window, [Out] int[] rectangle);
    [DllImport("user32.dll")] public static extern bool MoveWindow(IntPtr window, int x, int y, int width, int height, bool repaint);
    [DllImport("user32.dll")] public static extern bool SystemParametersInfo(uint action, uint parameter, [Out] int[] rectangle, uint flags);
    [DllImport("dwmapi.dll")] public static extern int DwmFlush();
}
'@
}

function Assert-Cpcp([bool]$Condition, [string]$Message) {
    if (-not $Condition) { throw $Message }
    Write-Output "[OKAY] $Message"
}

function Send-Cpcp([IntPtr]$Window, [uint32]$Message, [IntPtr]$Parameter = [IntPtr]::Zero, [IntPtr]$Data = [IntPtr]::Zero) {
    $result = [UIntPtr]::Zero
    if ([CpcpSmokeNative]::SendMessageTimeout($Window, $Message, $Parameter, $Data, 2, 5000, [ref]$result) -eq [IntPtr]::Zero) {
        throw "Window message failed: $Message"
    }
    return [IntPtr][long]$result.ToUInt64()
}

function Stop-Cpcp([Diagnostics.Process]$Process) {
    if ($null -eq $Process -or $Process.HasExited) { return }
    $window = [CpcpSmokeNative]::FindWindow('CPCPNewMain', 'CPCP_new')
    [uint32]$windowProcess = 0
    [void][CpcpSmokeNative]::GetWindowThreadProcessId($window, [ref]$windowProcess)
    if ($windowProcess -ne $Process.Id) { throw 'Refusing to close an unrelated CPCP process' }
    [void](Send-Cpcp $window 16)
    if (-not $Process.WaitForExit(5000)) { throw 'CPCP did not exit normally' }
}

function Get-CpcpText([IntPtr]$Window) {
    $buffer = [Runtime.InteropServices.Marshal]::AllocHGlobal(512)
    try {
        [void](Send-Cpcp $Window 13 ([IntPtr]256) $buffer)
        return [Runtime.InteropServices.Marshal]::PtrToStringAnsi($buffer)
    } finally { [Runtime.InteropServices.Marshal]::FreeHGlobal($buffer) }
}

function Set-CpcpText([IntPtr]$Window, [string]$Text) {
    $buffer = [Runtime.InteropServices.Marshal]::StringToHGlobalAnsi($Text)
    try { [void](Send-Cpcp $Window 12 ([IntPtr]::Zero) $buffer) }
    finally { [Runtime.InteropServices.Marshal]::FreeHGlobal($buffer) }
}

function Set-CpcpImage([Drawing.Bitmap]$Bitmap) {
    $data = New-Object System.Windows.Forms.DataObject
    $data.SetImage($Bitmap)
    [System.Windows.Forms.Clipboard]::SetDataObject($data, $true, 10, 100)
}

function Paste-CpcpEditorImage([IntPtr]$Editor, [int]$Width, [int]$Height) {
    $preview = [CpcpSmokeNative]::GetDlgItem($Editor, 307)
    [uint32]$processId = 0
    [void][CpcpSmokeNative]::GetWindowThreadProcessId($preview, [ref]$processId)
    $handle = [CpcpSmokeNative]::OpenProcess(16, $false, $processId)
    $timer = [Diagnostics.Stopwatch]::StartNew()
    try {
        do {
            [void](Send-Cpcp $Editor 273 ([IntPtr]302))
            $header = New-Object byte[] 8
            $entry = New-Object byte[] 72
            $read = [IntPtr]::Zero
            $state = [IntPtr][CpcpSmokeNative]::GetWindowLong($preview, -21)
            $matched = [CpcpSmokeNative]::ReadProcessMemory($handle, $state, $header, 8, [ref]$read) -and
                [BitConverter]::ToInt32($header, 4) -gt 0 -and
                [CpcpSmokeNative]::ReadProcessMemory($handle, [IntPtr][BitConverter]::ToInt32($header, 0), $entry, 72, [ref]$read) -and
                [BitConverter]::ToInt32($entry, 12) -eq $Width -and [BitConverter]::ToInt32($entry, 16) -eq $Height
        } while (-not $matched -and $timer.ElapsedMilliseconds -lt 3000)
        Assert-Cpcp $matched "Editor receives image $Width x $Height before saving"
    } finally { [void][CpcpSmokeNative]::CloseHandle($handle) }
}

function Start-Cpcp([string]$Directory, [string]$Executable = 'CPCP.exe') {
    $process = Start-Process -FilePath (Join-Path $Directory $Executable) -WorkingDirectory $Directory -ArgumentList '/cmshow' -PassThru
    if ($Executable -eq 'CPCP.exe' -and -not $process.WaitForInputIdle(10000)) { throw 'CPCP did not become ready' }
    return $process
}

function Open-CpcpEditor([IntPtr]$Viewer, [int]$Kind, [IntPtr]$Original = [IntPtr]::Zero) {
    $message = if ($Original -eq [IntPtr]::Zero) { 33911 } else { 33912 }
    Assert-Cpcp ([CpcpEditorSmokeNative]::PostMessage($Viewer, $message, [IntPtr]$Kind, $Original)) 'Open favorite editor'
    $timer = [Diagnostics.Stopwatch]::StartNew()
    do {
        $editor = [CpcpSmokeNative]::FindWindow('ClipMasterEditor', 'Favorite Clip')
    } while (($editor -eq [IntPtr]::Zero -or -not [CpcpSmokeNative]::IsWindowVisible($editor)) -and $timer.ElapsedMilliseconds -lt 3000)
    Assert-Cpcp ($editor -ne [IntPtr]::Zero) 'Favorite editor is ready'
    return $editor
}

function Get-CpcpTiles([int]$Identifier = 125, [switch]$NoRefresh) {
    $main = [CpcpSmokeNative]::FindWindow('CPCPNewMain', 'CPCP_new')
    if (-not $NoRefresh) { [void](Send-Cpcp $main 786 ([IntPtr]0x7F01)) }
    $viewer = [CpcpSmokeNative]::FindWindow('CPCPViewer', 'ClipMaster')
    if (-not $NoRefresh) { [void](Send-Cpcp $viewer 33890 ([IntPtr][int]$(switch ($Identifier) { 128 { 1 } 131 { 4 } default { 0 } }))) }
    $tiles = [CpcpSmokeNative]::GetDlgItem($viewer, $Identifier)
    [uint32]$processId = 0
    [void][CpcpSmokeNative]::GetWindowThreadProcessId($tiles, [ref]$processId)
    $handle = [CpcpSmokeNative]::OpenProcess(16, $false, $processId)
    try {
        $state = [IntPtr][CpcpSmokeNative]::GetWindowLong($tiles, -21)
        $header = New-Object byte[] 8
        $read = [IntPtr]::Zero
        if (-not [CpcpSmokeNative]::ReadProcessMemory($handle, $state, $header, 8, [ref]$read)) { throw 'Cannot read x86 tile state' }
        $count = [BitConverter]::ToInt32($header, 4)
        $entries = New-Object byte[] ($count * 72)
        if ($count -gt 0 -and -not [CpcpSmokeNative]::ReadProcessMemory($handle, [IntPtr][BitConverter]::ToInt32($header, 0), $entries, $entries.Length, [ref]$read)) {
            throw 'Cannot read x86 tile entries'
        }
        $layout = New-Object byte[] 44
        if (-not [CpcpSmokeNative]::ReadProcessMemory($handle, [IntPtr]($state.ToInt64() + 544), $layout, $layout.Length, [ref]$read)) {
            throw 'Cannot read x86 layout cache'
        }
        $positionCount = [BitConverter]::ToInt32($layout, 4)
        $positions = New-Object byte[] ($positionCount * 56)
        if ($positionCount -gt 0 -and -not [CpcpSmokeNative]::ReadProcessMemory($handle, [IntPtr][BitConverter]::ToInt32($layout, 0), $positions, $positions.Length, [ref]$read)) {
            throw 'Cannot read chronological positions'
        }
        $order = New-Object byte[] ($count * 4)
        if ($count -gt 0 -and -not [CpcpSmokeNative]::ReadProcessMemory($handle, [IntPtr][BitConverter]::ToInt32($layout, 40), $order, $order.Length, [ref]$read)) {
            throw 'Cannot read paint order'
        }
        return @{ Count = $count; Entries = $entries; Positions = $positions; Order = $order; Window = $tiles; Viewer = $viewer; Main = $main }
    } finally { if ($handle -ne [IntPtr]::Zero) { [void][CpcpSmokeNative]::CloseHandle($handle) } }
}

$BuildDirectory = (Resolve-Path -LiteralPath $BuildDirectory).Path
$temporary = Join-Path ([IO.Path]::GetTempPath()) ('CPCP_smoke_' + [Guid]::NewGuid().ToString('N'))
$existing = @(Get-Process CPCP -ErrorAction SilentlyContinue | Where-Object { $_.Path -eq (Join-Path $BuildDirectory 'CPCP.exe') })
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
    foreach ($process in $existing) { Stop-Cpcp $process }
    if ([CpcpSmokeNative]::FindWindow('CPCPNewMain', 'CPCP_new') -ne [IntPtr]::Zero) { throw 'Another CPCP instance is running' }
    [void][IO.Directory]::CreateDirectory($temporary)
    foreach ($file in 'CPCP.exe', 'CPCPSet.exe', 'CPCPHook.dll') { Copy-Item -LiteralPath (Join-Path $BuildDirectory $file) -Destination $temporary }
    [IO.File]::WriteAllText((Join-Path $temporary 'cpcp_app.ini'), "[GENERAL]`r`nportable=1`r`n")
    [IO.File]::WriteAllText((Join-Path $temporary 'cpcp.ini'), "[main]`r`nclipboard_watch=1`r`nshow_trayicon=0`r`n[config]`r`nstart_with_windows=0`r`n")
    if ($CategoryOnly) { [IO.File]::AppendAllText((Join-Path $temporary 'cpcp.ini'), "exclude_audio=0`r`n") }
    $testProcess = Start-Cpcp $temporary
    $clipboardChanged = $true
    [System.Windows.Forms.Clipboard]::SetDataObject('CPCP test baseline ' + [Guid]::NewGuid().ToString(), $true, 10, 100)
    [void](Send-Cpcp ([CpcpSmokeNative]::FindWindow('CPCPNewMain', 'CPCP_new')) 275 ([IntPtr]1))
    $initial = Get-CpcpTiles
    Assert-Cpcp ((Send-Cpcp $initial.Main 32870).ToInt32() -eq 1) 'Clipboard monitoring enabled'
    $clipboardChanged = $true
    [System.Windows.Forms.Clipboard]::SetDataObject('CPCP smoke password passwd secret token ' + [Guid]::NewGuid().ToString(), $true, 10, 100)
    $captureTimer = [Diagnostics.Stopwatch]::StartNew()
    do {
        [void](Send-Cpcp $initial.Main 275 ([IntPtr]1))
        $text = Get-CpcpTiles
    } while ($text.Count -eq $initial.Count -and $captureTimer.ElapsedMilliseconds -lt 3000)
    Assert-Cpcp ($text.Count -eq $initial.Count + 1) "Text copy adds one tile ($($initial.Count) -> $($text.Count))"
    if ($CategoryOnly) {
        $montage = New-Object Drawing.Bitmap 1296, 448
        $montageGraphics = [Drawing.Graphics]::FromImage($montage)
        $montageGraphics.Clear([Drawing.Color]::White)
        $masks = New-Object 'Collections.Generic.HashSet[string]'
        $categoryIndex = 0
        try {
            foreach ($letter in 'T', 'R', 'I', 'F', 'A', 'D') {
                $before = Get-CpcpTiles
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
                        [void]$paths.Add((Join-Path $temporary 'CPCP.exe'))
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
                    [void](Send-Cpcp $before.Main 275 ([IntPtr]1))
                    $tiles = Get-CpcpTiles
                } while ($tiles.Count -eq $before.Count -and $timer.ElapsedMilliseconds -lt 3000)
                Assert-Cpcp ($tiles.Count -eq $before.Count + 1) "Category $letter fixture is captured"
                if ($letter -eq 'D') {
                    $handle = [CpcpSmokeNative]::OpenProcess(56, $false, $testProcess.Id)
                    try {
                        $metadata = New-Object byte[] 100
                        $read = [IntPtr]::Zero
                        Assert-Cpcp ([CpcpSmokeNative]::ReadProcessMemory($handle, [IntPtr][BitConverter]::ToInt32($tiles.Entries, 0), $metadata, 100, [ref]$read)) 'Read isolated device fixture'
                        $address = [BitConverter]::ToInt32($metadata, 92)
                        while ($address -ne 0) {
                            Assert-Cpcp ([CpcpSmokeNative]::ReadProcessMemory($handle, [IntPtr]$address, $metadata, 100, [ref]$read)) 'Read isolated device format'
                            if ([BitConverter]::ToInt32($metadata, 20) -in 1, 7, 13) {
                                Assert-Cpcp ([CpcpSmokeNative]::WriteProcessMemory($handle, [IntPtr]($address + 20), [BitConverter]::GetBytes([int]129), 4, [ref]$read)) 'Simulate CF_DSPTEXT device format'
                            }
                            $address = [BitConverter]::ToInt32($metadata, 96)
                        }
                    } finally { [void][CpcpSmokeNative]::CloseHandle($handle) }
                    $tiles = Get-CpcpTiles
                }
                $favoritesBefore = Get-CpcpTiles 128
                $tiles = Get-CpcpTiles
                Assert-Cpcp ([CpcpEditorSmokeNative]::PostMessage($tiles.Viewer, 33909, [IntPtr]::Zero,
                    [IntPtr][BitConverter]::ToInt32($tiles.Entries, 0))) "Add category $letter to Favorites"
                $dialogTimer = [Diagnostics.Stopwatch]::StartNew()
                do {
                    $dialog = [CpcpSmokeNative]::FindWindow('#32770', 'ClipMaster')
                } while (($dialog -eq [IntPtr]::Zero -or -not [CpcpSmokeNative]::IsWindowVisible($dialog)) -and $dialogTimer.ElapsedMilliseconds -lt 3000)
                [uint32]$dialogProcess = 0
                [void][CpcpSmokeNative]::GetWindowThreadProcessId($dialog, [ref]$dialogProcess)
                Assert-Cpcp ($dialog -ne [IntPtr]::Zero -and $dialogProcess -eq $testProcess.Id) 'Isolated favorite confirmation is shown'
                [void](Send-Cpcp $dialog 273 ([IntPtr]1))
                [void](Send-Cpcp $tiles.Viewer 0)
                Assert-Cpcp ((Get-CpcpTiles 128).Count -eq $favoritesBefore.Count + 1) "Category $letter is saved in Favorites"
                foreach ($identifier in 125, 128) {
                    $tiles = Get-CpcpTiles $identifier
                    [void](Send-Cpcp $tiles.Window 277 ([IntPtr]6))
                    $client = New-Object int[] 4
                    [void][CpcpSmokeNative]::GetClientRect($tiles.Window, $client)
                    $bitmap = New-Object Drawing.Bitmap $client[2], $client[3]
                    $graphics = [Drawing.Graphics]::FromImage($bitmap)
                    try {
                        $screenBounds = New-Object int[] 4
                        [void][CpcpTabSmokeNative]::GetWindowRect($tiles.Window, $screenBounds)
                        [void](Send-Cpcp $tiles.Window 15)
                        [void][CpcpTabSmokeNative]::DwmFlush()
                        $graphics.CopyFromScreen($screenBounds[0], $screenBounds[1], 0, 0, $bitmap.Size)
                    } finally { $graphics.Dispose() }
                    try {
                        $left = [BitConverter]::ToInt32($tiles.Entries, 36)
                        $top = [BitConverter]::ToInt32($tiles.Entries, 40)
                        $right = [BitConverter]::ToInt32($tiles.Entries, 44)
                        $bottom = [BitConverter]::ToInt32($tiles.Entries, 48)
                        $dpi = [CpcpSmokeNative]::GetDpiForWindow($tiles.Window)
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
                        Assert-Cpcp ($gray -gt 20 -and $white -gt 5) "Category $letter in view $identifier uses gray $level and a white letter (gray=$gray, glyph=$white)"
                        $centerY = $bottom - $inset - [int]($size / 2)
                        Assert-Cpcp ($bitmap.GetPixel($right - $inset - $size - 1, $centerY).ToArgb() -ne $fill -and
                            $bitmap.GetPixel($right - $inset - $size + 1, $centerY).ToArgb() -eq $fill) "Category $letter in view $identifier uses the smaller 20px circle"
                        if ($identifier -eq 125) {
                            $historyMask = $mask.ToString()
                            Assert-Cpcp ($masks.Add($historyMask)) "Category $letter has a distinct letter"
                        } else {
                            Assert-Cpcp ($mask.ToString() -eq $historyMask) "Favorite category $letter matches the history badge"
                        }
                        $clip = $bitmap.Clone([Drawing.Rectangle]::FromLTRB($left, $top, $right, $bottom), $bitmap.PixelFormat)
                        $columnOffset = if ($identifier -eq 128) { 648 } else { 0 }
                        try { $montageGraphics.DrawImageUnscaled($clip, ($categoryIndex % 3) * 216 + $columnOffset, [int][Math]::Floor($categoryIndex / 3) * 224) }
                        finally { $clip.Dispose() }
                    } finally { $bitmap.Dispose() }
                }
                $categoryIndex++
            }
            $montage.Save((Join-Path ([IO.Path]::GetTempPath()) 'CPCP_categories.png'), [Drawing.Imaging.ImageFormat]::Png)
            Write-Output '[OKAY] All isolated category badge tests passed'
        } finally { $montageGraphics.Dispose(); $montage.Dispose() }
        return
    }
    if ($SampleOnly) {
        $tabs = [CpcpSmokeNative]::GetDlgItem($text.Viewer, 120)
        Assert-Cpcp ((Send-Cpcp $tabs 4868).ToInt32() -eq 6) 'Viewer has six tabs'
        $symbols = [CpcpSmokeNative]::GetDlgItem($text.Viewer, 132)
        Assert-Cpcp ($symbols -ne [IntPtr]::Zero) 'Symbols has an independent pane'
        Assert-Cpcp ([CpcpSmokeNative]::GetDlgItem($symbols, 4000) -ne [IntPtr]::Zero) 'Symbols has the Character Map button'
        $symbolCount = 0
        for ($identifier = 2000; $identifier -lt 2124; $identifier++) {
            if ([CpcpSmokeNative]::GetDlgItem($symbols, $identifier) -ne [IntPtr]::Zero) { $symbolCount++ }
        }
        Assert-Cpcp ($symbolCount -eq 124) 'All 124 symbols are expanded in the Symbols tab'
        $historyBefore = Get-CpcpTiles
        [void](Send-Cpcp $text.Viewer 33890 ([IntPtr]4))
        $samples = Get-CpcpTiles 131
        Assert-Cpcp ([CpcpSmokeNative]::IsWindowVisible($samples.Window)) 'Debug tab is shown before Setting'
        Assert-Cpcp ((Send-Cpcp $tabs 4875).ToInt32() -eq 4) 'Debug is tab index 4'
        Assert-Cpcp ($samples.Count -eq 18) "Debug creates 6 plus panels and 12 sample panels (actual: $($samples.Count))"
        $samplePath = Join-Path $temporary 'samples\Lorem Ipsum.txt'
        Assert-Cpcp ((Test-Path -LiteralPath $samplePath) -and ([IO.File]::ReadAllText($samplePath) -eq 'Lorem Ipsum')) 'Lorem Ipsum sample file is created'
        $dpi = [CpcpSmokeNative]::GetDpiForWindow($samples.Window)
        $cell = [int][Math]::Floor($dpi + 0.5)
        $gap = [int][Math]::Floor(8 * $dpi / 96.0 + 0.5)
        $small = [int][Math]::Floor(48 * $dpi / 96.0 + 0.5)
        $expectedKinds = 2, 3, 4, 2, 2, 2, 2, 2, 3, 4, 6, 5
        $expectedCategories = 'T', 'T', 'T', 'R', 'F', 'A', 'D', 'I', 'I', 'I', 'I', 'I'
        $colors = @(@(214, 69, 65), @(236, 150, 34), @(46, 160, 90), @(52, 120, 205), @(130, 80, 190))
        [void](Send-Cpcp $samples.Window 277 ([IntPtr]6))
        $client = New-Object int[] 4
        [void][CpcpSmokeNative]::GetClientRect($samples.Window, $client)
        $bitmap = New-Object Drawing.Bitmap $client[2], $client[3]
        $graphics = [Drawing.Graphics]::FromImage($bitmap)
        try {
            $screenBounds = New-Object int[] 4
            [void][CpcpTabSmokeNative]::GetWindowRect($samples.Window, $screenBounds)
            [void][CpcpTabSmokeNative]::DwmFlush()
            $graphics.CopyFromScreen($screenBounds[0], $screenBounds[1], 0, 0, $bitmap.Size)
        } finally { $graphics.Dispose() }
        try {
            for ($index = 0; $index -lt 12; $index++) {
                $entry = $index
                $kind = $expectedKinds[$index]
                $left = [BitConverter]::ToInt32($samples.Entries, $entry * 72 + 36)
                $top = [BitConverter]::ToInt32($samples.Entries, $entry * 72 + 40)
                $right = [BitConverter]::ToInt32($samples.Entries, $entry * 72 + 44)
                $bottom = [BitConverter]::ToInt32($samples.Entries, $entry * 72 + 48)
                $columns = switch ($kind) { 1 { 4 } 4 { 4 } 5 { 1 } default { 2 } }
                $height = switch ($kind) { 2 { $small } 3 { $cell } 4 { $cell } default { 2 * $cell + $gap } }
                Assert-Cpcp ([BitConverter]::ToInt32($samples.Entries, $entry * 72 + 52) -eq $kind -and ($right - $left) -eq $columns * ($cell + $gap) - $gap -and ($bottom - $top) -eq $height) "Sample $index ($($expectedCategories[$index])) uses panel kind $kind"
                $level = if ($expectedCategories[$index] -in 'F', 'A', 'D') { 88 } else { 160 }
                $badge = $bitmap.GetPixel($right - [int](13 * $dpi / 96) , $bottom - [int](21 * $dpi / 96))
                Assert-Cpcp ($badge.R -eq $level -and $badge.G -eq $level -and $badge.B -eq $level) "Sample $index shows the $($expectedCategories[$index]) category badge color"
                if ($index -ge 7) {
                    $color = $bitmap.GetPixel([int](($left + $right) / 2), [int](($top + $bottom) / 2) + [int](8 * $dpi / 96))
                    $expected = $colors[$index - 7]
                    Assert-Cpcp ($color.R -eq $expected[0] -and $color.G -eq $expected[1] -and $color.B -eq $expected[2]) "Image sample $($index - 6) uses a unique solid color"
                }
                for ($other = 0; $other -lt $entry; $other++) {
                    $otherLeft = [BitConverter]::ToInt32($samples.Entries, $other * 72 + 36)
                    $otherTop = [BitConverter]::ToInt32($samples.Entries, $other * 72 + 40)
                    $otherRight = [BitConverter]::ToInt32($samples.Entries, $other * 72 + 44)
                    $otherBottom = [BitConverter]::ToInt32($samples.Entries, $other * 72 + 48)
                    if (-not ($right -le $otherLeft -or $left -ge $otherRight -or $bottom -le $otherTop -or $top -ge $otherBottom)) { throw "Samples $index and $other overlap" }
                }
            }
            Write-Output '[OKAY] Sample panels do not overlap'
            $bitmap.Save((Join-Path ([IO.Path]::GetTempPath()) 'CPCP_samples.png'), [Drawing.Imaging.ImageFormat]::Png)
        } finally { $bitmap.Dispose() }
        for ($index = 0; $index -lt 12; $index++) {
            $before = Get-CpcpTiles
            $samples = Get-CpcpTiles 131
            $centerX = [int](([BitConverter]::ToInt32($samples.Entries, $index * 72 + 36) + [BitConverter]::ToInt32($samples.Entries, $index * 72 + 44)) / 2)
            $centerY = [int](([BitConverter]::ToInt32($samples.Entries, $index * 72 + 40) + [BitConverter]::ToInt32($samples.Entries, $index * 72 + 48)) / 2)
            [void](Send-Cpcp $samples.Window 514 ([IntPtr]::Zero) ([IntPtr](($centerY -shl 16) -bor $centerX)))
            $after = Get-CpcpTiles
            Assert-Cpcp ($after.Count -eq $before.Count + 1) "Clicking sample $index adds a ClipMaster clip"
            Assert-Cpcp ([BitConverter]::ToInt32($after.Entries, 52) -eq $expectedKinds[$index] -and ([BitConverter]::ToInt32($after.Entries, 8) -ne 0) -eq ($index -ge 7)) "Clicked sample $index keeps its size and preview in ClipMaster"
        }
        Assert-Cpcp ((Send-Cpcp $historyBefore.Main 33071 ([IntPtr]1)).ToInt32() -ne 0) 'Clicked samples are saved in history'
        Stop-Cpcp $testProcess
        $testProcess = Start-Cpcp $temporary
        $restored = Get-CpcpTiles
        Assert-Cpcp ($restored.Count -eq $historyBefore.Count + 12) 'Clicked samples survive restart'
        Write-Output '[OKAY] All isolated sample tab tests passed'
        return
    }
    $panel = [CpcpSmokeNative]::GetDlgItem($text.Viewer, 122)
    foreach ($control in 5420..5422) {
        Assert-Cpcp ([CpcpSmokeNative]::GetDlgItem($panel, $control) -eq [IntPtr]::Zero) "Initial alignment control $control is removed"
    }
    $unicodeOption = [CpcpSmokeNative]::GetDlgItem($panel, 5116)
    Assert-Cpcp ((Get-CpcpText $unicodeOption).StartsWith('Ansi2Unicode text')) 'Unicode option uses the new name and original control ID'
    [void](Send-Cpcp $text.Viewer 33890 ([IntPtr]4))
    $viewerBounds = New-Object int[] 4
    [void][CpcpTabSmokeNative]::GetWindowRect($text.Viewer, $viewerBounds)
    $workArea = New-Object int[] 4
    Assert-Cpcp ([CpcpTabSmokeNative]::SystemParametersInfo(48, 0, $workArea, 0)) 'Read desktop work area'
    Assert-Cpcp ([Math]::Abs($viewerBounds[0] + $viewerBounds[2] - $workArea[0] - $workArea[2]) -le 2 -and
        [Math]::Abs($viewerBounds[1] + $viewerBounds[3] - $workArea[1] - $workArea[3]) -le 2) 'Default startup window is centered horizontally and vertically'
    foreach ($width in @(($viewerBounds[2] - $viewerBounds[0]), 520)) {
        [void][CpcpTabSmokeNative]::MoveWindow($text.Viewer, $viewerBounds[0], $viewerBounds[1], $width, $viewerBounds[3] - $viewerBounds[1], $true)
        [void](Send-Cpcp $text.Viewer 0)
        $optionBounds = New-Object int[] 4
        $handlerBounds = New-Object int[] 4
        $excludeBounds = New-Object int[] 4
        $firstHandler = New-Object int[] 4
        [void][CpcpTabSmokeNative]::GetWindowRect($unicodeOption, $optionBounds)
        [void][CpcpTabSmokeNative]::GetWindowRect([CpcpSmokeNative]::GetDlgItem($panel, 6004), $handlerBounds)
        [void][CpcpTabSmokeNative]::GetWindowRect([CpcpSmokeNative]::GetDlgItem($panel, 6003), $excludeBounds)
        [void][CpcpTabSmokeNative]::GetWindowRect([CpcpSmokeNative]::GetDlgItem($panel, 5440), $firstHandler)
        Assert-Cpcp ($optionBounds[0] -gt $handlerBounds[0] -and $optionBounds[1] -gt $handlerBounds[1] -and
            $optionBounds[2] -lt $handlerBounds[2] -and $optionBounds[3] -lt $handlerBounds[3]) "Unicode option is inside the paste handler group at width $width"
        $apart = $optionBounds[2] -le $excludeBounds[0] -or $optionBounds[0] -ge $excludeBounds[2] -or
            $optionBounds[1] -ge $excludeBounds[3] -or $optionBounds[3] -le $excludeBounds[1]
        Assert-Cpcp ($apart -and $optionBounds[3] -le $firstHandler[1]) "Unicode option does not overlap excluded categories or handler fields at width $width"
    }
    [void][CpcpTabSmokeNative]::MoveWindow($text.Viewer, $viewerBounds[0], $viewerBounds[1], $viewerBounds[2] - $viewerBounds[0], $viewerBounds[3] - $viewerBounds[1], $true)
    if ($SettingsOnly) {
        $panelBounds = New-Object int[] 4
        [void][CpcpTabSmokeNative]::GetWindowRect($text.Viewer, $panelBounds)
        [void][CpcpTabSmokeNative]::GetWindowRect([CpcpSmokeNative]::GetDlgItem($panel, 6004), $handlerBounds)
        $bitmap = New-Object Drawing.Bitmap ($panelBounds[2] - $panelBounds[0]), ($panelBounds[3] - $panelBounds[1])
        $graphics = [Drawing.Graphics]::FromImage($bitmap)
        $context = $graphics.GetHdc()
        try { Assert-Cpcp ([CpcpTabSmokeNative]::PrintWindow($text.Viewer, $context, 0)) 'Paste handler settings render to a bitmap' }
        finally { $graphics.ReleaseHdc($context); $graphics.Dispose() }
        try {
            $crop = [Drawing.Rectangle]::FromLTRB($handlerBounds[0] - $panelBounds[0], $handlerBounds[1] - $panelBounds[1],
                $handlerBounds[2] - $panelBounds[0], $handlerBounds[3] - $panelBounds[1])
            $preview = $bitmap.Clone($crop, $bitmap.PixelFormat)
            try { $preview.Save((Join-Path ([IO.Path]::GetTempPath()) 'CPCP_unicode_handler.png'), [Drawing.Imaging.ImageFormat]::Png) }
            finally { $preview.Dispose() }
        } finally { $bitmap.Dispose() }
        foreach ($legacyAlignment in 0, 2) {
            Stop-Cpcp $testProcess
            $testProcess = $null
            [IO.File]::WriteAllText((Join-Path $temporary 'cpcp.ini'), "[main]`r`nclipboard_watch=0`r`nshow_trayicon=0`r`n[config]`r`nstart_with_windows=0`r`nremember_layout=0`r`nsaved_geometry=0`r`nalignment=$legacyAlignment`r`n")
            $testProcess = Start-Cpcp $temporary
            $startup = Get-CpcpTiles
            $bounds = New-Object int[] 4
            [void][CpcpTabSmokeNative]::GetWindowRect($startup.Viewer, $bounds)
            Assert-Cpcp ([Math]::Abs($bounds[0] + $bounds[2] - $workArea[0] - $workArea[2]) -le 2 -and
                [Math]::Abs($bounds[1] + $bounds[3] - $workArea[1] - $workArea[3]) -le 2) "Startup ignores legacy alignment=$legacyAlignment and stays centered"
        }
        Write-Output '[OKAY] All isolated settings layout tests passed'
        return
    }
    [void](Send-Cpcp $text.Viewer 33890)
    $favoriteLimit = [CpcpSmokeNative]::GetDlgItem($panel, 5107)
    $limitText = Get-CpcpText $favoriteLimit
    Assert-Cpcp ($limitText -eq '50') "Favorite count defaults to 50 (actual: $limitText)"
    Set-CpcpText $favoriteLimit '100'
    Assert-Cpcp ((Get-CpcpText $favoriteLimit) -eq '100') 'Favorite count input changes to 100'
    [void](Send-Cpcp $text.Viewer 273 ([IntPtr]123))
    $saveStatus = Get-CpcpText ([CpcpSmokeNative]::GetDlgItem($panel, 124))
    Assert-Cpcp ((Get-Content -LiteralPath (Join-Path $temporary 'cpcp.ini') -Raw) -match '(?m)^favorite_max=100\r?$') "Favorite count accepts and persists 100 (status: $saveStatus)"
    Set-CpcpText $favoriteLimit '101'
    [void](Send-Cpcp $text.Viewer 273 ([IntPtr]123))
    Assert-Cpcp ((Get-Content -LiteralPath (Join-Path $temporary 'cpcp.ini') -Raw) -match '(?m)^favorite_max=100\r?$') 'Favorite count rejects values above 100'
    Set-CpcpText $favoriteLimit '50'
    [void](Send-Cpcp $text.Viewer 273 ([IntPtr]123))
    Assert-Cpcp ([CpcpSmokeNative]::GetDlgItem($panel, 5109) -eq [IntPtr]::Zero) 'Password keyword setting is removed'
    foreach ($field in 5110..5119) {
        Assert-Cpcp ([CpcpSmokeNative]::GetDlgItem($panel, $field) -ne [IntPtr]::Zero) "Remaining setting $field keeps its control ID"
    }
    foreach ($marker in 'ExcludeClipboardContentFromMonitorProcessing', 'application/x-keepassxc-confidential', 'Clipboard Viewer Ignore') {
        $confidential = New-Object System.Windows.Forms.DataObject
        $confidential.SetText('Confidential marker regression ' + [Guid]::NewGuid().ToString())
        $confidential.SetData($marker, $false, [byte[]]@(1))
        [System.Windows.Forms.Clipboard]::SetDataObject($confidential, $true)
        [void](Send-Cpcp $initial.Main 275 ([IntPtr]1))
        Assert-Cpcp ((Get-CpcpTiles).Count -eq $text.Count) "Explicit confidential marker is respected: $marker"
    }
    Assert-Cpcp ([CpcpEditorSmokeNative]::PostMessage($text.Viewer, 33911, [IntPtr]5, [IntPtr]::Zero)) 'Open portrait favorite editor'
    $editorTimer = [Diagnostics.Stopwatch]::StartNew()
    do {
        $editor = [CpcpSmokeNative]::FindWindow('ClipMasterEditor', 'Favorite Clip')
    } while (($editor -eq [IntPtr]::Zero -or -not [CpcpSmokeNative]::IsWindowVisible($editor)) -and $editorTimer.ElapsedMilliseconds -lt 3000)
    Assert-Cpcp ($editor -ne [IntPtr]::Zero) 'Portrait favorite editor is ready'
    $sizes = [CpcpSmokeNative]::GetDlgItem($editor, 303)
    Assert-Cpcp ((Send-Cpcp $sizes 326).ToInt32() -eq 7 -and (Send-Cpcp $sizes 327).ToInt32() -eq 4) 'Fifth portrait panel size is selected with automatic sizing available'
    [void](Send-Cpcp ([CpcpSmokeNative]::GetDlgItem($editor, 301)) 258 ([IntPtr]65))
    [void](Send-Cpcp $editor 273 ([IntPtr]1))
    Assert-Cpcp ([CpcpSmokeNative]::FindWindow('ClipMasterEditor', 'Favorite Clip') -eq $editor) 'Portrait panel rejects text'
    foreach ($dimensions in @(@(240, 80), @(160, 160), @(80, 240))) {
        $bitmap = New-Object System.Drawing.Bitmap $dimensions[0], $dimensions[1]
        try {
            $graphics = [System.Drawing.Graphics]::FromImage($bitmap)
            try { $graphics.Clear([System.Drawing.Color]::DarkCyan) } finally { $graphics.Dispose() }
            Set-CpcpImage $bitmap
        } finally { $bitmap.Dispose() }
        Paste-CpcpEditorImage $editor $dimensions[0] $dimensions[1]
        [void](Send-Cpcp $editor 273 ([IntPtr]1))
        $remainingEditor = [CpcpSmokeNative]::FindWindow('ClipMasterEditor', 'Favorite Clip')
        if ($dimensions[1] -gt $dimensions[0]) {
            $status = if ($remainingEditor -eq [IntPtr]::Zero) { 'saved' } else { Get-CpcpText ([CpcpSmokeNative]::GetDlgItem($editor, 304)) }
            Assert-Cpcp ($remainingEditor -eq [IntPtr]::Zero) "Portrait panel accepts a tall image (status: $status)"
        } else {
            Assert-Cpcp ($remainingEditor -eq $editor) "Portrait panel rejects image $($dimensions[0]) x $($dimensions[1])"
        }
    }
    [void](Send-Cpcp $initial.Main 275 ([IntPtr]1))
    foreach ($kind in 1..6 + @(0)) {
        $before = Get-CpcpTiles 128
        $editor = @(Open-CpcpEditor $before.Viewer $kind)[-1]
        if ($kind -eq 5 -or $kind -eq 6) {
            $textControl = [CpcpSmokeNative]::GetDlgItem($editor, 301)
            Assert-Cpcp (([CpcpSmokeNative]::GetWindowLong($textControl, -16) -band 0x08000000) -ne 0) "Image-only panel $kind disables text input"
            Assert-Cpcp (([CpcpSmokeNative]::GetWindowLong([CpcpSmokeNative]::GetDlgItem($editor, 308), -16) -band 0x08000000) -ne 0) "Image-only panel $kind disables text mode"
            Set-CpcpText $textControl 'Text must not be saved in this panel'
            [void](Send-Cpcp $editor 273 ([IntPtr]1))
            Assert-Cpcp ([CpcpSmokeNative]::FindWindow('ClipMasterEditor', 'Favorite Clip') -eq $editor) "Image-only panel $kind rejects text on save"
        }
        if ($kind -ge 1 -and $kind -le 4) {
            [void](Send-Cpcp ([CpcpSmokeNative]::GetDlgItem($editor, 301)) 258 ([IntPtr](65 + $kind)))
        } else {
            $bitmap = if ($kind -eq 5) { New-Object System.Drawing.Bitmap 80, 240 } else { New-Object System.Drawing.Bitmap 400, 80 }
            try {
                $graphics = [System.Drawing.Graphics]::FromImage($bitmap)
                try { $graphics.Clear([System.Drawing.Color]::OrangeRed) } finally { $graphics.Dispose() }
                Set-CpcpImage $bitmap
            } finally { $bitmap.Dispose() }
            Paste-CpcpEditorImage $editor $(if ($kind -eq 5) { 80 } else { 400 }) $(if ($kind -eq 5) { 240 } else { 80 })
            if ($kind -eq 5 -or $kind -eq 6) {
                [void](Send-Cpcp $editor 273 ([IntPtr]308))
                Assert-Cpcp ([CpcpSmokeNative]::IsWindowVisible([CpcpSmokeNative]::GetDlgItem($editor, 307))) "Image-only panel $kind cannot switch its image to text"
            }
        }
        [void](Send-Cpcp $editor 273 ([IntPtr]1))
        Assert-Cpcp ([CpcpSmokeNative]::FindWindow('ClipMasterEditor', 'Favorite Clip') -eq [IntPtr]::Zero) "Favorite panel kind $kind saves"
        $favorite = Get-CpcpTiles 128
        Assert-Cpcp ($favorite.Count -eq $before.Count + 1) "Favorite panel kind $kind appears in Favorites"
        Assert-Cpcp ([BitConverter]::ToInt32($favorite.Entries, 52) -eq $kind) "Favorite panel kind $kind preserves its size metadata"
        $dpi = [CpcpSmokeNative]::GetDpiForWindow($favorite.Window)
        $cell = [int][Math]::Floor($dpi + 0.5)
        $gap = [int][Math]::Floor(8 * $dpi / 96.0 + 0.5)
        $columns = if ($kind -eq 5) { 1 } elseif ($kind -eq 1 -or $kind -eq 4 -or $kind -eq 0) { 4 } else { 2 }
        $height = if ($kind -eq 1) { [int][Math]::Floor(24 * $dpi / 96.0 + 0.5) } elseif ($kind -eq 2) { [int][Math]::Floor(48 * $dpi / 96.0 + 0.5) } elseif ($kind -eq 5 -or $kind -eq 6) { 2 * $cell + $gap } else { $cell }
        Assert-Cpcp (([BitConverter]::ToInt32($favorite.Entries, 44) - [BitConverter]::ToInt32($favorite.Entries, 36)) -eq $columns * ($cell + $gap) - $gap -and
            ([BitConverter]::ToInt32($favorite.Entries, 48) - [BitConverter]::ToInt32($favorite.Entries, 40)) -eq $height) "Favorite panel kind $kind displays at the correct size"
    }
    $automaticItem = [IntPtr][BitConverter]::ToInt32($favorite.Entries, 0)
    $editor = @(Open-CpcpEditor $favorite.Viewer 0 $automaticItem)[-1]
    Assert-Cpcp ((Send-Cpcp ([CpcpSmokeNative]::GetDlgItem($editor, 303)) 327).ToInt32() -eq 6) 'Editing an automatic favorite retains automatic sizing'
    [void](Send-Cpcp $editor 273 ([IntPtr]1))
    $favoritesSaved = Get-CpcpTiles 128
    Assert-Cpcp ([Convert]::ToBase64String($favorite.Entries, 36, 16) -eq [Convert]::ToBase64String($favoritesSaved.Entries, 36, 16)) 'Saving an automatic favorite does not shrink its panel'
    $client = New-Object int[] 4
    [void][CpcpSmokeNative]::GetClientRect($favoritesSaved.Window, $client)
    $placeholders = New-Object System.Drawing.Bitmap $client[2], $client[3]
    $graphics = [System.Drawing.Graphics]::FromImage($placeholders)
    $context = $graphics.GetHdc()
    try { Assert-Cpcp ([CpcpTabSmokeNative]::PrintWindow($favoritesSaved.Window, $context, 0)) 'Image-only placeholders render' }
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
            Assert-Cpcp ($grayPixels -gt 5) "Image-only placeholder $index displays its gray label"
        }
        $preview = $placeholders.Clone($crop, $placeholders.PixelFormat)
        try { $preview.Save((Join-Path ([IO.Path]::GetTempPath()) 'CPCP_image_only.png'), [Drawing.Imaging.ImageFormat]::Png) }
        finally { $preview.Dispose() }
    } finally { $placeholders.Dispose() }
    [void](Send-Cpcp $initial.Main 275 ([IntPtr]1))
    foreach ($dimensions in @(@(80, 240, 96, 200), @(96, 192, 96, 200), @(160, 160, 200, 200), @(240, 80, 200, 96), @(400, 80, 408, 96))) {
        $before = Get-CpcpTiles
        $bitmap = New-Object System.Drawing.Bitmap $dimensions[0], $dimensions[1]
        try {
            $graphics = [System.Drawing.Graphics]::FromImage($bitmap)
            try { $graphics.Clear([System.Drawing.Color]::CornflowerBlue) } finally { $graphics.Dispose() }
            Set-CpcpImage $bitmap
        } finally { $bitmap.Dispose() }
        $captureTimer = [Diagnostics.Stopwatch]::StartNew()
        do {
            [void](Send-Cpcp $initial.Main 275 ([IntPtr]1))
            $text = Get-CpcpTiles
        } while ($text.Count -eq $before.Count -and $captureTimer.ElapsedMilliseconds -lt 3000)
        Assert-Cpcp ($text.Count -eq $before.Count + 1) "Image $($dimensions[0]) x $($dimensions[1]) is captured"
        $dpi = [CpcpSmokeNative]::GetDpiForWindow($text.Window)
        $width = [BitConverter]::ToInt32($text.Entries, 44) - [BitConverter]::ToInt32($text.Entries, 36)
        $height = [BitConverter]::ToInt32($text.Entries, 48) - [BitConverter]::ToInt32($text.Entries, 40)
        $cell = [int][Math]::Floor($dpi + 0.5)
        $gap = [int][Math]::Floor(8 * $dpi / 96.0 + 0.5)
        $columns = [int](($dimensions[2] + 8) / 104)
        $rows = [int](($dimensions[3] + 8) / 104)
        Assert-Cpcp ($width -eq $columns * ($cell + $gap) - $gap -and $height -eq $rows * ($cell + $gap) - $gap) "Image $($dimensions[0]) x $($dimensions[1]) uses the correct panel"
        Assert-Cpcp ([Convert]::ToBase64String($before.Positions) -eq [Convert]::ToBase64String($text.Positions, 0, $before.Positions.Length)) 'Image shape preserves older cached positions'
        $favoritesBeforeCopy = Get-CpcpTiles 128
        $historyPanel = Get-CpcpTiles
        [void](Send-Cpcp $historyPanel.Window 277 ([IntPtr]6))
        $clickX = [BitConverter]::ToInt32($historyPanel.Entries, 36) + 10
        $clickY = [BitConverter]::ToInt32($historyPanel.Entries, 40) + 10
        $click = [IntPtr](($clickY -shl 16) -bor ($clickX -band 65535))
        Assert-Cpcp ([CpcpEditorSmokeNative]::PostMessage($historyPanel.Window, 517, [IntPtr]::Zero, $click)) 'Right-click history image to add it to Favorites'
        $dialogTimer = [Diagnostics.Stopwatch]::StartNew()
        do {
            $dialog = [CpcpSmokeNative]::FindWindow('#32770', 'ClipMaster')
        } while (($dialog -eq [IntPtr]::Zero -or -not [CpcpSmokeNative]::IsWindowVisible($dialog)) -and $dialogTimer.ElapsedMilliseconds -lt 3000)
        [uint32]$dialogProcess = 0
        [void][CpcpSmokeNative]::GetWindowThreadProcessId($dialog, [ref]$dialogProcess)
        Assert-Cpcp ($dialog -ne [IntPtr]::Zero -and $dialogProcess -eq $testProcess.Id) 'Isolated favorite confirmation is shown'
        [void](Send-Cpcp $dialog 273 ([IntPtr]1))
        [void](Send-Cpcp $historyPanel.Viewer 0)
        $favoritesAfterCopy = Get-CpcpTiles 128 -NoRefresh
        Assert-Cpcp ($favoritesAfterCopy.Count -eq $favoritesBeforeCopy.Count + 1) "History image $($dimensions[0]) x $($dimensions[1]) immediately appears in Favorites without tab refresh"
        $favoriteWidth = [BitConverter]::ToInt32($favoritesAfterCopy.Entries, 44) - [BitConverter]::ToInt32($favoritesAfterCopy.Entries, 36)
        $favoriteHeight = [BitConverter]::ToInt32($favoritesAfterCopy.Entries, 48) - [BitConverter]::ToInt32($favoritesAfterCopy.Entries, 40)
        Assert-Cpcp ($favoriteWidth -eq $width -and $favoriteHeight -eq $height -and
            [BitConverter]::ToInt32($favoritesAfterCopy.Entries, 8) -ne 0) 'Copied history image keeps its panel size and image preview'
    }
    for ($capture = 0; $capture -lt 18; $capture++) {
        $before = Get-CpcpTiles
        [System.Windows.Forms.Clipboard]::SetText(('Layout regression ' + [Guid]::NewGuid().ToString() + ' ') * (1 + $capture % 4))
        $captureTimer = [Diagnostics.Stopwatch]::StartNew()
        do {
            [void](Send-Cpcp $initial.Main 275 ([IntPtr]1))
            $text = Get-CpcpTiles
        } while ($text.Count -eq $before.Count -and $captureTimer.ElapsedMilliseconds -lt 3000)
        Assert-Cpcp ($text.Count -eq $before.Count + 1) "Layout capture $capture adds one panel"
        Assert-Cpcp ([Convert]::ToBase64String($before.Positions) -eq [Convert]::ToBase64String($text.Positions, 0, $before.Positions.Length)) "Layout capture $capture preserves all older cached positions"
    }
    $bitmap = New-Object System.Drawing.Bitmap 137, 83
    try {
        $graphics = [System.Drawing.Graphics]::FromImage($bitmap)
        try { $graphics.Clear([System.Drawing.Color]::MediumSeaGreen) } finally { $graphics.Dispose() }
        Set-CpcpImage $bitmap
    } finally { $bitmap.Dispose() }
    $captureTimer = [Diagnostics.Stopwatch]::StartNew()
    do {
        [void](Send-Cpcp $initial.Main 275 ([IntPtr]1))
        $image = Get-CpcpTiles
    } while ($image.Count -eq $text.Count -and $captureTimer.ElapsedMilliseconds -lt 3000)
    Assert-Cpcp ($image.Count -eq $text.Count + 1) "Image copy adds one tile ($($text.Count) -> $($image.Count))"
    Assert-Cpcp ([Convert]::ToBase64String($text.Positions) -eq [Convert]::ToBase64String($image.Positions, 0, $text.Positions.Length)) 'Image capture preserves older cached positions'
    $dpi = [CpcpSmokeNative]::GetDpiForWindow($image.Window)
    $cell = [int][Math]::Floor($dpi + 0.5)
    $gap = [int][Math]::Floor(8 * $dpi / 96.0 + 0.5)
    $margin = [int][Math]::Floor(12 * $dpi / 96.0 + 0.5)
    $client = New-Object int[] 4
    [void][CpcpSmokeNative]::GetClientRect($image.Window, $client)
    for ($index = 0; $index -lt $image.Count; $index++) {
        $left = [BitConverter]::ToInt32($image.Entries, $index * 72 + 36)
        $right = [BitConverter]::ToInt32($image.Entries, $index * 72 + 44)
        Assert-Cpcp ((($client[2] - $right - $margin) % ($cell + $gap)) -eq 0 -and (($right - $left + $gap) % ($cell + $gap)) -eq 0) "Tile $index follows the right-anchored grid"
        $top = [BitConverter]::ToInt32($image.Entries, $index * 72 + 40)
        $bottom = [BitConverter]::ToInt32($image.Entries, $index * 72 + 48)
        for ($other = 0; $other -lt $index; $other++) {
            $otherLeft = [BitConverter]::ToInt32($image.Entries, $other * 72 + 36)
            $otherTop = [BitConverter]::ToInt32($image.Entries, $other * 72 + 40)
            $otherRight = [BitConverter]::ToInt32($image.Entries, $other * 72 + 44)
            $otherBottom = [BitConverter]::ToInt32($image.Entries, $other * 72 + 48)
            Assert-Cpcp ($right -le $otherLeft -or $left -ge $otherRight -or $bottom -le $otherTop -or $top -ge $otherBottom) "Tiles $index and $other do not overlap"
        }
        if ($index -gt 0) {
            $previousIndex = [BitConverter]::ToInt32($image.Order, ($index - 1) * 4)
            $paintIndex = [BitConverter]::ToInt32($image.Order, $index * 4)
            $previousTop = [BitConverter]::ToInt32($image.Entries, $previousIndex * 72 + 40)
            $paintTop = [BitConverter]::ToInt32($image.Entries, $paintIndex * 72 + 40)
            $previousLeft = [BitConverter]::ToInt32($image.Entries, $previousIndex * 72 + 36)
            $paintLeft = [BitConverter]::ToInt32($image.Entries, $paintIndex * 72 + 36)
            Assert-Cpcp ($previousTop -lt $paintTop -or ($previousTop -eq $paintTop -and $previousLeft -le $paintLeft)) "Paint step $index proceeds top-to-bottom and left-to-right"
        }
    }
    foreach ($tab in 0, 1, 2, 3, 4, 0) {
        [void](Send-Cpcp $image.Viewer 33890 ([IntPtr]$tab))
        [void](Send-Cpcp $image.Main 33068)
        [void](Send-Cpcp $image.Viewer 33118)
        $root = Send-Cpcp ([CpcpSmokeNative]::GetDlgItem($image.Viewer, 100)) 4362
        if ($root -ne [IntPtr]::Zero) { [void](Send-Cpcp ([CpcpSmokeNative]::GetDlgItem($image.Viewer, 100)) 4363 ([IntPtr]9) $root) }
        foreach ($control in 100, 101) {
            Assert-Cpcp (([CpcpSmokeNative]::GetWindowLong([CpcpSmokeNative]::GetDlgItem($image.Viewer, $control), -16) -band 0x10000000) -eq 0) "Legacy control $control stays hidden on tab $tab"
        }
        $visibleControl = @(125, 128, 129, 132, 131)[$tab]
        Assert-Cpcp ([CpcpSmokeNative]::IsWindowVisible([CpcpSmokeNative]::GetDlgItem($image.Viewer, $visibleControl))) "Tab $tab content is visible"
    }
    $spaceAvailable = [CpcpSmokeNative]::RegisterHotKey([IntPtr]::Zero, 301, 1, 32)
    if ($spaceAvailable) { [void][CpcpSmokeNative]::UnregisterHotKey([IntPtr]::Zero, 301) }
    Assert-Cpcp (-not $spaceAvailable) 'Alt+Space is registered'
    $enterAvailable = [CpcpSmokeNative]::RegisterHotKey([IntPtr]::Zero, 302, 1, 13)
    if ($enterAvailable) { [void][CpcpSmokeNative]::UnregisterHotKey([IntPtr]::Zero, 302) }
    Assert-Cpcp $enterAvailable 'Alt+Enter is not registered'
    Assert-Cpcp ((Send-Cpcp $image.Main 33071 ([IntPtr]1)).ToInt32() -ne 0) 'History saves successfully'
    $favoritesSaved = Get-CpcpTiles 128
    Stop-Cpcp $testProcess
    $testProcess = Start-Cpcp $temporary
    $reloaded = Get-CpcpTiles
    Assert-Cpcp ($reloaded.Count -eq $image.Count) 'History survives restart'
    for ($index = 0; $index -lt $image.Count; $index++) {
        Assert-Cpcp ([Convert]::ToBase64String($image.Entries, $index * 72 + 36, 16) -eq
            [Convert]::ToBase64String($reloaded.Entries, $index * 72 + 36, 16)) "Tile $index keeps its layout after restart"
    }
    $favoritesReloaded = Get-CpcpTiles 128
    Assert-Cpcp ($favoritesReloaded.Count -eq $favoritesSaved.Count) 'All favorite panel types survive restart'
    for ($index = 0; $index -lt $favoritesSaved.Count; $index++) {
        Assert-Cpcp ([Convert]::ToBase64String($favoritesSaved.Entries, $index * 72 + 36, 20) -eq
            [Convert]::ToBase64String($favoritesReloaded.Entries, $index * 72 + 36, 20)) "Favorite panel $index preserves its kind and layout after restart"
    }
    $tabs = [CpcpSmokeNative]::GetDlgItem($reloaded.Viewer, 120)
    $tabClient = New-Object int[] 4
    [void][CpcpSmokeNative]::GetClientRect($tabs, $tabClient)
    $tabImage = New-Object System.Drawing.Bitmap $tabClient[2], $tabClient[3]
    $tabGraphics = [System.Drawing.Graphics]::FromImage($tabImage)
    $tabContext = $tabGraphics.GetHdc()
    try { Assert-Cpcp ([CpcpTabSmokeNative]::PrintWindow($tabs, $tabContext, 0)) 'Colored tabs render to a bitmap' }
    finally { $tabGraphics.ReleaseHdc($tabContext); $tabGraphics.Dispose() }
    try {
        $tabColors = @(
            [Drawing.Color]::FromArgb(219, 235, 252), [Drawing.Color]::FromArgb(219, 242, 226),
            [Drawing.Color]::FromArgb(255, 234, 210), [Drawing.Color]::FromArgb(250, 239, 207),
            [Drawing.Color]::FromArgb(230, 224, 248), [Drawing.Color]::FromArgb(250, 222, 230))
        foreach ($color in $tabColors) {
            $pixels = 0
            for ($pixelY = 0; $pixelY -lt $tabImage.Height; $pixelY++) {
                for ($pixelX = 0; $pixelX -lt $tabImage.Width; $pixelX++) {
                    if ($tabImage.GetPixel($pixelX, $pixelY).ToArgb() -eq $color.ToArgb()) { $pixels++ }
                }
            }
            Assert-Cpcp ($pixels -gt 20) "Tab color $($color.Name) is visible"
        }
        $tabScreenshot = Join-Path ([IO.Path]::GetTempPath()) 'CPCP_tab_colors.png'
        $tabImage.Save($tabScreenshot, [Drawing.Imaging.ImageFormat]::Png)
        Write-Output "[OKAY] Tab screenshot: $tabScreenshot"
    } finally { $tabImage.Dispose() }
    Stop-Cpcp $testProcess
    $testProcess = $null
    $iniFile = Join-Path $temporary 'cpcp.ini'
    $iniText = Get-Content -LiteralPath $iniFile -Raw
    [IO.File]::WriteAllText($iniFile, [regex]::Replace($iniText, '(?m)^favorite_max=\d+', 'favorite_max=200'))
    $testProcess = Start-Cpcp $temporary
    $limited = Get-CpcpTiles
    $limitText = Get-CpcpText ([CpcpSmokeNative]::GetDlgItem([CpcpSmokeNative]::GetDlgItem($limited.Viewer, 122), 5107))
    Assert-Cpcp ($limitText -eq '100') 'Persisted favorite count above 100 is clamped on load'
    $reloaded = $limited
    if ($LayoutOnly) {
        Write-Output '[OKAY] All isolated layout tests passed'
        return
    }
    $launcher = Start-Cpcp $temporary 'CPCPSet.exe'
    if (-not $launcher.WaitForExit(10000)) { throw 'Settings launcher did not exit' }
    [void](Send-Cpcp $reloaded.Main 0)
    Assert-Cpcp ((Send-Cpcp ([CpcpSmokeNative]::GetDlgItem($reloaded.Viewer, 120)) 4875).ToInt32() -eq 5) 'Settings launcher selects Settings'
    [void](Send-Cpcp $reloaded.Main 32871)
    $imageItem = [IntPtr][BitConverter]::ToInt32($reloaded.Entries, 0)
    Assert-Cpcp ((Send-Cpcp $reloaded.Main 33168 ([IntPtr]1) $imageItem).ToInt32() -eq 1) 'Saved image copies after restart'
    Assert-Cpcp ([System.Windows.Forms.Clipboard]::ContainsImage()) 'Saved image is available on the clipboard'
    $handle = [CpcpSmokeNative]::OpenProcess(56, $false, $testProcess.Id)
    try {
        $metadata = New-Object byte[] 100
        $transferred = [IntPtr]::Zero
        Assert-Cpcp ([CpcpSmokeNative]::ReadProcessMemory($handle, $imageItem, $metadata, 100, [ref]$transferred)) 'Read isolated image metadata'
        $formatAddress = [BitConverter]::ToInt32($metadata, 92)
        while ($formatAddress -ne 0) {
            Assert-Cpcp ([CpcpSmokeNative]::ReadProcessMemory($handle, [IntPtr]$formatAddress, $metadata, 100, [ref]$transferred)) 'Read isolated format metadata'
            if ([BitConverter]::ToInt32($metadata, 20) -eq 8) { break }
            $formatAddress = [BitConverter]::ToInt32($metadata, 96)
        }
        Assert-Cpcp ($formatAddress -ne 0) 'Image includes a DIB format for legacy regression'
        $zero = New-Object byte[] 4
        Assert-Cpcp ([CpcpSmokeNative]::WriteProcessMemory($handle, [IntPtr]($formatAddress + 12), $zero, 4, [ref]$transferred)) 'Simulate legacy unnamed format'
        Assert-Cpcp ([CpcpSmokeNative]::WriteProcessMemory($handle, [IntPtr]($formatAddress + 20), $zero, 4, [ref]$transferred)) 'Simulate legacy zero format ID'
    } finally { if ($handle -ne [IntPtr]::Zero) { [void][CpcpSmokeNative]::CloseHandle($handle) } }
    Assert-Cpcp ((Send-Cpcp $reloaded.Main 33168 ([IntPtr]1) $imageItem).ToInt32() -eq 1) 'Image with legacy unnamed format copies without an error'
    Assert-Cpcp ([System.Windows.Forms.Clipboard]::ContainsImage()) 'Legacy image retains a usable clipboard image'
    [void](Send-Cpcp $reloaded.Main 275 ([IntPtr]1))
    Write-Output '[OKAY] All isolated smoke tests passed'
} finally {
    if ($null -ne $testProcess -and -not $testProcess.HasExited) {
        try { Stop-Cpcp $testProcess } catch { $testProcess.Kill(); $testProcess.WaitForExit() }
    }
    if ($clipboardChanged) { [System.Windows.Forms.Clipboard]::SetDataObject($clipboardBackup, $true) }
    if (Test-Path -LiteralPath $temporary) { Remove-Item -LiteralPath $temporary -Recurse -Force }
    if ($existing.Count -gt 0) { Start-Process -FilePath (Join-Path $BuildDirectory 'CPCP.exe') -WorkingDirectory $BuildDirectory }
}