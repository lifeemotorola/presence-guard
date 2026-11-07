/*
ScreenForge
Copyright (C) 2026 Presence Guard contributors

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.
*/

using System;
using System.Drawing;
using System.Windows.Forms;
using ScreenForge.Core;

namespace ScreenForge.Ui;

public partial class MainForm
{
    private ComboBox cboSource;
    private ComboBox cboSourceDetail;
    private Button btnPickRegion;
    private Button btnRefreshSources;

    private ComboBox cboFps;
    private ComboBox cboQuality;
    private ComboBox cboEncoder;
    private CheckBox chkCursor;
    private CheckBox chkDuplication;

    private CheckBox chkSystem;
    private NumericUpDown numSystemGain;
    private CheckBox chkMicrophone;
    private ComboBox cboMicrophone;
    private NumericUpDown numMicGain;

    private TextBox txtFolder;
    private Button btnBrowse;
    private TextBox txtPattern;
    private ComboBox cboCountdown;
    private CheckBox chkHide;

    private Button btnRecord;
    private Button btnPause;
    private Button btnOpenFolder;
    private Label lblTimer;
    private Label lblStatus;
    private RichTextBox txtLog;

    private void BuildUi()
    {
        SuspendLayout();

        Text = "ScreenForge — screen recorder";
        Icon = Theme.AppIcon();
        BackColor = Theme.Back;
        ForeColor = Theme.Text;
        Font = Theme.Body;
        MinimumSize = new Size(900, 700);
        ClientSize = new Size(980, 780);
        StartPosition = FormStartPosition.CenterScreen;
        DoubleBuffered = true;

        var root = new TableLayoutPanel
        {
            Dock = DockStyle.Fill,
            ColumnCount = 1,
            RowCount = 5,
            Padding = new Padding(18, 14, 18, 12),
            BackColor = Theme.Back,
        };

        root.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 100));
        root.RowStyles.Add(new RowStyle(SizeType.AutoSize));
        root.RowStyles.Add(new RowStyle(SizeType.AutoSize));
        root.RowStyles.Add(new RowStyle(SizeType.Percent, 100));
        root.RowStyles.Add(new RowStyle(SizeType.AutoSize));
        root.RowStyles.Add(new RowStyle(SizeType.AutoSize));

        root.Controls.Add(BuildHeader(), 0, 0);
        root.Controls.Add(BuildSettings(), 0, 1);
        root.Controls.Add(BuildLog(), 0, 2);
        root.Controls.Add(BuildButtons(), 0, 3);
        root.Controls.Add(BuildStatus(), 0, 4);

        Controls.Add(root);

        Theme.Apply(this);
        ResumeLayout(true);
    }

    private Control BuildHeader()
    {
        var panel = new FlowLayoutPanel
        {
            FlowDirection = FlowDirection.TopDown,
            WrapContents = false,
            AutoSize = true,
            AutoSizeMode = AutoSizeMode.GrowAndShrink,
            Dock = DockStyle.Fill,
            Margin = new Padding(0, 0, 0, 10),
        };

        panel.Controls.Add(new Label
        {
            Text = "ScreenForge",
            Font = Theme.Heading,
            ForeColor = Theme.Text,
            AutoSize = true,
            Margin = new Padding(0),
        });

        panel.Controls.Add(new Label
        {
            Text = "Records a display, a window, or a rectangle you draw — with system sound and your microphone — and saves a normal MP4.",
            Font = Theme.Small,
            ForeColor = Theme.Muted,
            AutoSize = true,
            Margin = new Padding(0, 2, 0, 0),
        });

        return panel;
    }

    private Control BuildSettings()
    {
        var grid = new TableLayoutPanel
        {
            ColumnCount = 2,
            RowCount = 3,
            AutoSize = true,
            AutoSizeMode = AutoSizeMode.GrowAndShrink,
            Dock = DockStyle.Top,
            Margin = new Padding(0, 0, 0, 6),
        };

        grid.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 50));
        grid.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 50));
        grid.RowStyles.Add(new RowStyle(SizeType.AutoSize));
        grid.RowStyles.Add(new RowStyle(SizeType.AutoSize));
        grid.RowStyles.Add(new RowStyle(SizeType.AutoSize));

        Control source = BuildSourceGroup();
        grid.Controls.Add(source, 0, 0);
        grid.SetColumnSpan(source, 2);

        grid.Controls.Add(BuildVideoGroup(), 0, 1);
        grid.Controls.Add(BuildAudioGroup(), 1, 1);

        Control output = BuildOutputGroup();
        grid.Controls.Add(output, 0, 2);
        grid.SetColumnSpan(output, 2);

        return grid;
    }

    private GroupBox BuildSourceGroup()
    {
        GroupBox group = Group("What to record");

        cboSource = MakeCombo(150);
        cboSource.Items.AddRange(new object[] { "Whole display", "One window", "A region I draw" });
        cboSource.SelectedIndex = 0;
        cboSource.SelectedIndexChanged += (_, _) => OnSourceKindChanged();

        cboSourceDetail = MakeCombo(370);
        cboSourceDetail.SelectedIndexChanged += (_, _) => UpdateSummary();

        btnPickRegion = Theme.MakeGhostButton("Draw region…");
        btnPickRegion.Width = 130;
        btnPickRegion.Click += async (_, _) => await PickRegionAsync();

        btnRefreshSources = Theme.MakeGhostButton("Refresh");
        btnRefreshSources.Width = 90;
        btnRefreshSources.Click += (_, _) => RefreshSources();

        AddField(group, "Source", cboSource, 150);
        AddField(group, "Which one", cboSourceDetail, 370);
        AddField(group, "", btnPickRegion, 130);
        AddField(group, "", btnRefreshSources, 90);

        return group;
    }

    private GroupBox BuildVideoGroup()
    {
        GroupBox group = Group("Picture");

        cboFps = MakeCombo(96);
        foreach (int fps in Presets.SupportedFps)
            cboFps.Items.Add(fps);
        cboFps.SelectedItem = 60;
        cboFps.SelectedIndexChanged += (_, _) => UpdateSummary();

        cboQuality = MakeCombo(220);
        foreach (QualityPreset quality in Enum.GetValues<QualityPreset>())
            cboQuality.Items.Add(Presets.Describe(quality));
        cboQuality.SelectedIndex = (int)QualityPreset.High;
        cboQuality.SelectedIndexChanged += (_, _) => UpdateSummary();

        cboEncoder = MakeCombo(210);
        cboEncoder.Items.AddRange(new object[]
        {
            "Automatic (recommended)",
            "NVIDIA GPU (NVENC)",
            "AMD GPU (AMF)",
            "Intel GPU (Quick Sync)",
            "CPU only (x264)",
        });
        cboEncoder.SelectedIndex = 0;
        cboEncoder.SelectedIndexChanged += (_, _) => UpdateSummary();

        chkCursor = MakeCheck("Draw the mouse pointer", true);
        chkDuplication = MakeCheck("Use desktop duplication (full display only)", false);
        chkDuplication.CheckedChanged += (_, _) => UpdateSummary();

        AddField(group, "Frames per second", cboFps, 96);
        AddField(group, "Quality", cboQuality, 220);
        AddField(group, "Encoder", cboEncoder, 210);
        AddField(group, "", chkCursor, 240);
        AddField(group, "", chkDuplication, 300);

        return group;
    }

    private GroupBox BuildAudioGroup()
    {
        GroupBox group = Group("Sound");

        chkSystem = MakeCheck("System sound (what you hear)", true);
        chkSystem.CheckedChanged += (_, _) => UpdateSummary();

        numSystemGain = MakeNumber(0, 200, 100);
        numSystemGain.ValueChanged += (_, _) => UpdateSummary();

        chkMicrophone = MakeCheck("Microphone", false);
        chkMicrophone.CheckedChanged += (_, _) => OnMicrophoneToggled();

        cboMicrophone = MakeCombo(260);
        cboMicrophone.SelectedIndexChanged += (_, _) => UpdateSummary();

        numMicGain = MakeNumber(0, 200, 100);
        numMicGain.ValueChanged += (_, _) => UpdateSummary();

        AddField(group, "", chkSystem, 220);
        AddField(group, "Volume %", numSystemGain, 70);
        AddField(group, "", chkMicrophone, 120);
        AddField(group, "Device", cboMicrophone, 260);
        AddField(group, "Volume %", numMicGain, 70);

        return group;
    }

    private GroupBox BuildOutputGroup()
    {
        GroupBox group = Group("Where it goes");

        txtFolder = new TextBox { Width = 360, Text = "" };
        txtFolder.TextChanged += (_, _) => UpdateSummary();

        btnBrowse = Theme.MakeGhostButton("Browse…");
        btnBrowse.Width = 90;
        btnBrowse.Click += (_, _) => BrowseFolder();

        txtPattern = new TextBox { Width = 240, Text = FileNamer.DefaultPattern };
        txtPattern.TextChanged += (_, _) => UpdateSummary();

        cboCountdown = MakeCombo(96);
        for (int i = 0; i <= 10; i++)
            cboCountdown.Items.Add(i);
        cboCountdown.SelectedItem = 3;
        cboCountdown.SelectedIndexChanged += (_, _) => UpdateSummary();

        chkHide = MakeCheck("Hide this window while recording", true);

        AddField(group, "Folder", txtFolder, 360);
        AddField(group, "", btnBrowse, 90);
        AddField(group, "File name", txtPattern, 240);
        AddField(group, "Countdown (s)", cboCountdown, 96);
        AddField(group, "", chkHide, 250);

        return group;
    }

    private Control BuildLog()
    {
        var panel = new Panel
        {
            Dock = DockStyle.Fill,
            BackColor = Theme.Panel,
            Padding = new Padding(10, 8, 10, 8),
            Margin = new Padding(0, 8, 0, 8),
            MinimumSize = new Size(0, 120),
        };

        txtLog = new RichTextBox
        {
            Dock = DockStyle.Fill,
            ReadOnly = true,
            Font = Theme.Mono,
            BackColor = Theme.Panel,
            ForeColor = Theme.Muted,
            BorderStyle = BorderStyle.None,
            DetectUrls = false,
            ScrollBars = RichTextBoxScrollBars.Vertical,
        };

        panel.Controls.Add(txtLog);
        return panel;
    }

    private Control BuildButtons()
    {
        var row = new TableLayoutPanel
        {
            ColumnCount = 4,
            RowCount = 1,
            Dock = DockStyle.Top,
            AutoSize = true,
            AutoSizeMode = AutoSizeMode.GrowAndShrink,
            Margin = new Padding(0, 4, 0, 4),
        };

        row.ColumnStyles.Add(new ColumnStyle(SizeType.Absolute, 180));
        row.ColumnStyles.Add(new ColumnStyle(SizeType.Absolute, 130));
        row.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 100));
        row.ColumnStyles.Add(new ColumnStyle(SizeType.Absolute, 120));

        btnRecord = Theme.MakeButton("●  Record", Theme.Accent);
        btnRecord.Width = 170;
        btnRecord.Height = 46;
        btnRecord.Click += async (_, _) => await OnRecordClickedAsync();

        btnPause = Theme.MakeButton("Pause", Theme.Field);
        btnPause.ForeColor = Theme.Text;
        btnPause.Width = 120;
        btnPause.Height = 46;
        btnPause.Enabled = false;
        btnPause.Click += async (_, _) => await OnPauseClickedAsync();

        lblTimer = new Label
        {
            Text = "00:00:00",
            Font = Theme.Timer,
            ForeColor = Theme.Muted,
            AutoSize = true,
            Margin = new Padding(14, 6, 0, 0),
            TextAlign = ContentAlignment.MiddleLeft,
        };

        btnOpenFolder = Theme.MakeGhostButton("Open folder");
        btnOpenFolder.Width = 110;
        btnOpenFolder.Height = 30;
        btnOpenFolder.Margin = new Padding(0, 8, 0, 0);
        btnOpenFolder.Click += (_, _) => OpenOutputFolder();

        row.Controls.Add(btnRecord, 0, 0);
        row.Controls.Add(btnPause, 1, 0);
        row.Controls.Add(lblTimer, 2, 0);
        row.Controls.Add(btnOpenFolder, 3, 0);

        return row;
    }

    private Control BuildStatus()
    {
        lblStatus = new Label
        {
            Text = "Ready.",
            ForeColor = Theme.Muted,
            Font = Theme.Small,
            AutoSize = false,
            Height = 34,
            Dock = DockStyle.Fill,
            TextAlign = ContentAlignment.MiddleLeft,
            Margin = new Padding(0),
        };

        return lblStatus;
    }

    private static GroupBox Group(string title)
    {
        var group = new GroupBox
        {
            Text = title,
            AutoSize = true,
            AutoSizeMode = AutoSizeMode.GrowAndShrink,
            Dock = DockStyle.Fill,
            Padding = new Padding(10, 6, 10, 10),
            Margin = new Padding(4, 0, 4, 8),
            BackColor = Theme.Panel,
            ForeColor = Theme.Muted,
        };

        var flow = new FlowLayoutPanel
        {
            FlowDirection = FlowDirection.LeftToRight,
            WrapContents = true,
            AutoSize = true,
            AutoSizeMode = AutoSizeMode.GrowAndShrink,
            Dock = DockStyle.Top,
            BackColor = Theme.Panel,
            Margin = new Padding(0),
        };

        group.Controls.Add(flow);
        return group;
    }

    private static void AddField(GroupBox group, string caption, Control control, int width)
    {
        var flow = group.Controls[0] as FlowLayoutPanel;
        if (flow == null)
            return;

        var cell = new FlowLayoutPanel
        {
            FlowDirection = FlowDirection.TopDown,
            WrapContents = false,
            AutoSize = true,
            AutoSizeMode = AutoSizeMode.GrowAndShrink,
            Margin = new Padding(0, 0, 14, 6),
            BackColor = Theme.Panel,
        };

        if (!string.IsNullOrEmpty(caption))
        {
            cell.Controls.Add(new Label
            {
                Text = caption,
                Font = Theme.Small,
                ForeColor = Theme.Muted,
                AutoSize = true,
                Margin = new Padding(0, 0, 0, 2),
            });
        }

        control.Width = width;
        control.Margin = new Padding(0);
        cell.Controls.Add(control);

        flow.Controls.Add(cell);
    }

    private static ComboBox MakeCombo(int width) => new()
    {
        Width = width,
        DropDownStyle = ComboBoxStyle.DropDownList,
        BackColor = Theme.Field,
        ForeColor = Theme.Text,
        FlatStyle = FlatStyle.Flat,
        Font = Theme.Body,
    };

    private static CheckBox MakeCheck(string text, bool value) => new()
    {
        Text = text,
        Checked = value,
        AutoSize = true,
        ForeColor = Theme.Text,
        Font = Theme.Body,
        Margin = new Padding(0, 6, 0, 0),
    };

    private static NumericUpDown MakeNumber(decimal min, decimal max, decimal value) => new()
    {
        Width = 70,
        Minimum = min,
        Maximum = max,
        Value = value,
        Increment = 5,
        BackColor = Theme.Field,
        ForeColor = Theme.Text,
        BorderStyle = BorderStyle.FixedSingle,
        Font = Theme.Body,
    };
}
