/*
ScreenForge
Copyright (C) 2026 Emmanuel Suah

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.
*/

using System;
using System.ComponentModel;
using System.Runtime.InteropServices;
using ScreenForge.Core;

namespace ScreenForge.Interop;

/// <summary>
/// Observes button-down events on the desktop. It never consumes an input event,
/// stores click history, or observes keys. Install on a thread with a message pump
/// and do only quick work in the callback; Windows may remove a slow hook.
/// </summary>
internal sealed class MouseClickHook : IDisposable
{
    private const int WhMouseLl = 14;
    private readonly Action<MouseClickButton, int, int> onClick;
    private readonly NativeMethods.MouseHookProc callback; // keep the delegate alive
    private IntPtr hook;
    private bool disposed;

    public MouseClickHook(Action<MouseClickButton, int, int> onClick)
    {
        this.onClick = onClick ?? throw new ArgumentNullException(nameof(onClick));
        callback = OnMouseEvent;
    }

    public void Start()
    {
        if (disposed)
            throw new ObjectDisposedException(nameof(MouseClickHook));
        if (hook != IntPtr.Zero)
            return;

        IntPtr module = NativeMethods.GetModuleHandle(null);
        hook = NativeMethods.SetWindowsHookEx(WhMouseLl, callback, module, 0);
        if (hook == IntPtr.Zero)
            throw new Win32Exception(Marshal.GetLastWin32Error(), "Windows could not enable mouse-click highlights");
    }

    private IntPtr OnMouseEvent(int code, IntPtr message, IntPtr data)
    {
        try
        {
            if (code >= 0 && data != IntPtr.Zero &&
                ClickFeedback.TryMapMessage(message.ToInt64(), out MouseClickButton button))
            {
                MouseHookData click = Marshal.PtrToStructure<MouseHookData>(data);
                onClick(button, click.Point.X, click.Point.Y);
            }
        }
        catch (Exception)
        {
            // An overlay must never prevent the mouse input from reaching Windows.
        }

        return NativeMethods.CallNextHookEx(IntPtr.Zero, code, message, data);
    }

    public void Dispose()
    {
        if (disposed)
            return;
        disposed = true;
        if (hook != IntPtr.Zero)
        {
            NativeMethods.UnhookWindowsHookEx(hook);
            hook = IntPtr.Zero;
        }
    }
}
