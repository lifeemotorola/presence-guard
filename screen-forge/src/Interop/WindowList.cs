/*
ScreenForge
Copyright (C) 2026 Emmanuel Suah

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.
*/

using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Text;
using ScreenForge.Core;

namespace ScreenForge.Interop;

public sealed class WindowInfo
{
    public IntPtr Handle { get; set; }
    public string Title { get; set; } = "";
    public string Process { get; set; } = "";
    public CaptureRect Bounds { get; set; }

    public override string ToString() =>
        string.IsNullOrWhiteSpace(Process) ? Title : $"{Title} — {Process}";
}

/// <summary>
/// The windows worth offering. EnumWindows returns hundreds of things nobody
/// would call a window: hidden shell hosts, tool windows, and UWP surfaces that
/// are technically visible but cloaked. All three are filtered out here.
/// </summary>
public static class WindowList
{
    public static List<WindowInfo> Visible(int max = 200)
    {
        var found = new List<WindowInfo>();
        uint ownProcess = (uint)Process.GetCurrentProcess().Id;

        try
        {
            NativeMethods.EnumWindows((window, _) =>
            {
                if (found.Count >= max)
                    return false;

                if (!NativeMethods.IsWindowVisible(window))
                    return true;

                if (NativeMethods.GetWindowTextLength(window) == 0)
                    return true;

                long style = NativeMethods.GetWindowLongPtr(window, NativeMethods.GwlExStyle).ToInt64();
                if ((style & NativeMethods.WsExToolWindow) != 0)
                    return true;

                if (IsCloaked(window))
                    return true;

                if (!NativeMethods.GetWindowRect(window, out RECT rect))
                    return true;

                /* Minimised windows report -32000; there is nothing to record. */
                if (NativeMethods.IsIconic(window))
                    return true;

                if (rect.Width < 64 || rect.Height < 64)
                    return true;

                NativeMethods.GetWindowThreadProcessId(window, out uint processId);
                if (processId == ownProcess)
                    return true;

                var title = new StringBuilder(NativeMethods.GetWindowTextLength(window) + 2);
                NativeMethods.GetWindowText(window, title, title.Capacity);

                found.Add(new WindowInfo
                {
                    Handle = window,
                    Title = title.ToString(),
                    Process = ProcessName(processId),
                    Bounds = new CaptureRect(rect.Left, rect.Top, rect.Width, rect.Height),
                });

                return true;
            }, IntPtr.Zero);
        }
        catch (Exception)
        {
            /* no window list this time; the monitor list still works */
        }

        return found;
    }

    private static bool IsCloaked(IntPtr window)
    {
        try
        {
            int cloaked;
            if (NativeMethods.DwmGetWindowAttribute(window, NativeMethods.DwmwaCloaked, out cloaked, sizeof(int)) != 0)
                return false;

            return cloaked != 0;
        }
        catch (Exception)
        {
            return false;
        }
    }

    private static string ProcessName(uint processId)
    {
        try
        {
            using Process process = Process.GetProcessById((int)processId);
            return process.ProcessName;
        }
        catch (Exception)
        {
            return "";
        }
    }
}
