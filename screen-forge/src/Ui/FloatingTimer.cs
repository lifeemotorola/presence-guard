/*
ScreenForge
Copyright (C) 2026 Emmanuel Suah

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.
*/

using System;
using System.Drawing;
using System.Drawing.Drawing2D;
using System.Windows.Forms;
using Timer = System.Windows.Forms.Timer;
using ScreenForge.Core;

namespace ScreenForge.Ui;

/// <summary>
/// A small always-on-top clock while recording — placed in a part of the desktop
/// that is *not* being captured, because a timer burned into the video is a bug,
/// not a feature. If every display is being recorded it stays hidden.
/// </summary>
public sealed class FloatingTimer : Form
{
    private readonly Func<TimeSpan> elapsed;
    private readonly Func<bool> paused;
    private readonly Timer tick;

    public FloatingTimer(Func<TimeSpan> elapsed, Func<bool> paused)
    {
        this.elapsed = elapsed;
        this.paused = paused;

        FormBorderStyle = FormBorderStyle.None;
        StartPosition = FormStartPosition.Manual;
        ShowInTaskbar = false;
        TopMost = true;
        BackColor = Theme.Back;
        Opacity = 0.9;
        ClientSize = new Size(132, 42);
        DoubleBuffered = true;

        tick = new Timer { Interval = 500 };
        tick.Tick += (_, _) => Invalidate();
    }

    protected override bool ShowWithoutActivation => true;

    protected override CreateParams CreateParams
    {
        get
        {
            CreateParams parameters = base.CreateParams;
            parameters.ExStyle |= 0x08000000; /* WS_EX_NOACTIVATE */
            parameters.ExStyle |= 0x00000080; /* WS_EX_TOOLWINDOW: no Alt+Tab entry */
            return parameters;
        }
    }

    /// <summary>
    /// Picks a corner that the capture cannot see: the top-right of a display
    /// that is entirely outside the recorded rectangle, or nothing at all.
    /// </summary>
    public void PlaceAwayFrom(CaptureRect captured)
    {
        foreach (Screen screen in Screen.AllScreens)
        {
            Rectangle bounds = screen.Bounds;

            bool overlaps =
                bounds.Left < captured.X + captured.Width &&
                bounds.Right > captured.X &&
                bounds.Top < captured.Y + captured.Height &&
                bounds.Bottom > captured.Y;

            if (overlaps)
                continue;

            Location = new Point(bounds.Right - Width - 24, bounds.Top + 24);
            return;
        }

        /* Everything is being recorded: do not appear in the video. */
        Hide();
    }

    protected override void OnShown(EventArgs e)
    {
        base.OnShown(e);
        tick.Start();
    }

    protected override void OnPaint(PaintEventArgs e)
    {
        base.OnPaint(e);

        Graphics graphics = e.Graphics;
        graphics.SmoothingMode = SmoothingMode.AntiAlias;

        using (var path = Theme.Rounded(new Rectangle(0, 0, Width - 1, Height - 1), 10))
        using (var fill = new SolidBrush(Theme.Panel))
            graphics.FillPath(fill, path);

        bool isPaused = paused != null && paused();
        Color dot = isPaused ? Theme.Accent : Theme.Danger;

        using (var brush = new SolidBrush(dot))
            graphics.FillEllipse(brush, 14, Height / 2 - 6, 12, 12);

        TimeSpan time = elapsed?.Invoke() ?? TimeSpan.Zero;

        using var font = new Font("Consolas", 13f, FontStyle.Bold);
        using var text = new SolidBrush(Theme.Text);
        graphics.DrawString(time.ToString(@"hh\:mm\:ss"), font, text, 34, 8);
    }

    protected override void OnFormClosed(FormClosedEventArgs e)
    {
        tick.Stop();
        tick.Dispose();
        base.OnFormClosed(e);
    }
}
