using System.IO;
using ScreenForge.Core;
using Xunit;

namespace ScreenForge.Tests;

public class FfmpegLocatorTests
{
    [Fact]
    public void TheShippedCopyIsLookedForFirst()
    {
        var candidates = FfmpegLocator.Candidates(@"C:\Apps\ScreenForge", @"C:\Users\me\AppData", @"C:\Tools");

        Assert.Equal(@"C:\Apps\ScreenForge\ffmpeg.exe", candidates[0]);
        Assert.Contains(@"C:\Apps\ScreenForge\ffmpeg\ffmpeg.exe", candidates);
        Assert.Contains(@"C:\Tools\ffmpeg.exe", candidates);
    }

    [Fact]
    public void PathEntriesAreIncludedButNotDuplicated()
    {
        string path = @"C:\Apps\ScreenForge;C:\Tools;;C:\Tools";
        var candidates = FfmpegLocator.Candidates(@"C:\Apps\ScreenForge", null, path);

        int occurrences = candidates.FindAll(c => c == @"C:\Tools\ffmpeg.exe").Count;
        Assert.Equal(1, occurrences);
    }

    [Fact]
    public void FindingReturnsTheFirstThatExists()
    {
        string folder = Path.Combine(Path.GetTempPath(), "screenforge-bin-" + System.Guid.NewGuid().ToString("N"));
        Directory.CreateDirectory(folder);
        try
        {
            string expected = Path.Combine(folder, "ffmpeg.exe");
            File.WriteAllText(expected, "not really ffmpeg");

            Assert.Equal(expected, FfmpegLocator.Find(folder, null, null));
        }
        finally
        {
            Directory.Delete(folder, true);
        }
    }

    [Fact]
    public void NothingAnywhereIsNullNotAnException()
    {
        Assert.Null(FfmpegLocator.Find(null, null, null));
        Assert.Null(FfmpegLocator.Find("/nowhere/at/all", null, null));
        Assert.Empty(FfmpegLocator.Candidates(null, null, null));
    }
}
