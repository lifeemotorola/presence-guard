#!/usr/bin/env python3
# Copyright (C) 2026 Emmanuel Suah
# SPDX-License-Identifier: GPL-2.0-or-later
"""Build the two printable Presence Guard guides.

For maintainers: python -m pip install reportlab==4.4.10
Then run:         python docs/generate-guides.py
The finished PDFs are committed, so readers do not need Python or ReportLab.
"""

import argparse
from pathlib import Path
from xml.sax.saxutils import escape

from reportlab import rl_config
from reportlab.lib import colors
from reportlab.lib.enums import TA_LEFT
from reportlab.lib.pagesizes import A4
from reportlab.lib.styles import ParagraphStyle
from reportlab.platypus import (
    Flowable,
    KeepTogether,
    PageBreak,
    Paragraph,
    SimpleDocTemplate,
    Spacer,
    Table,
    TableStyle,
)

rl_config.invariant = 1  # stable metadata and document IDs across rebuilds
ROOT = Path(__file__).resolve().parent
PAGE_W, PAGE_H = A4
CONTENT_W = PAGE_W - 100
NAVY = colors.HexColor('#11273c')
TEXT = colors.HexColor('#24394a')
MUTED = colors.HexColor('#5d7180')
TEAL = colors.HexColor('#087e87')
TEAL_LIGHT = colors.HexColor('#e9f6f5')
GOLD = colors.HexColor('#e7a13e')
RULE = colors.HexColor('#d8e4e8')
LIGHT = colors.HexColor('#f4f8f9')
WHITE = colors.white

STYLES = {
    'eyebrow': ParagraphStyle('Eyebrow', fontName='Helvetica-Bold', fontSize=9,
                              leading=13, textColor=TEAL, spaceAfter=10),
    'title': ParagraphStyle('Title', fontName='Helvetica-Bold', fontSize=31,
                            leading=35, textColor=NAVY, spaceAfter=12),
    'subtitle': ParagraphStyle('Subtitle', fontName='Helvetica', fontSize=13,
                               leading=19, textColor=MUTED, spaceAfter=16),
    'h1': ParagraphStyle('H1', fontName='Helvetica-Bold', fontSize=22,
                         leading=27, textColor=NAVY, spaceAfter=10),
    'h2': ParagraphStyle('H2', fontName='Helvetica-Bold', fontSize=14,
                         leading=18, textColor=NAVY, spaceBefore=2, spaceAfter=5),
    'body': ParagraphStyle('Body', fontName='Helvetica', fontSize=11.2,
                           leading=16.8, textColor=TEXT, spaceAfter=8),
    'small': ParagraphStyle('Small', fontName='Helvetica', fontSize=9.8,
                            leading=14.4, textColor=TEXT, spaceAfter=5),
    'tiny': ParagraphStyle('Tiny', fontName='Helvetica', fontSize=8.2,
                           leading=12, textColor=MUTED, spaceAfter=0),
    'stepnum': ParagraphStyle('StepNum', fontName='Helvetica-Bold', fontSize=11,
                              leading=16, textColor=TEAL, alignment=TA_LEFT),
    'cardtitle': ParagraphStyle('CardTitle', fontName='Helvetica-Bold', fontSize=10.5,
                                leading=14.5, textColor=NAVY, spaceAfter=4),
    'code': ParagraphStyle('Code', fontName='Courier', fontSize=9.0,
                           leading=14.0, textColor=NAVY, spaceAfter=0),
}


def para(text, style='body'):
    return Paragraph(text, STYLES[style])


def link(url, text):
    return f'<link href="{escape(url)}" color="#087e87"><u>{escape(text)}</u></link>'


def step(number, title, body):
    details = [para(title, 'h2'), para(body)]
    table = Table([[para(f'{number:02}', 'stepnum'), details]],
                  colWidths=[36, CONTENT_W - 36], hAlign='LEFT')
    table.setStyle(TableStyle([
        ('VALIGN', (0, 0), (-1, -1), 'TOP'),
        ('LEFTPADDING', (0, 0), (-1, -1), 0),
        ('RIGHTPADDING', (0, 0), (-1, -1), 0),
        ('TOPPADDING', (0, 0), (-1, -1), 2),
        ('BOTTOMPADDING', (0, 0), (-1, -1), 10),
    ]))
    return KeepTogether(table)


def card(title, body, background=LIGHT):
    contents = [para(title, 'cardtitle'), para(body, 'small')]
    table = Table([[contents]], colWidths=[CONTENT_W], hAlign='LEFT')
    table.setStyle(TableStyle([
        ('BACKGROUND', (0, 0), (-1, -1), background),
        ('BOX', (0, 0), (-1, -1), 0.5, RULE),
        ('LEFTPADDING', (0, 0), (-1, -1), 13),
        ('RIGHTPADDING', (0, 0), (-1, -1), 13),
        ('TOPPADDING', (0, 0), (-1, -1), 10),
        ('BOTTOMPADDING', (0, 0), (-1, -1), 9),
    ]))
    return KeepTogether(table)


def codeblock(lines):
    contents = '<br/>'.join(escape(line).replace(' ', '&nbsp;') for line in lines)
    table = Table([[para(contents, 'code')]], colWidths=[CONTENT_W], hAlign='LEFT')
    table.setStyle(TableStyle([
        ('BACKGROUND', (0, 0), (-1, -1), LIGHT),
        ('BOX', (0, 0), (-1, -1), 0.5, RULE),
        ('LEFTPADDING', (0, 0), (-1, -1), 12),
        ('RIGHTPADDING', (0, 0), (-1, -1), 12),
        ('TOPPADDING', (0, 0), (-1, -1), 10),
        ('BOTTOMPADDING', (0, 0), (-1, -1), 10),
    ]))
    return KeepTogether(table)


class ProcessRibbon(Flowable):
    """A small, printable diagram drawn as vectors, not a screenshot."""

    def __init__(self, entries):
        super().__init__()
        self.entries = entries
        self.width = CONTENT_W
        self.height = 78

    def wrap(self, avail_width, avail_height):
        self.width = min(CONTENT_W, avail_width)
        return self.width, self.height

    def draw(self):
        canvas = self.canv
        gap = 13
        box_w = (self.width - gap * 2) / 3
        for i, (label, detail) in enumerate(self.entries):
            x = i * (box_w + gap)
            canvas.setFillColor(LIGHT)
            canvas.setStrokeColor(RULE)
            canvas.roundRect(x, 5, box_w, 65, 6, stroke=1, fill=1)
            canvas.setFillColor(TEAL)
            canvas.setFont('Helvetica-Bold', 8)
            canvas.drawString(x + 11, 52, f'0{i + 1}')
            canvas.setFillColor(NAVY)
            canvas.setFont('Helvetica-Bold', 10)
            canvas.drawString(x + 11, 34, label)
            canvas.setFillColor(MUTED)
            canvas.setFont('Helvetica', 8)
            canvas.drawString(x + 11, 20, detail)
            if i < 2:
                canvas.setStrokeColor(GOLD)
                canvas.setLineWidth(1.4)
                y = 37
                canvas.line(x + box_w + 3, y, x + box_w + gap - 3, y)
                canvas.line(x + box_w + gap - 6, y + 3, x + box_w + gap - 3, y)
                canvas.line(x + box_w + gap - 6, y - 3, x + box_w + gap - 3, y)


def page_canvas(title):
    def decorate(canvas, doc):
        canvas.saveState()
        canvas.setTitle(f'Presence Guard - {title}')
        canvas.setAuthor('Emmanuel Suah')
        canvas.setSubject('Presence Guard for OBS Studio')
        canvas.setFillColor(TEAL)
        canvas.rect(0, PAGE_H - 7, PAGE_W, 7, stroke=0, fill=1)
        if doc.page > 1:
            canvas.setFont('Helvetica-Bold', 8)
            canvas.setFillColor(NAVY)
            canvas.drawString(50, PAGE_H - 35, 'PRESENCE GUARD')
            canvas.setFont('Helvetica', 8)
            canvas.setFillColor(MUTED)
            canvas.drawRightString(PAGE_W - 50, PAGE_H - 35, title.upper())
            canvas.setStrokeColor(RULE)
            canvas.line(50, PAGE_H - 43, PAGE_W - 50, PAGE_H - 43)
        canvas.setStrokeColor(RULE)
        canvas.line(50, 43, PAGE_W - 50, 43)
        canvas.setFont('Helvetica', 8)
        canvas.setFillColor(MUTED)
        canvas.drawString(50, 29, 'Presence Guard  |  Emmanuel Suah')
        canvas.drawRightString(PAGE_W - 50, 29, f'{doc.page:02}')
        canvas.restoreState()
    return decorate


def build(path, title, story):
    doc = SimpleDocTemplate(str(path), pagesize=A4, leftMargin=50, rightMargin=50,
                            topMargin=60, bottomMargin=61, title=f'Presence Guard - {title}',
                            author='Emmanuel Suah', pageCompression=1)
    decorate = page_canvas(title)
    doc.build(story, onFirstPage=decorate, onLaterPages=decorate)


def cover(guide_name, subtitle, entries, audience, overview):
    return [
        Spacer(1, 30),
        para('PRESENCE GUARD  /  WINDOWS &amp; OBS STUDIO', 'eyebrow'),
        para(guide_name, 'title'),
        para(subtitle, 'subtitle'),
        Spacer(1, 10),
        ProcessRibbon(entries),
        Spacer(1, 25),
        card('A little protection for your live scene', audience, TEAL_LIGHT),
        Spacer(1, 18),
        card('Inside this guide', overview),
        Spacer(1, 18),
        para('A practical guide by Emmanuel Suah', 'small'),
        para('Presence Guard is a separate OBS plugin. This guide does not install ScreenForge or its mouse-click rings.', 'tiny'),
        PageBreak(),
    ]


def install_story():
    run_url = 'https://github.com/lifeemotorola/presence-guard/actions/runs/42463108613'
    actions_url = 'https://github.com/lifeemotorola/presence-guard/actions'
    story = cover(
        'Installation guide',
        'Install the prebuilt Windows plugin and open its dock in OBS Studio.',
        [('Download', 'Actions artifact'), ('Install', 'Plugin folder'), ('Open', 'OBS dock')],
        'No compiler is required. This guide uses the prebuilt Windows x64 artifact from GitHub Actions.',
        'Find the artifact, extract both ZIPs, place the plugin folder in OBS, then verify the dock and log.',
    )
    story += [
        para('Get the Windows package', 'h1'),
        para('You need Windows 10 or 11 (64-bit), OBS Studio, a GitHub account to download artifacts, and a few minutes.'),
        step(1, 'Open a successful build',
             f'Open {link(run_url, "this successful main-branch run")}. The Windows job in that run built the Presence Guard plugin.'),
        step(2, 'Find Artifacts at the bottom',
             'Scroll to <b>Artifacts</b> and download <b>presence-guard-0.1.0-windows-x64-6956a758a</b>. '
             'This is the OBS plugin, not the ScreenForge app.'),
        step(3, 'Extract both ZIP files',
             'Extract the downloaded Actions ZIP. Inside it, extract the second '
             '<b>presence-guard-0.1.0-windows-x64.zip</b>. The inner archive contains a <b>presence-guard</b> folder.'),
        Spacer(1, 8),
        card('If the artifact has expired',
             f'Go to {link(actions_url, "Actions")}, open a newer successful <b>main push run</b>, and download the '
             '<b>presence-guard-...-windows-x64-...</b> artifact at the bottom. Check that its Windows build succeeded. '
             'Artifacts expire; the DLL is not stored in the Code tab or a Release.', TEAL_LIGHT),
        Spacer(1, 10),
        card('OBS 33 compatibility',
             'The repository builds this plugin against OBS 31.1.1 headers. It has not been verified in OBS 33.x; '
             'if it does not load, check the OBS log and obtain a compatible build before relying on it.'),
        PageBreak(),
        para('Install and verify', 'h1'),
        step(1, 'Close OBS completely',
             'Exit OBS, including any copy left running in the system tray.'),
        step(2, 'Open the OBS plugin folder',
             'In File Explorer, paste <b>%ProgramData%\\obs-studio\\plugins</b> into the address bar. '
             'Create the <b>plugins</b> folder if necessary. Windows may ask for administrator permission.'),
        step(3, 'Copy the extracted folder',
             'Move the entire extracted <b>presence-guard</b> folder into <b>plugins</b>. '
             'Keep the <b>bin</b> and <b>data</b> folders together; do not copy only the DLL.'),
        codeblock([
            r'C:\ProgramData\obs-studio\plugins\presence-guard' + '\\',
            r'  bin\64bit\presence-guard.dll',
            r'  data\locale\en-US.ini',
        ]),
        Spacer(1, 12),
        step(4, 'Start OBS and open the dock',
             'Restart OBS normally, not in Safe Mode. Choose <b>Docks &gt; Presence Guard</b>. '
             'The dock may initially be floating or hidden.'),
        step(5, 'Confirm it loaded',
             'In OBS, choose <b>Help &gt; Log Files &gt; View Current Log</b> and search for '
             '<b>[presence-guard] loaded</b>. Then follow the separate usage guide.'),
        PageBreak(),
        para('Troubleshooting', 'h1'),
        step(1, 'The dock is missing',
             'Check the folder layout on the previous page and restart OBS. Safe Mode skips third-party plugins. '
             'Check the current OBS log for <b>presence-guard</b> and any loading or dependency errors.'),
        step(2, 'Windows blocked the download',
             'If an extracted DLL has an <b>Unblock</b> option under Properties, select it and apply. '
             'Only use artifacts from the repository you trust.'),
        step(3, 'The plugin will not load',
             'A major OBS update can make an older plugin binary incompatible. This build was compiled against '
             'OBS 31.1.1; OBS 33 has not been verified. Keep the OBS log message when asking for help.'),
        Spacer(1, 7),
        card('Uninstall',
             'Close OBS and delete the <b>presence-guard</b> folder from <b>%ProgramData%\\obs-studio\\plugins</b>. '
             'Profile settings remain in OBS; the usage guide explains where they are stored.'),
        Spacer(1, 14),
        para('Next: open the companion <b>Presence Guard usage guide</b> to configure the AFK and dead-air rules.'),
        para('Repository: ' + link('https://github.com/lifeemotorola/presence-guard', 'github.com/lifeemotorola/presence-guard'), 'small'),
    ]
    return story


def usage_story():
    story = cover(
        'Usage guide',
        'Keep your stream on a safe scene when you step away or the microphone goes quiet.',
        [('Watch', 'Activity & audio'), ('Switch', 'A safe scene'), ('Restore', 'Your live scene')],
        'Presence Guard watches for idle input and, optionally, sustained low microphone levels. '
        'It remembers the scene it changed from and can restore it when the condition clears.',
        'Enable and test the AFK rule, configure optional dead-air monitoring, and solve common setup issues.',
    )
    story += [
        para('Set up the AFK rule', 'h1'),
        para('First install the plugin using the companion installation guide. The OBS 33.x compatibility of the current binary has not been verified.'),
        step(1, 'Prepare a safe scene',
             'In OBS, create a scene such as <b>BRB</b>. Open <b>Docks &gt; Presence Guard</b>.'),
        step(2, 'Enable the guard',
             'Tick <b>Enable Presence Guard</b>. Controls remain greyed out while it is off. '
             'Changes apply immediately; there is no Save button.'),
        step(3, 'Choose the scene and delay',
             'Under <b>AFK rule</b>, set <b>Show scene</b> to your BRB scene. Set <b>After idle for</b> '
             'to a delay from 10 to 3,600 seconds; the default is 120 seconds.'),
        step(4, 'Choose when it may act',
             'Keep <b>Only act while streaming or recording</b> enabled for normal use. '
             'Temporarily untick it if you want to test while offline.'),
        Spacer(1, 7),
        card('What happens when you return',
             'After the idle threshold, the guard switches to your selected scene. Keyboard or mouse activity '
             'clears AFK and restores the previous scene. If you manually choose a different scene while it is '
             'parked, the guard leaves your choice alone.', TEAL_LIGHT),
        PageBreak(),
        para('Test before going live', 'h1'),
        step(1, 'Refresh the lists',
             'Click <b>Refresh</b> after adding or renaming a scene or audio source. '
             'Make sure an AFK scene is selected and the guard is enabled.'),
        step(2, 'Test the scene switch',
             'For an offline test, untick <b>Only act while streaming or recording</b>. '
             'Click <b>Test AFK</b> to switch to the AFK scene immediately.'),
        step(3, 'Restore and re-arm',
             'Click <b>Restore now</b> to return. The manual test stays parked until you restore it. '
             'Re-enable <b>Only act while streaming or recording</b> if that is how you want to use it.'),
        Spacer(1, 8),
        card('Read the status line',
             '<b>Watching</b> means the guard is monitoring; <b>AFK</b> or <b>Dead air</b> means it '
             'has switched scenes. <b>Guard is off</b> means Enable Presence Guard is unticked. '
             'A missing-microphone message means the audio rule is suspended.'),
        Spacer(1, 12),
        card('Important limits',
             'The automatic rules do not act in OBS Studio Mode. If a selected scene is removed or renamed, '
             'choose it again and refresh. If it changed scenes and streaming or recording stops while the live-only '
             'gate is enabled, it restores the previous scene.', TEAL_LIGHT),
        PageBreak(),
        para('Add the dead-air rule', 'h1'),
        step(1, 'Enable and pick a microphone',
             'Tick <b>Dead-air rule</b>. Select a <b>Microphone</b> source that is active in the '
             'live scene. Choose a <b>Show scene</b> destination, or use <b>(same as AFK scene)</b>.'),
        step(2, 'Set the delay and threshold',
             '<b>After silence for</b> defaults to 45 seconds (range 5-1,800). '
             '<b>Speech threshold</b> defaults to -45 dB (range -90 to 0 dB).'),
        step(3, 'Tune using your own voice',
             'Speak normally and observe the moving microphone level in the status line. '
             'Set the threshold a few dB below your normal speech level. '
             'A level at or above the threshold counts as speech.'),
        step(4, 'Check the behavior',
             'When measured audio remains below the threshold for the selected time, the guard switches '
             'to the configured scene. When the level reaches the threshold again, it restores the previous scene.'),
        Spacer(1, 8),
        card('No signal is not assumed to be silence',
             'If the selected microphone is muted, disconnected, absent from the live scene, or producing no '
             'meter data, the dead-air rule is suspended instead of inventing a silence reading. '
             'Check the source and the status line first.', TEAL_LIGHT),
        PageBreak(),
        para('Quick reference', 'h1'),
        card('Nothing happens',
             'Enable the guard, select an existing scene, turn off Studio Mode, and check the live-only gate. '
             'For dead air, select an active microphone and confirm the level is moving.'),
        Spacer(1, 10),
        card('It switches too quickly',
             'Increase the idle or silence delay. If the dead-air rule misreads your speech, observe your normal '
             'level and adjust the speech threshold a few dB below it.'),
        Spacer(1, 10),
        card('It does not return',
             'Try <b>Restore now</b>. The guard cannot restore a scene that has been deleted, and it does not '
             'override a scene that you selected manually while parked.'),
        Spacer(1, 10),
        card('Your settings',
             'Settings are saved as you edit in the OBS profile <b>basic.ini</b>, under <b>[PresenceGuard]</b>. '
             'Deleting that section resets the plugin settings. Back up the profile before editing it.'),
        Spacer(1, 14),
        para('For installation, missing-dock, or binary compatibility help, see the companion <b>Presence Guard installation guide</b>.'),
        para('Repository: ' + link('https://github.com/lifeemotorola/presence-guard', 'github.com/lifeemotorola/presence-guard'), 'small'),
    ]
    return story


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output-dir', type=Path, default=ROOT,
                        help='directory for generated PDFs (defaults to docs/)')
    output = parser.parse_args().output_dir
    output.mkdir(parents=True, exist_ok=True)
    build(output / 'presence-guard-install.pdf', 'Installation guide', install_story())
    build(output / 'presence-guard-usage.pdf', 'Usage guide', usage_story())
    print(f'Created Presence Guard guides in {output}')
