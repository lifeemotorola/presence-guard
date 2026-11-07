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
using System.Diagnostics;
using System.IO;
using System.Threading;
using NAudio.CoreAudioApi;
using NAudio.Wave;

namespace ScreenForge.Audio;

public sealed class AudioDeviceInfo
{
    public string Id { get; set; } = "";
    public string Name { get; set; } = "";
    public override string ToString() => Name;
}

/// <summary>
/// Records one audio source to one WAV file.
///
/// Windows has no ffmpeg-native system audio capture: a loopback device has to
/// be opened through WASAPI, which is what this does. The result is a plain WAV
/// that ffmpeg mixes with the other track and the video at the end, so a failure
/// here costs the soundtrack, not the recording.
/// </summary>
public sealed class AudioTrack : IDisposable
{
    private readonly IWaveIn capture;
    private readonly WaveFileWriter writer;
    private readonly object gate = new();
    private readonly ManualResetEventSlim finished = new(false);
    private bool disposed;

    private AudioTrack(IWaveIn source, string path)
    {
        capture = source;
        Path = path;

        /* The writer follows whatever the device gives us — usually 48 kHz
         * float — and ffmpeg resamples at mux time if a second device disagrees. */
        writer = new WaveFileWriter(path, capture.WaveFormat);

        capture.DataAvailable += OnDataAvailable;
        capture.RecordingStopped += OnRecordingStopped;
    }

    public string Path { get; }

    public Exception Error { get; private set; }

    public long BytesWritten { get; private set; }

    public static AudioTrack StartSystem(string path)
    {
        var capture = new WasapiLoopbackCapture();
        var track = new AudioTrack(capture, path);
        track.Start();
        return track;
    }

    public static AudioTrack StartMicrophone(string deviceId, string path)
    {
        IWaveIn capture;

        if (!string.IsNullOrWhiteSpace(deviceId))
        {
            using var enumerator = new MMDeviceEnumerator();
            capture = new WasapiCapture(enumerator.GetDevice(deviceId));
        }
        else
        {
            capture = new WasapiCapture();
        }

        var track = new AudioTrack(capture, path);
        track.Start();
        return track;
    }

    private void Start() => capture.StartRecording();

    private void OnDataAvailable(object sender, WaveInEventArgs e)
    {
        if (e.BytesRecorded <= 0)
            return;

        lock (gate)
        {
            try
            {
                writer.Write(e.Buffer, 0, e.BytesRecorded);
                BytesWritten += e.BytesRecorded;
            }
            catch (Exception ex)
            {
                Error = ex;
            }
        }
    }

    private void OnRecordingStopped(object sender, StoppedEventArgs e)
    {
        if (e.Exception != null)
            Error ??= e.Exception;

        finished.Set();
    }

    public void Stop()
    {
        try
        {
            capture.StopRecording();
        }
        catch (Exception ex)
        {
            Error ??= ex;
        }

        /* A device that never reports back must not hang the stop button. */
        finished.Wait(TimeSpan.FromSeconds(3));
        CloseWriter();
    }

    private void CloseWriter()
    {
        lock (gate)
        {
            try
            {
                writer.Dispose();
            }
            catch (Exception ex)
            {
                Error ??= ex;
            }
        }
    }

    public void Dispose()
    {
        if (disposed)
            return;

        disposed = true;

        try
        {
            capture.DataAvailable -= OnDataAvailable;
            capture.RecordingStopped -= OnRecordingStopped;
            capture.Dispose();
        }
        catch (Exception)
        {
            /* disposing a dead device throws; there is nothing to do about it */
        }

        CloseWriter();
        finished.Dispose();
    }
}

public static class AudioDevices
{
    /// <summary>Microphones the user can pick, newest default first.</summary>
    public static List<AudioDeviceInfo> Microphones()
    {
        var devices = new List<AudioDeviceInfo>();

        try
        {
            using var enumerator = new MMDeviceEnumerator();
            foreach (MMDevice device in enumerator.EnumerateAudioEndPoints(DataFlow.Capture, DeviceState.Active))
            {
                try
                {
                    devices.Add(new AudioDeviceInfo { Id = device.ID, Name = device.FriendlyName });
                }
                finally
                {
                    device.Dispose();
                }
            }
        }
        catch (Exception)
        {
            /* No microphone, no driver, or no permission: the list is simply empty. */
        }

        return devices;
    }

    public static bool HasSystemOutput()
    {
        try
        {
            using var enumerator = new MMDeviceEnumerator();
            return enumerator.HasDefaultAudioEndpoint(DataFlow.Render, Role.Multimedia);
        }
        catch (Exception)
        {
            return false;
        }
    }
}
