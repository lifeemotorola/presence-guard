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
using System.Linq;

namespace ScreenForge.Core;

/// <summary>
/// Which H.264 encoder this machine can actually use.
///
/// Two questions, deliberately kept apart: what the ffmpeg build *lists*, and
/// what it can *do*. A build lists h264_nvenc whether or not there is an NVIDIA
/// card in the machine, so the list is only a starting point — the real answer
/// comes from encoding three frames at startup.
/// </summary>
public static class EncoderProbe
{
    /// <summary>
    /// Reads the output of "ffmpeg -encoders". A line looks like:
    /// " V....D h264_nvenc           NVIDIA NVENC H.264 encoder (codec h264)".
    /// </summary>
    public static HashSet<string> Parse(string encoderListOutput)
    {
        var found = new HashSet<string>(StringComparer.OrdinalIgnoreCase);
        if (string.IsNullOrWhiteSpace(encoderListOutput))
            return found;

        foreach (string rawLine in encoderListOutput.Split('\n'))
        {
            string line = rawLine.Trim();
            if (line.Length < 8)
                continue;

            int split = line.IndexOf(' ');
            if (split <= 0)
                continue;

            string flags = line.Substring(0, split);
            if (flags.Length != 6 || !flags.All(c => "VASFXBD.".IndexOf(c) >= 0))
                continue;

            string rest = line.Substring(split).TrimStart();
            if (rest.Length == 0 || rest[0] == '=')
                continue;

            int end = rest.IndexOf(' ');
            string name = end > 0 ? rest.Substring(0, end) : rest;
            if (name.Length > 0)
                found.Add(name);
        }

        return found;
    }

    /// <summary>
    /// The encoders to try, best first. Hardware beats software for a recorder
    /// because it leaves the CPU to whatever is being recorded; software is
    /// always the last resort, and always present in the builds we ship.
    /// </summary>
    public static List<string> Candidates(EncoderKind preference, ISet<string> available)
    {
        var candidates = new List<string>();
        available ??= new HashSet<string>();

        if (preference == EncoderKind.X264)
        {
            candidates.Add(FfmpegArgs.EncoderX264);
            return candidates;
        }

        if (preference == EncoderKind.Auto)
        {
            foreach (string hardware in FfmpegArgs.HardwarePreference)
            {
                if (available.Contains(hardware) && !candidates.Contains(hardware))
                    candidates.Add(hardware);
            }
        }
        else
        {
            string wanted = FfmpegArgs.NameFor(preference);
            if (available.Contains(wanted))
                candidates.Add(wanted);
        }

        /* The software encoder is always worth trying: it needs no driver and
         * always works, only slower. */
        if (!candidates.Contains(FfmpegArgs.EncoderX264))
            candidates.Add(FfmpegArgs.EncoderX264);

        return candidates;
    }

    public static EncoderKind KindOf(string encoderName) => encoderName switch
    {
        FfmpegArgs.EncoderNvenc => EncoderKind.Nvenc,
        FfmpegArgs.EncoderAmf => EncoderKind.Amf,
        FfmpegArgs.EncoderQsv => EncoderKind.Qsv,
        _ => EncoderKind.X264,
    };

    public static string Describe(string encoderName) => encoderName switch
    {
        FfmpegArgs.EncoderNvenc => "NVIDIA GPU (NVENC)",
        FfmpegArgs.EncoderAmf => "AMD GPU (AMF)",
        FfmpegArgs.EncoderQsv => "Intel GPU (Quick Sync)",
        FfmpegArgs.EncoderX264 => "CPU (x264)",
        _ => encoderName ?? "unknown",
    };

    /// <summary>True when the encoder is doing the work on a GPU, so the CPU stays free.</summary>
    public static bool IsHardware(string encoderName) =>
        encoderName == FfmpegArgs.EncoderNvenc ||
        encoderName == FfmpegArgs.EncoderAmf ||
        encoderName == FfmpegArgs.EncoderQsv;
}
