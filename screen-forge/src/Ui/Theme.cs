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

/// <summary>
/// One palette and one set of drawing helpers, used by every control that paints
/// itself. WinForms defaults are a grey 1998 look; a recorder is opened for ten
/// seconds before something that matters, so it should be quiet, dark, and
/// obvious about the one button that matters.
/// </summary>
public static class Theme
{
    public static readonly Color Back = Color.FromArgb(13, 16, 21);
    public static readonly Color Panel = Color.FromArgb(22, 27, 35);
    public static readonly Color Raised = Color.FromArgb(30, 37, 47);
    public static readonly Color Field = Color.FromArgb(34, 41, 52);
    public static readonly Color Line = Color.FromArgb(46, 56, 70);
    public static readonly Color LineSoft = Color.FromArgb(35, 42, 53);
    public static readonly Color Text = Color.FromArgb(238, 242, 247);
    public static readonly Color Muted = Color.FromArgb(141, 152, 169);
    public static readonly Color Accent = Color.FromArgb(242, 178, 62);
    public static readonly Color AccentInk = Color.FromArgb(28, 22, 8);
    public static readonly Color Danger = Color.FromArgb(226, 87, 76);
    public static readonly Color Good = Color.FromArgb(78, 209, 142);

    public static readonly Font Body = new("Segoe UI", 9.5f, FontStyle.Regular, GraphicsUnit.Point);
    public static readonly Font Small = new("Segoe UI", 8.5f, FontStyle.Regular, GraphicsUnit.Point);
    public static readonly Font Tiny = new("Segoe UI", 7.75f, FontStyle.Regular, GraphicsUnit.Point);
    public static readonly Font Heading = new("Segoe UI Semibold", 19f, FontStyle.Bold, GraphicsUnit.Point);
    public static readonly Font CardTitle = new("Segoe UI Semibold", 8f, FontStyle.Bold, GraphicsUnit.Point);
    public static readonly Font Button = new("Segoe UI Semibold", 10f, FontStyle.Bold, GraphicsUnit.Point);
    public static readonly Font Timer = new("Consolas", 30f, FontStyle.Bold, GraphicsUnit.Point);
    public static readonly Font Mono = new("Consolas", 8.5f, FontStyle.Regular, GraphicsUnit.Point);

    public const int CardRadius = 14;
    public const int ControlRadius = 9;

    /// <summary>Repaints the stock controls. Custom ones paint themselves.</summary>
    public static void Apply(Control root)
    {
        if (root is null)
            return;

        switch (root)
        {
            case Form form:
                form.BackColor = Back;
                form.ForeColor = Text;
                break;

            case Label label:
                label.BackColor = Color.Transparent;
                break;

            case RichTextBox rich:
                rich.BackColor = Panel;
                rich.ForeColor = Muted;
                rich.BorderStyle = BorderStyle.None;
                rich.Font = Mono;
                break;

            case TextBox text:
                text.BackColor = Field;
                text.ForeColor = Text;
                text.BorderStyle = BorderStyle.FixedSingle;
                text.Font = Body;
                break;

            case ComboBox combo:
                combo.BackColor = Field;
                combo.ForeColor = Text;
                combo.FlatStyle = FlatStyle.Flat;
                combo.Font = Body;
                break;

            case NumericUpDown number:
                number.BackColor = Field;
                number.ForeColor = Text;
                number.BorderStyle = BorderStyle.FixedSingle;
                number.Font = Body;
                break;
        }

        foreach (Control child in root.Controls)
            Apply(child);
    }

    // ------------------------------------------------------------- drawing

    public static GraphicsPath Rounded(Rectangle bounds, int radius)
    {
        var path = new GraphicsPath();

        int limit = Math.Max(0, Math.Min(bounds.Width, bounds.Height) / 2);
        int r = Math.Max(0, Math.Min(radius, limit));

        if (r <= 0)
        {
            path.AddRectangle(bounds);
            return path;
        }

        int d = r * 2;
        path.AddArc(bounds.Left, bounds.Top, d, d, 180, 90);
        path.AddArc(bounds.Right - d, bounds.Top, d, d, 270, 90);
        path.AddArc(bounds.Right - d, bounds.Bottom - d, d, d, 0, 90);
        path.AddArc(bounds.Left, bounds.Bottom - d, d, d, 90, 90);
        path.CloseFigure();

        return path;
    }

    public static void FillRounded(Graphics graphics, Rectangle bounds, int radius, Color fill)
    {
        Smooth(graphics);

        using GraphicsPath path = Rounded(bounds, radius);
        using var brush = new SolidBrush(fill);
        graphics.FillPath(brush, path);
    }

    public static void DrawBorder(Graphics graphics, Rectangle bounds, int radius, Color color, float width = 1f)
    {
        Smooth(graphics);

        using GraphicsPath path = Rounded(bounds, radius);
        using var pen = new Pen(color, width);
        graphics.DrawPath(pen, path);
    }

    public static void Smooth(Graphics graphics) =>
        graphics.SmoothingMode = SmoothingMode.AntiAlias;

    public static Color Lighten(Color color, double amount) => Blend(color, Color.White, amount);

    public static Color Darken(Color color, double amount) => Blend(color, Color.Black, amount);

    public static Color Blend(Color from, Color to, double amount)
    {
        amount = Math.Clamp(amount, 0, 1);

        return Color.FromArgb(
            from.A,
            (int)(from.R + (to.R - from.R) * amount),
            (int)(from.G + (to.G - from.G) * amount),
            (int)(from.B + (to.B - from.B) * amount));
    }

    /// <summary>
    /// The window icon, drawn once at startup: a dark tile with a red record dot
    /// and an amber corner. No binary asset to keep in the repository.
    /// </summary>
    public static Icon AppIcon()
    {
        using var bitmap = new Bitmap(64, 64);

        using (Graphics graphics = Graphics.FromImage(bitmap))
        {
            graphics.SmoothingMode = SmoothingMode.AntiAlias;
            graphics.Clear(Color.Transparent);

            using (GraphicsPath path = Rounded(new Rectangle(2, 2, 60, 60), 16))
            using (var fill = new SolidBrush(Panel))
                graphics.FillPath(fill, path);

            using (var brush = new SolidBrush(Danger))
                graphics.FillEllipse(brush, 17, 17, 30, 30);

            using (var brush = new SolidBrush(Accent))
                graphics.FillEllipse(brush, 41, 41, 13, 13);
        }

        IntPtr handle = bitmap.GetHicon();
        return Icon.FromHandle(handle);
    }
}
