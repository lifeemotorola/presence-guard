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
using System.Threading.Tasks;
using ScreenForge.Core;

namespace ScreenForge.Recording;

/// <summary>
/// Picks the encoder this machine can actually use, by asking it to encode three
/// frames rather than by reading a list.
///
/// The answer is cached: probing takes a few hundred milliseconds per candidate
/// and nothing about it changes while the app is open.
/// </summary>
public static class EncoderSelector
{
    private static readonly Dictionary<EncoderKind, string> Cache = new();
    private static readonly object Gate = new();

    public static void ClearCache()
    {
        lock (Gate)
            Cache.Clear();
    }

    public static async Task<string> SelectAsync(string ffmpegPath, EncoderKind preference, Action<string> log)
    {
        lock (Gate)
        {
            if (Cache.TryGetValue(preference, out string cached))
                return cached;
        }

        string chosen = FfmpegArgs.EncoderX264;

        ProcessResult listing = await FfmpegProcess
            .RunAsync(ffmpegPath, FfmpegArgs.ProbeEncoders(), 15000)
            .ConfigureAwait(false);

        HashSet<string> available = EncoderProbe.Parse(listing.Output);
        List<string> candidates = EncoderProbe.Candidates(preference, available);

        log?.Invoke($"encoders listed: {string.Join(", ", available)}");
        log?.Invoke($"trying: {string.Join(", ", candidates)}");

        foreach (string candidate in candidates)
        {
            ProcessResult test = await FfmpegProcess
                .RunAsync(ffmpegPath, FfmpegArgs.TestEncoder(candidate), 20000)
                .ConfigureAwait(false);

            if (test.Ok)
            {
                chosen = candidate;
                log?.Invoke($"{candidate} works: {EncoderProbe.Describe(candidate)}");
                break;
            }

            log?.Invoke($"{candidate} refused: {FirstLine(test.Output)}");
        }

        lock (Gate)
            Cache[preference] = chosen;

        return chosen;
    }

    private static string FirstLine(string text)
    {
        if (string.IsNullOrWhiteSpace(text))
            return "no output";

        foreach (string line in text.Split('\n'))
        {
            string trimmed = line.Trim();
            if (trimmed.Length > 0)
                return trimmed;
        }

        return "no output";
    }
}
