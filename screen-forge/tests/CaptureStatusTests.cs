using System;
using System.Diagnostics;
using ScreenForge.Core;
using Xunit;

namespace ScreenForge.Tests;

public class CaptureStatusTests
{
    private static Process StartShell(string windowsCommand, string unixCommand)
    {
        bool windows = OperatingSystem.IsWindows();
        var info = new ProcessStartInfo(windows ? "cmd.exe" : "/bin/sh")
        {
            UseShellExecute = false,
            CreateNoWindow = true,
        };
        info.ArgumentList.Add(windows ? "/c" : "-c");
        info.ArgumentList.Add(windows ? windowsCommand : unixCommand);
        return Process.Start(info);
    }

    [Fact]
    public void NoActiveTakeIsNotReportedAsAFailure()
    {
        Assert.False(CaptureStatus.TryDescribeUnexpectedExit(null, "", out string reason));
        Assert.Null(reason);
    }

    [Fact]
    public void RunningProcessIsNotReportedAsStopped()
    {
        using var process = StartShell("ping -n 4 127.0.0.1 >nul", "sleep 3");
        try
        {
            Assert.False(CaptureStatus.TryDescribeUnexpectedExit(process, "", out string reason));
            Assert.Null(reason);
        }
        finally
        {
            if (!process.HasExited)
                process.Kill(true);
            process.WaitForExit();
        }
    }

    [Fact]
    public void UnexpectedExitIncludesTheCodeAndRecentError()
    {
        using var process = StartShell("exit /b 7", "exit 7");
        Assert.True(process.WaitForExit(5000));
        Assert.True(CaptureStatus.TryDescribeUnexpectedExit(process, "encoder failed", out string reason));
        Assert.Contains("7", reason);
        Assert.Contains("encoder failed", reason);
    }

    [Fact]
    public void EvenAnUnexpectedSuccessfulExitEndsTheTake()
    {
        using var process = StartShell("exit /b 0", "exit 0");
        Assert.True(process.WaitForExit(5000));
        Assert.True(CaptureStatus.TryDescribeUnexpectedExit(process, null, out string reason));
        Assert.Contains("exit code 0", reason);
    }
}
