/*
ScreenForge
Copyright (C) 2026 Emmanuel Suah

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.
*/

using System;

namespace ScreenForge.Interop;

/// <summary>
/// Stops the screensaver and the sleep timer for the length of a recording. A
/// black frame in the middle of a lesson because the desktop went to sleep is
/// the least funny bug a recorder can have.
/// </summary>
public static class KeepAwake
{
    public static void Begin()
    {
        try
        {
            NativeMethods.SetThreadExecutionState(
                NativeMethods.EsContinuous | NativeMethods.EsSystemRequired | NativeMethods.EsDisplayRequired);
        }
        catch (Exception)
        {
            /* if this fails the machine may sleep; nothing else breaks */
        }
    }

    public static void End()
    {
        try
        {
            NativeMethods.SetThreadExecutionState(NativeMethods.EsContinuous);
        }
        catch (Exception)
        {
            /* nothing to undo */
        }
    }
}
