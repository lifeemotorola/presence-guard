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
using System.Drawing;
using System.Windows.Forms;
using ScreenForge.Core;

namespace ScreenForge.Ui;

public partial class MainForm
{
    private readonly List<Card> cards = new();

    private Panel scrollHost;
    private FlowLayoutPanel stack;
    private bool relayouting;

    private ComboBox cboSource;
    private ComboBox cboSourceDetail;
    private AccentButton btnPickRegion;
    private AccentButton btnRefreshSources;

    private ComboBox cboFps;
    private ComboBox cboQuality;
    private ComboBox cboEncoder;
    private ToggleSwitch chkCursor;
    private ToggleSwitch chkDuplication;

    private ToggleSwitch chkSystem;
    private NumericUpDown numSystemGain;
    private ToggleSwitch chkMicrophone;
    private ComboBox cboMicrophone;
    private NumericUpDown numMicGain;

    private TextBox txtFolder;
    private AccentButton btnBrowse;
    private TextBox txtPattern;
    private ComboBox cboCountdown;
    private ToggleSwitch chkHide;

    private AccentButton btnRecord;
    private AccentButton btnPause;
    private AccentButton btnOpenFolder;
    private Label lblTimer;
    private Label lblEstimate;
    private Label lblStatus;
    private RichTextBox txtLog;

    /// <summary>
    /// The window is three bands: a header that is always whole, a middle that
    /// scrolls on the left and grows on the right, and a footer that is always
    /// visible. Nothing can be pushed off the bottom, at any window size or any
    /// display scaling — which is what a fixed-height form with AutoSize rows
    /// eventually does.
    /// </summary>
    private void BuildUi()
    {
        SuspendLayout();

        Text = "ScreenForge — screen recorder";
        Icon = Theme.AppIcon();
        BackColor = Theme.Back;
        ForeColor = Theme.Text;
        Font = Theme.Body;
        StartPosition = FormStartPosition.CenterScreen;
        MinimumSize = new Size(900, 600);
        ClientSize = new Size(1060, 780);
        DoubleBuffered = true;
        KeyPreview = true;

        var root = new TableLayoutPanel
        {
            Dock = DockStyle.Fill,
            ColumnCount = 1,
            RowCount = 3,
            BackColor = Theme.Back,
            Margin = new Padding(0),
            Padding = new Padding(0),
        };

        root.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 100));
        root.RowStyles.Add(new RowStyle(SizeType.AutoSize));
        root.RowStyles.Add(new RowStyle(SizeType.Percent, 100));
        root.RowStyles.Add(new RowStyle(SizeType.AutoSize));

        root.Controls.Add(BuildHeader(), 0, 0);
        root.Controls.Add(BuildContent(), 0, 1);
        root.Controls.Add(BuildFooter(), 0, 2);

        Controls.Add(root);

        Theme.Apply(this);
        ResumeLayout(true);
    }

    private Control BuildHeader()
    {
        var header = new TableLayoutPanel
        {
            Dock = DockStyle.Top,
            AutoSize = true,
            AutoSizeMode = AutoSizeMode.GrowAndShrink,
            ColumnCount = 3,
            RowCount = 1,
            BackColor = Theme.Back,
            Padding = new Padding(22, 18, 22, 12),
            Margin = new Padding(0),
        };

        header.ColumnStyles.Add(new ColumnStyle(SizeType.Absolute, 56));
        header.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 100));
        header.ColumnStyles.Add(new ColumnStyle(SizeType.AutoSize));

        var mark = new BrandMark { Anchor = AnchorStyles.Top | AnchorStyles.Left, Margin = new Padding(0, 2, 0, 0) };

        var titles = new FlowLayoutPanel
        {
            FlowDirection = FlowDirection.TopDown,
            WrapContents = false,
            AutoSize = true,
            AutoSizeMode = AutoSizeMode.GrowAndShrink,
            BackColor = Theme.Back,
            Margin = new Padding(0),
        };

        titles.Controls.Add(new Label
        {
            Text = "ScreenForge",
            Font = Theme.Heading,
            ForeColor = Theme.Text,
            AutoSize = true,
            Margin = new Padding(0),
        });

        titles.Controls.Add(new Label
        {
            Text = "Record a display, a window, or a region you draw — with sound — and get an ordinary MP4.",
            Font = Theme.Small,
            ForeColor = Theme.Muted,
            AutoSize = true,
            Margin = new Padding(0, 1, 0, 0),
        });

        var byline = new Label
        {
            Text = "by Emmanuel Suah  ·  v1.0",
            Font = Theme.Small,
            ForeColor = Theme.Muted,
            AutoSize = true,
            Anchor = AnchorStyles.Top | AnchorStyles.Right,
            Margin = new Padding(0, 8, 0, 0),
            TextAlign = ContentAlignment.MiddleRight,
        };

        header.Controls.Add(mark, 0, 0);
        header.Controls.Add(titles, 1, 0);
        header.Controls.Add(byline, 2, 0);

        return header;
    }

    private Control BuildContent()
    {
        var content = new TableLayoutPanel
        {
            Dock = DockStyle.Fill,
            ColumnCount = 2,
            RowCount = 1,
            BackColor = Theme.Back,
            Padding = new Padding(22, 0, 22, 0),
            Margin = new Padding(0),
        };

        content.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 55));
        content.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 45));
        content.RowStyles.Add(new RowStyle(SizeType.Percent, 100));

        content.Controls.Add(BuildSettingsColumn(), 0, 0);
        content.Controls.Add(BuildSessionColumn(), 1, 0);

        return content;
    }

    private Control BuildSettingsColumn()
    {
        var column = new Panel
        {
            Dock = DockStyle.Fill,
            BackColor = Theme.Back,
            Margin = new Padding(0, 0, 10, 10),
        };

        scrollHost = new Panel
        {
            Dock = DockStyle.Fill,
            BackColor = Theme.Back,
            AutoScroll = true,
            Margin = new Padding(0),
        };

        stack = new FlowLayoutPanel
        {
            FlowDirection = FlowDirection.TopDown,
            WrapContents = false,
            AutoSize = true,
            AutoSizeMode = AutoSizeMode.GrowAndShrink,
            Dock = DockStyle.Top,
            BackColor = Theme.Back,
            Margin = new Padding(0),
            Padding = new Padding(0),
        };

        stack.Controls.Add(BuildSourceCard());
        stack.Controls.Add(BuildVideoCard());
        stack.Controls.Add(BuildAudioCard());
        stack.Controls.Add(BuildOutputCard());

        scrollHost.Controls.Add(stack);
        scrollHost.Resize += (_, _) => RelayoutCards();
        column.Controls.Add(scrollHost);

        return column;
    }

    private Control BuildSessionColumn()
    {
        var column = new TableLayoutPanel
        {
            Dock = DockStyle.Fill,
            ColumnCount = 1,
            RowCount = 2,
            BackColor = Theme.Back,
            Margin = new Padding(10, 0, 0, 10),
        };

        column.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 100));
        column.RowStyles.Add(new RowStyle(SizeType.Absolute, 188));
        column.RowStyles.Add(new RowStyle(SizeType.Percent, 100));

        column.Controls.Add(BuildSessionCard(), 0, 0);
        column.Controls.Add(BuildLogCard(), 0, 1);

        return column;
    }

    private Control BuildSessionCard()
    {
        var card = new Card("Session", autoFit: false)
        {
            Dock = DockStyle.Fill,
            Margin = new Padding(0, 0, 0, 12),
        };

        var grid = new TableLayoutPanel
        {
            Dock = DockStyle.Fill,
            ColumnCount = 2,
            RowCount = 2,
            BackColor = Theme.Panel,
            Margin = new Padding(0),
        };

        grid.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 100));
        grid.ColumnStyles.Add(new ColumnStyle(SizeType.AutoSize));
        grid.RowStyles.Add(new RowStyle(SizeType.Absolute, 62));
        grid.RowStyles.Add(new RowStyle(SizeType.Percent, 100));

        lblTimer = new Label
        {
            Text = "00:00:00",
            Font = Theme.Timer,
            ForeColor = Theme.Muted,
            AutoSize = false,
            Dock = DockStyle.Fill,
            TextAlign = ContentAlignment.MiddleLeft,
            BackColor = Theme.Panel,
            Margin = new Padding(0),
        };

        lblEstimate = new Label
        {
            Text = "",
            Font = Theme.Small,
            ForeColor = Theme.Muted,
            AutoSize = true,
            Anchor = AnchorStyles.Right,
            TextAlign = ContentAlignment.MiddleRight,
            BackColor = Theme.Panel,
            Margin = new Padding(8, 0, 0, 0),
        };

        lblStatus = new Label
        {
            Text = "Ready.",
            Font = Theme.Small,
            ForeColor = Theme.Muted,
            AutoSize = false,
            Dock = DockStyle.Fill,
            TextAlign = ContentAlignment.TopLeft,
            BackColor = Theme.Panel,
            Margin = new Padding(0),
        };

        grid.Controls.Add(lblTimer, 0, 0);
        grid.Controls.Add(lblEstimate, 1, 0);
        grid.Controls.Add(lblStatus, 0, 1);
        grid.SetColumnSpan(lblStatus, 2);

        card.Controls.Add(grid);
        return card;
    }

    private Control BuildLogCard()
    {
        var card = new Card("Log", autoFit: false)
        {
            Dock = DockStyle.Fill,
            Margin = new Padding(0),
            MinimumSize = new Size(0, 120),
        };

        txtLog = new RichTextBox
        {
            Dock = DockStyle.Fill,
            ReadOnly = true,
            BackColor = Theme.Panel,
            ForeColor = Theme.Muted,
            BorderStyle = BorderStyle.None,
            Font = Theme.Mono,
            DetectUrls = false,
            ScrollBars = RichTextBoxScrollBars.Vertical,
            Margin = new Padding(0),
        };

        card.Controls.Add(txtLog);
        return card;
    }

    private Card BuildSourceCard()
    {
        Card card = NewCard("What to record");

        cboSource = MakeCombo(150);
        cboSource.Items.AddRange(new object[] { "Whole display", "One window", "A region I draw" });
        cboSource.SelectedIndex = 0;
        cboSource.SelectedIndexChanged += (_, _) => OnSourceKindChanged();

        cboSourceDetail = MakeCombo(330);
        cboSourceDetail.SelectedIndexChanged += (_, _) => UpdateSummary();

        btnPickRegion = new AccentButton("Draw region…", ButtonTone.Ghost) { Width = 132, Height = 30 };
        btnPickRegion.Click += async (_, _) => await PickRegionAsync();

        btnRefreshSources = new AccentButton("Refresh", ButtonTone.Ghost) { Width = 96, Height = 30 };
        btnRefreshSources.Click += (_, _) => RefreshSources();

        AddField(card, "Source", cboSource, 150);
        AddField(card, "Which one", cboSourceDetail, 330);
        AddField(card, null, btnPickRegion, 132, 18, 12);
        AddField(card, null, btnRefreshSources, 96, 18, 0);

        return card;
    }

    private Card BuildVideoCard()
    {
        Card card = NewCard("Picture");

        cboFps = MakeCombo(90);
        foreach (int fps in Presets.SupportedFps)
            cboFps.Items.Add(fps);
        cboFps.SelectedItem = 60;
        cboFps.SelectedIndexChanged += (_, _) => UpdateSummary();

        cboQuality = MakeCombo(210);
        foreach (QualityPreset quality in Enum.GetValues<QualityPreset>())
            cboQuality.Items.Add(Presets.Describe(quality));
        cboQuality.SelectedIndex = (int)QualityPreset.High;
        cboQuality.SelectedIndexChanged += (_, _) => UpdateSummary();

        cboEncoder = MakeCombo(200);
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

        chkCursor = MakeToggle("Draw the mouse pointer", true, 230);
        chkDuplication = MakeToggle("Desktop duplication (one full display only)", false, 340);
        chkDuplication.CheckedChanged += (_, _) => UpdateSummary();

        AddField(card, "Frames per second", cboFps, 90);
        AddField(card, "Quality", cboQuality, 210);
        AddField(card, "Encoder", cboEncoder, 200);
        AddField(card, null, chkCursor, 230, 10, 10);
        AddField(card, null, chkDuplication, 340, 4, 0);

        return card;
    }

    private Card BuildAudioCard()
    {
        Card card = NewCard("Sound");

        chkSystem = MakeToggle("System sound — what you hear", true, 260);
        chkSystem.CheckedChanged += (_, _) => UpdateSummary();

        numSystemGain = MakeNumber(0, 200, 100, 74);
        numSystemGain.ValueChanged += (_, _) => UpdateSummary();

        chkMicrophone = MakeToggle("Microphone", false, 150);
        chkMicrophone.CheckedChanged += (_, _) => OnMicrophoneToggled();

        cboMicrophone = MakeCombo(300);
        cboMicrophone.SelectedIndexChanged += (_, _) => UpdateSummary();

        numMicGain = MakeNumber(0, 200, 100, 74);
        numMicGain.ValueChanged += (_, _) => UpdateSummary();

        AddField(card, null, chkSystem, 260, 6, 10);
        AddField(card, "Volume %", numSystemGain, 74, 0, 0);
        AddField(card, null, chkMicrophone, 150, 14, 10);
        AddField(card, "Device", cboMicrophone, 300, 0, 0);
        AddField(card, "Volume %", numMicGain, 74, 0, 0);

        return card;
    }

    private Card BuildOutputCard()
    {
        Card card = NewCard("Where it goes");

        txtFolder = new TextBox { Width = 330, Text = "", Margin = new Padding(0) };
        txtFolder.TextChanged += (_, _) => UpdateSummary();

        btnBrowse = new AccentButton("Browse…", ButtonTone.Ghost) { Width = 96, Height = 30 };
        btnBrowse.Click += (_, _) => BrowseFolder();

        txtPattern = new TextBox { Width = 230, Text = FileNamer.DefaultPattern, Margin = new Padding(0) };
        txtPattern.TextChanged += (_, _) => UpdateSummary();

        cboCountdown = MakeCombo(90);
        for (int i = 0; i <= 10; i++)
            cboCountdown.Items.Add(i);
        cboCountdown.SelectedItem = 3;
        cboCountdown.SelectedIndexChanged += (_, _) => UpdateSummary();

        chkHide = MakeToggle("Hide this window while recording", true, 290);

        AddField(card, "Folder", txtFolder, 330);
        AddField(card, null, btnBrowse, 96, 18, 12);
        AddField(card, "File name", txtPattern, 230);
        AddField(card, "Countdown (s)", cboCountdown, 90);
        AddField(card, null, chkHide, 290, 12, 0);

        return card;
    }

    private Control BuildFooter()
    {
        var footer = new Panel
        {
            Dock = DockStyle.Bottom,
            Height = 96,
            BackColor = Theme.Back,
            Padding = new Padding(22, 14, 22, 14),
            Margin = new Padding(0),
        };

        var rail = new TableLayoutPanel
        {
            Dock = DockStyle.Fill,
            ColumnCount = 4,
            RowCount = 1,
            BackColor = Theme.Back,
            Margin = new Padding(0),
        };

        rail.ColumnStyles.Add(new ColumnStyle(SizeType.Absolute, 190));
        rail.ColumnStyles.Add(new ColumnStyle(SizeType.Absolute, 140));
        rail.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 100));
        rail.ColumnStyles.Add(new ColumnStyle(SizeType.Absolute, 130));

        btnRecord = new AccentButton("●   Record", ButtonTone.Primary)
        {
            Dock = DockStyle.Fill,
            Margin = new Padding(0, 0, 10, 0),
        };
        btnRecord.Click += async (_, _) => await OnRecordClickedAsync();

        btnPause = new AccentButton("Pause", ButtonTone.Ghost)
        {
            Dock = DockStyle.Fill,
            Margin = new Padding(0, 0, 10, 0),
            Enabled = false,
        };
        btnPause.Click += async (_, _) => await OnPauseClickedAsync();

        var hint = new Label
        {
            Text = "Ctrl+Shift+F9  start / stop      Ctrl+Shift+F10  pause / resume",
            Font = Theme.Small,
            ForeColor = Theme.Muted,
            AutoSize = false,
            Dock = DockStyle.Fill,
            TextAlign = ContentAlignment.MiddleLeft,
            BackColor = Theme.Back,
            Margin = new Padding(6, 0, 6, 0),
        };

        btnOpenFolder = new AccentButton("Open folder", ButtonTone.Ghost)
        {
            Dock = DockStyle.Fill,
            Margin = new Padding(0),
        };
        btnOpenFolder.Click += (_, _) => OpenOutputFolder();

        rail.Controls.Add(btnRecord, 0, 0);
        rail.Controls.Add(btnPause, 1, 0);
        rail.Controls.Add(hint, 2, 0);
        rail.Controls.Add(btnOpenFolder, 3, 0);

        var line = new Hairline { Dock = DockStyle.Top };

        footer.Controls.Add(rail);
        footer.Controls.Add(line);

        return footer;
    }

    // ------------------------------------------------------------- helpers

    private Card NewCard(string title)
    {
        var card = new Card(title)
        {
            Dock = DockStyle.None,
            Width = 460,
            Height = 120,
        };

        cards.Add(card);
        return card;
    }

    /// <summary>
    /// Every card is as tall as what is inside it, measured after the width is
    /// known — wrapping changes the height, so the width has to be decided first.
    /// </summary>
    public void RelayoutCards()
    {
        if (relayouting || scrollHost == null || stack == null)
            return;

        relayouting = true;

        try
        {
            int width = stack.ClientSize.Width - 2;
            if (width < 240)
                width = Math.Max(240, scrollHost.ClientSize.Width - 24);

            foreach (Card card in cards)
            {
                card.Width = width;
                card.FitToContent();
            }

            stack.PerformLayout();
        }
        finally
        {
            relayouting = false;
        }
    }

    private static void AddField(Card card, string caption, Control control, int width, int topPad = 0, int rightPad = 14)
    {
        var cell = new FlowLayoutPanel
        {
            FlowDirection = FlowDirection.TopDown,
            WrapContents = false,
            AutoSize = true,
            AutoSizeMode = AutoSizeMode.GrowAndShrink,
            BackColor = Theme.Panel,
            Margin = new Padding(0, 0, rightPad, 8),
            Padding = new Padding(0, topPad, 0, 0),
        };

        if (!string.IsNullOrEmpty(caption))
        {
            cell.Controls.Add(new Label
            {
                Text = caption,
                Font = Theme.Small,
                ForeColor = Theme.Muted,
                AutoSize = true,
                Margin = new Padding(0, 0, 0, 3),
                BackColor = Theme.Panel,
            });
        }

        control.Width = width;
        control.Margin = new Padding(0);
        cell.Controls.Add(control);

        card.Body.Controls.Add(cell);
    }

    private static ComboBox MakeCombo(int width) => new()
    {
        Width = width,
        DropDownStyle = ComboBoxStyle.DropDownList,
        BackColor = Theme.Field,
        ForeColor = Theme.Text,
        FlatStyle = FlatStyle.Flat,
        Font = Theme.Body,
        Margin = new Padding(0),
    };

    private static ToggleSwitch MakeToggle(string text, bool value, int width) => new(text, value)
    {
        Width = width,
        Margin = new Padding(0),
    };

    private static NumericUpDown MakeNumber(decimal min, decimal max, decimal value, int width) => new()
    {
        Width = width,
        Minimum = min,
        Maximum = max,
        Value = value,
        Increment = 5,
        BackColor = Theme.Field,
        ForeColor = Theme.Text,
        BorderStyle = BorderStyle.FixedSingle,
        Font = Theme.Body,
        Margin = new Padding(0),
    };
}
