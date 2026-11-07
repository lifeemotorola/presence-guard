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
using System.Diagnostics;
using System.Text;
using System.Threading;
using System.Threading.Tasks;

namespace ScreenForge.Recording;

public sealed class ProcessResult
{
    public int ExitCode { get; set; }
    public bool TimedOut { get; set; }
    public string Output { get; set; } = "";

    public bool Ok => !TimedOut && ExitCode == 0;
}

/// <summary>Runs ffmpeg to completion and hands back what it said.</summary>
public static class FfmpegProcess
{
    public static ProcessStartInfo StartInfo(string ffmpegPath, IEnumerable<string> args)
    {
        var info = new ProcessStartInfo(ffmpegPath)
        {
            UseShellExecute = false,
            CreateNoWindow = true,
            RedirectStandardInput = true,
            RedirectStandardOutput = true,
            RedirectStandardError = true,
            StandardErrorEncoding = Encoding.UTF8,
            StandardOutputEncoding = Encoding.UTF8,
        };

        foreach (string arg in args)
            info.ArgumentList.Add(arg);

        return info;
    }

    public static async Task<ProcessResult> RunAsync(
        string ffmpegPath,
        IEnumerable<string> args,
        int timeoutMs,
        CancellationToken cancellationToken = default)
    {
        var result = new ProcessResult();
        var output = new StringBuilder();

        using var process = new Process { StartInfo = StartInfo(ffmpegPath, args) };

        process.ErrorDataReceived += (_, e) =>
        {
            if (e.Data == null)
                return;
            lock (output)
                output.AppendLine(e.Data);
        };

        process.OutputDataReceived += (_, e) =>
        {
            if (e.Data == null)
                return;
            lock (output)
                output.AppendLine(e.Data);
        };

        process.Start();
        process.BeginErrorReadLine();
        process.BeginOutputReadLine();

        using var timeout = new CancellationTokenSource(timeoutMs);
        using var linked = CancellationTokenSource.CreateLinkedTokenSource(timeout.Token, cancellationToken);

        try
        {
            await process.WaitForExitAsync(linked.Token).ConfigureAwait(false);
            result.ExitCode = process.ExitCode;
        }
        catch (OperationCanceledException)
        {
            result.TimedOut = true;
            try
            {
                if (!process.HasExited)
                    process.Kill(true);
            }
            catch (Exception)
            {
                /* already gone */
            }
        }

        lock (output)
            result.Output = output.ToString();

        return result;
    }
}
