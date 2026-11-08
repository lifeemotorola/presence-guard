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
/// Records a WASAPI source to WAV. Loopback can have gaps when the playback
/// device is idle. AudioTimeline inserts those gaps when the next packet arrives
/// and pads trailing silence when capture stops, using a monotonic clock rather
/// than a periodic timer. Access to the timeline and writer is serialized.
/// </summary>
public sealed class AudioTrack : IDisposable
{
    private readonly IWaveIn capture;
    private readonly WaveFileWriter writer;
    private readonly AudioTimeline timeline;
    private readonly Stopwatch clock = new();
    private readonly object gate = new();
    private readonly ManualResetEventSlim finished = new(false);
    private bool writerClosed;
    private bool stopRequested;
    private bool disposed;

    private AudioTrack(IWaveIn source, string path)
    {
        capture = source;
        Path = path;
        writer = new WaveFileWriter(path, capture.WaveFormat);

        try
        {
            WaveFormat format = capture.WaveFormat;
            // 8-bit PCM is unsigned; all ordinary wider PCM/float silence is zero.
            byte silence = format.BitsPerSample == 8 ? (byte)128 : (byte)0;
            timeline = new AudioTimeline(writer, format.AverageBytesPerSecond, format.BlockAlign, silence);
        }
        catch
        {
            writer.Dispose();
            throw;
        }

        capture.DataAvailable += OnDataAvailable;
        capture.RecordingStopped += OnRecordingStopped;
    }

    public string Path { get; }
    public Exception Error { get; private set; }
    public long BytesWritten { get { lock (gate) return timeline.BytesWritten - timeline.SilenceWritten; } }
    public long SilenceWritten { get { lock (gate) return timeline.SilenceWritten; } }

    public static AudioTrack StartSystem(string path) => Start(new WasapiLoopbackCapture(), path);

    public static AudioTrack StartMicrophone(string deviceId, string path)
    {
        IWaveIn source;
        if (!string.IsNullOrWhiteSpace(deviceId))
        {
            using var enumerator = new MMDeviceEnumerator();
            source = new WasapiCapture(enumerator.GetDevice(deviceId));
        }
        else
        {
            source = new WasapiCapture();
        }

        return Start(source, path);
    }

    private static AudioTrack Start(IWaveIn source, string path)
    {
        AudioTrack track = null;
        try
        {
            track = new AudioTrack(source, path);
            track.clock.Start();
            source.StartRecording();
            return track;
        }
        catch
        {
            if (track != null)
                track.Dispose();
            else
                source.Dispose();
            throw;
        }
    }

    private void OnDataAvailable(object sender, WaveInEventArgs e)
    {
        if (e.BytesRecorded <= 0)
            return;

        lock (gate)
        {
            if (writerClosed || disposed)
                return;

            try
            {
                timeline.Append(e.Buffer, e.BytesRecorded, clock.Elapsed);
            }
            catch (Exception ex)
            {
                Error ??= ex;
            }
        }
    }

    private void OnRecordingStopped(object sender, StoppedEventArgs e)
    {
        if (e.Exception != null)
        {
            lock (gate)
                Error ??= e.Exception;
        }

        try { finished.Set(); }
        catch (ObjectDisposedException) { /* a late device callback */ }
    }

    public void Stop()
    {
        lock (gate)
        {
            if (stopRequested || disposed)
                return;
            stopRequested = true;
            clock.Stop();
        }

        try { capture.StopRecording(); }
        catch (Exception ex) { lock (gate) Error ??= ex; }

        // A device that never reports back must not hang the stop button.
        finished.Wait(TimeSpan.FromSeconds(3));
        CloseWriter(padTrailingSilence: true);
    }

    private void CloseWriter(bool padTrailingSilence)
    {
        lock (gate)
        {
            if (writerClosed)
                return;

            try
            {
                if (padTrailingSilence)
                    timeline.Complete(clock.Elapsed);
            }
            catch (Exception ex)
            {
                Error ??= ex;
            }
            finally
            {
                try { writer.Dispose(); }
                catch (Exception ex) { Error ??= ex; }
                writerClosed = true;
            }
        }
    }

    public void Dispose()
    {
        lock (gate)
        {
            if (disposed)
                return;
            disposed = true;
            clock.Stop();
        }

        try
        {
            capture.DataAvailable -= OnDataAvailable;
            capture.RecordingStopped -= OnRecordingStopped;
            capture.Dispose();
        }
        catch (Exception)
        {
            /* disposing a dead device can throw */
        }

        CloseWriter(padTrailingSilence: false);
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
