/*
Export Kit for Photoshop
Copyright (C) 2026 Emmanuel Suah
SPDX-License-Identifier: GPL-2.0-or-later
*/
"use strict";

/* UXP resolves require() in an HTML script from the plugin root. Loading the
 * panel as a CommonJS module lets its own imports resolve from src/. */
require("./src/panel.js");
