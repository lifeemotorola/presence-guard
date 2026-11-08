using System;
using ScreenForge.Core;
using Xunit;

namespace ScreenForge.Tests;

/// <summary>
/// The arithmetic that keeps a soundtrack continuous when the desktop goes quiet.
/// It is here rather than inside the capture class because a wrong number here is
/// a drift of milliseconds per second — inaudible for a minute, ruinous for an
/// hour — and this is the only place it can be checked without a sound card.
/// </summary>
public class SilenceTests
{
    private const int FortyeightKhzStereo16Bit = 192000;  // 48000 Hz × 2 ch × 2 bytes
    private const int FortyfourOneStereo16Bit = 176400;   // 44100 Hz × 2 ch × 2 bytes
    private const int EightKhzMono8Bit = 8000;            //  8000 Hz × 1 ch × 1 byte
    private const int FortyeightKhzStereoFloat = 384000;  // 48000 Hz × 2 ch × 4 bytes

    [Fact]
    public void FiftyMillisecondsAtFortyEightKhzStereoIsNineThousandSixHundredBytes()
    {
        Assert.Equal(9600, Silence.BytesFor(FortyeightKhzStereo16Bit, 4, 50));
        Assert.Equal(9600, Silence.ChunkBytes(FortyeightKhzStereo16Bit, 4));
    }

    [Fact]
    public void TheChunkIsAlwaysFiftyMillisecondsOfThatDevice()
    {
        Assert.Equal(8820, Silence.ChunkBytes(FortyfourOneStereo16Bit, 4));
        Assert.Equal(19200, Silence.ChunkBytes(FortyeightKhzStereoFloat, 8));
        Assert.Equal(400, Silence.ChunkBytes(EightKhzMono8Bit, 1));
    }

    [Fact]
    public void SilenceIsAlwaysAWholeNumberOfSamples()
    {
        /* 44100 bytes per second over 50 ms is 2205 bytes, which is not a whole
         * number of 4-byte samples: writing it would put the track half a sample
         * out of step and turn the padding into a click. */
        int bytes = Silence.BytesFor(44100, 4, 50);

        Assert.Equal(0, bytes % 4);
        Assert.Equal(2204, bytes);
    }

    [Fact]
    public void ADeviceThatReportsNoBlockSizeStillGetsSilence()
    {
        Assert.Equal(9600, Silence.BytesFor(FortyeightKhzStereo16Bit, 0, 50));
        Assert.Equal(9600, Silence.BytesFor(FortyeightKhzStereo16Bit, -8, 50));
    }

    [Fact]
    public void NonsenseAsksForNothingRatherThanForABigBuffer()
    {
        Assert.Equal(0, Silence.BytesFor(0, 4, 50));
        Assert.Equal(0, Silence.BytesFor(-48000, 4, 50));
        Assert.Equal(0, Silence.BytesFor(FortyeightKhzStereo16Bit, 4, 0));
        Assert.Equal(0, Silence.BytesFor(FortyeightKhzStereo16Bit, 4, -50));
    }

    [Fact]
    public void AVeryLargeRequestIsCappedInsteadOfOverflowing()
    {
        int bytes = Silence.BytesFor(int.MaxValue, 4, 50);

        Assert.True(bytes > 0);
        Assert.True(bytes <= 4 * 1024 * 1024);
    }
}
