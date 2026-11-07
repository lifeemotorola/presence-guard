/*
ScreenForge
Copyright (C) 2026 Presence Guard contributors

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.
*/

using System;
using System.Collections.Generic;
using ScreenForge.Core;

namespace ScreenForge.Interop;

public sealed class DisplayInfo
{
    public int Index { get; set; }
    public string Name { get; set; } = "";
    public bool Primary { get; set; }
    public CaptureRect Bounds { get; set; }
    public int ScalePercent { get; set; } = 100;

    public string Label =>
        $"Display {Index + 1} — {Bounds.Width}×{Bounds.Height}{(ScalePercent != 100 ? $" at {ScalePercent}%" : "")}{(Primary ? " (main)" : "")}";

    public override string ToString() => Label;
}

/// <summary>
/// The monitors, in the same coordinate space gdigrab uses: pixels, negative
/// coordinates for anything left of or above the primary display, and the true
/// size even on a 150% scaled screen.
/// </summary>
public static class Displays
{
    public static List<DisplayInfo> All()
    {
        var found = new List<(int X, int Y, int Width, int Height, bool Primary, string Name)>();

        try
        {
            NativeMethods.EnumDisplayMonitors(IntPtr.Zero, IntPtr.Zero, (monitor, _, __, ___) =>
            {
                var info = new MONITORINFO { cbSize = System.Runtime.InteropServices.Marshal.SizeOf<MONITORINFO>() };
                if (!NativeMethods.GetMonitorInfo(monitor, ref info))
                    return true;

                found.Add((
                    info.rcMonitor.Left,
                    info.rcMonitor.Top,
                    info.rcMonitor.Width,
                    info.rcMonitor.Height,
                    (info.dwFlags & NativeMethods.MonitorInfoPrimary) != 0,
                    "\\\\.\\DISPLAY" + (found.Count + 1)));

                return true;
            }, IntPtr.Zero);
        }
        catch (Exception)
        {
            /* Without user32 there is no screen to record; the caller falls back
             * to the primary resolution below. */
        }

        if (found.Count == 0)
            found.Add((0, 0, 1920, 1080, true, "\\\\.\\DISPLAY1"));

        /* Left to right, the way Windows labels them, so "Display 1" means the
         * one on the left even when it is not the primary. */
        found.Sort((a, b) => a.X != b.X ? a.X.CompareTo(b.X) : a.Y.CompareTo(b.Y));

        var displays = new List<DisplayInfo>();
        for (int i = 0; i < found.Count; i++)
        {
            displays.Add(new DisplayInfo
            {
                Index = i,
                Name = found[i].Name,
                Primary = found[i].Primary,
                Bounds = new CaptureRect(found[i].X, found[i].Y, found[i].Width, found[i].Height),
                ScalePercent = 100,
            });
        }

        return displays;
    }

    /// <summary>The whole desktop, all monitors together: what "region" is drawn on.</summary>
    public static CaptureRect VirtualDesktop(List<DisplayInfo> displays)
    {
        if (displays == null || displays.Count == 0)
            return new CaptureRect(0, 0, 1920, 1080);

        int left = int.MaxValue, top = int.MaxValue, right = int.MinValue, bottom = int.MinValue;

        foreach (DisplayInfo display in displays)
        {
            left = Math.Min(left, display.Bounds.X);
            top = Math.Min(top, display.Bounds.Y);
            right = Math.Max(right, display.Bounds.X + display.Bounds.Width);
            bottom = Math.Max(bottom, display.Bounds.Y + display.Bounds.Height);
        }

        return new CaptureRect(left, top, Math.Max(1, right - left), Math.Max(1, bottom - top));
    }

    public static DisplayInfo At(List<DisplayInfo> displays, CaptureRect rect)
    {
        if (displays == null || displays.Count == 0)
            return null;

        int centreX = rect.X + rect.Width / 2;
        int centreY = rect.Y + rect.Height / 2;

        foreach (DisplayInfo display in displays)
        {
            CaptureRect b = display.Bounds;
            if (centreX >= b.X && centreX < b.X + b.Width && centreY >= b.Y && centreY < b.Y + b.Height)
                return display;
        }

        return displays[0];
    }
}
