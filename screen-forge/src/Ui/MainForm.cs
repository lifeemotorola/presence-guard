/*
ScreenForge
Copyright (C) 2026 Emmanuel Suah

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.
*/

using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Drawing;
using System.IO;
using System.Threading.Tasks;
using System.Windows.Forms;
using Timer = System.Windows.Forms.Timer;
using ScreenForge.Audio;
using ScreenForge.Core;
using ScreenForge.Interop;
using ScreenForge.Recording;

namespace ScreenForge.Ui;

public partial class MainForm : Form
{
    private readonly List<DisplayInfo> displays = new();
    private List<WindowInfo> windows = new();

    private AppSettings settings = new();
    private string settingsPath = "";
    private string ffmpegPath = "";
    private CaptureRect? region;

    private RecordingSession session;
    private Hotkeys hotkeys;
    private NotifyIcon tray;
    private FloatingTimer chip;
    private Timer heartbeat;
    private bool busy;

    /* Hotkeys repeat while a key is held; a quarter of a second apart is a
     * repeat, not a second decision. */
    private long lastToggleTicks;

    /* Set when ffmpeg dies mid-take, so the stop that follows can say why. */
    private string lostCaptureReason;

    private ToolStripMenuItem trayToggle;
    private ToolStripMenuItem trayPause;

    public MainForm()
    {
        BuildUi();
        WireEvents();
    }

    protected override void OnLoad(EventArgs e)
    {
        base.OnLoad(e);

        settingsPath = Path.Combine(
            Environment.GetFolderPath(Environment.SpecialFolder.ApplicationData),
            "ScreenForge",
            "settings.json");

        settings = SettingsStore.Load(settingsPath, DefaultOutputFolder());

        FitToScreen();

        LoadSettingsIntoUi();
        RefreshSources();
        RefreshMicrophones();
        LocateFfmpeg();

        hotkeys = new Hotkeys(Handle);
        hotkeys.ToggleRequested += () => BeginInvoke(new Action(ToggleRecording));
        hotkeys.PauseRequested += () => BeginInvoke(new Action(TogglePause));
        hotkeys.Register();

        heartbeat = new Timer { Interval = 250 };
        heartbeat.Tick += (_, _) => Tick();
        heartbeat.Start();

        CreateTray();
        AppendLog("ScreenForge 1.0.1 — press Ctrl+Shift+F9 to start or stop.");
        AppendLog(hotkeys.Status);
        UpdateSummary();
    }

    protected override void OnFormClosing(FormClosingEventArgs e)
    {
        if (session != null)
        {
            DialogResult answer = MessageBox.Show(
                this,
                "A recording is still running. Stop it and save, or keep recording?",
                "Still recording",
                MessageBoxButtons.YesNoCancel,
                MessageBoxIcon.Warning,
                MessageBoxDefaultButton.Button1);

            if (answer == DialogResult.Cancel)
            {
                e.Cancel = true;
                return;
            }

            if (answer == DialogResult.Yes)
            {
                /* Saving takes a moment; let the message loop run so the app
                 * closes after the file is finished rather than during it. */
                e.Cancel = true;
                BeginInvoke(new Action(async () =>
                {
                    await StopRecordingAsync();
                    Close();
                }));
                return;
            }
        }

        SaveSettings();
        tray?.Dispose();
        hotkeys?.Dispose();
        chip?.Dispose();
        base.OnFormClosing(e);
    }

    protected override void WndProc(ref Message message)
    {
        if (hotkeys != null && hotkeys.HandleMessage(message.Msg, message.WParam))
            return;

        base.WndProc(ref message);
    }

    private void WireEvents()
    {
        Shown += (_, _) =>
        {
            RelayoutCards();

            if (string.IsNullOrEmpty(ffmpegPath))
                WarnAboutFfmpeg();
        };

        Resize += (_, _) => RelayoutCards();
    }

    /// <summary>
    /// Opens at a size that fits the screen it was launched on. The old fixed
    /// 980×780 window did not fit a 1366×768 laptop once Windows scaled the UI
    /// to 125%, and the bottom of the window is where the buttons were.
    /// </summary>
    private void FitToScreen()
    {
        Rectangle area = Screen.PrimaryScreen != null
            ? Screen.PrimaryScreen.WorkingArea
            : new Rectangle(0, 0, 1280, 800);

        int width = Math.Min(1080, Math.Max(840, area.Width - 80));
        int height = Math.Min(820, Math.Max(560, area.Height - 80));

        MinimumSize = new Size(Math.Min(840, width), Math.Min(560, height));
        ClientSize = new Size(width, height);
        StartPosition = FormStartPosition.CenterScreen;
    }

    private static string DefaultOutputFolder()
    {
        string videos = Environment.GetFolderPath(Environment.SpecialFolder.MyVideos);
        if (string.IsNullOrWhiteSpace(videos))
            videos = Environment.GetFolderPath(Environment.SpecialFolder.UserProfile);

        return Path.Combine(videos, "ScreenForge");
    }

    private void LocateFfmpeg()
    {
        string appFolder = AppContext.BaseDirectory;
        string appData = Environment.GetFolderPath(Environment.SpecialFolder.ApplicationData);

        ffmpegPath = FfmpegLocator.Find(appFolder, appData, Environment.GetEnvironmentVariable("PATH"));

        if (string.IsNullOrEmpty(ffmpegPath))
        {
            AppendLog("ffmpeg.exe not found — put it next to ScreenForge.exe.");
            return;
        }

        AppendLog("ffmpeg: " + ffmpegPath);
    }

    private void LoadSettingsIntoUi()
    {
        txtFolder.Text = settings.OutputFolder;
        txtPattern.Text = settings.NamePattern;
        chkHide.Checked = settings.HideWhileRecording;
        chkCursor.Checked = settings.CaptureCursor;
        chkDuplication.Checked = settings.UseDesktopDuplication;
        chkSystem.Checked = settings.RecordSystem;
        chkMicrophone.Checked = settings.RecordMicrophone;

        numSystemGain.Value = Math.Clamp(settings.SystemGainPercent, 0, 200);
        numMicGain.Value = Math.Clamp(settings.MicrophoneGainPercent, 0, 200);

        cboCountdown.SelectedItem = Math.Clamp(settings.CountdownSeconds, 0, 10);
        if (cboCountdown.SelectedIndex < 0)
            cboCountdown.SelectedItem = 3;

        cboFps.SelectedItem = Presets.ClampFps(settings.Fps);
        cboQuality.SelectedIndex = (int)settings.Quality;
        cboEncoder.SelectedIndex = (int)settings.Encoder;

        cboSource.SelectedIndex = SourceIndex(settings.SourceKind);
    }

    private void SaveSettings()
    {
        if (settings == null)
            return;

        settings.Fps = cboFps.SelectedItem is int fps ? fps : settings.Fps;
        settings.Quality = (QualityPreset)Math.Max(0, cboQuality.SelectedIndex);
        settings.Encoder = (EncoderKind)Math.Max(0, cboEncoder.SelectedIndex);
        settings.CaptureCursor = chkCursor.Checked;
        settings.UseDesktopDuplication = chkDuplication.Checked;
        settings.RecordSystem = chkSystem.Checked;
        settings.RecordMicrophone = chkMicrophone.Checked;
        settings.SystemGainPercent = (int)numSystemGain.Value;
        settings.MicrophoneGainPercent = (int)numMicGain.Value;
        settings.MicrophoneDeviceId = (cboMicrophone.SelectedItem as AudioDeviceInfo)?.Id ?? "";
        settings.OutputFolder = txtFolder.Text.Trim();
        settings.NamePattern = txtPattern.Text.Trim();
        settings.CountdownSeconds = cboCountdown.SelectedItem is int countdown ? countdown : 3;
        settings.HideWhileRecording = chkHide.Checked;
        settings.SourceKind = SourceKind();

        if (settings.SourceKind == CaptureKind.Monitor)
            settings.SelectedMonitor = Math.Max(0, cboSourceDetail.SelectedIndex);

        try
        {
            SettingsStore.Save(settingsPath, settings);
        }
        catch (Exception ex)
        {
            AppendLog("could not save settings: " + ex.Message);
        }
    }

    // ---------------------------------------------------------------- sources

    private void RefreshSources()
    {
        displays.Clear();
        displays.AddRange(Displays.All());

        windows = WindowList.Visible();

        int monitorChoice = Math.Max(0, cboSourceDetail.SelectedIndex);
        PopulateSourceDetail();

        if (cboSource.SelectedIndex == 0 && monitorChoice < cboSourceDetail.Items.Count)
            cboSourceDetail.SelectedIndex = monitorChoice;

        UpdateSummary();
    }

    private void PopulateSourceDetail()
    {
        cboSourceDetail.Items.Clear();

        switch (SourceKind())
        {
            case CaptureKind.Window:
                foreach (WindowInfo window in windows)
                    cboSourceDetail.Items.Add(window);

                if (cboSourceDetail.Items.Count == 0)
                    cboSourceDetail.Items.Add("No visible windows — open the one you want, then press Refresh");

                cboSourceDetail.SelectedIndex = 0;
                break;

            case CaptureKind.Region:
                cboSourceDetail.Items.Add(region.HasValue ? DescribeRegion(region.Value) : "No region drawn yet");
                cboSourceDetail.SelectedIndex = 0;
                break;

            default:
                foreach (DisplayInfo display in displays)
                    cboSourceDetail.Items.Add(display);

                cboSourceDetail.SelectedIndex = Math.Clamp(settings.SelectedMonitor, 0, Math.Max(0, cboSourceDetail.Items.Count - 1));
                break;
        }
    }

    private CaptureKind SourceKind() => cboSource.SelectedIndex switch
    {
        1 => CaptureKind.Window,
        2 => CaptureKind.Region,
        _ => CaptureKind.Monitor,
    };

    private void OnSourceKindChanged()
    {
        PopulateSourceDetail();
        btnPickRegion.Enabled = SourceKind() == CaptureKind.Region;
        cboSourceDetail.Enabled = SourceKind() != CaptureKind.Region;
        chkDuplication.Enabled = SourceKind() == CaptureKind.Monitor;
        UpdateSummary();
    }

    private void RefreshMicrophones()
    {
        string previous = (cboMicrophone.SelectedItem as AudioDeviceInfo)?.Id ?? settings.MicrophoneDeviceId;

        cboMicrophone.Items.Clear();

        List<AudioDeviceInfo> microphones = AudioDevices.Microphones();
        foreach (AudioDeviceInfo microphone in microphones)
            cboMicrophone.Items.Add(microphone);

        if (cboMicrophone.Items.Count == 0)
        {
            cboMicrophone.Items.Add("No microphone found");
            cboMicrophone.SelectedIndex = 0;
            chkMicrophone.Enabled = false;
            return;
        }

        chkMicrophone.Enabled = true;

        int index = 0;
        for (int i = 0; i < microphones.Count; i++)
        {
            if (microphones[i].Id == previous)
            {
                index = i;
                break;
            }
        }

        cboMicrophone.SelectedIndex = index;
    }

    private void OnMicrophoneToggled()
    {
        cboMicrophone.Enabled = chkMicrophone.Checked && cboMicrophone.Items.Count > 0 && cboMicrophone.SelectedItem is AudioDeviceInfo;
        numMicGain.Enabled = chkMicrophone.Checked;
        UpdateSummary();
    }

    private static int SourceIndex(CaptureKind kind) => kind switch
    {
        CaptureKind.Window => 1,
        CaptureKind.Region => 2,
        _ => 0,
    };

    private CaptureTarget CurrentTarget()
    {
        switch (SourceKind())
        {
            case CaptureKind.Window:
                if (cboSourceDetail.SelectedItem is WindowInfo window)
                {
                    return new CaptureTarget
                    {
                        Kind = CaptureKind.Window,
                        Rect = window.Bounds,
                        Description = Shorten(window.Title, 40),
                    };
                }

                return null;

            case CaptureKind.Region:
                if (region.HasValue)
                {
                    return new CaptureTarget
                    {
                        Kind = CaptureKind.Region,
                        Rect = region.Value,
                        Description = DescribeRegion(region.Value),
                    };
                }

                return null;

            default:
                DisplayInfo display = cboSourceDetail.SelectedItem as DisplayInfo
                    ?? (displays.Count > 0 ? displays[0] : null);

                if (display == null)
                    return null;

                return new CaptureTarget
                {
                    Kind = CaptureKind.Monitor,
                    Rect = display.Bounds,
                    Description = $"Display {display.Index + 1}",
                };
        }
    }

    private async Task PickRegionAsync()
    {
        bool wasVisible = Visible;
        Hide();
        await Task.Delay(180);

        try
        {
            using var selector = new RegionSelector(displays);
            DialogResult answer = selector.ShowDialog();

            if (answer == DialogResult.OK)
            {
                region = selector.Selection;
                cboSource.SelectedIndex = 2;
                PopulateSourceDetail();
                AppendLog("region: " + DescribeRegion(region.Value));
            }
        }
        catch (Exception ex)
        {
            AppendLog("region picker failed: " + ex.Message);
        }
        finally
        {
            if (wasVisible)
            {
                Show();
                WindowState = FormWindowState.Normal;
                Activate();
            }
        }

        UpdateSummary();
    }

    // ------------------------------------------------------------ recording

    private async Task OnRecordClickedAsync()
    {
        if (session != null)
            await StopRecordingAsync();
        else
            await StartRecordingAsync();
    }

    private async Task OnPauseClickedAsync()
    {
        if (session == null || busy)
            return;

        busy = true;
        try
        {
            RecordingSession live = session;

            if (live.Paused)
            {
                /* Starting a take spawns ffmpeg and waits for it to survive its
                 * first moments; that is not work for the UI thread. */
                await Task.Run(() => live.Resume());
                btnPause.Text = "Pause";
                lblStatus.Text = "Recording…";
                AppendLog("resumed");
            }
            else
            {
                await Task.Run(() => live.Pause());
                btnPause.Text = "Resume";
                lblStatus.Text = "Paused — the takes are joined when you stop.";
                AppendLog("paused");
            }
        }
        catch (Exception ex)
        {
            AppendLog("pause failed: " + ex.Message);
            Warn("Pausing failed: " + ex.Message);
            btnPause.Text = session != null && session.Paused ? "Resume" : "Pause";
        }
        finally
        {
            busy = false;
        }
    }

    private async Task StartRecordingAsync()
    {
        if (session != null || busy)
            return;

        if (string.IsNullOrEmpty(ffmpegPath))
        {
            WarnAboutFfmpeg();
            return;
        }

        CaptureTarget target = CurrentTarget();
        if (target == null)
        {
            Warn("Choose what to record first: a display, a window, or draw a region.");
            return;
        }

        if (target.Rect.Width < 32 || target.Rect.Height < 32)
        {
            Warn("That area is too small to record.");
            return;
        }

        SaveSettings();

        string folder = settings.OutputFolder;
        try
        {
            Directory.CreateDirectory(folder);
        }
        catch (Exception)
        {
            folder = DefaultOutputFolder();
            Directory.CreateDirectory(folder);
            txtFolder.Text = folder;
        }

        string baseName = FileNamer.Expand(
            settings.NamePattern,
            DateTime.Now,
            target.Description,
            settings.Quality.ToString(),
            settings.Fps);

        string outputPath = FileNamer.Unique(folder, baseName);

        if (settings.CountdownSeconds > 0)
        {
            // A hotkey pressed during the modal countdown must not start a
            // second countdown or create a second capture session.
            busy = true;
            try
            {
                using var countdown = new CountdownForm(settings.CountdownSeconds, target, displays);
                countdown.ShowDialog(this);
                if (countdown.Cancelled)
                {
                    lblStatus.Text = "Cancelled.";
                    return;
                }
            }
            finally
            {
                busy = false;
            }
        }

        busy = true;
        lostCaptureReason = null;
        btnRecord.Enabled = false;
        SetRecordingUi(false);
        btnRecord.Enabled = false;
        lblStatus.Text = "Starting…";

        RecordingSession created = null;

        try
        {
            created = await RecordingSession.CreateAsync(ffmpegPath, settings, target, outputPath, AppendLog);
            await Task.Run(() => created.Start());

            session = created;
            KeepAwake.Begin();
            AppendLog($"recording {target.Description} → {outputPath}");

            if (settings.HideWhileRecording)
                Hide();

            ShowChip(target);
            SetRecordingUi(true);
            lblStatus.Text = $"Recording {target.Description}. Press Ctrl+Shift+F9 to stop.";
            tray?.ShowBalloonTip(2000, "Recording", "ScreenForge is recording. Ctrl+Shift+F9 stops.", ToolTipIcon.Info);
        }
        catch (Exception ex)
        {
            AppendLog("could not start: " + ex.Message);

            try
            {
                created?.Dispose();
            }
            catch (Exception)
            {
                /* nothing left to release */
            }

            KeepAwake.End();
            SetRecordingUi(false);
            lblStatus.Text = "Ready.";
            Warn("The recording did not start.\n\n" + ex.Message);
        }
        finally
        {
            busy = false;
            btnRecord.Enabled = true;
        }
    }

    private async Task StopRecordingAsync()
    {
        if (session == null || busy)
            return;

        busy = true;
        HideChip();

        string lost = lostCaptureReason;
        lostCaptureReason = null;

        RecordingSession finishing = session;
        lblStatus.Text = "Finishing the file…";
        btnPause.Enabled = false;

        RecordingResult result;

        try
        {
            // StopTake waits for ffmpeg and audio devices. Keep the UI responsive.
            result = await Task.Run(() => finishing.StopAsync());
        }
        catch (Exception ex)
        {
            result = new RecordingResult { Success = false, Message = ex.Message };
        }
        finally
        {
            session = null;
            finishing.Dispose();
            KeepAwake.End();
            busy = false;
            RestoreAfterRecording();
            SetRecordingUi(false);
        }

        lblStatus.Text = result.Success
            ? $"Saved {Path.GetFileName(result.Path)} ({Megabytes(result.Bytes)})"
            : "Ready.";

        if (result.Success)
        {
            AppendLog($"saved {result.Path} ({Megabytes(result.Bytes)}, {Clock(result.Duration)})");

            if (lost != null)
            {
                /* The capture died; the user did not press stop. Say so, and say
                 * what survived, instead of showing a cheerful "Recording saved". */
                Warn(
                    "The recording stopped on its own.\n\n" + lost +
                    "\n\nWhat had been recorded up to that moment is saved:\n" + result.Path +
                    ShortNote(result));
            }
            else
            {
                DialogResult open = MessageBox.Show(
                    this,
                    $"Saved:\n{result.Path}\n\n{Megabytes(result.Bytes)} · {Clock(result.Duration)} · {result.Takes} take(s)" +
                    ShortNote(result) + "\n\nOpen the folder?",
                    "Recording saved",
                    MessageBoxButtons.YesNo,
                    MessageBoxIcon.Information);

                if (open == DialogResult.Yes)
                    OpenOutputFolder();
            }
        }
        else
        {
            AppendLog("recording failed: " + result.Message);
            Warn((lost == null ? "The recording could not be finished." : "The recording stopped unexpectedly.\n\n" + lost) +
                 "\n\n" + result.Message +
                 (string.IsNullOrEmpty(result.LogPath) ? "" : $"\n\nDetails: {result.LogPath}"));
        }
    }

    private void ToggleRecording()
    {
        long now = Environment.TickCount64;

        if (now - lastToggleTicks < 400)
            return;

        lastToggleTicks = now;

        if (session != null)
            _ = StopRecordingAsync();
        else
            _ = StartRecordingAsync();
    }

    private void HandleCaptureFailure(string reason)
    {
        if (session == null || busy)
            return;

        lostCaptureReason = reason;
        AppendLog("capture lost: " + reason);
        _ = StopRecordingAsync();
    }

    private void TogglePause()
    {
        if (session == null)
            return;

        _ = OnPauseClickedAsync();
    }

    private void SetRecordingUi(bool recording)
    {
        btnRecord.Text = recording ? "■   Stop and save" : "●   Record";
        btnRecord.Tone = recording ? ButtonTone.Danger : ButtonTone.Primary;
        btnRecord.Enabled = true;

        btnPause.Enabled = recording;
        btnPause.Text = "Pause";

        cboSource.Enabled = !recording;
        cboSourceDetail.Enabled = !recording && SourceKind() != CaptureKind.Region;
        cboFps.Enabled = !recording;
        cboQuality.Enabled = !recording;
        cboEncoder.Enabled = !recording;
        chkCursor.Enabled = !recording;
        chkDuplication.Enabled = !recording && SourceKind() == CaptureKind.Monitor;
        chkSystem.Enabled = !recording;
        chkMicrophone.Enabled = !recording && cboMicrophone.Items.Count > 0 && cboMicrophone.SelectedItem is AudioDeviceInfo;
        txtFolder.Enabled = !recording;
        txtPattern.Enabled = !recording;
        cboCountdown.Enabled = !recording;
        btnPickRegion.Enabled = !recording && SourceKind() == CaptureKind.Region;
        btnRefreshSources.Enabled = !recording;

        if (trayToggle != null)
            trayToggle.Text = recording ? "Stop and save" : "Start recording";

        if (!recording)
            lblTimer.Text = "00:00:00";

        UpdateSummary();
    }

    private void Tick()
    {
        if (session == null)
        {
            if (lblTimer.Text != "00:00:00" && !busy)
            {
                lblTimer.Text = "00:00:00";
                lblTimer.ForeColor = Theme.Muted;
                lblEstimate.Text = "";
            }

            return;
        }

        // A dead ffmpeg must not leave a running timer over a frozen file.
        // Skip transitional states such as pause/stop, which intentionally end it.
        if (!busy && session.TryGetCaptureFailure(out string reason))
        {
            HandleCaptureFailure(reason);
            return;
        }

        TimeSpan elapsed = session.Elapsed;
        lblTimer.Text = Clock(elapsed);

        int bitrate = Presets.BitrateKbps(
            session.Rect.Width,
            session.Rect.Height,
            settings.Fps,
            settings.Quality);

        long estimate = Presets.EstimatedBytes(bitrate, elapsed, settings.RecordSystem || settings.RecordMicrophone);

        lblTimer.ForeColor = session.Paused ? Theme.Accent : Theme.Text;
        lblEstimate.Text = session.Paused ? "paused" : "≈ " + Megabytes(estimate);

        if (tray != null)
            tray.Text = $"ScreenForge — {(session.Paused ? "paused" : "recording")} {Clock(elapsed)}";
    }

    private void UpdateSummary()
    {
        CaptureTarget target = CurrentTarget();
        if (target == null)
        {
            if (!busy && session == null)
                lblStatus.Text = "Choose a source.";
            return;
        }

        if (session != null)
            return;

        int fps = cboFps.SelectedItem is int f ? f : 60;
        QualityPreset quality = (QualityPreset)Math.Max(0, cboQuality.SelectedIndex);

        CaptureRect rect = Presets.Normalise(target.Rect);
        int bitrate = Presets.BitrateKbps(rect.Width, rect.Height, fps, quality);
        long perMinute = Presets.EstimatedBytes(bitrate, TimeSpan.FromMinutes(1), chkSystem.Checked || chkMicrophone.Checked);

        lblStatus.Text =
            $"{target.Description} · {rect.Width}×{rect.Height} · {fps} fps · {quality} · about {Megabytes(perMinute)} per minute";
    }

    // ------------------------------------------------------------------ misc

    private void BrowseFolder()
    {
        using var dialog = new FolderBrowserDialog
        {
            Description = "Where should recordings be saved?",
            UseDescriptionForTitle = true,
            SelectedPath = Directory.Exists(txtFolder.Text) ? txtFolder.Text : DefaultOutputFolder(),
            ShowNewFolderButton = true,
        };

        if (dialog.ShowDialog(this) == DialogResult.OK)
            txtFolder.Text = dialog.SelectedPath;
    }

    private void OpenOutputFolder()
    {
        string folder = txtFolder.Text.Trim();

        try
        {
            if (!Directory.Exists(folder))
                Directory.CreateDirectory(folder);

            Process.Start(new ProcessStartInfo { FileName = folder, UseShellExecute = true });
        }
        catch (Exception ex)
        {
            Warn("Could not open the folder: " + ex.Message);
        }
    }

    private void RestoreAfterRecording()
    {
        if (!Visible)
        {
            Show();
            WindowState = FormWindowState.Normal;
        }

        Activate();
    }

    private void ShowChip(CaptureTarget target)
    {
        try
        {
            chip?.Dispose();
            chip = new FloatingTimer(() => session?.Elapsed ?? TimeSpan.Zero, () => session?.Paused ?? false);
            chip.PlaceAwayFrom(target.Rect);
            chip.Show();
        }
        catch (Exception ex)
        {
            AppendLog("timer chip: " + ex.Message);
        }
    }

    private void HideChip()
    {
        try
        {
            chip?.Close();
            chip?.Dispose();
        }
        catch (Exception)
        {
            /* already gone */
        }

        chip = null;
    }

    private void CreateTray()
    {
        var menu = new ContextMenuStrip();

        trayToggle = new ToolStripMenuItem("Start recording", null, (_, _) => ToggleRecording());
        var show = new ToolStripMenuItem("Show ScreenForge", null, (_, _) =>
        {
            Show();
            WindowState = FormWindowState.Normal;
            Activate();
        });
        trayPause = new ToolStripMenuItem("Pause", null, (_, _) => TogglePause());
        var folder = new ToolStripMenuItem("Open output folder", null, (_, _) => OpenOutputFolder());
        var exit = new ToolStripMenuItem("Exit", null, (_, _) => Close());

        menu.Items.Add(show);
        menu.Items.Add(new ToolStripSeparator());
        menu.Items.Add(trayToggle);
        menu.Items.Add(trayPause);
        menu.Items.Add(new ToolStripSeparator());
        menu.Items.Add(folder);
        menu.Items.Add(exit);

        tray = new NotifyIcon
        {
            Icon = Icon,
            Text = "ScreenForge",
            Visible = true,
            ContextMenuStrip = menu,
        };

        tray.DoubleClick += (_, _) =>
        {
            Show();
            WindowState = FormWindowState.Normal;
            Activate();
        };
    }

    private void AppendLog(string line)
    {
        if (string.IsNullOrWhiteSpace(line))
            return;

        if (InvokeRequired)
        {
            BeginInvoke(new Action(() => AppendLog(line)));
            return;
        }

        if (txtLog == null)
            return;

        txtLog.AppendText(line.TrimEnd() + Environment.NewLine);
        txtLog.SelectionStart = txtLog.TextLength;
        txtLog.ScrollToCaret();
    }

    /// <summary>The session's own remark, when it has one worth showing.</summary>
    private static string ShortNote(RecordingResult result)
    {
        if (result == null || string.IsNullOrWhiteSpace(result.Message))
            return "";

        if (result.Message == "Saved." || result.Message.StartsWith("Joined ", StringComparison.Ordinal))
            return "";

        return "\n\n" + result.Message;
    }

    private void Warn(string message)
    {
        MessageBox.Show(this, message, "ScreenForge", MessageBoxButtons.OK, MessageBoxIcon.Warning);
    }

    private void WarnAboutFfmpeg()
    {
        MessageBox.Show(
            this,
            "ScreenForge records through ffmpeg, and could not find ffmpeg.exe.\n\n" +
            "Put ffmpeg.exe in the same folder as ScreenForge.exe (or in an \"ffmpeg\" subfolder) and start the app again.\n\n" +
            "Searched: the app folder, %APPDATA%\\ffmpeg, and PATH.",
            "ffmpeg is missing",
            MessageBoxButtons.OK,
            MessageBoxIcon.Warning);
    }

    private static string DescribeRegion(CaptureRect rect) => $"Region {rect.Width}×{rect.Height}";

    private static string Shorten(string text, int max)
    {
        if (string.IsNullOrWhiteSpace(text))
            return "Window";

        text = text.Trim();
        return text.Length <= max ? text : text.Substring(0, max - 1).TrimEnd() + "…";
    }

    private static string Clock(TimeSpan time) =>
        time.TotalHours >= 1
            ? $"{(int)time.TotalHours}:{time.Minutes:00}:{time.Seconds:00}"
            : $"{time.Minutes:00}:{time.Seconds:00}";

    private static string Megabytes(long bytes) =>
        bytes >= 1024L * 1024 * 1024
            ? (bytes / 1024.0 / 1024 / 1024).ToString("0.0") + " GB"
            : Math.Max(0, bytes / 1024.0 / 1024).ToString("0.0") + " MB";
}
