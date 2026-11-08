/*
ScreenForge
Copyright (C) 2026 Emmanuel Suah

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.
*/

using System;

namespace ScreenForge.Core;

public enum MouseClickButton
{
    Left,
    Right,
    Middle,
}

/// <summary>Platform-independent decisions for the optional click indicators.</summary>
public static class ClickFeedback
{
    public const int DurationMilliseconds = 600;
    public const int FrameMilliseconds = 33;
    public const int WindowSize = 104;
    public const int MaximumVisible = 8;

    /// <summary>Only button-down messages produce a marker; movement and release do not.</summary>
    public static bool TryMapMessage(long message, out MouseClickButton button)
    {
        switch (message)
        {
            case 0x0201: // WM_LBUTTONDOWN
                button = MouseClickButton.Left;
                return true;
            case 0x0204: // WM_RBUTTONDOWN
                button = MouseClickButton.Right;
                return true;
            case 0x0207: // WM_MBUTTONDOWN
                button = MouseClickButton.Middle;
                return true;
            default:
                button = default;
                return false;
        }
    }

    /// <summary>
    /// Hook coordinates and capture rectangles are virtual-desktop pixels. Right
    /// and bottom are exclusive; long arithmetic also handles negative monitors.
    /// </summary>
    public static bool Contains(CaptureRect rect, int x, int y) =>
        rect.Width > 0 && rect.Height > 0 &&
        x >= (long)rect.X && y >= (long)rect.Y &&
        x < (long)rect.X + rect.Width && y < (long)rect.Y + rect.Height;

    public static double Progress(long elapsedMilliseconds) =>
        Math.Clamp((double)elapsedMilliseconds / DurationMilliseconds, 0, 1);

    /// <summary>Expands quickly, then eases to a stop, always inside the window.</summary>
    public static float Radius(long elapsedMilliseconds)
    {
        double progress = Progress(elapsedMilliseconds);
        return (float)(12 + 27 * (1 - (1 - progress) * (1 - progress)));
    }

    public static byte Opacity(long elapsedMilliseconds) =>
        (byte)Math.Round(235 * (1 - Progress(elapsedMilliseconds)));
}
