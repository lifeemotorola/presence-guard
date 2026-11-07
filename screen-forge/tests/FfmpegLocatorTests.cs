using System.IO;
using ScreenForge.Core;
using Xunit;

namespace ScreenForge.Tests;

public class FfmpegLocatorTests
{
    /* The locator builds its paths with Path.Combine, and on the Linux runner a
     * backslash is just a character in a folder name. Mirroring the joins keeps
     * these tests about the search order instead of about the separator. */
    private static string Exe(params string[] parts)
    {
        var all = new string[parts.Length + 1];
        parts.CopyTo(all, 0);
        all[parts.Length] = FfmpegLocator.ExecutableName;
        return Path.Combine(all);
    }

    [Fact]
    public void TheShippedCopyIsLookedForFirst()
    {
        /* The PATH string is split on the separator, and on the Linux runner the
         * separator is a colon — so a Windows "C:\Tools" would be split into "C"
         * and "\Tools" before the locator ever saw it. Plain folder names keep
         * the test about the search order. */
        var candidates = FfmpegLocator.Candidates(@"C:\Apps\ScreenForge", @"C:\Users\me\AppData", "tools");

        Assert.Equal(Exe(@"C:\Apps\ScreenForge"), candidates[0]);
        Assert.Contains(Exe(@"C:\Apps\ScreenForge", "ffmpeg"), candidates);
        Assert.Contains(Exe(@"C:\Apps\ScreenForge", "bin"), candidates);
        Assert.Contains(Exe(@"C:\Users\me\AppData", "ffmpeg"), candidates);
        Assert.Contains(Exe("tools"), candidates);
        Assert.Equal(candidates.Count - 1, candidates.IndexOf(Exe("tools")));
    }

    [Fact]
    public void PathEntriesAreIncludedButNotDuplicated()
    {
        string folder = Path.Combine(Path.GetTempPath(), "screenforge-on-path");
        string path = string.Join(Path.PathSeparator.ToString(), new[] { folder, folder, "" });

        var candidates = FfmpegLocator.Candidates(null, null, path);

        Assert.Single(candidates);
        Assert.Equal(Exe(folder), candidates[0]);
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
