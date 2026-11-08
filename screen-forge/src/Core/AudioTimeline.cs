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

namespace ScreenForge.Core;

/// <summary>
/// Places captured PCM packets on a wall-clock timeline. WASAPI loopback can
/// return no samples while nothing is playing, so simply concatenating packets
/// would move later sound earlier. Packet durations are subtracted from the
/// elapsed time before filling a gap; a packet already buffered by the device
/// is therefore not mistaken for missing audio.
///
/// The caller serializes access and supplies a monotonic elapsed time. This
/// class deliberately has no timer: timer jitter must not accumulate as drift.
/// </summary>
public sealed class AudioTimeline
{
    private const int ChunkSize = 65536;
    private readonly Stream output;
    private readonly int bytesPerSecond;
    private readonly int blockAlign;
    private readonly byte[] silence;

    public AudioTimeline(Stream output, int bytesPerSecond, int blockAlign, byte silenceByte = 0)
    {
        this.output = output ?? throw new ArgumentNullException(nameof(output));
        if (!output.CanWrite)
            throw new ArgumentException("the audio stream must be writable", nameof(output));
        ValidateFormat(bytesPerSecond, blockAlign);

        this.bytesPerSecond = bytesPerSecond;
        this.blockAlign = blockAlign;
        silence = new byte[ChunkSize / blockAlign * blockAlign];
        if (silenceByte != 0)
            Array.Fill(silence, silenceByte);
    }

    public long BytesWritten { get; private set; }
    public long SilenceWritten { get; private set; }

    /// <summary>
    /// Appends one packet, inserting only the silence before that packet. An
    /// empty callback is ignored: a buffered packet may still cover that time.
    /// </summary>
    public void Append(byte[] buffer, int count, TimeSpan elapsed)
    {
        if (buffer == null)
            throw new ArgumentNullException(nameof(buffer));
        if (count < 0 || count > buffer.Length || count % blockAlign != 0)
            throw new ArgumentOutOfRangeException(nameof(count));
        if (count == 0)
            return;

        long elapsedBytes = BytesAt(elapsed, bytesPerSecond, blockAlign);
        long packetStart = Math.Max(0, elapsedBytes - count);
        if (packetStart > BytesWritten)
            WriteSilence(packetStart - BytesWritten);

        output.Write(buffer, 0, count);
        BytesWritten = checked(BytesWritten + count);
    }

    /// <summary>Fills any trailing silence after capture has stopped.</summary>
    public void Complete(TimeSpan elapsed)
    {
        long elapsedBytes = BytesAt(elapsed, bytesPerSecond, blockAlign);
        if (elapsedBytes > BytesWritten)
            WriteSilence(elapsedBytes - BytesWritten);
    }

    /// <summary>Whole sample frames that fit into the elapsed time.</summary>
    public static long BytesAt(TimeSpan elapsed, int bytesPerSecond, int blockAlign)
    {
        ValidateFormat(bytesPerSecond, blockAlign);
        if (elapsed <= TimeSpan.Zero)
            return 0;

        // Split whole seconds and the remainder so multiplication cannot wrap.
        long seconds = elapsed.Ticks / TimeSpan.TicksPerSecond;
        long remainder = elapsed.Ticks % TimeSpan.TicksPerSecond;
        long maxAligned = long.MaxValue - long.MaxValue % blockAlign;
        if (seconds > maxAligned / bytesPerSecond)
            return maxAligned;

        long whole = seconds * bytesPerSecond;
        long partial = remainder * bytesPerSecond / TimeSpan.TicksPerSecond;
        if (partial > maxAligned - whole)
            return maxAligned;

        long bytes = whole + partial;
        return bytes - bytes % blockAlign;
    }

    private static void ValidateFormat(int bytesPerSecond, int blockAlign)
    {
        if (blockAlign <= 0 || blockAlign > ChunkSize)
            throw new ArgumentOutOfRangeException(nameof(blockAlign));
        if (bytesPerSecond <= 0 || bytesPerSecond % blockAlign != 0)
            throw new ArgumentOutOfRangeException(nameof(bytesPerSecond));
    }

    private void WriteSilence(long count)
    {
        // Both the timeline and every packet end on a frame boundary.
        while (count > 0)
        {
            int chunk = (int)Math.Min(count, silence.Length);
            output.Write(silence, 0, chunk);
            BytesWritten = checked(BytesWritten + chunk);
            SilenceWritten = checked(SilenceWritten + chunk);
            count -= chunk;
        }
    }
}
