/*
ScreenForge
Copyright (C) 2026 Emmanuel Suah

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.
*/

using System;

namespace ScreenForge.Core;

/// <summary>
/// The arithmetic behind keeping a soundtrack continuous.
///
/// WASAPI loopback only hands over packets while the playback device is actually
/// playing something. A desktop that goes quiet — which is most of a screen
/// recording, and all of a tutorial with the sound off — stops producing data,
/// the WAV stops growing at the last noise it heard, and anything downstream that
/// assumes the sound is as long as the picture ends up cutting the picture to the
/// length of that noise. That is why this exists: given a device's format, how
/// many bytes of silence mean "fifty milliseconds".
/// </summary>
public static class Silence
{
    /// <summary>
    /// How often a gap is filled. Short enough that the track stays in step with
    /// the picture, long enough that writing it costs nothing.
    /// </summary>
    public const int ChunkMilliseconds = 50;

    /// <summary>A ceiling, so a nonsense device format cannot ask for a huge buffer.</summary>
    private const long Ceiling = 4L * 1024 * 1024;

    /// <summary>
    /// Bytes of silence for a gap, rounded down to whole samples (a partial
    /// sample is not silence, it is a click).
    /// </summary>
    public static int BytesFor(int averageBytesPerSecond, int blockAlign, int milliseconds)
    {
        if (averageBytesPerSecond <= 0 || milliseconds <= 0)
            return 0;

        int align = Math.Max(1, blockAlign);
        long bytes = (long)averageBytesPerSecond * milliseconds / 1000;
        long aligned = bytes / align * align;

        if (aligned <= 0)
            aligned = align;

        return (int)Math.Min(aligned, Ceiling);
    }

    /// <summary>One tick's worth of silence for a device in this format.</summary>
    public static int ChunkBytes(int averageBytesPerSecond, int blockAlign) =>
        BytesFor(averageBytesPerSecond, blockAlign, ChunkMilliseconds);
}
