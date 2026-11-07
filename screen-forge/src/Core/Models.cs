/*
ScreenForge
Copyright (C) 2026 Emmanuel Suah

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.
*/

namespace ScreenForge.Core;

public enum CaptureKind
{
    Monitor = 0,
    Region = 1,
    Window = 2,
}

public enum QualityPreset
{
    Low = 0,
    Medium = 1,
    High = 2,
    Maximum = 3,
}

public enum EncoderKind
{
    Auto = 0,
    Nvenc = 1,
    Amf = 2,
    Qsv = 3,
    X264 = 4,
}

/// <summary>A rectangle in virtual desktop pixels.</summary>
public readonly struct CaptureRect
{
    public CaptureRect(int x, int y, int width, int height)
    {
        X = x;
        Y = y;
        Width = width;
        Height = height;
    }

    public int X { get; }
    public int Y { get; }
    public int Width { get; }
    public int Height { get; }

    public int Area => Width * Height;

    public override string ToString() => $"{Width}x{Height} at {X},{Y}";
}

/// <summary>Where the picture comes from.</summary>
public sealed class CaptureTarget
{
    public CaptureKind Kind { get; set; } = CaptureKind.Monitor;
    public CaptureRect Rect { get; set; } = new CaptureRect(0, 0, 1920, 1080);
    public string Description { get; set; } = "Display";

    public CaptureTarget Clone() => new CaptureTarget
    {
        Kind = Kind,
        Rect = Rect,
        Description = Description,
    };
}

public sealed class VideoSettings
{
    public int Fps { get; set; } = 60;
    public QualityPreset Quality { get; set; } = QualityPreset.High;
    public EncoderKind Encoder { get; set; } = EncoderKind.Auto;
    public bool CaptureCursor { get; set; } = true;
    public bool UseDesktopDuplication { get; set; }
}

public sealed class AudioSettings
{
    public bool RecordSystem { get; set; } = true;
    public bool RecordMicrophone { get; set; }
    public string MicrophoneDeviceId { get; set; } = "";
    public int SystemGainPercent { get; set; } = 100;
    public int MicrophoneGainPercent { get; set; } = 100;

    public bool AnyEnabled => RecordSystem || RecordMicrophone;
}

public sealed class OutputSettings
{
    public string Folder { get; set; } = "";
    public string NamePattern { get; set; } = "Recording {date} {time}";
    public int CountdownSeconds { get; set; } = 3;
    public bool HideWhileRecording { get; set; } = true;
}

public sealed class AppSettings
{
    public string OutputFolder { get; set; } = "";
    public string NamePattern { get; set; } = "Recording {date} {time}";
    public int CountdownSeconds { get; set; } = 3;
    public bool HideWhileRecording { get; set; } = true;
    public int Fps { get; set; } = 60;
    public QualityPreset Quality { get; set; } = QualityPreset.High;
    public EncoderKind Encoder { get; set; } = EncoderKind.Auto;
    public bool CaptureCursor { get; set; } = true;
    public bool UseDesktopDuplication { get; set; }
    public bool RecordSystem { get; set; } = true;
    public bool RecordMicrophone { get; set; }
    public string MicrophoneDeviceId { get; set; } = "";
    public int SystemGainPercent { get; set; } = 100;
    public int MicrophoneGainPercent { get; set; } = 100;
    public int SelectedMonitor { get; set; }
    public CaptureKind SourceKind { get; set; } = CaptureKind.Monitor;

    public OutputSettings ToOutputSettings() => new OutputSettings
    {
        Folder = OutputFolder,
        NamePattern = NamePattern,
        CountdownSeconds = CountdownSeconds,
        HideWhileRecording = HideWhileRecording,
    };

    public AudioSettings ToAudioSettings() => new AudioSettings
    {
        RecordSystem = RecordSystem,
        RecordMicrophone = RecordMicrophone,
        MicrophoneDeviceId = MicrophoneDeviceId,
        SystemGainPercent = SystemGainPercent,
        MicrophoneGainPercent = MicrophoneGainPercent,
    };

    public VideoSettings ToVideoSettings() => new VideoSettings
    {
        Fps = Fps,
        Quality = Quality,
        Encoder = Encoder,
        CaptureCursor = CaptureCursor,
        UseDesktopDuplication = UseDesktopDuplication,
    };
}

public sealed class RecordingResult
{
    public bool Success { get; set; }
    public string Path { get; set; } = "";
    public string Message { get; set; } = "";
    public long Bytes { get; set; }
    public TimeSpan Duration { get; set; }
    public int Takes { get; set; }
    public string LogPath { get; set; } = "";
}
