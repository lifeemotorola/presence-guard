using System.Collections.Generic;
using ScreenForge.Core;
using Xunit;

namespace ScreenForge.Tests;

public class EncoderProbeTests
{
    private const string Sample = @"
Encoders:
 V..... = Video
 ------
 V....D h264_amf              AMD AMF H.264 Encoder (codec h264)
 V....D h264_mf               MediaFoundation H.264 encoder (codec h264)
 V....D h264_nvenc            NVIDIA NVENC H.264 encoder (codec h264)
 V....D h264_qsv              H.264 (Intel Quick Sync Video) (codec h264)
 V....D libx264               libx264 H.264 / AVC (codec h264)
 V....D libx265               libx265 H.265 / HEVC (codec hevc)
 A....D aac                    AAC (Advanced Audio Coding)
";

    [Fact]
    public void TheListIsParsed()
    {
        HashSet<string> found = EncoderProbe.Parse(Sample);

        Assert.Contains("h264_nvenc", found);
        Assert.Contains("h264_amf", found);
        Assert.Contains("h264_qsv", found);
        Assert.Contains("libx264", found);
        Assert.Contains("aac", found);
        Assert.DoesNotContain("Encoders:", found);
        Assert.DoesNotContain("------", found);
    }

    [Fact]
    public void RubbishInGivesAnEmptySetNotAnException()
    {
        Assert.Empty(EncoderProbe.Parse(null));
        Assert.Empty(EncoderProbe.Parse(""));
        Assert.Empty(EncoderProbe.Parse("nothing useful here"));
    }

    [Fact]
    public void AutoPrefersHardwareThenFallsBackToTheCpu()
    {
        var available = EncoderProbe.Parse(Sample);
        List<string> candidates = EncoderProbe.Candidates(EncoderKind.Auto, available);

        Assert.True(candidates[0] == FfmpegArgs.EncoderNvenc, "an NVIDIA card is the best default");
        Assert.Contains(FfmpegArgs.EncoderX264, candidates);
        Assert.True(candidates[candidates.Count - 1] == FfmpegArgs.EncoderX264, "software is the last resort");
    }

    [Fact]
    public void AutoOnAMachineWithNoHardwareStillHasSomethingToTry()
    {
        var available = new HashSet<string> { "libx264" };
        List<string> candidates = EncoderProbe.Candidates(EncoderKind.Auto, available);

        Assert.Single(candidates);
        Assert.Equal(FfmpegArgs.EncoderX264, candidates[0]);
    }

    [Fact]
    public void AnExplicitChoiceIsRespectedButKeepsTheSafetyNet()
    {
        var available = EncoderProbe.Parse(Sample);

        List<string> nvenc = EncoderProbe.Candidates(EncoderKind.Nvenc, available);
        Assert.Equal(FfmpegArgs.EncoderNvenc, nvenc[0]);
        Assert.Contains(FfmpegArgs.EncoderX264, nvenc);

        List<string> software = EncoderProbe.Candidates(EncoderKind.X264, available);
        Assert.Single(software);
    }

    [Fact]
    public void AskingForHardwareThatIsNotThereFallsBack()
    {
        var available = new HashSet<string> { "libx264" };
        List<string> candidates = EncoderProbe.Candidates(EncoderKind.Amf, available);

        Assert.Equal(new List<string> { FfmpegArgs.EncoderX264 }, candidates);
    }

    [Fact]
    public void EncoderNamesMapToTheUiAndBack()
    {
        Assert.Equal(EncoderKind.Nvenc, EncoderProbe.KindOf("h264_nvenc"));
        Assert.Equal(EncoderKind.Amf, EncoderProbe.KindOf("h264_amf"));
        Assert.Equal(EncoderKind.Qsv, EncoderProbe.KindOf("h264_qsv"));
        Assert.Equal(EncoderKind.X264, EncoderProbe.KindOf("libx264"));
        Assert.Equal(EncoderKind.X264, EncoderProbe.KindOf("something else"));

        Assert.Contains("NVIDIA", EncoderProbe.Describe("h264_nvenc"));
        Assert.Contains("CPU", EncoderProbe.Describe("libx264"));
        Assert.True(EncoderProbe.IsHardware("h264_nvenc"));
        Assert.False(EncoderProbe.IsHardware("libx264"));
    }
}
