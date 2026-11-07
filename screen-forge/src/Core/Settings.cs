/*
ScreenForge
Copyright (C) 2026 Emmanuel Suah

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.
*/

using System;
using System.IO;
using System.Text.Json;

namespace ScreenForge.Core;

/// <summary>
/// Settings live in a plain JSON file next to the app's data, and every value
/// read back is clamped: the file is easy to edit by hand and a nonsense value
/// there must not become a broken recording.
/// </summary>
public static class SettingsStore
{
    private static readonly JsonSerializerOptions Options = new()
    {
        WriteIndented = true,
        PropertyNameCaseInsensitive = true,
    };

    public static string DefaultFolder(string videosFolder) =>
        string.IsNullOrWhiteSpace(videosFolder) ? Environment.CurrentDirectory : videosFolder;

    public static AppSettings CreateDefault(string videosFolder) => new()
    {
        OutputFolder = DefaultFolder(videosFolder),
        NamePattern = FileNamer.DefaultPattern,
        CountdownSeconds = 3,
        HideWhileRecording = true,
        Fps = 60,
        Quality = QualityPreset.High,
        Encoder = EncoderKind.Auto,
        CaptureCursor = true,
        UseDesktopDuplication = false,
        RecordSystem = true,
        RecordMicrophone = false,
        SystemGainPercent = 100,
        MicrophoneGainPercent = 100,
        SelectedMonitor = 0,
        SourceKind = CaptureKind.Monitor,
    };

    public static AppSettings Load(string path, string videosFolder)
    {
        AppSettings settings = CreateDefault(videosFolder);

        try
        {
            if (!File.Exists(path))
                return Normalise(settings);

            string json = File.ReadAllText(path);
            AppSettings loaded = JsonSerializer.Deserialize<AppSettings>(json, Options);
            if (loaded == null)
                return Normalise(settings);

            settings = loaded;
        }
        catch (Exception)
        {
            /* A corrupt settings file is not worth a dialog: the app starts with
             * sensible defaults and rewrites the file on exit. */
            settings = CreateDefault(videosFolder);
        }

        return Normalise(settings);
    }

    public static void Save(string path, AppSettings settings)
    {
        string folder = Path.GetDirectoryName(path);
        if (!string.IsNullOrEmpty(folder))
            Directory.CreateDirectory(folder);

        File.WriteAllText(path, JsonSerializer.Serialize(Normalise(settings), Options));
    }

    /// <summary>Every value the app relies on, made safe.</summary>
    public static AppSettings Normalise(AppSettings settings, string videosFolder = null)
    {
        settings ??= CreateDefault(videosFolder ?? Environment.CurrentDirectory);

        settings.Fps = Presets.ClampFps(settings.Fps <= 0 ? 60 : settings.Fps);

        if (!Enum.IsDefined(typeof(QualityPreset), settings.Quality))
            settings.Quality = QualityPreset.High;

        if (!Enum.IsDefined(typeof(EncoderKind), settings.Encoder))
            settings.Encoder = EncoderKind.Auto;

        if (!Enum.IsDefined(typeof(CaptureKind), settings.SourceKind))
            settings.SourceKind = CaptureKind.Monitor;

        settings.CountdownSeconds = Math.Clamp(settings.CountdownSeconds, 0, 10);
        settings.SystemGainPercent = Math.Clamp(settings.SystemGainPercent, 0, 200);
        settings.MicrophoneGainPercent = Math.Clamp(settings.MicrophoneGainPercent, 0, 200);
        settings.SelectedMonitor = Math.Max(0, settings.SelectedMonitor);

        settings.NamePattern = string.IsNullOrWhiteSpace(settings.NamePattern)
            ? FileNamer.DefaultPattern
            : settings.NamePattern;

        if (string.IsNullOrWhiteSpace(settings.OutputFolder))
            settings.OutputFolder = DefaultFolder(videosFolder);

        settings.MicrophoneDeviceId ??= "";

        return settings;
    }
}
