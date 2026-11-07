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
using System.Globalization;
using System.IO;
using System.Text;

namespace ScreenForge.Core;

public sealed class VideoCaptureRequest
{
    public CaptureRect Rect { get; set; }
    public int Fps { get; set; } = 60;
    public QualityPreset Quality { get; set; } = QualityPreset.High;
    public EncoderKind Encoder { get; set; } = EncoderKind.X264;
    public bool CaptureCursor { get; set; } = true;
    public bool UseDesktopDuplication { get; set; }
    public int MonitorIndex { get; set; }
    public string OutputPath { get; set; } = "";
}

public sealed class MuxRequest
{
    public string VideoPath { get; set; } = "";
    public string SystemAudioPath { get; set; }
    public string MicrophonePath { get; set; }
    public int SystemGainPercent { get; set; } = 100;
    public int MicrophoneGainPercent { get; set; } = 100;
    public int AudioBitrateKbps { get; set; } = 192;
    public string OutputPath { get; set; } = "";
}

/// <summary>
/// The command lines, and nothing else.
///
/// This is the file that decides whether a recording is watchable, so it has no
/// side effects at all: it takes a request and returns arguments, which makes it
/// the one part of a recorder that can be tested without a screen.
/// </summary>
public static class FfmpegArgs
{
    public const string EncoderNvenc = "h264_nvenc";
    public const string EncoderAmf = "h264_amf";
    public const string EncoderQsv = "h264_qsv";
    public const string EncoderX264 = "libx264";

    /// <summary>Order in which hardware encoders are preferred.</summary>
    public static readonly string[] HardwarePreference = { EncoderNvenc, EncoderAmf, EncoderQsv };

    public static string NameFor(EncoderKind kind) => kind switch
    {
        EncoderKind.Nvenc => EncoderNvenc,
        EncoderKind.Amf => EncoderAmf,
        EncoderKind.Qsv => EncoderQsv,
        _ => EncoderX264,
    };

    /// <summary>
    /// The input half of a recording: where the pixels come from.
    ///
    /// Desktop duplication is offered for a full monitor only — it copies the
    /// composited desktop in the GPU, which is what makes 4K60 possible at all,
    /// but it cannot be pointed at a rectangle.
    /// </summary>
    public static List<string> Input(VideoCaptureRequest request)
    {
        if (request == null)
            throw new ArgumentNullException(nameof(request));

        CaptureRect rect = Presets.Normalise(request.Rect);
        if (rect.Width <= 2 || rect.Height <= 2)
            throw new ArgumentException("the capture area is too small", nameof(request));

        int fps = Presets.ClampFps(request.Fps);
        var args = new List<string>();

        if (request.UseDesktopDuplication)
        {
            args.Add("-f");
            args.Add("lavfi");
            args.Add("-i");
            args.Add(string.Format(
                CultureInfo.InvariantCulture,
                "ddagrab=output_idx={0}:framerate={1}:draw_mouse={2}",
                Math.Max(0, request.MonitorIndex),
                fps,
                request.CaptureCursor ? 1 : 0));

            /* Desktop duplication hands over GPU frames; this brings them back
             * to something every encoder accepts. */
            args.Add("-vf");
            args.Add("hwdownload,format=bgra");
        }
        else
        {
            args.Add("-f");
            args.Add("gdigrab");
            args.Add("-framerate");
            args.Add(fps.ToString(CultureInfo.InvariantCulture));
            args.Add("-draw_mouse");
            args.Add(request.CaptureCursor ? "1" : "0");
            args.Add("-offset_x");
            args.Add(rect.X.ToString(CultureInfo.InvariantCulture));
            args.Add("-offset_y");
            args.Add(rect.Y.ToString(CultureInfo.InvariantCulture));
            args.Add("-video_size");
            args.Add(rect.Width.ToString(CultureInfo.InvariantCulture) + "x" + rect.Height.ToString(CultureInfo.InvariantCulture));
            /* The desktop grabs buffers as fast as it can; this is the
             * difference between a smooth capture and dropped frames on a busy
             * machine. */
            args.Add("-rtbufsize");
            args.Add("256M");
            args.Add("-i");
            args.Add("desktop");
        }

        return args;
    }

    /// <summary>The encoder half: hardware where possible, x264 where not.</summary>
    public static List<string> Encoder(VideoCaptureRequest request)
    {
        CaptureRect rect = Presets.Normalise(request.Rect);
        int fps = Presets.ClampFps(request.Fps);
        int bitrate = Presets.BitrateKbps(rect.Width, rect.Height, fps, request.Quality);
        int keyframes = Math.Max(1, fps * 2);

        var args = new List<string>();

        switch (request.Encoder)
        {
            case EncoderKind.Nvenc:
                args.AddRange(new[]
                {
                    "-c:v", EncoderNvenc,
                    "-preset", "p5",
                    "-tune", "hq",
                    "-rc", "vbr",
                    "-b:v", bitrate.ToString(CultureInfo.InvariantCulture) + "k",
                    "-maxrate", (bitrate * 3 / 2).ToString(CultureInfo.InvariantCulture) + "k",
                    "-bufsize", (bitrate * 2).ToString(CultureInfo.InvariantCulture) + "k",
                    "-pix_fmt", "yuv420p",
                    "-g", keyframes.ToString(CultureInfo.InvariantCulture),
                });
                break;

            case EncoderKind.Amf:
                args.AddRange(new[]
                {
                    "-c:v", EncoderAmf,
                    "-quality", "balanced",
                    "-rc", "vbr_peak",
                    "-b:v", bitrate.ToString(CultureInfo.InvariantCulture) + "k",
                    "-maxrate", (bitrate * 3 / 2).ToString(CultureInfo.InvariantCulture) + "k",
                    "-bufsize", (bitrate * 2).ToString(CultureInfo.InvariantCulture) + "k",
                    "-pix_fmt", "yuv420p",
                    "-g", keyframes.ToString(CultureInfo.InvariantCulture),
                });
                break;

            case EncoderKind.Qsv:
                args.AddRange(new[]
                {
                    "-c:v", EncoderQsv,
                    "-preset", "fast",
                    "-b:v", bitrate.ToString(CultureInfo.InvariantCulture) + "k",
                    "-maxrate", (bitrate * 3 / 2).ToString(CultureInfo.InvariantCulture) + "k",
                    "-bufsize", (bitrate * 2).ToString(CultureInfo.InvariantCulture) + "k",
                    "-pix_fmt", "nv12",
                    "-g", keyframes.ToString(CultureInfo.InvariantCulture),
                });
                break;

            default:
                /* Software: constant quality rather than a bitrate, because that
                 * is what x264 is good at, and it keeps text crisp at whatever
                 * the scene happens to be. */
                args.AddRange(new[]
                {
                    "-c:v", EncoderX264,
                    "-preset", "veryfast",
                    "-crf", Presets.Crf(request.Quality).ToString(CultureInfo.InvariantCulture),
                    "-pix_fmt", "yuv420p",
                    "-g", keyframes.ToString(CultureInfo.InvariantCulture),
                });
                break;
        }

        return args;
    }

    /// <summary>A complete recording command for one take.</summary>
    public static List<string> RecordVideo(VideoCaptureRequest request)
    {
        if (string.IsNullOrWhiteSpace(request.OutputPath))
            throw new ArgumentException("a recording needs somewhere to go", nameof(request));

        var args = new List<string>
        {
            "-hide_banner",
            "-y",
            /* Quiet enough to keep the log readable, loud enough to diagnose. */
            "-loglevel", "warning",
            "-nostats",
        };

        args.AddRange(Input(request));
        args.AddRange(Encoder(request));

        /* No audio in this file: sound is recorded separately and mixed in at
         * the end, because a dropped audio device must not take the picture
         * with it. */
        args.Add("-an");

        /* Fragmented MP4 can be recovered if the process is killed; the finished
         * file is remuxed at the end, so the user never sees this container. */
        args.Add("-movflags");
        args.Add("+frag_keyframe+empty_moov+default_base_moof");
        args.Add(request.OutputPath);

        return args;
    }

    /// <summary>Mixing and remuxing: the step that turns takes into a real MP4.</summary>
    public static List<string> Mux(MuxRequest request)
    {
        if (request == null)
            throw new ArgumentNullException(nameof(request));
        if (string.IsNullOrWhiteSpace(request.VideoPath))
            throw new ArgumentException("no video to mux", nameof(request));
        if (string.IsNullOrWhiteSpace(request.OutputPath))
            throw new ArgumentException("a mux needs a destination", nameof(request));

        var args = new List<string> { "-hide_banner", "-y", "-loglevel", "warning", "-nostats" };

        var audioInputs = new List<(string Path, int Gain)>();
        if (!string.IsNullOrWhiteSpace(request.SystemAudioPath))
            audioInputs.Add((request.SystemAudioPath, request.SystemGainPercent));
        if (!string.IsNullOrWhiteSpace(request.MicrophonePath))
            audioInputs.Add((request.MicrophonePath, request.MicrophoneGainPercent));

        args.Add("-i");
        args.Add(request.VideoPath);

        foreach (var input in audioInputs)
        {
            args.Add("-i");
            args.Add(input.Path);
        }

        int bitrate = Math.Clamp(request.AudioBitrateKbps, 64, 512);

        if (audioInputs.Count == 0)
        {
            /* Nothing to mix: copy the picture and be done. */
            args.AddRange(new[] { "-map", "0:v:0", "-c:v", "copy", "-an" });
        }
        else if (audioInputs.Count == 1)
        {
            args.AddRange(new[]
            {
                "-map", "0:v:0",
                "-map", "1:a:0",
                "-c:v", "copy",
                "-af", Gain(audioInputs[0].Gain),
                "-c:a", "aac",
                "-b:a", bitrate.ToString(CultureInfo.InvariantCulture) + "k",
                "-ar", "48000",
                /* The microphone is stopped a moment after the picture: cut the
                 * sound at the last frame instead of leaving a frozen image with
                 * half a second of silence behind it. */
                "-shortest",
            });
        }
        else
        {
            /* Two sources, two faders, one soundtrack. normalize=0 keeps the
             * mix from quietly halving everything. */
            var filter = new StringBuilder();
            filter.Append("[1:a]").Append(Gain(audioInputs[0].Gain)).Append("[a1];");
            filter.Append("[2:a]").Append(Gain(audioInputs[1].Gain)).Append("[a2];");
            filter.Append("[a1][a2]amix=inputs=2:duration=longest:normalize=0[aout]");

            args.AddRange(new[]
            {
                "-filter_complex", filter.ToString(),
                "-map", "0:v:0",
                "-map", "[aout]",
                "-c:v", "copy",
                "-c:a", "aac",
                "-b:a", bitrate.ToString(CultureInfo.InvariantCulture) + "k",
                "-ar", "48000",
                "-shortest",
            });
        }

        args.Add("-movflags");
        args.Add("+faststart");
        args.Add(request.OutputPath);

        return args;
    }

    /// <summary>Joining the takes back together after a pause.</summary>
    public static List<string> Concat(string listFile, string outputPath)
    {
        if (string.IsNullOrWhiteSpace(listFile))
            throw new ArgumentException("a concat needs a list", nameof(listFile));
        if (string.IsNullOrWhiteSpace(outputPath))
            throw new ArgumentException("a concat needs a destination", nameof(outputPath));

        return new List<string>
        {
            "-hide_banner",
            "-y",
            "-loglevel", "warning",
            "-nostats",
            "-f", "concat",
            "-safe", "0",
            "-i", listFile,
            "-c", "copy",
            "-movflags", "+faststart",
            outputPath,
        };
    }

    /// <summary>One line of a concat list. Single quotes are the only escaping the demuxer needs.</summary>
    public static string ConcatEntry(string path)
    {
        string escaped = (path ?? "").Replace("\\", "/").Replace("'", @"'\''");
        return $"file '{escaped}'";
    }

    public static string ConcatList(IEnumerable<string> paths)
    {
        var builder = new StringBuilder();
        foreach (string path in paths)
            builder.AppendLine(ConcatEntry(path));
        return builder.ToString();
    }

    public static List<string> ProbeEncoders() => new()
    {
        "-hide_banner",
        "-encoders",
    };

    /// <summary>
    /// Actually *uses* an encoder rather than trusting the list: a build can
    /// advertise h264_nvenc on a machine with no NVIDIA driver, and the failure
    /// belongs at startup, not ten minutes into a recording.
    /// </summary>
    public static List<string> TestEncoder(string encoderName) => new()
    {
        "-hide_banner",
        "-loglevel", "error",
        "-f", "lavfi",
        "-i", "color=c=black:s=320x240:r=30:d=0.2",
        "-frames:v", "3",
        "-c:v", encoderName,
        "-f", "null",
        "-",
    };

    private static string Gain(int percent)
    {
        int clamped = Math.Clamp(percent, 0, 200);
        double gain = clamped / 100.0;
        return "volume=" + gain.ToString("0.00", CultureInfo.InvariantCulture);
    }

    /// <summary>A single string for the log, with quoting that can be pasted into a shell.</summary>
    public static string ToCommandLine(string executable, IEnumerable<string> args)
    {
        var builder = new StringBuilder(executable);
        foreach (string arg in args)
        {
            builder.Append(' ');
            if (arg.Length == 0 || arg.IndexOfAny(new[] { ' ', '"', '\t' }) >= 0)
                builder.Append('"').Append(arg.Replace("\"", "\\\"")).Append('"');
            else
                builder.Append(arg);
        }
        return builder.ToString();
    }

    /// <summary>The log file a recording's ffmpeg output goes into, next to the video.</summary>
    public static string SidecarLogPath(string outputPath) => outputPath + ".log";

    public static bool LooksLikeVideo(string path) =>
        !string.IsNullOrWhiteSpace(path) &&
        (path.EndsWith(".mp4", StringComparison.OrdinalIgnoreCase) ||
         path.EndsWith(".mkv", StringComparison.OrdinalIgnoreCase) ||
         path.EndsWith(".mov", StringComparison.OrdinalIgnoreCase));
}
