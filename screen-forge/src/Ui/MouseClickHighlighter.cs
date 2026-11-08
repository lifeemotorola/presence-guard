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
using System.Threading;
using System.Windows.Forms;
using ScreenForge.Core;
using ScreenForge.Interop;

namespace ScreenForge.Ui;

/// <summary>
/// Turns clicks inside the recorded rectangle into temporary, click-through
/// desktop rings. The hook is active only for a recording and is removed on
/// stop; input is never suppressed and click coordinates are not saved.
/// </summary>
internal sealed class MouseClickHighlighter : IDisposable
{
    private const int MaximumPending = 16;
    private readonly Control dispatcher;
    private readonly CaptureRect area;
    private readonly Action<string> reportError;
    private readonly MouseClickHook hook;
    private readonly List<ClickPulse> pulses = new();
    private volatile bool active;
    private bool disposed;
    private int generation;
    private int pending;

    public MouseClickHighlighter(Control dispatcher, CaptureRect area, Action<string> reportError)
    {
        this.dispatcher = dispatcher ?? throw new ArgumentNullException(nameof(dispatcher));
        this.area = Presets.Normalise(area);
        this.reportError = reportError ?? (_ => { });
        hook = new MouseClickHook(ObserveClick);
    }

    public void Start() => hook.Start();

    internal int VisiblePulseCount => pulses.Count;

    public void SetActive(bool value)
    {
        if (disposed || active == value)
            return;
        active = value;
        generation++;
        if (!value)
            ClearPulses();
    }

    // This executes within the low-level hook. Copy only these three fields and
    // post the drawing to the UI queue so Windows never waits for the animation.
    internal void ObserveClick(MouseClickButton button, int x, int y)
    {
        if (!active || disposed || !ClickFeedback.Contains(area, x, y))
            return;
        if (Interlocked.Increment(ref pending) > MaximumPending)
        {
            Interlocked.Decrement(ref pending);
            return;
        }

        int clickGeneration = generation;
        try
        {
            dispatcher.BeginInvoke(new Action(() =>
            {
                try
                {
                    if (clickGeneration == generation)
                        ShowClick(button, x, y);
                }
                finally { Interlocked.Decrement(ref pending); }
            }));
        }
        catch (InvalidOperationException)
        {
            Interlocked.Decrement(ref pending); // the form is closing
        }
    }

    private void ShowClick(MouseClickButton button, int x, int y)
    {
        if (!active || disposed)
            return;

        ClickPulse pulse = null;
        try
        {
            if (pulses.Count >= ClickFeedback.MaximumVisible)
                pulses[0].Dispose();

            pulse = new ClickPulse(button, x, y, OnRenderFailure);
            pulse.Disposed += (_, _) => pulses.Remove(pulse);
            pulses.Add(pulse);
            pulse.ShowPulse();
        }
        catch (Exception ex)
        {
            pulse?.Dispose();
            OnRenderFailure(ex);
        }
    }

    private void OnRenderFailure(Exception ex)
    {
        if (disposed)
            return;
        Dispose();
        try { reportError("Mouse-click highlights were turned off: " + ex.Message); }
        catch (Exception) { /* the recorder is closing */ }
    }

    private void ClearPulses()
    {
        while (pulses.Count > 0)
        {
            ClickPulse pulse = pulses[0];
            pulses.RemoveAt(0);
            pulse.Dispose();
        }
    }

    public void Dispose()
    {
        if (disposed)
            return;
        disposed = true;
        active = false;
        generation++;
        hook.Dispose();
        ClearPulses();
    }
}
