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
using System.Drawing;
using System.Windows.Forms;
using ScreenForge.Core;
using ScreenForge.Interop;

namespace ScreenForge.Ui;

/// <summary>
/// The whole desktop, dimmed, with a crosshair. Drag a rectangle; that rectangle
/// in desktop pixels is exactly what gdigrab is told to grab, negative monitor
/// coordinates and all.
/// </summary>
public sealed class RegionSelector : Form
{
    private readonly List<DisplayInfo> displays;
    private Point anchor;
    private Rectangle dragging;
    private bool active;

    public RegionSelector(List<DisplayInfo> displays)
    {
        this.displays = displays ?? new List<DisplayInfo>();

        FormBorderStyle = FormBorderStyle.None;
        StartPosition = FormStartPosition.Manual;
        Bounds = SystemInformation.VirtualScreen;
        TopMost = true;
        Opacity = 0.35;
        BackColor = Color.Black;
        Cursor = Cursors.Cross;
        ShowInTaskbar = false;
        KeyPreview = true;
        DoubleBuffered = true;
        Text = "Draw the area to record";
    }

    /// <summary>The chosen rectangle, in desktop pixels.</summary>
    public CaptureRect Selection { get; private set; }

    protected override void OnMouseDown(MouseEventArgs e)
    {
        base.OnMouseDown(e);

        if (e.Button != MouseButtons.Left)
        {
            Cancel();
            return;
        }

        anchor = e.Location;
        dragging = new Rectangle(e.Location, Size.Empty);
        active = true;
        Invalidate();
    }

    protected override void OnMouseMove(MouseEventArgs e)
    {
        base.OnMouseMove(e);

        if (!active)
            return;

        dragging = Normalise(anchor, e.Location);
        Invalidate();
    }

    protected override void OnMouseUp(MouseEventArgs e)
    {
        base.OnMouseUp(e);

        if (!active)
            return;

        active = false;
        dragging = Normalise(anchor, e.Location);

        if (dragging.Width < 16 || dragging.Height < 16)
        {
            Invalidate();
            return;
        }

        Accept(dragging);
    }

    protected override void OnKeyDown(KeyEventArgs e)
    {
        base.OnKeyDown(e);

        if (e.KeyCode == Keys.Escape)
            Cancel();
        else if (e.KeyCode == Keys.Enter && dragging.Width >= 16 && dragging.Height >= 16)
            Accept(dragging);
    }

    protected override void OnPaint(PaintEventArgs e)
    {
        base.OnPaint(e);

        Graphics graphics = e.Graphics;
        graphics.SmoothingMode = System.Drawing.Drawing2D.SmoothingMode.AntiAlias;

        using (var hint = new Font("Segoe UI", 16f, FontStyle.Bold))
        using (var brush = new SolidBrush(Color.White))
        {
            const string text = "Drag over the area you want to record  ·  Esc to cancel";
            SizeF size = graphics.MeasureString(text, hint);
            graphics.DrawString(text, hint, brush, (Width - size.Width) / 2f, 40f);
        }

        if (dragging.Width <= 0 || dragging.Height <= 0)
            return;

        using (var fill = new SolidBrush(Color.FromArgb(60, 232, 176, 75)))
            graphics.FillRectangle(fill, dragging);

        using var pen = new Pen(Theme.Accent, 3f);
        graphics.DrawRectangle(pen, dragging);

        CaptureRect desktop = ToDesktop(dragging);
        string label = $"{desktop.Width} × {desktop.Height}";

        using var font = new Font("Segoe UI", 11f, FontStyle.Bold);
        SizeF labelSize = graphics.MeasureString(label, font);
        float labelX = Math.Min(dragging.X + 8, Width - labelSize.Width - 12);
        float labelY = Math.Max(6, dragging.Y - labelSize.Height - 8);

        using var backing = new SolidBrush(Color.FromArgb(220, 17, 20, 26));
        graphics.FillRectangle(backing, labelX - 6, labelY - 2, labelSize.Width + 12, labelSize.Height + 4);
        graphics.DrawString(label, font, Brushes.White, labelX, labelY);
    }

    private void Accept(Rectangle rectangle)
    {
        CaptureRect desktop = ToDesktop(rectangle);

        /* Even sizes, rounded down, so the encoder never gets an odd frame. */
        Selection = Presets.Normalise(desktop);
        DialogResult = DialogResult.OK;
        Close();
    }

    private void Cancel()
    {
        DialogResult = DialogResult.Cancel;
        Close();
    }

    private CaptureRect ToDesktop(Rectangle rectangle) =>
        new CaptureRect(Left + rectangle.X, Top + rectangle.Y, rectangle.Width, rectangle.Height);

    private static Rectangle Normalise(Point from, Point to) => new(
        Math.Min(from.X, to.X),
        Math.Min(from.Y, to.Y),
        Math.Abs(from.X - to.X),
        Math.Abs(from.Y - to.Y));
}
