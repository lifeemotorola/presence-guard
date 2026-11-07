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
using System.IO;

namespace ScreenForge.Core;

/// <summary>
/// Finds ffmpeg. It ships next to the app, so that is where it is looked for
/// first — a copy on PATH could be any version, and the recorder depends on
/// specifics (fragmented MP4, gdigrab options, encoder names).
/// </summary>
public static class FfmpegLocator
{
    public const string ExecutableName = "ffmpeg.exe";

    /// <summary>Every place worth looking, in order.</summary>
    public static List<string> Candidates(string appFolder, string appDataFolder, string pathVariable)
    {
        var candidates = new List<string>();

        if (!string.IsNullOrWhiteSpace(appFolder))
        {
            candidates.Add(Path.Combine(appFolder, ExecutableName));
            candidates.Add(Path.Combine(appFolder, "ffmpeg", ExecutableName));
            candidates.Add(Path.Combine(appFolder, "bin", ExecutableName));
        }

        if (!string.IsNullOrWhiteSpace(appDataFolder))
        {
            candidates.Add(Path.Combine(appDataFolder, "ffmpeg", ExecutableName));
            candidates.Add(Path.Combine(appDataFolder, "ffmpeg", "bin", ExecutableName));
        }

        if (!string.IsNullOrWhiteSpace(pathVariable))
        {
            foreach (string folder in pathVariable.Split(Path.PathSeparator))
            {
                if (string.IsNullOrWhiteSpace(folder))
                    continue;

                string candidate = Path.Combine(folder.Trim().Trim('"'), ExecutableName);
                if (!candidates.Contains(candidate))
                    candidates.Add(candidate);
            }
        }

        return candidates;
    }

    /// <summary>The first candidate that exists, or null.</summary>
    public static string Find(string appFolder, string appDataFolder, string pathVariable)
    {
        foreach (string candidate in Candidates(appFolder, appDataFolder, pathVariable))
        {
            try
            {
                if (File.Exists(candidate))
                    return candidate;
            }
            catch (Exception)
            {
                /* An unreadable PATH entry is not worth stopping for. */
            }
        }

        return null;
    }
}
