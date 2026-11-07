using System.IO;
using ScreenForge.Core;
using Xunit;

namespace ScreenForge.Tests;

public class FfmpegLocatorTests
{
    /* The locator joins folder names with the platform's own separator, so the
     * expectations have to as well — otherwise these tests only pass on Windows,
     * and they run on Linux. */
    private static string Exe(string folder) => Path.Combine(folder, FfmpegLocator.ExecutableName);

    [Fact]
    public void TheShippedCopyIsLookedForFirst()
    {
        var candidates = FfmpegLocator.Candidates(@"C:\Apps\ScreenForge", @"C:\Users\me\AppData", @"C:\Tools");

        Assert.Equal(Exe(@"C:\Apps\ScreenForge"), candidates[0]);
        Assert.Contains(Exe(@"C:\Apps\ScreenForge\ffmpeg"), candidates);
        Assert.Contains(Exe(@"C:\Apps\ScreenForge\bin"), candidates);
        Assert.Contains(Exe(@"C:\Users\me\AppData\ffmpeg"), candidates);
        Assert.Contains(Exe(@"C:\Tools"), candidates);
    }

    [Fact]
    public void PathEntriesAreIncludedButNotDuplicated()
    {
        string separator = Path.PathSeparator.ToString();
        string path = string.Join(separator, new[] { @"C:\Apps\ScreenForge", @"C:\Tools", "", @"C:\Tools" });

        var candidates = FfmpegLocator.Candidates(@"C:\Apps\ScreenForge", null, path);

        int occurrences = candidates.FindAll(candidate => candidate == Exe(@"C:\Tools")).Count;
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
