using System;
using System.IO;
using ScreenForge.Core;
using Xunit;

namespace ScreenForge.Tests;

public class SettingsTests
{
    private static string TempFile() =>
        Path.Combine(Path.GetTempPath(), "screenforge-" + Guid.NewGuid().ToString("N") + ".json");

    [Fact]
    public void AMissingFileGivesDefaults()
    {
        AppSettings settings = SettingsStore.Load(TempFile(), @"C:\Videos");

        Assert.Equal(@"C:\Videos", settings.OutputFolder);
        Assert.Equal(60, settings.Fps);
        Assert.Equal(QualityPreset.High, settings.Quality);
        Assert.Equal(EncoderKind.Auto, settings.Encoder);
        Assert.Equal(3, settings.CountdownSeconds);
        Assert.True(settings.RecordSystem);
        Assert.False(settings.RecordMicrophone);
        Assert.False(settings.ShowMouseClicks);
    }

    [Fact]
    public void SavingAndLoadingKeepsTheChoices()
    {
        string path = TempFile();
        try
        {
            AppSettings settings = SettingsStore.CreateDefault(@"C:\Videos");
            settings.Fps = 30;
            settings.Quality = QualityPreset.Medium;
            settings.Encoder = EncoderKind.Nvenc;
            settings.NamePattern = "{source} {time}";
            settings.RecordMicrophone = true;
            settings.ShowMouseClicks = true;
            settings.MicrophoneGainPercent = 140;
            settings.SourceKind = CaptureKind.Window;

            SettingsStore.Save(path, settings);
            AppSettings loaded = SettingsStore.Load(path, @"C:\Videos");

            Assert.Equal(30, loaded.Fps);
            Assert.Equal(QualityPreset.Medium, loaded.Quality);
            Assert.Equal(EncoderKind.Nvenc, loaded.Encoder);
            Assert.Equal("{source} {time}", loaded.NamePattern);
            Assert.True(loaded.RecordMicrophone);
            Assert.True(loaded.ShowMouseClicks);
            Assert.Equal(140, loaded.MicrophoneGainPercent);
            Assert.Equal(CaptureKind.Window, loaded.SourceKind);
        }
        finally
        {
            if (File.Exists(path)) File.Delete(path);
        }
    }

    [Fact]
    public void SettingsFromBeforeClickHighlightsKeepTheFeatureOff()
    {
        string path = TempFile();
        try
        {
            File.WriteAllText(path, "{ \"CaptureCursor\": true }");
            Assert.False(SettingsStore.Load(path, @"C:\Videos").ShowMouseClicks);
        }
        finally
        {
            if (File.Exists(path)) File.Delete(path);
        }
    }

    [Fact]
    public void ACorruptFileIsIgnoredInsteadOfCrashingTheApp()
    {
        string path = TempFile();
        try
        {
            File.WriteAllText(path, "{ this is not json ");
            AppSettings settings = SettingsStore.Load(path, @"C:\Videos");
            Assert.Equal(60, settings.Fps);
        }
        finally
        {
            if (File.Exists(path)) File.Delete(path);
        }
    }

    [Fact]
    public void HandEditedNonsenseIsClamped()
    {
        var settings = new AppSettings
        {
            Fps = 47,
            CountdownSeconds = 99,
            SystemGainPercent = 500,
            MicrophoneGainPercent = -20,
            NamePattern = "   ",
            OutputFolder = "",
            SelectedMonitor = -3,
        };

        AppSettings fixedUp = SettingsStore.Normalise(settings, @"C:\Videos");

        Assert.Equal(48, fixedUp.Fps);
        Assert.Equal(10, fixedUp.CountdownSeconds);
        Assert.Equal(200, fixedUp.SystemGainPercent);
        Assert.Equal(0, fixedUp.MicrophoneGainPercent);
        Assert.Equal(FileNamer.DefaultPattern, fixedUp.NamePattern);
        Assert.Equal(@"C:\Videos", fixedUp.OutputFolder);
        Assert.Equal(0, fixedUp.SelectedMonitor);
    }

    [Fact]
    public void UnknownEnumsFallBackRatherThanThrow()
    {
        var settings = new AppSettings { Quality = (QualityPreset)42, Encoder = (EncoderKind)99 };
        AppSettings fixedUp = SettingsStore.Normalise(settings, @"C:\Videos");

        Assert.Equal(QualityPreset.High, fixedUp.Quality);
        Assert.Equal(EncoderKind.Auto, fixedUp.Encoder);
    }
}
