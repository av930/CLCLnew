param([string]$BuildDirectory = (Join-Path $PSScriptRoot '..\x64\Release'))

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
if ([IntPtr]::Size -ne 8) { throw 'Run this test in 64-bit Windows PowerShell' }
Add-Type -AssemblyName System.Windows.Forms
Add-Type -AssemblyName System.Drawing
if (-not ('ClclX64Native' -as [type])) {
    Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
public static class ClclX64Native {
    [StructLayout(LayoutKind.Sequential)] public struct Rect { public int Left, Top, Right, Bottom; }
    [StructLayout(LayoutKind.Sequential)] public struct Date { public ushort Year, Month, DayOfWeek, Day, Hour, Minute, Second, Milliseconds; }
    [StructLayout(LayoutKind.Sequential)] public struct Entry {
        public IntPtr Item, Text, Image;
        public int Width, Height;
        public Date Modified;
        public Rect Bounds;
        public int Kind;
        public Rect Divider;
    }
    [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern IntPtr FindWindow(string name, string title);
    [DllImport("user32.dll")] public static extern IntPtr GetDlgItem(IntPtr window, int id);
    [DllImport("user32.dll")] public static extern IntPtr GetWindowLongPtr(IntPtr window, int index);
    [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr window, out uint process);
    [DllImport("user32.dll")] public static extern IntPtr SendMessageTimeout(IntPtr window, uint message, IntPtr parameter, IntPtr data, uint flags, uint timeout, out UIntPtr result);
    [DllImport("user32.dll")] public static extern bool PostMessage(IntPtr window, uint message, IntPtr parameter, IntPtr data);
    [DllImport("kernel32.dll")] static extern IntPtr OpenProcess(uint access, bool inherit, uint process);
    [DllImport("kernel32.dll")] static extern bool ReadProcessMemory(IntPtr process, IntPtr address, byte[] buffer, int size, out IntPtr read);
    [DllImport("kernel32.dll")] static extern bool CloseHandle(IntPtr handle);
    public static Entry[] Read(IntPtr window) {
        uint owner;
        GetWindowThreadProcessId(window, out owner);
        IntPtr process = OpenProcess(16, false, owner), read;
        try {
            byte[] header = new byte[16];
            if (!ReadProcessMemory(process, GetWindowLongPtr(window, -21), header, 16, out read)) throw new Exception("Cannot read x64 tile state");
            int count = BitConverter.ToInt32(header, 8), size = Marshal.SizeOf(typeof(Entry));
            if (count < 0 || count > 10000) throw new Exception("Invalid tile count");
            Entry[] result = new Entry[count];
            if (count == 0) return result;
            byte[] bytes = new byte[count * size];
            if (!ReadProcessMemory(process, new IntPtr(BitConverter.ToInt64(header, 0)), bytes, bytes.Length, out read)) throw new Exception("Cannot read x64 tile entries");
            GCHandle pinned = GCHandle.Alloc(bytes, GCHandleType.Pinned);
            try {
                for (int index=0; index<count; index++) result[index]=(Entry)Marshal.PtrToStructure(IntPtr.Add(pinned.AddrOfPinnedObject(), index*size), typeof(Entry));
            } finally { pinned.Free(); }
            return result;
        } finally { CloseHandle(process); }
    }
}
'@
}

function Assert-X64([bool]$Condition, [string]$Message) {
    if (-not $Condition) { throw $Message }
    Write-Output "[OKAY] $Message"
}
function Send-X64([IntPtr]$Window, [uint32]$Message, [IntPtr]$Parameter = [IntPtr]::Zero, [IntPtr]$Data = [IntPtr]::Zero) {
    $result = [UIntPtr]::Zero
    if ([ClclX64Native]::SendMessageTimeout($Window, $Message, $Parameter, $Data, 2, 5000, [ref]$result) -eq [IntPtr]::Zero) { throw "Message failed: $Message" }
    return [IntPtr][long]$result.ToUInt64()
}
function Wait-X64([scriptblock]$Check) {
    $timer = [Diagnostics.Stopwatch]::StartNew()
    do { $ready = & $Check } while (-not $ready -and $timer.ElapsedMilliseconds -lt 5000)
    if (-not $ready) { throw 'Timed out waiting for CLCL' }
}
function Stop-X64([Diagnostics.Process]$Process) {
    if ($null -eq $Process -or $Process.HasExited) { return }
    $main = [ClclX64Native]::FindWindow('CLCLNewMain', 'CLCL_new')
    [uint32]$owner = 0
    [void][ClclX64Native]::GetWindowThreadProcessId($main, [ref]$owner)
    if ($owner -ne $Process.Id) { throw 'Refusing to close an unrelated process' }
    [void](Send-X64 $main 16)
    if (-not $Process.WaitForExit(5000)) { throw 'CLCL did not exit normally' }
}
function Open-X64 {
    $process = Start-Process -FilePath (Join-Path $temporary 'CLCL.exe') -WorkingDirectory $temporary -ArgumentList '/cmshow' -PassThru
    [void]$process.WaitForInputIdle(10000)
    return $process
}
function Tiles-X64([int]$Id = 125) {
    [void](Send-X64 $viewer 33890 ([IntPtr][int]$(if ($Id -eq 128) { 1 } else { 0 })))
    return ,([ClclX64Native]::Read([ClclX64Native]::GetDlgItem($viewer, $Id)))
}

$BuildDirectory = (Resolve-Path -LiteralPath $BuildDirectory).Path
foreach ($name in 'CLCL.exe','CLCLHook.dll','CLCLSet.exe') {
    $bytes = [IO.File]::ReadAllBytes((Join-Path $BuildDirectory $name))
    $offset = [BitConverter]::ToInt32($bytes, 60)
    Assert-X64 ([BitConverter]::ToUInt16($bytes, $offset + 4) -eq 0x8664) "$name is AMD64"
}
$existing = @(Get-Process CLCL -ErrorAction SilentlyContinue)
$restart = @($existing | ForEach-Object { $_.Path })
$temporary = Join-Path ([IO.Path]::GetTempPath()) ('CLCL_x64_' + [Guid]::NewGuid().ToString('N'))
$testProcess = $null
$backup = New-Object Windows.Forms.DataObject
$clipboard = [Windows.Forms.Clipboard]::GetDataObject()
if ($null -ne $clipboard) {
    foreach ($format in $clipboard.GetFormats($false)) {
        $content = $clipboard.GetData($format, $false)
        if ($null -ne $content) { $backup.SetData($format, $false, $content) }
    }
}
try {
    foreach ($process in $existing) { Stop-X64 $process }
    [void][IO.Directory]::CreateDirectory($temporary)
    foreach ($name in 'CLCL.exe','CLCLHook.dll','CLCLSet.exe') { Copy-Item (Join-Path $BuildDirectory $name) $temporary }
    [IO.File]::WriteAllText((Join-Path $temporary 'clcl_app.ini'), "[GENERAL]`r`nportable=1`r`n")
    [IO.File]::WriteAllText((Join-Path $temporary 'clcl.ini'), "[main]`r`nclipboard_watch=1`r`nshow_trayicon=0`r`n[config]`r`nstart_with_windows=0`r`n")
    $testProcess = Open-X64
    Wait-X64 { [ClclX64Native]::GetDlgItem([ClclX64Native]::FindWindow('CLCLViewer', 'ClipMaster'),120) -ne [IntPtr]::Zero }
    $main = [ClclX64Native]::FindWindow('CLCLNewMain', 'CLCL_new')
    $viewer = [ClclX64Native]::FindWindow('CLCLViewer', 'ClipMaster')
    Assert-X64 ((Send-X64 ([ClclX64Native]::GetDlgItem($viewer, 120)) 4868).ToInt64() -eq 5) 'Five tabs are available'
    [Windows.Forms.Clipboard]::SetDataObject('x64 baseline ' + [Guid]::NewGuid(), $true, 10, 100)
    [void](Send-X64 $main 275 ([IntPtr]1))
    $before = (Tiles-X64).Count
    [Windows.Forms.Clipboard]::SetDataObject('x64 clipboard regression ' + [Guid]::NewGuid(), $true, 10, 100)
    Wait-X64 { [void](Send-X64 $main 275 ([IntPtr]1)); (Tiles-X64).Count -eq $before + 1 }
    Assert-X64 ((Tiles-X64).Count -eq $before + 1) 'Text capture works'
    $bitmap = New-Object Drawing.Bitmap 320,160
    try {
        $graphics = [Drawing.Graphics]::FromImage($bitmap)
        try { $graphics.Clear([Drawing.Color]::ForestGreen) } finally { $graphics.Dispose() }
        $data = New-Object Windows.Forms.DataObject
        $data.SetImage($bitmap)
        [Windows.Forms.Clipboard]::SetDataObject($data, $true, 10, 100)
        Wait-X64 { [void](Send-X64 $main 275 ([IntPtr]1)); (Tiles-X64)[0].Image -ne [IntPtr]::Zero }
    } finally { $bitmap.Dispose() }
    $history = Tiles-X64
    Assert-X64 ($history[0].Width -eq 320 -and $history[0].Height -eq 160) 'Image preview works'
    $favoriteBefore = (Tiles-X64 128).Count
    Assert-X64 ([ClclX64Native]::PostMessage($viewer,33909,[IntPtr]::Zero,$history[0].Item)) 'Favorite command accepts a 64-bit item pointer'
    Wait-X64 { [ClclX64Native]::FindWindow('#32770','ClipMaster') -ne [IntPtr]::Zero }
    $dialog = [ClclX64Native]::FindWindow('#32770','ClipMaster')
    [uint32]$owner = 0
    [void][ClclX64Native]::GetWindowThreadProcessId($dialog,[ref]$owner)
    Assert-X64 ($owner -eq $testProcess.Id) 'Favorite confirmation belongs to the isolated test'
    [void](Send-X64 $dialog 273 ([IntPtr]1))
    Wait-X64 { (Tiles-X64 128).Count -eq $favoriteBefore + 1 }
    Assert-X64 ((Tiles-X64 128)[0].Image -ne [IntPtr]::Zero) 'Image favorite is saved'
    $historyCount = (Tiles-X64).Count
    $favoriteCount = (Tiles-X64 128).Count
    [void](Send-X64 $main 32875 ([IntPtr]1))
    Stop-X64 $testProcess
    $testProcess = Open-X64
    Wait-X64 { [ClclX64Native]::GetDlgItem([ClclX64Native]::FindWindow('CLCLViewer', 'ClipMaster'),120) -ne [IntPtr]::Zero }
    $main = [ClclX64Native]::FindWindow('CLCLNewMain', 'CLCL_new')
    $viewer = [ClclX64Native]::FindWindow('CLCLViewer', 'ClipMaster')
    Assert-X64 ((Tiles-X64).Count -eq $historyCount) 'History survives restart'
    Assert-X64 ((Tiles-X64 128).Count -eq $favoriteCount) 'Favorites survive restart'
    Write-Output '[OKAY] All isolated x64 release tests passed'
} finally {
    if ($null -ne $testProcess -and -not $testProcess.HasExited) { Stop-X64 $testProcess }
    [Windows.Forms.Clipboard]::SetDataObject($backup,$true,10,100)
    if (Test-Path $temporary) { Remove-Item $temporary -Recurse -Force }
    foreach ($path in $restart) { Start-Process -FilePath $path -WorkingDirectory (Split-Path $path) }
}