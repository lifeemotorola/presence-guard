/*
ScreenForge
Copyright (C) 2026 Emmanuel Suah

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.
*/

using System;
using System.Windows.Forms;
using ScreenForge.Ui;

namespace ScreenForge;

internal static class Program
{
    [STAThread]
    private static void Main()
    {
        /* Per-monitor DPI: on a laptop with a 4K internal panel and a 1080p
         * external monitor, anything else gives either a blurry UI or the wrong
         * capture rectangle. */
        try
        {
            Application.SetHighDpiMode(HighDpiMode.PerMonitorV2);
        }
        catch (Exception)
        {
            /* older Windows builds ignore the request; the manifest still asks */
        }

        Application.EnableVisualStyles();
        Application.SetCompatibleTextRenderingDefault(false);

        Application.ThreadException += (_, args) => Report(args.Exception);
        AppDomain.CurrentDomain.UnhandledException += (_, args) => Report(args.ExceptionObject as Exception);

        Application.Run(new MainForm());
    }

    private static void Report(Exception exception)
    {
        if (exception == null)
            return;

        try
        {
            MessageBox.Show(
                "ScreenForge hit an unexpected problem:\n\n" + exception.Message +
                "\n\nA recording in progress is left as a fragmented MP4 in %TEMP%\\screenforge-* and can still be recovered.",
                "ScreenForge",
                MessageBoxButtons.OK,
                MessageBoxIcon.Error);
        }
        catch (Exception)
        {
            /* nothing sensible left to do */
        }
    }
}
