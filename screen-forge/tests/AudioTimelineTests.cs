using System;
using System.IO;
using System.Linq;
using ScreenForge.Core;
using Xunit;

namespace ScreenForge.Tests;

public class AudioTimelineTests
{
    [Fact]
    public void QuietGapKeepsLaterSoundAtTheTimeItWasCaptured()
    {
        using var output = new MemoryStream();
        var timeline = new AudioTimeline(output, 1000, 1);

        timeline.Append(Enumerable.Repeat((byte)1, 1000).ToArray(), 1000, TimeSpan.FromSeconds(1));
        // Nothing is delivered for the next three seconds. This packet spans 4s-5s.
        timeline.Append(Enumerable.Repeat((byte)2, 1000).ToArray(), 1000, TimeSpan.FromSeconds(5));
        timeline.Complete(TimeSpan.FromSeconds(6));

        byte[] result = output.ToArray();
        Assert.Equal(6000, result.Length);
        Assert.All(result[..1000], b => Assert.Equal(1, b));
        Assert.All(result[1000..4000], b => Assert.Equal(0, b));
        Assert.All(result[4000..5000], b => Assert.Equal(2, b));
        Assert.All(result[5000..], b => Assert.Equal(0, b));
        Assert.Equal(4000, timeline.SilenceWritten);
    }

    [Fact]
    public void EmptyCallbacksDoNotPreemptBufferedAudio()
    {
        using var output = new MemoryStream();
        var timeline = new AudioTimeline(output, 1000, 1);
        timeline.Append(Array.Empty<byte>(), 0, TimeSpan.FromMilliseconds(50));
        timeline.Append(Array.Empty<byte>(), 0, TimeSpan.FromMilliseconds(100));
        timeline.Append(Enumerable.Repeat((byte)7, 100).ToArray(), 100, TimeSpan.FromMilliseconds(100));

        Assert.Equal(100, timeline.BytesWritten);
        Assert.Equal(0, timeline.SilenceWritten);
        Assert.All(output.ToArray(), b => Assert.Equal(7, b));
    }

    [Fact]
    public void NoCallbacksCanStillProduceATrackOfTheRightLength()
    {
        using var output = new MemoryStream();
        var timeline = new AudioTimeline(output, 192000, 4);
        timeline.Complete(TimeSpan.FromSeconds(3));
        timeline.Complete(TimeSpan.FromSeconds(3)); // finishing twice cannot add more

        Assert.Equal(576000, timeline.BytesWritten);
        Assert.Equal(timeline.BytesWritten, timeline.SilenceWritten);
        Assert.All(output.ToArray(), b => Assert.Equal(0, b));
    }

    [Fact]
    public void BufferedPacketsAreNotCountedAsAGap()
    {
        using var output = new MemoryStream();
        var timeline = new AudioTimeline(output, 1000, 1);
        byte[] packet = Enumerable.Repeat((byte)3, 100).ToArray();
        timeline.Append(packet, packet.Length, TimeSpan.FromMilliseconds(100));
        timeline.Append(packet, packet.Length, TimeSpan.FromMilliseconds(200));
        timeline.Append(packet, packet.Length, TimeSpan.FromMilliseconds(350)); // 50ms late
        timeline.Append(packet, packet.Length, TimeSpan.FromMilliseconds(400));
        timeline.Append(packet, packet.Length, TimeSpan.FromMilliseconds(500));

        Assert.Equal(550, timeline.BytesWritten);
        Assert.Equal(50, timeline.SilenceWritten); // jitter is not added repeatedly
    }

    [Fact]
    public void UnsignedEightBitSilenceIsTheMidpointNotZero()
    {
        using var output = new MemoryStream();
        var timeline = new AudioTimeline(output, 8000, 1, 128);
        timeline.Complete(TimeSpan.FromMilliseconds(25));
        Assert.Equal(200, output.Length);
        Assert.All(output.ToArray(), b => Assert.Equal(128, b));
    }

    [Fact]
    public void PaddingAndPacketsRespectMultichannelFrameBoundaries()
    {
        using var output = new MemoryStream();
        var timeline = new AudioTimeline(output, 176400, 4);
        byte[] packet = new byte[1764]; // 10ms at 44.1kHz stereo 16-bit
        timeline.Append(packet, packet.Length, TimeSpan.FromMilliseconds(123));
        timeline.Complete(TimeSpan.FromMilliseconds(500));

        Assert.Equal(AudioTimeline.BytesAt(TimeSpan.FromMilliseconds(500), 176400, 4), output.Length);
        Assert.Equal(0, timeline.SilenceWritten % 4);
        Assert.Throws<ArgumentOutOfRangeException>(() => timeline.Append(packet, 3, TimeSpan.FromSeconds(1)));
    }

    [Fact]
    public void LongDurationsDoNotOverflowAndExtremeValuesAreClamped()
    {
        Assert.Equal(4_147_200_000L, AudioTimeline.BytesAt(TimeSpan.FromHours(6), 192000, 4));
        long maximum = AudioTimeline.BytesAt(TimeSpan.MaxValue, int.MaxValue - 3, 4);
        Assert.True(maximum > 0);
        Assert.Equal(0, maximum % 4);
        Assert.Equal(0, AudioTimeline.BytesAt(TimeSpan.FromSeconds(-1), 192000, 4));
    }

    [Fact]
    public void InvalidFormatsAreRejectedRatherThanWritingPartialFrames()
    {
        using var output = new MemoryStream();
        Assert.Throws<ArgumentOutOfRangeException>(() => new AudioTimeline(output, 0, 4));
        Assert.Throws<ArgumentOutOfRangeException>(() => new AudioTimeline(output, 101, 4));
        Assert.Throws<ArgumentOutOfRangeException>(() => new AudioTimeline(output, 192000, 0));
    }
}
