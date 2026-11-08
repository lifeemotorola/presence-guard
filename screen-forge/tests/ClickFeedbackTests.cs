using ScreenForge.Core;
using Xunit;

namespace ScreenForge.Tests;

public class ClickFeedbackTests
{
    [Theory]
    [InlineData(0x0201, MouseClickButton.Left)]
    [InlineData(0x0204, MouseClickButton.Right)]
    [InlineData(0x0207, MouseClickButton.Middle)]
    public void OnlyTheButtonDownMessageShowsItsCorrespondingClick(int message, MouseClickButton expected)
    {
        Assert.True(ClickFeedback.TryMapMessage(message, out MouseClickButton actual));
        Assert.Equal(expected, actual);
    }

    [Theory]
    [InlineData(0x0200)] // motion
    [InlineData(0x0202)] // left up
    [InlineData(0x0205)] // right up
    [InlineData(0x0208)] // middle up
    [InlineData(0x020A)] // scroll wheel
    [InlineData(-1)]
    public void MovingReleasingOrScrollingDoesNotCreateAHighlight(int message)
    {
        Assert.False(ClickFeedback.TryMapMessage(message, out _));
    }

    [Fact]
    public void CapturedAreaUsesVirtualDesktopCoordinatesAndExclusiveEdges()
    {
        var rect = new CaptureRect(-1920, -300, 1920, 1080);
        Assert.True(ClickFeedback.Contains(rect, -1920, -300));
        Assert.True(ClickFeedback.Contains(rect, -1, 779));
        Assert.False(ClickFeedback.Contains(rect, -1921, 0));
        Assert.False(ClickFeedback.Contains(rect, 0, 0));
        Assert.False(ClickFeedback.Contains(rect, -1, 780));
        Assert.False(ClickFeedback.Contains(new CaptureRect(0, 0, 0, 1), 0, 0));
    }

    [Fact]
    public void AreaComparisonDoesNotOverflowForLargeVirtualCoordinates()
    {
        var rect = new CaptureRect(int.MaxValue - 50, 10, 100, 10);
        Assert.True(ClickFeedback.Contains(rect, int.MaxValue, 10));
        Assert.False(ClickFeedback.Contains(rect, int.MaxValue - 51, 10));
    }

    [Fact]
    public void ClickPulseExpandsAndFadesWithoutLeavingItsWindow()
    {
        Assert.Equal(0, ClickFeedback.Progress(-1));
        Assert.Equal(1, ClickFeedback.Progress(100000));
        Assert.True(ClickFeedback.Radius(300) > ClickFeedback.Radius(0));
        Assert.True(ClickFeedback.Opacity(0) > ClickFeedback.Opacity(300));
        Assert.Equal(0, ClickFeedback.Opacity(ClickFeedback.DurationMilliseconds));
        Assert.True(ClickFeedback.Radius(ClickFeedback.DurationMilliseconds) + 5 < ClickFeedback.WindowSize / 2);
        Assert.True(ClickFeedback.MaximumVisible > 0);
    }
}
