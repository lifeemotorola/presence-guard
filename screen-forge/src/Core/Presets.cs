/*
ScreenForge
Copyright (C) 2026 Presence Guard contributors

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.
*/

using System;

namespace ScreenForge.Core;

/// <summary>
/// The numbers behind "Low / Medium / High / Maximum".
///
/// Bitrate is derived from the pixels actually being captured and the frame rate
/// rather than being a fixed number per preset: 12 Mbit/s is generous for 1080p60
/// and an insult to a 4K desktop, and the user should not have to know that.
/// </summary>
public static class Presets
{
    public const int ReferenceWidth = 1920;
    public const int ReferenceHeight = 1080;
    public const int ReferenceFps = 30;

    public static readonly int[] SupportedFps = { 15, 24, 30, 48, 60, 120, 144 };

    /// <summary>Video bitrate in kbit/s for this resolution, frame rate and preset.</summary>
    public static int BitrateKbps(int width, int height, int fps, QualityPreset quality)
    {
        if (width <= 0 || height <= 0)
            throw new ArgumentOutOfRangeException(nameof(width), "the capture area has no size");

        int safeFps = fps <= 0 ? ReferenceFps : fps;
        double baseKbps = quality switch
        {
            QualityPreset.Low => 3000,
            QualityPreset.Medium => 6000,
            QualityPreset.High => 12000,
            QualityPreset.Maximum => 24000,
            _ => 6000,
        };

        double pixelScale = (double)width * height / ((double)ReferenceWidth * ReferenceHeight);
        double fpsScale = (double)safeFps / ReferenceFps;

        /* Square-rooted frame rate: doubling the frames does not double what the
         * eye needs, and a linear rule makes 120 fps files absurd. */
        double kbps = baseKbps * pixelScale * Math.Sqrt(fpsScale);

        /* Never go below something watchable, never above what a desktop disk
         * will happily swallow for an hour. */
        return (int)Math.Round(Math.Clamp(kbps, 1200, 80000));
    }

    /// <summary>Quality for the software encoder (lower is better and bigger).</summary>
    public static int Crf(QualityPreset quality) => quality switch
    {
        QualityPreset.Low => 28,
        QualityPreset.Medium => 24,
        QualityPreset.High => 20,
        QualityPreset.Maximum => 16,
        _ => 22,
    };

    /// <summary>Constant quality value for NVIDIA's encoder (lower is better).</summary>
    public static int NvencCq(QualityPreset quality) => quality switch
    {
        QualityPreset.Low => 32,
        QualityPreset.Medium => 27,
        QualityPreset.High => 23,
        QualityPreset.Maximum => 19,
        _ => 25,
    };

    public static string Describe(QualityPreset quality) => quality switch
    {
        QualityPreset.Low => "Low — small files, good for chat and tutorials",
        QualityPreset.Medium => "Medium — the everyday setting",
        QualityPreset.High => "High — sharp text, bigger files",
        QualityPreset.Maximum => "Maximum — closest to the original, largest files",
        _ => "Medium",
    };

    /// <summary>
    /// H.264 in yuv420p needs even dimensions. Rounding *down* keeps the image
    /// inside the area the user picked instead of grabbing a column of pixels
    /// from outside it.
    /// </summary>
    public static int ToEven(int value)
    {
        if (value <= 2)
            return 2;
        return value % 2 == 0 ? value : value - 1;
    }

    public static CaptureRect Normalise(CaptureRect rect)
    {
        return new CaptureRect(rect.X, rect.Y, ToEven(rect.Width), ToEven(rect.Height));
    }

    public static bool IsSupportedFps(int fps) => Array.IndexOf(SupportedFps, fps) >= 0;

    /// <summary>Nearest supported frame rate, so a hand-edited settings file cannot break a recording.</summary>
    public static int ClampFps(int fps)
    {
        if (IsSupportedFps(fps))
            return fps;

        int best = SupportedFps[0];
        int bestDelta = int.MaxValue;
        foreach (int candidate in SupportedFps)
        {
            int delta = Math.Abs(candidate - fps);
            if (delta < bestDelta)
            {
                bestDelta = delta;
                best = candidate;
            }
        }
        return best;
    }

    /// <summary>
    /// Rough size of a recording, for the "will this fit?" check before starting.
    /// Video dominates; audio is estimated at 192 kbit/s stereo.
    /// </summary>
    public static long EstimatedBytes(int bitrateKbps, TimeSpan duration, bool audio)
    {
        double bitsPerSecond = bitrateKbps * 1000.0 + (audio ? 192000.0 : 0.0);
        return (long)(bitsPerSecond * Math.Max(0, duration.TotalSeconds) / 8.0);
    }
}
