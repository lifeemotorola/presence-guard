using System;
using System.Collections.Generic;
using System.Linq;
using ScreenForge.Core;
using Xunit;

namespace ScreenForge.Tests;

public class FfmpegArgsTests
{
    private static VideoCaptureRequest Request(EncoderKind encoder = EncoderKind.X264, int fps = 60) => new()
    {
        Rect = new CaptureRect(100, 50, 1921, 1081),
        Fps = fps,
        Quality = QualityPreset.High,
        Encoder = encoder,
        CaptureCursor = true,
        OutputPath = @"C:\temp\take 1.mp4",
    };

    private static string Join(IEnumerable<string> args) => string.Join(" ", args);

    [Fact]
    public void TheInputIsGdigrabOnTheChosenRectangle()
    {
        string args = Join(FfmpegArgs.Input(Request()));

        Assert.Contains("-f gdigrab", args);
        Assert.Contains("-framerate 60", args);
        Assert.Contains("-offset_x 100", args);
        Assert.Contains("-offset_y 50", args);
        Assert.Contains("-video_size 1920x1080", args);
        Assert.Contains("-i desktop", args);
        Assert.Contains("-draw_mouse 1", args);
    }

    [Fact]
    public void OddSizesAreRoundedDownSoH264CanEncodeThem()
    {
        var request = Request();
        request.Rect = new CaptureRect(0, 0, 1367, 769);

        string args = Join(FfmpegArgs.Input(request));
        Assert.Contains("-video_size 1366x768", args);
    }

    [Fact]
    public void TheCursorCanBeLeftOut()
    {
        var request = Request();
        request.CaptureCursor = false;
        Assert.Contains("-draw_mouse 0", Join(FfmpegArgs.Input(request)));
    }

    [Fact]
    public void DesktopDuplicationUsesTheMonitorIndexAndDownloadsTheFrames()
    {
        var request = Request();
        request.UseDesktopDuplication = true;
        request.MonitorIndex = 2;
        request.Rect = new CaptureRect(0, 0, 3840, 2160);

        string args = Join(FfmpegArgs.Input(request));

        Assert.Contains("-f lavfi", args);
        Assert.Contains("ddagrab=output_idx=2:framerate=60:draw_mouse=1", args);
        Assert.Contains("-vf hwdownload,format=bgra", args);
        Assert.DoesNotContain("gdigrab", args);
    }

    [Fact]
    public void AnAreaTooSmallToRecordIsRefused()
    {
        var request = Request();
        request.Rect = new CaptureRect(0, 0, 1, 1);
        Assert.Throws<ArgumentException>(() => FfmpegArgs.Input(request));
    }

    [Fact]
    public void TheSoftwareEncoderUsesQualityNotABitrate()
    {
        string args = Join(FfmpegArgs.Encoder(Request(EncoderKind.X264)));

        Assert.Contains("-c:v libx264", args);
        Assert.Contains("-preset veryfast", args);
        Assert.Contains("-crf 20", args);
        Assert.DoesNotContain("-b:v", args);
    }

    [Fact]
    public void HardwareEncodersGetABitrateWithHeadroomForMovement()
    {
        /* 1080p30 High is the reference point: 12 Mbit/s, with half again as
         * much allowed in the peaks and two seconds of buffer. */
        string nvenc = Join(FfmpegArgs.Encoder(Request(EncoderKind.Nvenc, fps: 30)));
        Assert.Contains("-c:v h264_nvenc", nvenc);
        Assert.Contains("-b:v 12000k", nvenc);
        Assert.Contains("-maxrate 18000k", nvenc);
        Assert.Contains("-bufsize 24000k", nvenc);
        Assert.Contains("-pix_fmt yuv420p", nvenc);

        string amf = Join(FfmpegArgs.Encoder(Request(EncoderKind.Amf, fps: 30)));
        Assert.Contains("-c:v h264_amf", amf);
        Assert.Contains("-b:v 12000k", amf);

        string qsv = Join(FfmpegArgs.Encoder(Request(EncoderKind.Qsv, fps: 30)));
        Assert.Contains("-c:v h264_qsv", qsv);
        Assert.Contains("-pix_fmt nv12", qsv, "Quick Sync wants nv12");
    }

    [Fact]
    public void ARecordingHasNoAudioAndIsFragmentedSoItCanBeRecovered()
    {
        string args = Join(FfmpegArgs.RecordVideo(Request()));

        Assert.Contains("-an", args);
        Assert.Contains("+frag_keyframe+empty_moov+default_base_moof", args);
        Assert.Contains(@"C:\temp\take 1.mp4", args);
        Assert.Contains("-loglevel warning", args);
    }

    [Fact]
    public void ARrecordingNeedsSomewhereToGo()
    {
        var request = Request();
        request.OutputPath = "";
        Assert.Throws<ArgumentException>(() => FfmpegArgs.RecordVideo(request));
    }

    [Fact]
    public void MuxingWithoutAudioJustCopiesThePicture()
    {
        var mux = new MuxRequest
        {
            VideoPath = "take.mp4",
            OutputPath = "out.mp4",
        };

        string args = Join(FfmpegArgs.Mux(mux));
        Assert.Contains("-map 0:v:0", args);
        Assert.Contains("-c:v copy", args);
        Assert.Contains("-an", args);
        Assert.DoesNotContain("aac", args);
    }

    [Fact]
    public void MuxingOneTrackAppliesItsFaderAndEncodesAac()
    {
        var mux = new MuxRequest
        {
            VideoPath = "take.mp4",
            SystemAudioPath = "system.wav",
            SystemGainPercent = 80,
            OutputPath = "out.mp4",
        };

        string args = Join(FfmpegArgs.Mux(mux));
        Assert.Contains("-i system.wav", args);
        Assert.Contains("-map 1:a:0", args);
        Assert.Contains("-af volume=0.80", args);
        Assert.Contains("-c:a aac", args);
        Assert.Contains("-b:a 192k", args);
        Assert.Contains("-shortest", args);
    }

    [Fact]
    public void MuxingTwoTracksMixesThemWithoutHalvingTheVolume()
    {
        var mux = new MuxRequest
        {
            VideoPath = "take.mp4",
            SystemAudioPath = "system.wav",
            MicrophonePath = "mic.wav",
            SystemGainPercent = 100,
            MicrophoneGainPercent = 150,
            OutputPath = "out.mp4",
        };

        string args = Join(FfmpegArgs.Mux(mux));
        Assert.Contains("[1:a]volume=1.00[a1]", args);
        Assert.Contains("[2:a]volume=1.50[a2]", args);
        Assert.Contains("amix=inputs=2", args);
        Assert.Contains("normalize=0", args);
        Assert.Contains("-map [aout]", args);
        Assert.Contains("-shortest", args);
    }

    [Fact]
    public void GainsAreClampedInsteadOfRefused()
    {
        var mux = new MuxRequest
        {
            VideoPath = "take.mp4",
            SystemAudioPath = "system.wav",
            SystemGainPercent = 900,
            OutputPath = "out.mp4",
        };

        Assert.Contains("volume=2.00", Join(FfmpegArgs.Mux(mux)));
    }

    [Fact]
    public void MuxingAlwaysWritesAFastStartFile()
    {
        var mux = new MuxRequest { VideoPath = "take.mp4", OutputPath = "out.mp4" };
        Assert.Contains("+faststart", Join(FfmpegArgs.Mux(mux)));
    }

    [Fact]
    public void ConcatEntriesEscapeQuotesAndUseForwardSlashes()
    {
        Assert.Equal("file 'C:/videos/take 1.mp4'", FfmpegArgs.ConcatEntry(@"C:\videos\take 1.mp4"));
        Assert.Equal(@"file 'C:/it'\''s.mp4'", FfmpegArgs.ConcatEntry(@"C:\it's.mp4"));
    }

    [Fact]
    public void AConcatListHasOneEntryPerTake()
    {
        string list = FfmpegArgs.ConcatList(new[] { "a.mp4", "b.mp4", "c.mp4" });
        string[] lines = list.Split('\n', StringSplitOptions.RemoveEmptyEntries);
        Assert.Equal(3, lines.Length);
        Assert.Equal("file 'a.mp4'", lines[0].Trim());
    }

    [Fact]
    public void TheEncoderTestActuallyEncodesAFewFrames()
    {
        string args = Join(FfmpegArgs.TestEncoder(FfmpegArgs.EncoderNvenc));
        Assert.Contains("-frames:v 3", args);
        Assert.Contains("-c:v h264_nvenc", args);
        Assert.Contains("-f null", args);
        Assert.Contains("color=c=black", args);
    }

    [Fact]
    public void TheCommandLineInTheLogCanBePastedIntoAShell()
    {
        string line = FfmpegArgs.ToCommandLine(
            @"C:\Apps\ffmpeg.exe",
            new[] { "-i", @"C:\My Videos\take 1.mp4", "-c", "copy" });

        Assert.Equal(@"C:\Apps\ffmpeg.exe -i ""C:\My Videos\take 1.mp4"" -c copy", line);
    }
}
