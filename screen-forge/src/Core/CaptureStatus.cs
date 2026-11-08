/*
ScreenForge
Copyright (C) 2026 Emmanuel Suah

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.
*/

using System;
using System.Diagnostics;

namespace ScreenForge.Core;

/// <summary>
/// Reports an unexpected encoder exit while a take is active. The caller only
/// checks between start/stop transitions, so a deliberate quit is not an error.
/// </summary>
public static class CaptureStatus
{
    public static bool TryDescribeUnexpectedExit(Process process, string details, out string reason)
    {
        reason = null;
        if (process == null)
            return false;

        try
        {
            if (!process.HasExited)
                return false;

            reason = $"ffmpeg stopped unexpectedly (exit code {process.ExitCode}).";
            if (!string.IsNullOrWhiteSpace(details))
                reason += " " + details.Trim();
            return true;
        }
        catch (Exception ex)
        {
            // A process handle that is no longer usable cannot be recording.
            reason = "Could not monitor the capture process: " + ex.Message;
            return true;
        }
    }
}
