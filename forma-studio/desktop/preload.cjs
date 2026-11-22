/* Forma Studio's narrow desktop bridge. Copyright (C) 2026 Emmanuel Suah. GPL-2.0-or-later. */
'use strict';
const { contextBridge, ipcRenderer } = require('electron');

contextBridge.exposeInMainWorld('forma', Object.freeze({
  openProject: () => ipcRenderer.invoke('forma:open'),
  acceptOpenedProject: () => ipcRenderer.invoke('forma:accept-open'),
  forgetProject: () => ipcRenderer.invoke('forma:forget'),
  saveProject: (text, name, saveAs) => ipcRenderer.invoke('forma:save', text, name, saveAs),
  exportFile: (kind, bytes, name) => ipcRenderer.invoke('forma:export', kind, bytes, name),
}));
