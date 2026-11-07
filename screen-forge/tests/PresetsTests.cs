using System;
using ScreenForge.Core;
using Xunit;

namespace ScreenForge.Tests;

public class PresetsTests
{
    [Fact]
    public void BitrateGrowsWithResolutionAndPreset()
    {
        int at1080 = Presets.BitrateKbps(1920, 1080, 30, QualityPreset.High);
        int at4k = Presets.BitrateKbps(3840, 2160, 30, QualityPreset.High);
        int at1080Low = Presets.BitrateKbps(1920, 1080, 30, QualityPreset.Low);

        Assert.True(at4k > at1080, "4K needs more bits than 1080p");
        Assert.True(at1080 > at1080Low, "the higher preset needs more bits than the lower one");
        Assert.Equal(12000, at1080);
    }

    [Fact]
    public void FrameRateRaisesTheBitrateButNotLinearly()
    {
        int at30 = Presets.BitrateKbps(1920, 1080, 30, QualityPreset.High);
        int at60 = Presets.BitrateKbps(1920, 1080, 60, QualityPreset.High);
        int at120 = Presets.BitrateKbps(1920, 1080, 120, QualityPreset.High);

        Assert.True(at60 > at30);
        Assert.True(at60 < at30 * 2, "60 fps should not double the file size");
        Assert.True(at120 > at60);
    }

    [Fact]
    public void TinyAndEnormousAreasStayInsideSaneLimits()
    {
        Assert.True(Presets.BitrateKbps(320, 240, 15, QualityPreset.Low) == 1200, "never below watchable");
        Assert.True(Presets.BitrateKbps(7680, 4320, 144, QualityPreset.Maximum) <= 80000, "never absurd");
    }

    [Fact]
    public void ASizeLessCaptureAreaIsRefused()
    {
        Assert.Throws<ArgumentOutOfRangeException>(() => Presets.BitrateKbps(0, 1080, 30, QualityPreset.High));
        Assert.Throws<ArgumentOutOfRangeException>(() => Presets.BitrateKbps(1920, -1, 30, QualityPreset.High));
    }

    [Theory]
    [InlineData(100, 100)]
    [InlineData(101, 100)]
    [InlineData(1, 2)]
    [InlineData(0, 2)]
    public void DimensionsAreAlwaysEven(int input, int expected)
    {
        Assert.Equal(expected, Presets.ToEven(input));
    }

    [Fact]
    public void NormalisingARectangleKeepsItsCorner()
    {
        var rect = Presets.Normalise(new CaptureRect(11, 22, 801, 601));
        Assert.Equal(11, rect.X);
        Assert.Equal(22, rect.Y);
        Assert.Equal(800, rect.Width);
        Assert.Equal(600, rect.Height);
    }

    [Theory]
    [InlineData(60, 60)]
    [InlineData(30, 30)]
    [InlineData(59, 60)]
    [InlineData(31, 30)]
    [InlineData(-5, 15)]
    [InlineData(1000, 144)]
    public void FrameRatesAreSnappedToTheSupportedList(int input, int expected)
    {
        Assert.Equal(expected, Presets.ClampFps(input));
    }

    [Fact]
    public void QualityValuesMoveInTheRightDirection()
    {
        Assert.True(Presets.Crf(QualityPreset.Low) > Presets.Crf(QualityPreset.Maximum));
        Assert.True(Presets.NvencCq(QualityPreset.Low) > Presets.NvencCq(QualityPreset.Maximum));
        Assert.InRange(Presets.Crf(QualityPreset.High), 1, 51);
        Assert.InRange(Presets.NvencCq(QualityPreset.High), 1, 51);
    }

    [Fact]
    public void EstimatesGrowWithTimeAndBitrate()
    {
        long minute = Presets.EstimatedBytes(12000, TimeSpan.FromMinutes(1), true);
        long twoMinutes = Presets.EstimatedBytes(12000, TimeSpan.FromMinutes(2), true);
        long quiet = Presets.EstimatedBytes(3000, TimeSpan.FromMinutes(1), true);

        Assert.True(twoMinutes > minute);
        Assert.True(minute > quiet);
        Assert.Equal(0, Presets.EstimatedBytes(12000, TimeSpan.Zero, true));
    }
}
