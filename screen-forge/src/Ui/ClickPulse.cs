/*
ScreenForge
Copyright (C) 2026 Emmanuel Suah

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.
*/

using System;
using System.ComponentModel;
using System.Diagnostics;
using System.Drawing;
using System.Drawing.Drawing2D;
using System.Drawing.Imaging;
using System.Runtime.InteropServices;
using System.Windows.Forms;
using ScreenForge.Core;
using ScreenForge.Interop;
using Timer = System.Windows.Forms.Timer;

namespace ScreenForge.Ui;

/// <summary>
/// A short-lived, per-pixel-alpha topmost window. Layered + transparent +
/// no-activate keeps the ring visible in desktop capture without stealing focus
/// or receiving the click. It is a desktop effect, not a post-processing filter.
/// </summary>
internal sealed class ClickPulse : Form
{
    private const int WsExTransparent = 0x20;
    private const int WsExToolWindow = 0x80;
    private const int WsExLayered = 0x80000;
    private const int WsExNoActivate = 0x08000000;
    private const int WmNcHitTest = 0x0084;
    private const int HtTransparent = -1;
    private const uint UlwAlpha = 0x00000002;
    private const byte AcSrcAlpha = 0x01;

    private readonly MouseClickButton button;
    private readonly int left;
    private readonly int top;
    private readonly Action<Exception> reportError;
    private readonly Stopwatch clock = new();
    private readonly Timer timer;

    public ClickPulse(MouseClickButton button, int x, int y, Action<Exception> reportError)
    {
        this.button = button;
        this.reportError = reportError ?? (_ => { });
        left = x - ClickFeedback.WindowSize / 2;
        top = y - ClickFeedback.WindowSize / 2;

        AutoScaleMode = AutoScaleMode.None;
        FormBorderStyle = FormBorderStyle.None;
        StartPosition = FormStartPosition.Manual;
        ShowInTaskbar = false;
        TopMost = true;
        Bounds = new Rectangle(left, top, ClickFeedback.WindowSize, ClickFeedback.WindowSize);

        timer = new Timer { Interval = ClickFeedback.FrameMilliseconds };
        timer.Tick += (_, _) => Animate();
    }

    protected override bool ShowWithoutActivation => true;

    protected override CreateParams CreateParams
    {
        get
        {
            CreateParams parameters = base.CreateParams;
            parameters.ExStyle |= WsExTransparent | WsExToolWindow | WsExLayered | WsExNoActivate;
            return parameters;
        }
    }

    protected override void WndProc(ref Message message)
    {
        if (message.Msg == WmNcHitTest)
        {
            message.Result = new IntPtr(HtTransparent);
            return;
        }
        base.WndProc(ref message);
    }

    public void ShowPulse()
    {
        Show();
        clock.Start();
        DrawFrame();
        timer.Start();
    }

    private void Animate()
    {
        if (clock.ElapsedMilliseconds >= ClickFeedback.DurationMilliseconds)
        {
            Dispose();
            return;
        }

        try { DrawFrame(); }
        catch (Exception ex) { reportError(ex); }
    }

    private void DrawFrame()
    {
        long elapsed = clock.ElapsedMilliseconds;
        int alpha = ClickFeedback.Opacity(elapsed);
        float radius = ClickFeedback.Radius(elapsed);
        float centre = ClickFeedback.WindowSize / 2f;
        Color color = button switch
        {
            MouseClickButton.Right => Color.FromArgb(75, 192, 255),
            MouseClickButton.Middle => Color.FromArgb(186, 130, 255),
            _ => Color.FromArgb(255, 190, 64),
        };

        using var bitmap = new Bitmap(ClickFeedback.WindowSize, ClickFeedback.WindowSize, PixelFormat.Format32bppPArgb);
        using (Graphics graphics = Graphics.FromImage(bitmap))
        {
            graphics.Clear(Color.Transparent);
            graphics.SmoothingMode = SmoothingMode.AntiAlias;
            var circle = new RectangleF(centre - radius, centre - radius, radius * 2, radius * 2);
            using var fill = new SolidBrush(Color.FromArgb(alpha / 7, color));
            using var shadow = new Pen(Color.FromArgb(alpha * 3 / 4, Color.Black), 7f);
            using var ring = new Pen(Color.FromArgb(alpha, color), 3.5f);
            graphics.FillEllipse(fill, circle);
            graphics.DrawEllipse(shadow, circle);
            graphics.DrawEllipse(ring, circle);
        }

        ShowBitmap(bitmap);
    }

    private void ShowBitmap(Bitmap bitmap)
    {
        IntPtr screenDc = NativeMethods.GetDC(IntPtr.Zero);
        if (screenDc == IntPtr.Zero)
            throw new Win32Exception(Marshal.GetLastWin32Error(), "Could not get the screen device context");

        IntPtr memoryDc = IntPtr.Zero;
        IntPtr image = IntPtr.Zero;
        IntPtr previous = IntPtr.Zero;
        try
        {
            memoryDc = NativeMethods.CreateCompatibleDC(screenDc);
            if (memoryDc == IntPtr.Zero)
                throw new Win32Exception(Marshal.GetLastWin32Error(), "Could not create a drawing context");

            image = bitmap.GetHbitmap(Color.FromArgb(0));
            previous = NativeMethods.SelectObject(memoryDc, image);
            if (previous == IntPtr.Zero || previous == new IntPtr(-1))
                throw new Win32Exception(Marshal.GetLastWin32Error(), "Could not select the indicator image");

            var destination = new NativePoint(left, top);
            var size = new NativeSize(bitmap.Width, bitmap.Height);
            var source = new NativePoint(0, 0);
            var blend = new BlendFunction { Operation = 0, Flags = 0, SourceAlpha = 255, AlphaFormat = AcSrcAlpha };
            if (!NativeMethods.UpdateLayeredWindow(Handle, screenDc, ref destination, ref size, memoryDc,
                    ref source, 0, ref blend, UlwAlpha))
                throw new Win32Exception(Marshal.GetLastWin32Error(), "Could not display the click indicator");
        }
        finally
        {
            if (previous != IntPtr.Zero && previous != new IntPtr(-1))
                NativeMethods.SelectObject(memoryDc, previous);
            if (image != IntPtr.Zero)
                NativeMethods.DeleteObject(image);
            if (memoryDc != IntPtr.Zero)
                NativeMethods.DeleteDC(memoryDc);
            NativeMethods.ReleaseDC(IntPtr.Zero, screenDc);
        }
    }

    protected override void Dispose(bool disposing)
    {
        if (disposing)
            timer?.Dispose();
        base.Dispose(disposing);
    }
}
