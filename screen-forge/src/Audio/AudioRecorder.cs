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
using ScreenForge.Core;

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
///
/// One thing about loopback that is not obvious and used to cut recordings
/// short: a playback device that is not playing anything sends *nothing*, not
/// silence. A desktop that goes quiet — the normal state of a screen recording —
/// therefore leaves a WAV that ends at the last sound it heard, and a muxer that
/// trims the picture to its shortest stream then trims the whole recording to
/// that length. So this writes silence whenever the device has been quiet for a
/// tick, which keeps the track as long as the take and keeps later sound in its
/// right place instead of dragging it forward.
/// </summary>
public sealed class AudioTrack : IDisposable
{
    private readonly IWaveIn capture;
    private readonly WaveFileWriter writer;
    private readonly object gate = new();
    private readonly ManualResetEventSlim finished = new(false);

    /* The silence watchdog. Not a Windows Forms timer: this runs on the thread
     * pool, because it must keep filling gaps while the UI is busy saving. */
    private readonly System.Threading.Timer keepAlive;
    private readonly byte[] silence;
    private readonly int silenceChunk;
    private long lastDataTicks;
    private long silenceBytes;
    private volatile bool running;
    private bool disposed;

    private AudioTrack(IWaveIn source, string path)
    {
        capture = source;
        Path = path;

        /* The writer follows whatever the device gives us — usually 48 kHz
         * float — and ffmpeg resamples at mux time if a second device disagrees. */
        writer = new WaveFileWriter(path, capture.WaveFormat);

        silenceChunk = Silence.ChunkBytes(capture.WaveFormat.AverageBytesPerSecond, capture.WaveFormat.BlockAlign);
        silence = silenceChunk > 0 ? new byte[silenceChunk] : Array.Empty<byte>();
        keepAlive = new System.Threading.Timer(_ => FillGap(), null, Timeout.Infinite, Timeout.Infinite);

        capture.DataAvailable += OnDataAvailable;
        capture.RecordingStopped += OnRecordingStopped;
    }

    public string Path { get; }

    public Exception Error { get; private set; }

    public long BytesWritten { get; private set; }

    /// <summary>How much of the track was silence this class had to invent.</summary>
    public long SilenceWritten => Interlocked.Read(ref silenceBytes);

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

    private void Start()
    {
        lastDataTicks = Environment.TickCount64;
        running = true;

        try
        {
            capture.StartRecording();
        }
        catch (Exception ex)
        {
            running = false;
            Error = ex;
            throw;
        }

        keepAlive.Change(Silence.ChunkMilliseconds, Silence.ChunkMilliseconds);
    }

    private void OnDataAvailable(object sender, WaveInEventArgs e)
    {
        Interlocked.Exchange(ref lastDataTicks, Environment.TickCount64);

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

    /// <summary>
    /// Writes a tick of silence when the device has gone quiet, so the track stays
    /// as long as the picture and the next real sound lands where it happened.
    /// </summary>
    private void FillGap()
    {
        if (!running || silence.Length == 0)
            return;

        long now = Environment.TickCount64;

        if (now - Interlocked.Read(ref lastDataTicks) < Silence.ChunkMilliseconds)
            return;

        lock (gate)
        {
            if (!running || disposed)
                return;

            try
            {
                writer.Write(silence, 0, silence.Length);
                Interlocked.Add(ref silenceBytes, silence.Length);
            }
            catch (Exception ex)
            {
                Error ??= ex;
            }
        }

        Interlocked.Exchange(ref lastDataTicks, now);
    }

    private void OnRecordingStopped(object sender, StoppedEventArgs e)
    {
        if (e.Exception != null)
            Error ??= e.Exception;

        finished.Set();
    }

    public void Stop()
    {
        running = false;

        try
        {
            keepAlive.Change(Timeout.Infinite, Timeout.Infinite);
        }
        catch (ObjectDisposedException)
        {
            /* already disposed while closing */
        }

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
        running = false;

        try
        {
            keepAlive.Dispose();
        }
        catch (Exception)
        {
            /* a timer that is already gone needs nothing */
        }

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
