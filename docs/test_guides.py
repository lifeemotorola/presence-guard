#!/usr/bin/env python3
# Copyright (C) 2026 Emmanuel Suah
# SPDX-License-Identifier: GPL-2.0-or-later
"""Validate the committed printable guides and their regeneration source.

Maintainers can run this with reportlab==4.4.10 and pypdf==6.1.3 installed.
"""

from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

from pypdf import PdfReader

DOCS = Path(__file__).resolve().parent
NAMES = ('presence-guard-install.pdf', 'presence-guard-usage.pdf')


def contents(path):
    reader = PdfReader(str(path), strict=True)
    pages = [page.extract_text() or '' for page in reader.pages]
    return reader, pages


def links(reader):
    found = []
    for page in reader.pages:
        for ref in page.get('/Annots', []):
            annot = ref.get_object()
            action = annot.get('/A')
            if action and action.get('/URI'):
                found.append(str(action['/URI']))
    return found


class GuideTests(unittest.TestCase):
    def test_pdf_structure_and_content(self):
        install, install_pages = contents(DOCS / NAMES[0])
        usage, usage_pages = contents(DOCS / NAMES[1])
        self.assertEqual(len(install_pages), 4)
        self.assertEqual(len(usage_pages), 5)
        for reader, pages in ((install, install_pages), (usage, usage_pages)):
            self.assertEqual(reader.metadata.author, 'Emmanuel Suah')
            self.assertTrue(all(len(page.strip()) > 100 for page in pages))
            self.assertNotIn('\ufffd', ''.join(pages))
            for page in reader.pages:
                self.assertGreater(float(page.mediabox.width), 590)
                self.assertGreater(float(page.mediabox.height), 840)

        install_text = '\n'.join(install_pages)
        for text in ('Artifacts', 'presence-guard-0.1.0-windows-x64',
                     'OBS 33', '31.1.1', 'Safe Mode', 'ProgramData', 'presence-guard.dll'):
            self.assertIn(text, install_text)
        self.assertIn('https://github.com/lifeemotorola/presence-guard/actions/runs/42463108613', links(install))
        self.assertIn('https://github.com/lifeemotorola/presence-guard/actions', links(install))

        usage_text = '\n'.join(usage_pages)
        for text in ('Enable Presence Guard', 'Only act while streaming or recording',
                     'After idle for', 'Dead-air rule', 'Speech threshold',
                     'Test AFK', 'Restore now', 'Studio Mode', '[PresenceGuard]'):
            self.assertIn(text, usage_text)

    def test_generator_matches_committed_guide_text_and_links(self):
        with tempfile.TemporaryDirectory() as directory:
            subprocess.run([sys.executable, str(DOCS / 'generate-guides.py'),
                            '--output-dir', directory], check=True)
            for name in NAMES:
                with self.subTest(name=name):
                    committed, expected_pages = contents(DOCS / name)
                    generated, actual_pages = contents(Path(directory) / name)
                    self.assertEqual(actual_pages, expected_pages)
                    self.assertEqual(links(generated), links(committed))


if __name__ == '__main__':
    unittest.main()
