using System;
using System.IO;
using ScreenForge.Core;
using Xunit;

namespace ScreenForge.Tests;

public class FileNamerTests
{
    private static readonly DateTime When = new(2026, 3, 9, 14, 5, 7, DateTimeKind.Local);

    [Fact]
    public void TokensAreReplaced()
    {
        string name = FileNamer.Expand("Recording {date} {time}", When, "Display 1", "High", 60);
        Assert.Equal("Recording 2026-03-09 14-05-07", name);
    }

    [Fact]
    public void EveryDocumentedTokenWorks()
    {
        string name = FileNamer.Expand("{source}-{quality}-{fps}", When, "Monitor 2", "High", 60);
        Assert.Equal("Monitor 2-High-60", name);
    }

    [Fact]
    public void AnEmptyPatternFallsBackToTheDefault()
    {
        string name = FileNamer.Expand("   ", When, "Display", "High", 60);
        Assert.StartsWith("Recording 2026-03-09", name);
    }

    [Fact]
    public void CharactersWindowsWouldRefuseAreRemoved()
    {
        string name = FileNamer.Expand("a/b\\c:d*e?f\"g<h>i|j", When, "", "", 60);
        Assert.DoesNotContain("/", name);
        Assert.DoesNotContain("\\", name);
        Assert.DoesNotContain(":", name);
        Assert.DoesNotContain("*", name);
        Assert.DoesNotContain("?", name);
        Assert.DoesNotContain("\"", name);
        Assert.DoesNotContain("<", name);
        Assert.DoesNotContain(">", name);
        Assert.DoesNotContain("|", name);
    }

    [Fact]
    public void LongNamesAreCutAndTrailingPunctuationIsDropped()
    {
        string name = FileNamer.Sanitise(new string('x', 400) + "  ");
        Assert.True(name.Length <= 120);
        Assert.False(name.EndsWith(" "));
        Assert.False(name.EndsWith("."));
    }

    [Fact]
    public void AnEmptyResultIsNeverEmpty()
    {
        Assert.Equal("Recording", FileNamer.Sanitise(""));
        Assert.Equal("Recording", FileNamer.Sanitise("   "));
        Assert.Equal("Recording", FileNamer.Sanitise("///"));
    }

    [Fact]
    public void ANameThatExistsGetsANumber()
    {
        string folder = Path.Combine(Path.GetTempPath(), "screenforge-tests-" + Guid.NewGuid().ToString("N"));
        Directory.CreateDirectory(folder);
        try
        {
            string first = FileNamer.Unique(folder, "Recording");
            Assert.Equal(Path.Combine(folder, "Recording.mp4"), first);

            File.WriteAllText(first, "x");
            string second = FileNamer.Unique(folder, "Recording");
            Assert.Equal(Path.Combine(folder, "Recording (2).mp4"), second);

            File.WriteAllText(second, "x");
            Assert.Equal(Path.Combine(folder, "Recording (3).mp4"), FileNamer.Unique(folder, "Recording"));
        }
        finally
        {
            Directory.Delete(folder, true);
        }
    }

    [Fact]
    public void TheExtensionIsNotDoubled()
    {
        string folder = Path.GetTempPath();
        string path = FileNamer.Unique(folder, "Nothing-here-" + Guid.NewGuid().ToString("N"), "mp4");
        Assert.EndsWith(".mp4", path);
        Assert.DoesNotContain(".mp4.mp4", path);
    }
}
