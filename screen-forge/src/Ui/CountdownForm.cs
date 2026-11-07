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
using System.Drawing;
using System.Drawing.Drawing2D;
using System.Windows.Forms;
using Timer = System.Windows.Forms.Timer;
using ScreenForge.Core;
using ScreenForge.Interop;

namespace ScreenForge.Ui;

/// <summary>
/// Three seconds of warning, centred on the thing about to be recorded, so the
/// user has time to arrange a window or close a tab. Esc cancels the recording
/// before it starts, which is cheaper than deleting the file afterwards.
/// </summary>
public sealed class CountdownForm : Form
{
    private readonly Timer tick;
    private int remaining;

    public CountdownForm(int seconds, CaptureTarget target, List<DisplayInfo> displays)
    {
        remaining = Math.Max(1, seconds);

        FormBorderStyle = FormBorderStyle.None;
        StartPosition = FormStartPosition.Manual;
        ShowInTaskbar = false;
        TopMost = true;
        BackColor = Theme.Back;
        Opacity = 0.94;
        ClientSize = new Size(320, 190);
        KeyPreview = true;
        DoubleBuffered = true;

        CaptureRect area = target?.Rect ?? new CaptureRect(0, 0, 1920, 1080);
        DisplayInfo display = Displays.At(displays, area);
        Rectangle screen = display != null
            ? new Rectangle(display.Bounds.X, display.Bounds.Y, display.Bounds.Width, display.Bounds.Height)
            : Screen.PrimaryScreen.Bounds;

        Location = new Point(
            screen.X + Math.Max(0, (screen.Width - Width) / 2),
            screen.Y + Math.Max(0, screen.Height / 3 - Height / 2));

        tick = new Timer { Interval = 1000 };
        tick.Tick += (_, _) =>
        {
            remaining--;
            if (remaining <= 0)
            {
                tick.Stop();
                DialogResult = DialogResult.OK;
                Close();
            }
            else
            {
                Invalidate();
            }
        };
    }

    public bool Cancelled { get; private set; }

    protected override void OnShown(EventArgs e)
    {
        base.OnShown(e);
        tick.Start();
    }

    protected override void OnKeyDown(KeyEventArgs e)
    {
        base.OnKeyDown(e);

        if (e.KeyCode != Keys.Escape)
            return;

        Cancelled = true;
        tick.Stop();
        DialogResult = DialogResult.Cancel;
        Close();
    }

    protected override void OnMouseClick(MouseEventArgs e)
    {
        base.OnMouseClick(e);

        Cancelled = true;
        tick.Stop();
        DialogResult = DialogResult.Cancel;
        Close();
    }

    protected override void OnPaint(PaintEventArgs e)
    {
        base.OnPaint(e);

        Graphics graphics = e.Graphics;
        graphics.SmoothingMode = SmoothingMode.AntiAlias;

        using (var pen = new Pen(Theme.Line, 1f))
        {
            graphics.DrawRectangle(pen, 0, 0, Width - 1, Height - 1);
        }

        using (var font = new Font("Segoe UI", 74f, FontStyle.Bold))
        using (var brush = new SolidBrush(Theme.Accent))
        {
            string text = remaining.ToString();
            SizeF size = graphics.MeasureString(text, font);
            graphics.DrawString(text, font, brush, (Width - size.Width) / 2f, 16f);
        }

        using (var small = new Font("Segoe UI", 9.5f))
        using (var brush = new SolidBrush(Theme.Muted))
        {
            const string caption = "Recording starts — click or press Esc to cancel";
            SizeF size = graphics.MeasureString(caption, small);
            graphics.DrawString(caption, small, brush, (Width - size.Width) / 2f, Height - 32f);
        }
    }
}
