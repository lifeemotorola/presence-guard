# Exercises the packaged native app and the real Windows keyboard hook.
# This script types only synthetic test keys into a separate disposable window.
# Copyright (C) 2026 Emmanuel Suah. SPDX-License-Identifier: GPL-2.0-or-later
param(
  [Parameter(Mandatory=$true)][string]$App,
  [Parameter(Mandatory=$true)][string]$Target,
  [string]$Screenshot = ''
)
$ErrorActionPreference = 'Stop'

Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
using System.Text;

[ComImport, Guid("618736e0-3c3d-11cf-810c-00aa00389b71"), InterfaceType(ComInterfaceType.InterfaceIsIDispatch)]
public interface IAccessibleNative {
  [DispId(-5003)]
  string this[[In, MarshalAs(UnmanagedType.Struct)] object child] {
    [return: MarshalAs(UnmanagedType.BStr)] get;
  }
}
public static class Native {
  [StructLayout(LayoutKind.Sequential)] public struct RECT { public int Left, Top, Right, Bottom; }
  [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern IntPtr FindWindow(string cls, string title);
  // PowerShell converts a null string argument to empty; call the P/Invoke in
  // C# when a genuine null wildcard is needed.
  public static IntPtr FindClass(string cls) { return FindWindow(cls, null); }
  public static IntPtr FindTitle(string title) { return FindWindow(null, title); }
  [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern IntPtr FindWindowEx(IntPtr parent, IntPtr after, string cls, string title);
  public static IntPtr FindButton(IntPtr parent, string title) { return FindWindowEx(parent, IntPtr.Zero, "Button", title); }
  [DllImport("user32.dll")] public static extern IntPtr GetDlgItem(IntPtr window, int id);
  [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern IntPtr SendMessage(IntPtr window, uint msg, IntPtr wp, StringBuilder lp);
  [DllImport("user32.dll")] public static extern bool PostMessage(IntPtr window, uint msg, IntPtr wp, IntPtr lp);
  [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr window, out RECT rect);
  [DllImport("user32.dll")] public static extern bool SetWindowPos(IntPtr window, IntPtr after, int x, int y, int cx, int cy, uint flags);
  [DllImport("user32.dll")] public static extern int GetWindowLongW(IntPtr window, int index);
  [DllImport("user32.dll")] public static extern bool SetCursorPos(int x, int y);
  [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr window);
  [DllImport("user32.dll")] public static extern IntPtr GetForegroundWindow();
  [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr window, out uint pid);
  [DllImport("user32.dll")] public static extern bool IsWindowVisible(IntPtr window);
  [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern int GetClassName(IntPtr window, StringBuilder name, int capacity);
  [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern int GetWindowText(IntPtr window, StringBuilder name, int capacity);
  public delegate bool EnumWindowsProc(IntPtr window, IntPtr extra);
  [DllImport("user32.dll")] public static extern bool EnumWindows(EnumWindowsProc callback, IntPtr extra);
  [DllImport("user32.dll")] public static extern bool EnumChildWindows(IntPtr parent, EnumWindowsProc callback, IntPtr extra);
  [DllImport("user32.dll")] public static extern int GetDlgCtrlID(IntPtr hwnd);
  public static string DescribeChildren(IntPtr parent) {
    var result = new StringBuilder();
    EnumWindowsProc callback = delegate(IntPtr hwnd, IntPtr extra) {
      var cls = new StringBuilder(256); var title = new StringBuilder(512);
      GetClassName(hwnd, cls, cls.Capacity); GetWindowText(hwnd, title, title.Capacity);
      result.Append(" [").Append(cls).Append(" | id=").Append(GetDlgCtrlID(hwnd)).Append(" | ").Append(title).Append("]");
      return true;
    };
    EnumChildWindows(parent, callback, IntPtr.Zero);
    return result.Length == 0 ? "no child windows" : result.ToString();
  }
  public static string DescribeWindows(int pid) {
    var result = new StringBuilder();
    EnumWindowsProc callback = delegate(IntPtr hwnd, IntPtr extra) {
      uint owner;
      GetWindowThreadProcessId(hwnd, out owner);
      if (owner == (uint)pid) {
        var cls = new StringBuilder(256); var title = new StringBuilder(512);
        GetClassName(hwnd, cls, cls.Capacity); GetWindowText(hwnd, title, title.Capacity);
        result.Append(" [").Append(cls).Append(" | ").Append(title).Append(" | visible=").Append(IsWindowVisible(hwnd)).Append("]");
      }
      return true;
    };
    EnumWindows(callback, IntPtr.Zero);
    return result.Length == 0 ? "no top-level windows" : result.ToString();
  }
  [DllImport("user32.dll")] public static extern void mouse_event(uint flags, uint dx, uint dy, uint data, UIntPtr extra);
  [DllImport("user32.dll")] public static extern void keybd_event(byte key, byte scan, uint flags, UIntPtr extra);
  [DllImport("oleacc.dll")] public static extern int AccessibleObjectFromWindow(IntPtr hwnd, uint objectId, ref Guid iid,
    [MarshalAs(UnmanagedType.Interface)] out IAccessibleNative accessible);
  public static string Text(IntPtr hwnd) {
    var text = new StringBuilder(512);
    SendMessage(hwnd, 0x000D, (IntPtr)text.Capacity, text);
    return text.ToString();
  }
  public static string AccessibleName(IntPtr hwnd) {
    Guid iid = new Guid("618736e0-3c3d-11cf-810c-00aa00389b71");
    IAccessibleNative result;
    int hr = AccessibleObjectFromWindow(hwnd, 0xFFFFFFFC, ref iid, out result);
    if (hr != 0 || result == null) throw new Exception("AccessibleObjectFromWindow: " + hr);
    return result[0];
  }
  public static void Click(IntPtr hwnd) {
    RECT r;
    if (!GetWindowRect(hwnd, out r)) throw new Exception("Cannot find click target");
    SetCursorPos((r.Left + r.Right) / 2, (r.Top + r.Bottom) / 2);
    mouse_event(0x0002, 0, 0, 0, UIntPtr.Zero);
    mouse_event(0x0004, 0, 0, 0, UIntPtr.Zero);
  }
  public static void Down(byte key) { keybd_event(key, 0, 0, UIntPtr.Zero); }
  public static void Up(byte key) { keybd_event(key, 0, 2, UIntPtr.Zero); }
  public static void Tap(byte key) { Down(key); System.Threading.Thread.Sleep(12); Up(key); System.Threading.Thread.Sleep(12); }
}
'@

function Wait-For([scriptblock]$condition, [string]$message, [int]$milliseconds = 12000) {
  $clock = [Diagnostics.Stopwatch]::StartNew()
  do {
    if (& $condition) { return }
    Start-Sleep -Milliseconds 100
  } while ($clock.ElapsedMilliseconds -lt $milliseconds)
  throw "Timed out: $message"
}
function Assert([bool]$condition, [string]$message) { if (-not $condition) { throw $message } }
function Control([IntPtr]$window, [int]$id) {
  $control = [Native]::GetDlgItem($window, $id)
  if ($control -eq [IntPtr]::Zero) { throw "Missing control $id" }
  return $control
}
function Click-Control([IntPtr]$window, [int]$id) { [Native]::Click((Control $window $id)) }
function Text([IntPtr]$window, [int]$id) { return [Native]::Text((Control $window $id)) }
function Activate-Target([IntPtr]$targetWindow, [int]$id = 201) {
  [void][Native]::SetForegroundWindow($targetWindow)
  [Native]::Click((Control $targetWindow $id))
  Wait-For { [Native]::GetForegroundWindow() -eq $targetWindow } 'test input window did not become active'
}
function Tap-Many([byte]$key, [int]$count) { for ($i = 0; $i -lt $count; $i++) { [Native]::Tap($key) } }
function Save-WidgetScreenshot([IntPtr]$widget, [string]$path) {
  if (-not $path) { return }
  Add-Type -AssemblyName System.Drawing
  $rect = New-Object Native+RECT
  Assert ([Native]::GetWindowRect($widget, [ref]$rect)) 'Cannot capture widget rect.'
  $bitmap = [System.Drawing.Bitmap]::new($rect.Right - $rect.Left, $rect.Bottom - $rect.Top)
  $graphics = [System.Drawing.Graphics]::FromImage($bitmap)
  try {
    $graphics.CopyFromScreen($rect.Left, $rect.Top, 0, 0, $bitmap.Size)
    $bitmap.Save($path, [System.Drawing.Imaging.ImageFormat]::Png)
  } finally { $graphics.Dispose(); $bitmap.Dispose() }
}
function Close-Dialog([IntPtr]$dialog, [string]$label) {
  try { Wait-For { [Native]::FindButton($dialog, $label) -ne [IntPtr]::Zero } "dialog button $label" } catch {
    throw "Dialog $dialog button $label unavailable: $([Native]::DescribeChildren($dialog))"
  }
  [Native]::Click([Native]::FindButton($dialog, $label))
}

# Preserve the caller's three preferences if these tests are run outside CI.
if ([Native]::FindClass('TypeBloomWidgetWindow') -ne [IntPtr]::Zero) {
  throw 'Close any running TypeBloom instance before the smoke test.'
}
$subkey = 'Software\TypeBloom'
$previousKey = [Microsoft.Win32.Registry]::CurrentUser.OpenSubKey($subkey)
$hadKey = $null -ne $previousKey
$previous = @{}
if ($hadKey) {
  foreach ($name in @('X', 'Y', 'AlwaysOnTop')) {
    if ($previousKey.GetValueNames() -contains $name) {
      $previous[$name] = @($previousKey.GetValue($name), $previousKey.GetValueKind($name))
    }
  }
  $previousKey.Close()
}
$testKey = [Microsoft.Win32.Registry]::CurrentUser.CreateSubKey($subkey)
foreach ($name in @('X', 'Y', 'AlwaysOnTop')) { $testKey.DeleteValue($name, $false) }
$testKey.Close()
$appProcess = $null
$targetProcess = $null
try {
  $appProcess = Start-Process -FilePath (Resolve-Path $App).Path -PassThru
  try {
    Wait-For {
      $appProcess.Refresh()
      if ($appProcess.HasExited) { throw "TypeBloom exited before creating a window (exit code $($appProcess.ExitCode))." }
      $initialError = [Native]::FindWindow('#32770', 'TypeBloom')
      if ($initialError -ne [IntPtr]::Zero) {
        $static = [Native]::GetDlgItem($initialError, -1)
        throw "TypeBloom reported a startup error: $([Native]::Text($static))"
      }
      [Native]::FindClass('TypeBloomWidgetWindow') -ne [IntPtr]::Zero
    } 'widget window'
  } catch {
    $appProcess.Refresh()
    $description = if ($appProcess.HasExited) { "exited: $($appProcess.ExitCode)" } else { "process $($appProcess.Id): $([Native]::DescribeWindows($appProcess.Id))" }
    throw "$_; $description"
  }
  $widget = [Native]::FindClass('TypeBloomWidgetWindow')
  Wait-For { (Text $widget 1001) -eq 'Start tracking' } 'ready state'
  Assert ((Text $widget 1101) -match '^Ready') 'App must start paused.'
  Assert ((Text $widget 1103) -eq '0 keys in the last 15 seconds') 'Fresh count must be empty.'
  Assert (([Native]::GetWindowLongW($widget, -20) -band 0x8) -ne 0) 'Widget must start on top by default.'
  foreach ($entry in @(@(1001, 'Start tracking'), @(1002, 'Reset'), @(1003, 'Always on top: on'),
                        @(1004, 'Privacy and how it works'), @(1005, 'Close TypeBloom'))) {
    $name = [Native]::AccessibleName((Control $widget ([int]$entry[0])))
    Assert ($name -eq $entry[1]) "Accessible name mismatch for $($entry[0]): $name"
  }
  Write-Host 'Ready, floating, and named accessible controls: OK'
  Save-WidgetScreenshot $widget $Screenshot

  $targetProcess = Start-Process -FilePath (Resolve-Path $Target).Path -PassThru
  Wait-For { [Native]::FindTitle('TypeBloom Test Input') -ne [IntPtr]::Zero } 'separate input window'
  $targetWindow = [Native]::FindTitle('TypeBloom Test Input')
  Activate-Target $targetWindow
  Tap-Many 0x41 5
  Start-Sleep -Milliseconds 400
  Assert ((Text $widget 1103) -eq '0 keys in the last 15 seconds') 'Must not record before consent.'

  Click-Control $widget 1004
  Wait-For { [Native]::FindWindow('#32770', 'TypeBloom Privacy') -ne [IntPtr]::Zero } 'privacy dialog'
  $dialog = [Native]::FindWindow('#32770', 'TypeBloom Privacy')
  Close-Dialog $dialog 'OK'
  Wait-For { [Native]::FindWindow('#32770', 'TypeBloom Privacy') -eq [IntPtr]::Zero } 'privacy dialog close'
  Click-Control $widget 1001
  Wait-For { [Native]::FindWindow('#32770', 'Start tracking?') -ne [IntPtr]::Zero } 'consent prompt'
  $dialog = [Native]::FindWindow('#32770', 'Start tracking?')
  Close-Dialog $dialog 'Cancel'
  Wait-For { [Native]::FindWindow('#32770', 'Start tracking?') -eq [IntPtr]::Zero } 'consent cancellation'
  Assert ((Text $widget 1001) -eq 'Start tracking') 'Cancel must leave tracking off.'
  Click-Control $widget 1001
  Wait-For { [Native]::FindWindow('#32770', 'Start tracking?') -ne [IntPtr]::Zero } 'consent prompt again'
  Close-Dialog ([Native]::FindWindow('#32770', 'Start tracking?')) 'OK'
  Wait-For { (Text $widget 1001) -eq 'Pause tracking' } 'hook installed and active'
  Assert ((Text $widget 1101) -match '^Live') 'Live state missing.'
  Write-Host 'Informed consent and real hook installation: OK'

  Activate-Target $targetWindow
  Tap-Many 0x41 15  # A
  Tap-Many 0x20 5   # space
  Tap-Many 0x08 3   # backspace should not count
  [Native]::Down(0xA2) # left Ctrl + A should not count
  [Native]::Tap(0x41)
  [Native]::Up(0xA2)
  Wait-For { (Text $widget 1103) -eq '20 keys in the last 15 seconds' } '20 typing keys, excluding Backspace and Ctrl+A'
  Assert ((Text $widget 1102) -match '^[1-9][0-9]* WPM$') 'Live WPM must be positive.'
  # Password controls are intentionally NOT identified: the numeric count can
  # include them, which the consent dialog and documentation make explicit.
  Activate-Target $targetWindow 202
  Tap-Many 0x41 4
  Wait-For { (Text $widget 1103) -eq '24 keys in the last 15 seconds' } 'password-field keys contribute only to the count'
  Write-Host 'Cross-app counts, shortcut filtering, password-field limitation, and WPM: OK'

  Save-WidgetScreenshot $widget $Screenshot

  Click-Control $widget 1001
  Wait-For { (Text $widget 1001) -eq 'Resume tracking' } 'pause'
  Assert ((Text $widget 1103) -eq '0 keys in the last 15 seconds') 'Pause must clear the reading.'
  Activate-Target $targetWindow
  Tap-Many 0x41 6
  Start-Sleep -Milliseconds 500
  Assert ((Text $widget 1103) -eq '0 keys in the last 15 seconds') 'Paused hook must not count.'
  Click-Control $widget 1001
  Wait-For { (Text $widget 1001) -eq 'Pause tracking' } 'resume without a second consent prompt'
  Activate-Target $targetWindow
  Tap-Many 0x41 5
  Wait-For { (Text $widget 1103) -eq '5 keys in the last 15 seconds' } 'resumed count'
  Click-Control $widget 1002
  Wait-For { (Text $widget 1103) -eq '0 keys in the last 15 seconds' } 'reset count'
  Activate-Target $targetWindow
  Tap-Many 0x41 4
  Wait-For { (Text $widget 1103) -eq '4 keys in the last 15 seconds' } 'post-reset count'
  [void][Native]::PostMessage($widget, 0x02B1, [IntPtr]7, [IntPtr]::Zero) # WM_WTSSESSION_CHANGE / LOCK
  Wait-For { (Text $widget 1001) -eq 'Resume tracking' } 'automatic pause on lock notification'
  Assert ((Text $widget 1103) -eq '0 keys in the last 15 seconds') 'Lock must clear the reading.'
  Write-Host 'Pause, resume, reset, and lock notification: OK'

  Click-Control $widget 1003
  Wait-For { (Text $widget 1003) -eq 'Always on top: off' } 'unpin'
  Assert (([Native]::GetWindowLongW($widget, -20) -band 0x8) -eq 0) 'Unpin must remove topmost style.'
  $second = Start-Process -FilePath (Resolve-Path $App).Path -PassThru
  Wait-For { $second.Refresh(); $second.HasExited } 'second instance exits' 10000
  Assert ($second.ExitCode -eq 0) 'Second instance should bring back the existing widget and exit.'
  [void][Native]::SetWindowPos($widget, [IntPtr]::Zero, 50, 60, 0, 0, 0x0015) # NOSIZE | NOZORDER | NOACTIVATE
  [void][Native]::PostMessage($widget, 0x0232, [IntPtr]::Zero, [IntPtr]::Zero) # WM_EXITSIZEMOVE
  $before = New-Object Native+RECT
  [void][Native]::GetWindowRect($widget, [ref]$before)
  Click-Control $widget 1005
  Wait-For { $appProcess.Refresh(); $appProcess.HasExited } 'close exits without a tray process'
  Assert ($appProcess.ExitCode -eq 0) 'App did not exit cleanly.'
  $appProcess = Start-Process -FilePath (Resolve-Path $App).Path -PassThru
  Wait-For { [Native]::FindClass('TypeBloomWidgetWindow') -ne [IntPtr]::Zero } 'reopen widget'
  $widget = [Native]::FindClass('TypeBloomWidgetWindow')
  Wait-For { (Text $widget 1001) -eq 'Start tracking' } 'reopen starts paused'
  Assert ((Text $widget 1003) -eq 'Always on top: off') 'Pin preference must persist.'
  $after = New-Object Native+RECT
  [void][Native]::GetWindowRect($widget, [ref]$after)
  Assert ($after.Left -eq $before.Left -and $after.Top -eq $before.Top) 'Window position must persist.'
  Click-Control $widget 1005
  Wait-For { $appProcess.Refresh(); $appProcess.HasExited } 'close reopened widget'
  $key = [Microsoft.Win32.Registry]::CurrentUser.OpenSubKey($subkey)
  $names = @($key.GetValueNames() | Sort-Object)
  $key.Close()
  Assert (($names -join ',') -eq 'AlwaysOnTop,X,Y') "Only position and pin settings may be stored: $($names -join ',')"
  Write-Host 'Pin, single instance, persisted position, paused restart, and clean exit: OK'
} finally {
  if ($appProcess) { $appProcess.Refresh(); if (-not $appProcess.HasExited) { Stop-Process -Id $appProcess.Id -Force } }
  if ($targetProcess) { $targetProcess.Refresh(); if (-not $targetProcess.HasExited) { Stop-Process -Id $targetProcess.Id -Force } }
  $key = [Microsoft.Win32.Registry]::CurrentUser.CreateSubKey($subkey)
  foreach ($name in @('X', 'Y', 'AlwaysOnTop')) {
    $key.DeleteValue($name, $false)
    if ($previous.ContainsKey($name)) { $key.SetValue($name, $previous[$name][0], $previous[$name][1]) }
  }
  $empty = $key.ValueCount -eq 0 -and $key.SubKeyCount -eq 0
  $key.Close()
  if (-not $hadKey -and $empty) { [Microsoft.Win32.Registry]::CurrentUser.DeleteSubKey($subkey, $false) }
}
Write-Host 'TypeBloom Windows smoke test passed.'
