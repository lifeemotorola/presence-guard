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

namespace ScreenForge.Ui;

public enum ButtonTone
{
    Primary,
    Danger,
    Ghost,
}

/// <summary>
/// A rounded panel with a small caption, used instead of GroupBox. The group box
/// draws its own grey groove that cannot be recoloured on every Windows version;
/// this one is painted, so it matches the rest of the window everywhere.
///
/// Auto-fitting cards are measured by <see cref="FitToContent"/>, which the
/// window calls after every resize: the height is derived from what is actually
/// inside, never guessed.
/// </summary>
public sealed class Card : Panel
{
    private readonly bool autoFit;
    private readonly FlowLayoutPanel body;

    public Card(string title, bool autoFit = true)
    {
        Title = title;
        this.autoFit = autoFit;

        DoubleBuffered = true;
        BackColor = Theme.Back;
        Padding = new Padding(18, 38, 18, 16);
        Margin = new Padding(0, 0, 0, 12);

        if (!autoFit)
            return;

        body = new FlowLayoutPanel
        {
            FlowDirection = FlowDirection.LeftToRight,
            WrapContents = true,
            AutoSize = true,
            AutoSizeMode = AutoSizeMode.GrowAndShrink,
            Dock = DockStyle.Top,
            BackColor = Theme.Panel,
            Margin = new Padding(0),
        };

        Controls.Add(body);
    }

    public string Title { get; set; }

    /// <summary>Where fields go. Null on a card that is filled with a single control instead.</summary>
    public FlowLayoutPanel Body => body;

    public void FitToContent()
    {
        if (!autoFit || body == null)
            return;

        int width = Math.Max(140, ClientSize.Width - Padding.Horizontal);
        Size preferred = body.GetPreferredSize(new Size(width, 0));

        Height = preferred.Height + Padding.Vertical;
    }

    protected override void OnPaintBackground(PaintEventArgs e)
    {
        Theme.Smooth(e.Graphics);
        e.Graphics.Clear(BackColor);

        var bounds = new Rectangle(0, 0, Math.Max(1, Width - 1), Math.Max(1, Height - 1));
        Theme.FillRounded(e.Graphics, bounds, Theme.CardRadius, Theme.Panel);
        Theme.DrawBorder(e.Graphics, bounds, Theme.CardRadius, Theme.LineSoft);
    }

    protected override void OnPaint(PaintEventArgs e)
    {
        base.OnPaint(e);

        if (string.IsNullOrWhiteSpace(Title))
            return;

        var caption = new Rectangle(Padding.Left, 15, Math.Max(1, Width - Padding.Horizontal), 18);

        TextRenderer.DrawText(
            e.Graphics,
            Title.ToUpperInvariant(),
            Theme.CardTitle,
            caption,
            Theme.Muted,
            TextFormatFlags.Left | TextFormatFlags.VerticalCenter | TextFormatFlags.EndEllipsis);
    }
}

/// <summary>
/// The only two kinds of button this window needs, painted rather than themed:
/// a filled one that starts and stops, and a quiet one for everything else.
/// </summary>
public sealed class AccentButton : Control
{
    private bool hovering;
    private bool pressing;
    private ButtonTone tone;

    public AccentButton(string text, ButtonTone tone)
    {
        Text = text;
        this.tone = tone;

        SetStyle(
            ControlStyles.UserPaint |
            ControlStyles.AllPaintingInWmPaint |
            ControlStyles.OptimizedDoubleBuffer |
            ControlStyles.ResizeRedraw |
            ControlStyles.SupportsTransparentBackColor,
            true);

        Font = Theme.Button;
        Cursor = Cursors.Hand;
        TabStop = true;
        BackColor = Color.Transparent;
        MinimumSize = new Size(80, 34);
    }

    public ButtonTone Tone
    {
        get => tone;
        set
        {
            tone = value;
            Invalidate();
        }
    }

    protected override void OnTextChanged(EventArgs e)
    {
        base.OnTextChanged(e);
        Invalidate();
    }

    protected override void OnEnabledChanged(EventArgs e)
    {
        base.OnEnabledChanged(e);
        Cursor = Enabled ? Cursors.Hand : Cursors.Default;
        Invalidate();
    }

    protected override void OnMouseEnter(EventArgs e)
    {
        base.OnMouseEnter(e);
        hovering = true;
        Invalidate();
    }

    protected override void OnMouseLeave(EventArgs e)
    {
        base.OnMouseLeave(e);
        hovering = false;
        pressing = false;
        Invalidate();
    }

    protected override void OnMouseDown(MouseEventArgs e)
    {
        base.OnMouseDown(e);

        if (e.Button != MouseButtons.Left)
            return;

        pressing = true;
        Focus();
        Invalidate();
    }

    protected override void OnMouseUp(MouseEventArgs e)
    {
        base.OnMouseUp(e);
        pressing = false;
        Invalidate();
    }

    protected override void OnKeyDown(KeyEventArgs e)
    {
        base.OnKeyDown(e);

        if (e.KeyCode == Keys.Space || e.KeyCode == Keys.Enter)
        {
            e.Handled = true;
            OnClick(EventArgs.Empty);
        }
    }

    protected override void OnPaint(PaintEventArgs e)
    {
        Graphics graphics = e.Graphics;
        Theme.Smooth(graphics);

        Color fill;
        Color ink;

        switch (Tone)
        {
            case ButtonTone.Danger:
                fill = Theme.Danger;
                ink = Color.White;
                break;

            case ButtonTone.Ghost:
                fill = Theme.Raised;
                ink = Theme.Text;
                break;

            default:
                fill = Theme.Accent;
                ink = Theme.AccentInk;
                break;
        }

        if (!Enabled)
        {
            fill = Theme.Blend(fill, Theme.Back, 0.62);
            ink = Theme.Blend(ink, Theme.Back, 0.45);
        }
        else if (pressing)
        {
            fill = Theme.Darken(fill, 0.12);
        }
        else if (hovering)
        {
            fill = Theme.Lighten(fill, 0.09);
        }

        var bounds = new Rectangle(0, 0, Math.Max(1, Width - 1), Math.Max(1, Height - 1));

        Theme.FillRounded(graphics, bounds, Theme.ControlRadius, fill);

        if (Tone == ButtonTone.Ghost)
            Theme.DrawBorder(graphics, bounds, Theme.ControlRadius, Theme.Line);

        TextRenderer.DrawText(
            graphics,
            Text,
            Font,
            bounds,
            ink,
            TextFormatFlags.HorizontalCenter | TextFormatFlags.VerticalCenter | TextFormatFlags.EndEllipsis);
    }
}

/// <summary>
/// A switch, because a tick box inside a dark panel looks like a mistake. Same
/// contract as CheckBox — Checked, CheckedChanged, Text, Enabled — so the rest
/// of the window does not care which one it is.
/// </summary>
public sealed class ToggleSwitch : Control
{
    private const int TrackWidth = 38;
    private const int TrackHeight = 20;

    private bool isChecked;
    private bool hovering;

    public ToggleSwitch(string text, bool value)
    {
        Text = text;
        isChecked = value;

        SetStyle(
            ControlStyles.UserPaint |
            ControlStyles.AllPaintingInWmPaint |
            ControlStyles.OptimizedDoubleBuffer |
            ControlStyles.ResizeRedraw |
            ControlStyles.SupportsTransparentBackColor,
            true);

        Font = Theme.Body;
        BackColor = Color.Transparent;
        Cursor = Cursors.Hand;
        TabStop = true;
        Height = 24;
    }

    public event EventHandler CheckedChanged;

    public bool Checked
    {
        get => isChecked;
        set
        {
            if (isChecked == value)
                return;

            isChecked = value;
            Invalidate();
            CheckedChanged?.Invoke(this, EventArgs.Empty);
        }
    }

    protected override void OnEnabledChanged(EventArgs e)
    {
        base.OnEnabledChanged(e);
        Cursor = Enabled ? Cursors.Hand : Cursors.Default;
        Invalidate();
    }

    protected override void OnMouseEnter(EventArgs e)
    {
        base.OnMouseEnter(e);
        hovering = true;
        Invalidate();
    }

    protected override void OnMouseLeave(EventArgs e)
    {
        base.OnMouseLeave(e);
        hovering = false;
        Invalidate();
    }

    protected override void OnMouseUp(MouseEventArgs e)
    {
        base.OnMouseUp(e);

        if (Enabled && e.Button == MouseButtons.Left)
            Checked = !Checked;
    }

    protected override void OnKeyDown(KeyEventArgs e)
    {
        base.OnKeyDown(e);

        if (!Enabled)
            return;

        if (e.KeyCode == Keys.Space)
        {
            e.Handled = true;
            Checked = !Checked;
        }
    }

    protected override void OnPaint(PaintEventArgs e)
    {
        Graphics graphics = e.Graphics;
        Theme.Smooth(graphics);

        int top = (Height - TrackHeight) / 2;
        var track = new Rectangle(1, top, TrackWidth, TrackHeight);

        Color trackFill = isChecked ? Theme.Accent : Theme.Field;

        if (!Enabled)
            trackFill = Theme.Blend(trackFill, Theme.Back, 0.6);
        else if (hovering)
            trackFill = isChecked ? Theme.Lighten(trackFill, 0.08) : Theme.Lighten(trackFill, 0.1);

        Theme.FillRounded(graphics, track, TrackHeight / 2, trackFill);

        if (!isChecked)
            Theme.DrawBorder(graphics, track, TrackHeight / 2, Theme.Line);

        int knobX = isChecked ? track.Right - TrackHeight + 2 : track.Left + 2;
        var knob = new Rectangle(knobX, top + 2, TrackHeight - 4, TrackHeight - 4);

        using (var brush = new SolidBrush(isChecked ? Theme.AccentInk : Theme.Muted))
            graphics.FillEllipse(brush, knob);

        var caption = new Rectangle(TrackWidth + 10, 0, Math.Max(1, Width - TrackWidth - 10), Height);

        TextRenderer.DrawText(
            graphics,
            Text,
            Font,
            caption,
            Enabled ? Theme.Text : Theme.Muted,
            TextFormatFlags.Left | TextFormatFlags.VerticalCenter | TextFormatFlags.EndEllipsis);
    }
}

/// <summary>The little tile in the header: same shape as the window icon.</summary>
public sealed class BrandMark : Control
{
    public BrandMark()
    {
        SetStyle(
            ControlStyles.UserPaint |
            ControlStyles.AllPaintingInWmPaint |
            ControlStyles.OptimizedDoubleBuffer |
            ControlStyles.ResizeRedraw |
            ControlStyles.SupportsTransparentBackColor,
            true);

        BackColor = Color.Transparent;
        Size = new Size(42, 42);
    }

    protected override void OnPaint(PaintEventArgs e)
    {
        Graphics graphics = e.Graphics;
        Theme.Smooth(graphics);

        var bounds = new Rectangle(0, 0, Width - 1, Height - 1);

        using (GraphicsPath path = Theme.Rounded(bounds, Math.Min(Width, Height) / 4))
        using (var brush = new SolidBrush(Theme.Raised))
            graphics.FillPath(brush, path);

        int dot = Math.Max(8, (int)(Width * 0.4));

        using (var brush = new SolidBrush(Theme.Danger))
            graphics.FillEllipse(brush, (Width - dot) / 2 - 2, (Height - dot) / 2 - 2, dot, dot);

        int tick = Math.Max(4, (int)(Width * 0.18));

        using (var brush = new SolidBrush(Theme.Accent))
            graphics.FillEllipse(brush, Width - tick - 7, Height - tick - 7, tick, tick);
    }
}

/// <summary>A one-pixel rule, for separating the footer from the work above it.</summary>
public sealed class Hairline : Control
{
    public Hairline()
    {
        SetStyle(ControlStyles.UserPaint | ControlStyles.AllPaintingInWmPaint | ControlStyles.OptimizedDoubleBuffer, true);
        Height = 1;
        BackColor = Theme.Back;
    }

    protected override void OnPaint(PaintEventArgs e)
    {
        using var pen = new Pen(Theme.LineSoft);
        e.Graphics.DrawLine(pen, 0, 0, Width, 0);
    }
}
