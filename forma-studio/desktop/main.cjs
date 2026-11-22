/* Forma Studio desktop host. Copyright (C) 2026 Emmanuel Suah. GPL-2.0-or-later. */
'use strict';
const path = require('node:path');
const fs = require('node:fs/promises');
const { randomUUID } = require('node:crypto');
const { app, BrowserWindow, Menu, dialog, ipcMain, session } = require('electron');

app.setName('Forma Studio');
app.setAppUserModelId('com.emmanuelsuah.formastudio');
app.commandLine.appendSwitch('disable-background-networking');
app.commandLine.appendSwitch('disable-component-update');
// Only the Windows CI smoke test requests software rendering and an isolated profile.
if (process.env.FORMA_TEST_SOFTWARE_GL === '1') {
  app.commandLine.appendSwitch('use-angle', 'swiftshader');
  app.commandLine.appendSwitch('enable-unsafe-swiftshader');
}
if (process.env.FORMA_TEST_DATA_DIR) app.setPath('userData', path.resolve(process.env.FORMA_TEST_DATA_DIR));

let mainWindow = null;
let currentProjectPath = null;
let pendingProjectPath = null;
const PROJECT_LIMIT = 10 * 1024 * 1024;
const EXPORT_LIMIT = 100 * 1024 * 1024;
function trusted(event) {
  if (!mainWindow || event.sender !== mainWindow.webContents || event.senderFrame !== mainWindow.webContents.mainFrame) throw new Error('Untrusted window.');
}
function safeName(name) {
  if (typeof name !== 'string') return 'Untitled scene';
  const cleaned = name.replace(/[\\/:*?"<>|\x00-\x1f]/g, '-').trim().replace(/[. ]+$/g, '').slice(0, 70);
  return cleaned && !/^(con|prn|aux|nul|com[1-9]|lpt[1-9])$/i.test(cleaned) ? cleaned : 'Untitled scene';
}
async function writeAtomically(destination, contents) {
  const temporary = path.join(path.dirname(destination), `.${path.basename(destination)}.${randomUUID()}.tmp`);
  try {
    await fs.writeFile(temporary, contents, { flag: 'wx' });
    await fs.rename(temporary, destination);
  } finally { await fs.rm(temporary, { force: true }).catch(() => {}); }
}

ipcMain.handle('forma:open', async (event) => {
  trusted(event);
  pendingProjectPath = null;
  const result = await dialog.showOpenDialog(mainWindow, { title: 'Open a Forma project', properties: ['openFile'], filters: [{ name: 'Forma project', extensions: ['forma', 'json'] }] });
  if (result.canceled || !result.filePaths[0]) return { canceled: true };
  const file = result.filePaths[0];
  const handle = await fs.open(file, 'r');
  let text;
  try {
    const info = await handle.stat();
    if (!info.isFile() || info.size > PROJECT_LIMIT) throw new Error('Project files must be 10 MB or smaller.');
    // Bound the actual read as well as the stat: the selected file may grow
    // while it is being opened. Never read an arbitrarily large file into RAM.
    const buffer = Buffer.allocUnsafe(PROJECT_LIMIT + 1);
    let length = 0;
    while (length < buffer.length) {
      const { bytesRead } = await handle.read(buffer, length, buffer.length - length, null);
      if (!bytesRead) break;
      length += bytesRead;
    }
    if (length > PROJECT_LIMIT) throw new Error('Project files must be 10 MB or smaller.');
    text = buffer.toString('utf8', 0, length);
  } finally { await handle.close(); }
  pendingProjectPath = file;
  return { canceled: false, text };
});
ipcMain.handle('forma:accept-open', (event) => { trusted(event); if (!pendingProjectPath) throw new Error('No opened project is pending.'); currentProjectPath = pendingProjectPath; pendingProjectPath = null; });
ipcMain.handle('forma:forget', (event) => { trusted(event); currentProjectPath = null; pendingProjectPath = null; });
ipcMain.handle('forma:save', async (event, text, name, saveAs) => {
  trusted(event);
  if (typeof text !== 'string' || Buffer.byteLength(text, 'utf8') > PROJECT_LIMIT || typeof saveAs !== 'boolean') throw new Error('Invalid project data.');
  let destination = currentProjectPath;
  if (saveAs || !destination) {
    const result = await dialog.showSaveDialog(mainWindow, { title: 'Save Forma project', defaultPath: `${safeName(name)}.forma`, filters: [{ name: 'Forma project', extensions: ['forma'] }] });
    if (result.canceled || !result.filePath) return { canceled: true };
    destination = result.filePath;
  }
  await writeAtomically(destination, text);
  currentProjectPath = destination;
  pendingProjectPath = null;
  return { canceled: false };
});
ipcMain.handle('forma:export', async (event, kind, bytes, name) => {
  trusted(event);
  if (!['glb', 'png'].includes(kind) || !(bytes instanceof Uint8Array) || bytes.byteLength === 0 || bytes.byteLength > EXPORT_LIMIT) throw new Error('Invalid or oversized export.');
  const result = await dialog.showSaveDialog(mainWindow, { title: kind === 'glb' ? 'Export current frame as GLB' : 'Save viewport image', defaultPath: `${safeName(name)}.${kind}`, filters: [{ name: kind === 'glb' ? 'glTF binary' : 'PNG image', extensions: [kind] }] });
  if (result.canceled || !result.filePath) return { canceled: true };
  await writeAtomically(result.filePath, Buffer.from(bytes));
  return { canceled: false };
});

function createWindow() {
  mainWindow = new BrowserWindow({
    width: 1500, height: 940, minWidth: 1000, minHeight: 700,
    backgroundColor: '#20212b', title: 'Forma Studio', show: false,
    icon: path.join(__dirname, '..', 'app', 'assets', process.platform === 'win32' ? 'icon.ico' : 'icon.png'),
    webPreferences: {
      preload: path.join(__dirname, 'preload.cjs'),
      nodeIntegration: false, contextIsolation: true, sandbox: true,
      webSecurity: true, allowRunningInsecureContent: false, devTools: false,
    },
  });
  mainWindow.removeMenu();
  mainWindow.webContents.setWindowOpenHandler(() => ({ action: 'deny' }));
  mainWindow.webContents.on('will-navigate', (event) => event.preventDefault());
  mainWindow.webContents.on('will-attach-webview', (event) => event.preventDefault());
  mainWindow.webContents.on('will-prevent-unload', (event) => {
    const choice = dialog.showMessageBoxSync(mainWindow, { type: 'warning', title: 'Unsaved changes', message: 'Your scene has unsaved changes.', detail: 'Cancel to go back and save, or discard the changes and close.', buttons: ['Cancel', 'Discard changes'], defaultId: 0, cancelId: 0, noLink: true });
    if (choice === 1) event.preventDefault();
  });
  mainWindow.once('ready-to-show', () => mainWindow.show());
  mainWindow.on('closed', () => { mainWindow = null; currentProjectPath = null; pendingProjectPath = null; });
  mainWindow.loadFile(path.join(__dirname, '..', 'app', 'index.html'));
}

if (!app.requestSingleInstanceLock()) app.quit();
else {
  app.on('second-instance', () => { if (mainWindow) { if (mainWindow.isMinimized()) mainWindow.restore(); mainWindow.focus(); } });
  app.whenReady().then(() => {
    Menu.setApplicationMenu(null);
    session.defaultSession.setPermissionRequestHandler((_contents, _permission, callback) => callback(false));
    session.defaultSession.setPermissionCheckHandler(() => false);
    // Forma is offline; local project files are accessed only via the narrow IPC bridge.
    session.defaultSession.webRequest.onBeforeRequest({ urls: ['http://*/*', 'https://*/*', 'ws://*/*', 'wss://*/*'] }, (_details, callback) => callback({ cancel: true }));
    createWindow();
    app.on('activate', () => { if (BrowserWindow.getAllWindows().length === 0) createWindow(); });
  });
}
app.on('window-all-closed', () => { if (process.platform !== 'darwin') app.quit(); });
