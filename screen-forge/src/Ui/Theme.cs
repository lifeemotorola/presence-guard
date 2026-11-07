/*
ScreenForge
Copyright (C) 2026 Presence Guard contributors

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
/// One palette, applied everywhere. A recorder is a tool you open for ten
/// seconds before a lesson; it should look like it belongs on the same shelf as
/// the rest of the suite rather than like a 1998 settings dialog.
/// </summary>
public static class Theme
{
    public static readonly Color Back = Color.FromArgb(17, 20, 26);
    public static readonly Color Panel = Color.FromArgb(25, 29, 38);
    public static readonly Color Field = Color.FromArgb(33, 38, 49);
    public static readonly Color Line = Color.FromArgb(47, 54, 68);
    public static readonly Color Text = Color.FromArgb(233, 237, 243);
    public static readonly Color Muted = Color.FromArgb(147, 158, 175);
    public static readonly Color Accent = Color.FromArgb(232, 176, 75);
    public static readonly Color Danger = Color.FromArgb(224, 86, 76);
    public static readonly Color Good = Color.FromArgb(94, 196, 138);

    public static readonly Font Body = new("Segoe UI", 9.5f, FontStyle.Regular, GraphicsUnit.Point);
    public static readonly Font Small = new("Segoe UI", 8.25f, FontStyle.Regular, GraphicsUnit.Point);
    public static readonly Font Heading = new("Segoe UI Semibold", 17f, FontStyle.Bold, GraphicsUnit.Point);
    public static readonly Font Button = new("Segoe UI Semibold", 10f, FontStyle.Bold, GraphicsUnit.Point);
    public static readonly Font Timer = new("Consolas", 20f, FontStyle.Bold, GraphicsUnit.Point);
    public static readonly Font Mono = new("Consolas", 8.75f, FontStyle.Regular, GraphicsUnit.Point);

    /// <summary>Repaints a whole tree: WinForms defaults to battleship grey on every control.</summary>
    public static void Apply(Control root)
    {
        if (root is null)
            return;

        root.Font = Body;

        switch (root)
        {
            case Form form:
                form.BackColor = Back;
                form.ForeColor = Text;
                break;

            case GroupBox box:
                box.BackColor = Panel;
                box.ForeColor = Muted;
                break;

            case Label label:
                label.BackColor = Color.Transparent;
                label.ForeColor = label.ForeColor == Control.DefaultForeColor ? Text : label.ForeColor;
                break;

            case CheckBox check:
                check.BackColor = Color.Transparent;
                check.ForeColor = Text;
                break;

            case RichTextBox rich:
                rich.BackColor = Field;
                rich.ForeColor = Text;
                rich.BorderStyle = BorderStyle.None;
                break;

            case TextBox text:
                text.BackColor = Field;
                text.ForeColor = Text;
                text.BorderStyle = BorderStyle.FixedSingle;
                break;

            case ComboBox combo:
                combo.BackColor = Field;
                combo.ForeColor = Text;
                combo.FlatStyle = FlatStyle.Flat;
                break;

            case NumericUpDown number:
                number.BackColor = Field;
                number.ForeColor = Text;
                number.BorderStyle = BorderStyle.FixedSingle;
                break;

            case ListBox list:
                list.BackColor = Field;
                list.ForeColor = Text;
                list.BorderStyle = BorderStyle.None;
                break;

            case Panel panel:
                panel.BackColor = panel.BackColor == Control.DefaultBackColor ? Panel : panel.BackColor;
                break;
        }

        foreach (Control child in root.Controls)
            Apply(child);
    }

    public static Button MakeButton(string text, Color accent)
    {
        var button = new Button
        {
            Text = text,
            AutoSize = false,
            Height = 42,
            FlatStyle = FlatStyle.Flat,
            BackColor = accent,
            ForeColor = accent == Accent ? Color.FromArgb(26, 22, 12) : Color.White,
            Font = Button,
            Cursor = Cursors.Hand,
            UseVisualStyleBackColor = false,
        };

        button.FlatAppearance.BorderSize = 0;
        button.FlatAppearance.MouseOverBackColor = Lighten(accent, 0.12);
        button.FlatAppearance.MouseDownBackColor = Darken(accent, 0.12);
        return button;
    }

    public static Button MakeGhostButton(string text)
    {
        var button = new Button
        {
            Text = text,
            AutoSize = false,
            Height = 30,
            FlatStyle = FlatStyle.Flat,
            BackColor = Field,
            ForeColor = Text,
            Font = Body,
            Cursor = Cursors.Hand,
            UseVisualStyleBackColor = false,
        };

        button.FlatAppearance.BorderColor = Line;
        button.FlatAppearance.BorderSize = 1;
        button.FlatAppearance.MouseOverBackColor = Lighten(Field, 0.1);
        return button;
    }

    public static Color Lighten(Color color, double amount) => Blend(color, Color.White, amount);

    public static Color Darken(Color color, double amount) => Blend(color, Color.Black, amount);

    private static Color Blend(Color from, Color to, double amount)
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

            using (var path = Rounded(new Rectangle(2, 2, 60, 60), 14))
            using (var fill = new SolidBrush(Panel))
                graphics.FillPath(fill, path);

            using (var brush = new SolidBrush(Danger))
                graphics.FillEllipse(brush, 17, 17, 30, 30);

            using (var brush = new SolidBrush(Accent))
                graphics.FillEllipse(brush, 42, 42, 12, 12);
        }

        IntPtr handle = bitmap.GetHicon();
        return Icon.FromHandle(handle);
    }

    public static GraphicsPath Rounded(Rectangle bounds, int radius)
    {
        var path = new GraphicsPath();
        int diameter = radius * 2;

        path.AddArc(bounds.Left, bounds.Top, diameter, diameter, 180, 90);
        path.AddArc(bounds.Right - diameter, bounds.Top, diameter, diameter, 270, 90);
        path.AddArc(bounds.Right - diameter, bounds.Bottom - diameter, diameter, diameter, 0, 90);
        path.AddArc(bounds.Left, bounds.Bottom - diameter, diameter, diameter, 90, 90);
        path.CloseFigure();

        return path;
    }
}
