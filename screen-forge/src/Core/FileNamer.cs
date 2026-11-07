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
using System.IO;
using System.Text;

namespace ScreenForge.Core;

/// <summary>
/// Turns "Recording {date} {time}" into a file name, and refuses to produce
/// anything Windows cannot save.
/// </summary>
public static class FileNamer
{
    public const string DefaultPattern = "Recording {date} {time}";

    private static readonly HashSet<char> Illegal = new(Path.GetInvalidFileNameChars())
    {
        '<', '>', ':', '"', '/', '\\', '|', '?', '*',
    };

    public static readonly string[] Tokens = { "{date}", "{time}", "{source}", "{quality}", "{fps}" };

    public static string Expand(
        string pattern,
        DateTime when,
        string source,
        string quality,
        int fps)
    {
        string expanded = string.IsNullOrWhiteSpace(pattern) ? DefaultPattern : pattern;

        expanded = expanded
            .Replace("{date}", when.ToString("yyyy-MM-dd"))
            .Replace("{time}", when.ToString("HH-mm-ss"))
            .Replace("{source}", source ?? "")
            .Replace("{quality}", quality ?? "")
            .Replace("{fps}", fps.ToString());

        return Sanitise(expanded);
    }

    public static string Sanitise(string name)
    {
        if (string.IsNullOrWhiteSpace(name))
            return "Recording";

        var builder = new StringBuilder(name.Length);
        foreach (char c in name.Trim())
        {
            if (Illegal.Contains(c) || char.IsControl(c))
                builder.Append('-');
            else
                builder.Append(c);
        }

        string cleaned = builder.ToString();

        /* Collapse the runs of dashes that a stripped token leaves behind. */
        while (cleaned.Contains("--", StringComparison.Ordinal))
            cleaned = cleaned.Replace("--", "-", StringComparison.Ordinal);

        cleaned = cleaned.Trim(' ', '.', '-', '_');

        if (cleaned.Length > 120)
            cleaned = cleaned.Substring(0, 120).TrimEnd(' ', '.', '-', '_');

        return string.IsNullOrWhiteSpace(cleaned) ? "Recording" : cleaned;
    }

    /// <summary>
    /// A name that does not collide with anything already in the folder:
    /// "Recording 2026-01-01" then "Recording 2026-01-01 (2)".
    /// </summary>
    public static string Unique(string folder, string baseName, string extension = ".mp4")
    {
        if (!extension.StartsWith('.'))
            extension = "." + extension;

        string candidate = Path.Combine(folder, baseName + extension);
        int counter = 2;

        while (File.Exists(candidate))
        {
            candidate = Path.Combine(folder, $"{baseName} ({counter}){extension}");
            counter++;

            if (counter > 9999)
                throw new IOException("cannot find a free file name in " + folder);
        }

        return candidate;
    }
}
