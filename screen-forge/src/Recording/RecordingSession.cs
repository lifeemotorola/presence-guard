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
using System.Globalization;
using System.IO;
using System.Text;
using System.Threading;
using System.Threading.Tasks;
using ScreenForge.Audio;
using ScreenForge.Core;

namespace ScreenForge.Recording;

/// <summary>
/// One recording, from the first frame to a playable file.
///
/// A pause is not a pause in ffmpeg — it stops the take and starts a new one, and
/// the takes are joined at the end. That is more work than pausing a process, and
/// it is also the only version that survives the app being killed: every take is
/// a complete, fragmented MP4 on disk from the moment it starts.
/// </summary>
public sealed class RecordingSession : IDisposable
{
    private sealed class Take
    {
        public int Index;
        public string VideoPath;
        public string SystemPath;
        public string MicrophonePath;
        public AudioTrack System;
        public AudioTrack Microphone;
        public Process Process;
        public readonly List<string> FfmpegNoise = new();
        public double Milliseconds;
        public bool HasPicture;
    }

    private readonly string ffmpegPath;
    private readonly VideoSettings video;
    private readonly AudioSettings audio;
    private readonly CaptureTarget target;
    private readonly string outputPath;
    private readonly string workFolder;
    private readonly Action<string> log;
    private readonly List<Take> takes = new();
    private readonly Stopwatch running = new();
    private readonly StringBuilder transcript = new();
    private double finishedMilliseconds;

    private Take current;
    private bool disposed;

    private RecordingSession(
        string ffmpegPath,
        VideoSettings video,
        AudioSettings audio,
        CaptureTarget target,
        string outputPath,
        string workFolder,
        Action<string> log)
    {
        this.ffmpegPath = ffmpegPath;
        this.video = video;
        this.audio = audio;
        this.target = target;
        this.outputPath = outputPath;
        this.workFolder = workFolder;
        this.log = log ?? (_ => { });
    }

    public string EncoderName { get; private set; } = FfmpegArgs.EncoderX264;

    /// <summary>The area being captured, in desktop pixels.</summary>
    public CaptureRect Rect => target.Rect;

    public string OutputPath => outputPath;

    public bool Paused { get; private set; }

    public int TakeCount => takes.Count;

    public TimeSpan Elapsed => Paused
        ? TimeSpan.FromMilliseconds(finishedMilliseconds)
        : TimeSpan.FromMilliseconds(finishedMilliseconds + running.Elapsed.TotalMilliseconds);

    /// <summary>Bytes written so far, so the UI can show that something is happening.</summary>
    public long BytesOnDisk
    {
        get
        {
            long total = 0;
            foreach (Take take in takes)
            {
                try
                {
                    foreach (string path in new[] { take.VideoPath, take.SystemPath, take.MicrophonePath })
                    {
                        if (!string.IsNullOrEmpty(path) && File.Exists(path))
                            total += new FileInfo(path).Length;
                    }
                }
                catch (Exception)
                {
                    /* the file is being written right now; a share violation is normal */
                }
            }

            return total;
        }
    }

    public static async Task<RecordingSession> CreateAsync(
        string ffmpegPath,
        AppSettings settings,
        CaptureTarget target,
        string outputPath,
        Action<string> log)
    {
        if (string.IsNullOrWhiteSpace(ffmpegPath))
            throw new ArgumentException("ffmpeg was not found", nameof(ffmpegPath));

        string encoder = await EncoderSelector
            .SelectAsync(ffmpegPath, settings.Encoder, log)
            .ConfigureAwait(false);

        string workFolder = Path.Combine(
            Path.GetTempPath(),
            "screenforge-" + DateTime.Now.ToString("yyyyMMdd-HHmmss", CultureInfo.InvariantCulture) + "-" + Guid.NewGuid().ToString("N").Substring(0, 6));

        Directory.CreateDirectory(workFolder);

        var session = new RecordingSession(
            ffmpegPath,
            settings.ToVideoSettings(),
            settings.ToAudioSettings(),
            target,
            outputPath,
            workFolder,
            log)
        {
            EncoderName = encoder,
        };

        session.Note($"encoder: {encoder} ({EncoderProbe.Describe(encoder)})");
        session.Note($"capture: {target.Kind} {target.Rect} \"{target.Description}\"");
        session.Note($"fps {session.video.Fps}, {session.video.Quality}, audio system={session.audio.RecordSystem} mic={session.audio.RecordMicrophone}");
        session.Note($"output: {outputPath}");

        return session;
    }

    public void Start()
    {
        if (current != null)
            return;

        StartTake();
    }

    private void StartTake()
    {
        int index = takes.Count + 1;
        var take = new Take
        {
            Index = index,
            VideoPath = Path.Combine(workFolder, $"take-{index}.mp4"),
        };

        /* Audio first: a soundtrack that begins a few milliseconds early is
         * inaudible, one that begins late loses the first word. */
        if (audio.RecordSystem)
        {
            try
            {
                take.SystemPath = Path.Combine(workFolder, $"system-{index}.wav");
                take.System = AudioTrack.StartSystem(take.SystemPath);
                Note("system audio: recording");
            }
            catch (Exception ex)
            {
                take.SystemPath = null;
                Note($"system audio unavailable ({ex.Message}); recording picture only");
            }
        }

        if (audio.RecordMicrophone)
        {
            try
            {
                take.MicrophonePath = Path.Combine(workFolder, $"mic-{index}.wav");
                take.Microphone = AudioTrack.StartMicrophone(audio.MicrophoneDeviceId, take.MicrophonePath);
                Note("microphone: recording");
            }
            catch (Exception ex)
            {
                take.MicrophonePath = null;
                Note($"microphone unavailable ({ex.Message}); recording picture only");
            }
        }

        var request = new VideoCaptureRequest
        {
            Rect = target.Rect,
            Fps = video.Fps,
            Quality = video.Quality,
            Encoder = EncoderProbe.KindOf(EncoderName),
            CaptureCursor = video.CaptureCursor,
            UseDesktopDuplication = video.UseDesktopDuplication && target.Kind == CaptureKind.Monitor,
            MonitorIndex = 0,
            OutputPath = take.VideoPath,
        };

        List<string> args = FfmpegArgs.RecordVideo(request);
        Note("$ " + FfmpegArgs.ToCommandLine(ffmpegPath, args));

        var process = new Process
        {
            StartInfo = FfmpegProcess.StartInfo(ffmpegPath, args),
            EnableRaisingEvents = true,
        };

        process.ErrorDataReceived += (_, e) =>
        {
            if (string.IsNullOrWhiteSpace(e.Data))
                return;

            lock (take.FfmpegNoise)
            {
                if (take.FfmpegNoise.Count < 200)
                    take.FfmpegNoise.Add(e.Data);
            }

            Note(e.Data);
        };

        process.Start();
        process.BeginErrorReadLine();

        /* Give ffmpeg a moment to fail on a bad argument, a busy desktop or a
         * missing encoder, so the error lands in the dialog and not in a log the
         * user will never open. */
        if (process.WaitForExit(600))
        {
            string why = take.FfmpegNoise.Count > 0
                ? take.FfmpegNoise[take.FfmpegNoise.Count - 1]
                : "ffmpeg stopped immediately";

            StopAudio(take);
            throw new InvalidOperationException($"the capture did not start: {why}");
        }

        take.Process = process;
        current = take;
        takes.Add(take);
        running.Restart();
        Paused = false;

        Note($"take {index} running");
    }

    public void Pause()
    {
        if (current == null || Paused)
            return;

        StopTake();
        Paused = true;
        Note("paused");
    }

    public void Resume()
    {
        if (!Paused)
            return;

        StartTake();
    }

    private void StopTake()
    {
        Take take = current;
        if (take == null)
            return;

        running.Stop();
        take.Milliseconds = running.Elapsed.TotalMilliseconds;
        finishedMilliseconds += take.Milliseconds;

        Process process = take.Process;
        take.Process = null;

        if (process != null)
        {
            /* "q" is ffmpeg's own shutdown: it flushes the muxer and closes the
             * file properly. Killing it works too, because the file is
             * fragmented, but this is the clean path. */
            try
            {
                if (!process.HasExited)
                {
                    process.StandardInput.Write("q");
                    process.StandardInput.Flush();
                }
            }
            catch (Exception)
            {
                /* stdin already closed: fall through to the wait and the kill */
            }

            try
            {
                if (!process.WaitForExit(8000))
                {
                    Note("ffmpeg did not stop by itself; terminating");
                    process.Kill(true);
                    process.WaitForExit(3000);
                }
            }
            catch (Exception ex)
            {
                Note($"stopping ffmpeg: {ex.Message}");
            }

            try
            {
                process.CancelErrorRead();
                process.Dispose();
            }
            catch (Exception)
            {
                /* nothing left to release */
            }
        }

        StopAudio(take);

        take.HasPicture = File.Exists(take.VideoPath) && new FileInfo(take.VideoPath).Length > 1024;

        if (!take.HasPicture)
            Note($"take {take.Index} produced no picture");

        current = null;
    }

    private void StopAudio(Take take)
    {
        try
        {
            take.System?.Stop();
        }
        catch (Exception ex)
        {
            Note($"stopping system audio: {ex.Message}");
        }

        try
        {
            take.Microphone?.Stop();
        }
        catch (Exception ex)
        {
            Note($"stopping microphone: {ex.Message}");
        }

        take.System?.Dispose();
        take.Microphone?.Dispose();
        take.System = null;
        take.Microphone = null;

        /* A WAV with only a header (44 bytes) is a device that never delivered a
         * buffer: drop it rather than muxing silence. */
        take.SystemPath = Usable(take.SystemPath);
        take.MicrophonePath = Usable(take.MicrophonePath);
    }

    private static string Usable(string path)
    {
        try
        {
            return !string.IsNullOrWhiteSpace(path) && File.Exists(path) && new FileInfo(path).Length > 1024
                ? path
                : null;
        }
        catch (Exception)
        {
            return null;
        }
    }

    /// <summary>Stops, mixes, joins and moves the result into place.</summary>
    public async Task<RecordingResult> StopAsync()
    {
        if (Paused && current == null && takes.Count == 0)
            return Failure("nothing was recorded");

        StopTake();
        Paused = false;

        var result = new RecordingResult
        {
            Takes = takes.Count,
            Duration = TimeSpan.FromMilliseconds(finishedMilliseconds),
        };

        try
        {
            List<string> mixed = new();

            foreach (Take take in takes)
            {
                if (!take.HasPicture)
                    continue;

                string mixedPath = Path.Combine(workFolder, $"mixed-{take.Index}.mp4");
                var mux = new MuxRequest
                {
                    VideoPath = take.VideoPath,
                    SystemAudioPath = take.SystemPath,
                    MicrophonePath = take.MicrophonePath,
                    SystemGainPercent = audio.SystemGainPercent,
                    MicrophoneGainPercent = audio.MicrophoneGainPercent,
                    AudioBitrateKbps = 192,
                    OutputPath = mixedPath,
                };

                List<string> args = FfmpegArgs.Mux(mux);
                Note("$ " + FfmpegArgs.ToCommandLine(ffmpegPath, args));

                ProcessResult muxResult = await FfmpegProcess.RunAsync(ffmpegPath, args, 600000).ConfigureAwait(false);
                if (!muxResult.Ok || !File.Exists(mixedPath))
                {
                    /* Mixing is the only step that can lose everything, so if it
                     * fails the take is kept as it stands, silence and all. */
                    Note($"mixing take {take.Index} failed: {LastLines(muxResult.Output)}");
                    mixed.Add(take.VideoPath);
                }
                else
                {
                    mixed.Add(mixedPath);
                }
            }

            if (mixed.Count == 0)
                return Failure("ffmpeg did not record any picture — see the log for the reason");

            string finalPath = outputPath;
            Directory.CreateDirectory(Path.GetDirectoryName(finalPath) ?? ".");

            if (mixed.Count == 1)
            {
                File.Move(mixed[0], finalPath, true);
            }
            else
            {
                string listPath = Path.Combine(workFolder, "takes.txt");
                File.WriteAllText(listPath, FfmpegArgs.ConcatList(mixed));

                List<string> concatArgs = FfmpegArgs.Concat(listPath, finalPath);
                Note("$ " + FfmpegArgs.ToCommandLine(ffmpegPath, concatArgs));

                ProcessResult concatResult = await FfmpegProcess.RunAsync(ffmpegPath, concatArgs, 600000).ConfigureAwait(false);
                if (!concatResult.Ok || !File.Exists(finalPath))
                {
                    Note($"joining the takes failed: {LastLines(concatResult.Output)}");
                    File.Copy(mixed[0], finalPath, true);
                }
            }

            result.Success = true;
            result.Path = finalPath;
            result.Bytes = new FileInfo(finalPath).Length;
            result.Message = mixed.Count > 1
                ? $"Joined {mixed.Count} takes into one file."
                : "Saved.";
        }
        catch (Exception ex)
        {
            Note("failed: " + ex);
            return Failure(ex.Message);
        }
        finally
        {
            current = null;
        }

        result.LogPath = WriteLog(result.Path);
        Cleanup();
        return result;
    }

    /// <summary>Stops and throws the takes away. Used when the user gives up.</summary>
    public void Cancel()
    {
        StopTake();
        Cleanup();
    }

    private RecordingResult Failure(string message)
    {
        var result = new RecordingResult { Success = false, Message = message };
        result.LogPath = WriteLog(null);
        Cleanup();
        return result;
    }

    private string WriteLog(string videoPath)
    {
        try
        {
            Note($"elapsed {Elapsed.TotalSeconds:0.0}s, {takes.Count} take(s)");
            string text = transcript.ToString();

            if (string.IsNullOrWhiteSpace(videoPath))
                return "";

            string sidecar = FfmpegArgs.SidecarLogPath(videoPath);
            File.WriteAllText(sidecar, text);
            return sidecar;
        }
        catch (Exception)
        {
            return "";
        }
    }

    private void Cleanup()
    {
        try
        {
            if (Directory.Exists(workFolder))
                Directory.Delete(workFolder, true);
        }
        catch (Exception)
        {
            /* a temp folder left behind is not worth an error dialog */
        }
    }

    private string LastLines(string text)
    {
        if (string.IsNullOrWhiteSpace(text))
            return "no output";

        string[] lines = text.TrimEnd().Split('\n');
        int from = Math.Max(0, lines.Length - 3);
        return string.Join(" | ", lines[from..]).Trim();
    }

    private void Note(string line)
    {
        if (string.IsNullOrWhiteSpace(line))
            return;

        lock (transcript)
            transcript.AppendLine($"[{DateTime.Now:HH:mm:ss}] {line.Trim()}");

        try
        {
            log(line);
        }
        catch (Exception)
        {
            /* the log window closed while recording */
        }
    }

    public void Dispose()
    {
        if (disposed)
            return;

        disposed = true;

        try
        {
            current?.Process?.Dispose();
        }
        catch (Exception)
        {
            /* already gone */
        }

        Cleanup();
    }
}
