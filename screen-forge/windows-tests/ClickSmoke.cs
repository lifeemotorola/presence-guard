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
using System.Runtime.InteropServices;
using System.Threading;
using System.Windows.Forms;
using ScreenForge.Core;
using ScreenForge.Interop;
using ScreenForge.Ui;

internal static class ClickSmoke
{
    [DllImport("user32.dll")]
    private static extern IntPtr WindowFromPoint(NativePoint point);

    [DllImport("dwmapi.dll")]
    private static extern int DwmFlush();

    [STAThread]
    private static int Main()
    {
        try
        {
            Application.SetHighDpiMode(HighDpiMode.PerMonitorV2);
            Application.EnableVisualStyles();
            using var hook = new MouseClickHook((_, _, _) => { });
            hook.Start();

            Rectangle screen = Screen.PrimaryScreen?.Bounds ?? new Rectangle(0, 0, 1024, 768);
            int x = screen.Left + screen.Width / 2;
            int y = screen.Top + screen.Height / 2;
            Color backgroundColor = Color.FromArgb(25, 32, 42);
            using var background = new Form
            {
                FormBorderStyle = FormBorderStyle.None,
                StartPosition = FormStartPosition.Manual,
                Bounds = new Rectangle(x - 100, y - 100, 200, 200),
                BackColor = backgroundColor,
                TopMost = true,
                ShowInTaskbar = false,
            };
            background.Show();
            Pump();

            Bitmap baseline;
            bool desktopVisible;
            try
            {
                baseline = Capture(x, y);
                desktopVisible = Similar(baseline.GetPixel(52, 52), backgroundColor);
            }
            catch (ExternalException ex)
            {
                baseline = new Bitmap(ClickFeedback.WindowSize, ClickFeedback.WindowSize);
                desktopVisible = false;
                Console.WriteLine("Screen capture unavailable: " + ex.Message);
            }
            using Bitmap before = baseline;
            Console.WriteLine(desktopVisible
                ? "Interactive desktop detected; testing capture and click-through."
                : "No capturable interactive desktop; testing native window creation only.");

            foreach (MouseClickButton button in Enum.GetValues<MouseClickButton>())
            {
                Exception paintError = null;
                using var pulse = new ClickPulse(button, x, y, ex => paintError = ex);
                pulse.ShowPulse(); // exercises layered-window/GDI setup
                Pump();
                if (paintError != null)
                    throw new InvalidOperationException("Click indicator animation failed", paintError);

                long styles = NativeMethods.GetWindowLongPtr(pulse.Handle, -20).ToInt64();
                const long required = 0x00000020 | 0x00000080 | 0x00080000 | 0x08000000;
                if ((styles & required) != required)
                    throw new InvalidOperationException("Click indicator is missing its non-activating, click-through or layered styles.");

                if (desktopVisible)
                {
                    using Bitmap after = Capture(x, y);
                    int changed = ChangedPixels(before, after);
                    if (changed < 30)
                        throw new InvalidOperationException($"{button} indicator did not appear in a CAPTUREBLT screen grab ({changed} pixels changed).");
                    if (WindowFromPoint(new NativePoint(x, y)) != background.Handle)
                        throw new InvalidOperationException("The indicator blocked the window beneath the click.");
                    Console.WriteLine($"{button}: {changed} changed pixels; click-through OK.");
                }
                pulse.Dispose();
                Pump();
            }

            string failure = null;
            using (var highlighter = new MouseClickHighlighter(background,
                new CaptureRect(x - 100, y - 100, 200, 200), text => failure = text))
            {
                highlighter.Start();
                highlighter.ObserveClick(MouseClickButton.Left, x, y);
                Pump();
                Require(highlighter.VisiblePulseCount == 0, "The inactive highlighter drew a click.");

                highlighter.SetActive(true);
                highlighter.ObserveClick(MouseClickButton.Left, x + 300, y);
                Pump();
                Require(highlighter.VisiblePulseCount == 0, "A click outside the capture area was drawn.");

                highlighter.ObserveClick(MouseClickButton.Left, x, y);
                Pump();
                Require(failure == null && highlighter.VisiblePulseCount == 1, failure ?? "Active click was not displayed.");

                highlighter.SetActive(false);
                Require(highlighter.VisiblePulseCount == 0, "Pause did not clear the indicator.");
                highlighter.SetActive(true);
                highlighter.ObserveClick(MouseClickButton.Middle, x, y);
                highlighter.SetActive(false); // a queued click must not reappear, even after resume
                highlighter.SetActive(true);
                Pump();
                Require(highlighter.VisiblePulseCount == 0, "A queued click appeared after pause and resume.");

                highlighter.ObserveClick(MouseClickButton.Right, x, y);
                Pump();
                Require(failure == null && highlighter.VisiblePulseCount == 1, failure ?? "Resume did not display a click.");
            }

            Console.WriteLine(desktopVisible
                ? "::notice title=Click smoke test::Hook, capture, click-through and pause/resume checks passed."
                : "::notice title=Click smoke test::Hook and pause/resume checks passed; desktop capture check skipped on this runner.");
            return 0;
        }
        catch (Exception ex)
        {
            Console.Error.WriteLine(ex);
            return 1;
        }
    }

    private static void Require(bool condition, string message)
    {
        if (!condition)
            throw new InvalidOperationException(message);
    }

    private static void Pump()
    {
        Application.DoEvents();
        Thread.Sleep(100);
        Application.DoEvents();
        DwmFlush();
    }

    private static Bitmap Capture(int x, int y)
    {
        var image = new Bitmap(ClickFeedback.WindowSize, ClickFeedback.WindowSize);
        using Graphics graphics = Graphics.FromImage(image);
        graphics.CopyFromScreen(x - image.Width / 2, y - image.Height / 2, 0, 0, image.Size,
            CopyPixelOperation.SourceCopy | CopyPixelOperation.CaptureBlt);
        return image;
    }

    private static bool Similar(Color a, Color b) =>
        Math.Abs(a.R - b.R) < 10 && Math.Abs(a.G - b.G) < 10 && Math.Abs(a.B - b.B) < 10;

    private static int ChangedPixels(Bitmap before, Bitmap after)
    {
        int changed = 0;
        for (int y = 0; y < before.Height; y++)
            for (int x = 0; x < before.Width; x++)
                if (!Similar(before.GetPixel(x, y), after.GetPixel(x, y)))
                    changed++;
        return changed;
    }
}
