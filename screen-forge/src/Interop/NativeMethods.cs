/*
ScreenForge
Copyright (C) 2026 Emmanuel Suah

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.
*/

using System;
using System.Runtime.InteropServices;

namespace ScreenForge.Interop;

[StructLayout(LayoutKind.Sequential)]
internal struct RECT
{
    public int Left;
    public int Top;
    public int Right;
    public int Bottom;

    public int Width => Right - Left;
    public int Height => Bottom - Top;
}

[StructLayout(LayoutKind.Sequential)]
internal struct MONITORINFO
{
    public int cbSize;
    public RECT rcMonitor;
    public RECT rcWork;
    public uint dwFlags;
}

[StructLayout(LayoutKind.Sequential)]
internal struct NativePoint
{
    public int X;
    public int Y;
    public NativePoint(int x, int y) { X = x; Y = y; }
}

[StructLayout(LayoutKind.Sequential)]
internal struct NativeSize
{
    public int Width;
    public int Height;
    public NativeSize(int width, int height) { Width = width; Height = height; }
}

[StructLayout(LayoutKind.Sequential)]
internal struct MouseHookData
{
    public NativePoint Point;
    public uint MouseData;
    public uint Flags;
    public uint Time;
    public UIntPtr ExtraInfo;
}

[StructLayout(LayoutKind.Sequential, Pack = 1)]
internal struct BlendFunction
{
    public byte Operation;
    public byte Flags;
    public byte SourceAlpha;
    public byte AlphaFormat;
}

internal static class NativeMethods
{
    internal const uint MonitorInfoPrimary = 1;

    internal const int DwmwaCloaked = 14;
    internal const int GwlExStyle = -20;
    internal const long WsExToolWindow = 0x00000080;

    internal const uint EsContinuous = 0x80000000;
    internal const uint EsSystemRequired = 0x00000001;
    internal const uint EsDisplayRequired = 0x00000002;

    internal const int WmHotkey = 0x0312;

    internal delegate bool MonitorEnumProc(IntPtr monitor, IntPtr window, IntPtr rect, IntPtr data);

    internal delegate bool EnumWindowsProc(IntPtr window, IntPtr param);

    internal delegate IntPtr MouseHookProc(int code, IntPtr message, IntPtr data);

    [DllImport("user32.dll")]
    [return: MarshalAs(UnmanagedType.Bool)]
    internal static extern bool EnumDisplayMonitors(IntPtr deviceContext, IntPtr clip, MonitorEnumProc callback, IntPtr data);

    [DllImport("user32.dll", CharSet = CharSet.Unicode)]
    [return: MarshalAs(UnmanagedType.Bool)]
    internal static extern bool GetMonitorInfo(IntPtr monitor, ref MONITORINFO info);

    [DllImport("user32.dll")]
    [return: MarshalAs(UnmanagedType.Bool)]
    internal static extern bool EnumWindows(EnumWindowsProc callback, IntPtr param);

    [DllImport("user32.dll")]
    [return: MarshalAs(UnmanagedType.Bool)]
    internal static extern bool IsWindowVisible(IntPtr window);

    [DllImport("user32.dll")]
    [return: MarshalAs(UnmanagedType.Bool)]
    internal static extern bool IsIconic(IntPtr window);

    [DllImport("user32.dll", CharSet = CharSet.Unicode)]
    internal static extern int GetWindowText(IntPtr window, System.Text.StringBuilder text, int max);

    [DllImport("user32.dll", CharSet = CharSet.Unicode)]
    internal static extern int GetWindowTextLength(IntPtr window);

    [DllImport("user32.dll")]
    [return: MarshalAs(UnmanagedType.Bool)]
    internal static extern bool GetWindowRect(IntPtr window, out RECT rect);

    [DllImport("user32.dll")]
    internal static extern uint GetWindowThreadProcessId(IntPtr window, out uint processId);

    [DllImport("user32.dll")]
    internal static extern IntPtr GetWindowLongPtr(IntPtr window, int index);

    [DllImport("dwmapi.dll")]
    internal static extern int DwmGetWindowAttribute(IntPtr window, int attribute, out int value, int size);

    [DllImport("user32.dll", SetLastError = true)]
    [return: MarshalAs(UnmanagedType.Bool)]
    internal static extern bool RegisterHotKey(IntPtr window, int id, uint modifiers, uint key);

    [DllImport("user32.dll", SetLastError = true)]
    [return: MarshalAs(UnmanagedType.Bool)]
    internal static extern bool UnregisterHotKey(IntPtr window, int id);

    [DllImport("kernel32.dll")]
    internal static extern uint SetThreadExecutionState(uint flags);

    [DllImport("kernel32.dll", CharSet = CharSet.Unicode, SetLastError = true)]
    internal static extern IntPtr GetModuleHandle(string moduleName);

    [DllImport("user32.dll", CharSet = CharSet.Unicode, SetLastError = true)]
    internal static extern IntPtr SetWindowsHookEx(int id, MouseHookProc callback, IntPtr module, uint threadId);

    [DllImport("user32.dll", SetLastError = true)]
    [return: MarshalAs(UnmanagedType.Bool)]
    internal static extern bool UnhookWindowsHookEx(IntPtr hook);

    [DllImport("user32.dll")]
    internal static extern IntPtr CallNextHookEx(IntPtr hook, int code, IntPtr message, IntPtr data);

    [DllImport("user32.dll", SetLastError = true)]
    internal static extern IntPtr GetDC(IntPtr window);

    [DllImport("user32.dll")]
    internal static extern int ReleaseDC(IntPtr window, IntPtr dc);

    [DllImport("gdi32.dll", SetLastError = true)]
    internal static extern IntPtr CreateCompatibleDC(IntPtr dc);

    [DllImport("gdi32.dll", SetLastError = true)]
    [return: MarshalAs(UnmanagedType.Bool)]
    internal static extern bool DeleteDC(IntPtr dc);

    [DllImport("gdi32.dll", SetLastError = true)]
    internal static extern IntPtr SelectObject(IntPtr dc, IntPtr objectHandle);

    [DllImport("gdi32.dll", SetLastError = true)]
    [return: MarshalAs(UnmanagedType.Bool)]
    internal static extern bool DeleteObject(IntPtr objectHandle);

    [DllImport("user32.dll", SetLastError = true)]
    [return: MarshalAs(UnmanagedType.Bool)]
    internal static extern bool UpdateLayeredWindow(
        IntPtr window, IntPtr targetDc, ref NativePoint destination, ref NativeSize size,
        IntPtr sourceDc, ref NativePoint source, uint colorKey, ref BlendFunction blend, uint flags);
}
