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

namespace ScreenForge.Interop;

/// <summary>
/// Ctrl+Shift+F9 / F10, registered with Windows so they work from inside the
/// window being recorded. If another program already owns a combination the
/// registration simply fails, and the buttons in the app still work.
/// </summary>
public sealed class Hotkeys : IDisposable
{
    public const int IdToggle = 0x5C01;
    public const int IdPause = 0x5C02;

    private const uint ModControl = 0x0002;
    private const uint ModShift = 0x0004;
    private const uint VkF9 = 0x78;
    private const uint VkF10 = 0x79;

    private readonly IntPtr window;
    private readonly List<int> registered = new();

    public Hotkeys(IntPtr window)
    {
        this.window = window;
    }

    public event Action ToggleRequested;

    public event Action PauseRequested;

    public bool ToggleRegistered { get; private set; }

    public bool PauseRegistered { get; private set; }

    public string Status =>
        ToggleRegistered && PauseRegistered
            ? "Hotkeys: Ctrl+Shift+F9 start/stop, Ctrl+Shift+F10 pause"
            : ToggleRegistered
                ? "Hotkeys: Ctrl+Shift+F9 start/stop (pause key taken by another app)"
                : "Hotkeys unavailable — another app already owns Ctrl+Shift+F9";

    public void Register()
    {
        ToggleRegistered = Register(IdToggle, VkF9);
        PauseRegistered = Register(IdPause, VkF10);
    }

    private bool Register(int id, uint key)
    {
        try
        {
            if (NativeMethods.RegisterHotKey(window, id, ModControl | ModShift, key))
            {
                registered.Add(id);
                return true;
            }
        }
        catch (Exception)
        {
            /* registration is a nicety, not a requirement */
        }

        return false;
    }

    /// <summary>Called from the form's WndProc. True when the message was one of ours.</summary>
    public bool HandleMessage(int message, IntPtr wParam)
    {
        if (message != NativeMethods.WmHotkey)
            return false;

        switch (wParam.ToInt32())
        {
            case IdToggle:
                ToggleRequested?.Invoke();
                return true;

            case IdPause:
                PauseRequested?.Invoke();
                return true;

            default:
                return false;
        }
    }

    public void Dispose()
    {
        foreach (int id in registered)
        {
            try
            {
                NativeMethods.UnregisterHotKey(window, id);
            }
            catch (Exception)
            {
                /* the window may already be gone */
            }
        }

        registered.Clear();
    }
}
