/*
 * KeyBloom desktop host.
 * Copyright (C) 2026 Emmanuel Suah
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
'use strict';
const path = require('node:path');
const { app, BrowserWindow, Menu, session } = require('electron');

app.setName('KeyBloom');
app.setAppUserModelId('com.presenceguard.keybloom');
app.commandLine.appendSwitch('disable-background-networking');
app.commandLine.appendSwitch('disable-component-update');
// Only the automated desktop smoke test supplies this override; normal users
// keep Chromium's app data in their own Windows user profile.
if (process.env.KEYBLOOM_TEST_DATA_DIR) app.setPath('userData', path.resolve(process.env.KEYBLOOM_TEST_DATA_DIR));

function createWindow() {
  const window = new BrowserWindow({
    width: 1440,
    height: 900,
    minWidth: 900,
    minHeight: 640,
    backgroundColor: '#f8f8f3',
    title: 'KeyBloom — Your typing studio',
    icon: path.join(__dirname, '..', 'app', 'assets', process.platform === 'win32' ? 'icon.ico' : 'icon.png'),
    show: false,
    webPreferences: {
      nodeIntegration: false,
      contextIsolation: true,
      sandbox: true,
      webSecurity: true,
      allowRunningInsecureContent: false,
      devTools: false,
    },
  });
  window.removeMenu();
  window.webContents.setWindowOpenHandler(() => ({ action: 'deny' }));
  window.webContents.on('will-navigate', (event) => event.preventDefault());
  window.webContents.on('will-attach-webview', (event) => event.preventDefault());
  window.once('ready-to-show', () => window.show());
  window.loadFile(path.join(__dirname, '..', 'app', 'index.html'));
  return window;
}

if (!app.requestSingleInstanceLock()) {
  app.quit();
} else {
  app.on('second-instance', () => {
    const existing = BrowserWindow.getAllWindows()[0];
    if (existing) { if (existing.isMinimized()) existing.restore(); existing.focus(); }
  });
  app.whenReady().then(() => {
    Menu.setApplicationMenu(null);
    session.defaultSession.setPermissionRequestHandler((_contents, _permission, callback) => callback(false));
    session.defaultSession.setPermissionCheckHandler(() => false);
    // The app is intentionally local-only, even if a future renderer makes a request.
    session.defaultSession.webRequest.onBeforeRequest(
      { urls: ['http://*/*', 'https://*/*', 'ws://*/*', 'wss://*/*'] },
      (_details, callback) => callback({ cancel: true }),
    );
    createWindow();
    app.on('activate', () => { if (BrowserWindow.getAllWindows().length === 0) createWindow(); });
  });
}
app.on('window-all-closed', () => { if (process.platform !== 'darwin') app.quit(); });
