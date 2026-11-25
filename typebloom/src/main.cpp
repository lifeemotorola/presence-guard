/*
 * TypeBloom — a small, local-only Windows typing speed widget.
 * Copyright (C) 2026 Emmanuel Suah. SPDX-License-Identifier: GPL-2.0-or-later
 *
 * The low-level keyboard hook uses the virtual key only to classify an event.
 * It never decodes text, reads the foreground window's contents, or stores a
 * key code. Only timestamps and aggregate counts are kept, for at most 15 s.
 */
#include <windows.h>
#include <windowsx.h>
#include <objidl.h>
#include <gdiplus.h>
#include <wtsapi32.h>

#include <algorithm>
#include <cstdint>
#include <string>

#include "meter.h"

namespace {
using namespace Gdiplus;
using typebloom::Meter;
using typebloom::Snapshot;

constexpr wchar_t kClass[] = L"TypeBloomWidgetWindow";
constexpr wchar_t kRegistryPath[] = L"Software\\TypeBloom";
constexpr int kWidth = 360;
constexpr int kHeight = 302;
constexpr UINT_PTR kTimer = 1;
constexpr int kStart = 1001;
constexpr int kReset = 1002;
constexpr int kPin = 1003;
constexpr int kPrivacy = 1004;
constexpr int kClose = 1005;
constexpr int kStatus = 1101;
constexpr int kValue = 1102;
constexpr int kCount = 1103;

const Color kBackground(255, 19, 32, 30);
const Color kBorder(255, 48, 68, 62);
const Color kText(255, 240, 247, 241);
const Color kMuted(255, 158, 181, 169);
const Color kAccent(255, 185, 242, 205);
const Color kAccentInk(255, 20, 48, 35);
const Color kButton(255, 39, 59, 52);

struct App {
	HINSTANCE instance = nullptr;
	HWND window = nullptr;
	HWND start = nullptr;
	HWND reset = nullptr;
	HWND pin = nullptr;
	HWND privacy = nullptr;
	HWND close = nullptr;
	HWND status = nullptr;
	HWND value = nullptr;
	HWND count = nullptr;
	HHOOK hook = nullptr;
	UINT dpi = 96;
	bool topmost = true;
	bool tracking = false;
	bool consent = false;
	bool has_started = false;
	bool failed = false;
	int region_width = 0;
	int region_height = 0;
	Meter meter;
	typebloom::ModifierState modifiers;
	Snapshot snapshot;
} app;

int px(int dip)
{
	return MulDiv(dip, static_cast<int>(app.dpi), 96);
}

void rounded(Graphics &graphics, float x, float y, float w, float h, float radius, Color fill,
	     const Color *outline = nullptr)
{
	GraphicsPath path;
	radius = std::min(radius, std::min(w, h) / 2.0f);
	const float d = radius * 2;
	path.AddArc(x, y, d, d, 180, 90);
	path.AddArc(x + w - d, y, d, d, 270, 90);
	path.AddArc(x + w - d, y + h - d, d, d, 0, 90);
	path.AddArc(x, y + h - d, d, d, 90, 90);
	path.CloseFigure();
	SolidBrush brush(fill);
	graphics.FillPath(&brush, &path);
	if (outline) {
		Pen pen(*outline, 1);
		graphics.DrawPath(&pen, &path);
	}
}

void label(Graphics &graphics, const std::wstring &value, float x, float y, float w, float h, float size, Color color,
	   int style = FontStyleRegular, StringAlignment align = StringAlignmentNear)
{
	FontFamily family(L"Segoe UI");
	Font font(&family, size, style, UnitPixel);
	SolidBrush brush(color);
	StringFormat format;
	format.SetAlignment(align);
	format.SetLineAlignment(StringAlignmentCenter);
	format.SetFormatFlags(StringFormatFlagsNoWrap);
	format.SetTrimming(StringTrimmingEllipsisCharacter);
	graphics.DrawString(value.c_str(), -1, &font, RectF(x, y, w, h), &format, &brush);
}

void configure(Graphics &graphics)
{
	graphics.SetSmoothingMode(SmoothingModeAntiAlias);
	graphics.SetTextRenderingHint(TextRenderingHintAntiAliasGridFit);
	const float scale = static_cast<float>(app.dpi) / 96.0f;
	graphics.ScaleTransform(scale, scale);
}

void set_text(HWND window, const std::wstring &value)
{
	if (!window)
		return;
	wchar_t old[160]{};
	GetWindowTextW(window, old, static_cast<int>(sizeof(old) / sizeof(old[0])));
	if (value != old) {
		SetWindowTextW(window, value.c_str());
		NotifyWinEvent(EVENT_OBJECT_NAMECHANGE, window, OBJID_CLIENT, CHILDID_SELF);
	}
}

void update_ui()
{
	app.snapshot = app.tracking ? app.meter.snapshot(GetTickCount64()) : Snapshot{};
	set_text(app.start, app.tracking      ? L"Pause tracking"
			    : app.has_started ? L"Resume tracking"
					      : L"Start tracking");
	set_text(app.pin, app.topmost ? L"Always on top: on" : L"Always on top: off");
	set_text(app.status, app.tracking      ? L"Live - monitoring across apps"
			     : app.failed      ? L"Unavailable - tracking did not start"
			     : app.has_started ? L"Paused - tracking is off"
					       : L"Ready - tracking is off");
	set_text(app.value, app.tracking ? std::to_wstring(app.snapshot.wpm) + L" WPM" : L"\u2014 WPM");
	set_text(app.count, std::to_wstring(app.snapshot.keys) + L" keys in the last 15 seconds");
	if (app.window)
		InvalidateRect(app.window, nullptr, FALSE);
}

void move_control(HWND control, int x, int y, int width, int height)
{
	if (control)
		MoveWindow(control, px(x), px(y), px(width), px(height), TRUE);
}

void layout()
{
	move_control(app.close, 314, 13, 30, 30);
	move_control(app.status, 17, 72, 150, 20);
	move_control(app.value, 16, 93, 184, 73);
	move_control(app.count, 205, 164, 139, 20);
	move_control(app.start, 16, 204, 205, 43);
	move_control(app.reset, 230, 204, 114, 43);
	move_control(app.pin, 16, 265, 181, 27);
	move_control(app.privacy, 250, 265, 94, 27);
}

void draw_background(HDC dc, int width, int height)
{
	Graphics graphics(dc);
	graphics.Clear(kBackground);
	configure(graphics);
	Pen border(kBorder, 1);
	graphics.DrawRectangle(&border, 0.5f, 0.5f, kWidth - 1.0f, kHeight - 1.0f);
	rounded(graphics, 16, 14, 30, 30, 9, kAccent);
	// A tiny keyboard mark, drawn rather than read from user input.
	SolidBrush ink(kAccentInk);
	graphics.FillRectangle(&ink, 22.0f, 22.0f, 4.0f, 4.0f);
	graphics.FillRectangle(&ink, 29.0f, 22.0f, 4.0f, 4.0f);
	graphics.FillRectangle(&ink, 36.0f, 22.0f, 4.0f, 4.0f);
	graphics.FillRectangle(&ink, 22.0f, 29.0f, 4.0f, 4.0f);
	graphics.FillRectangle(&ink, 29.0f, 29.0f, 11.0f, 4.0f);
	label(graphics, L"TypeBloom", 56, 11, 180, 22, 16, kText, FontStyleBold);
	label(graphics, L"YOUR FLOATING SPEED METER", 57, 32, 200, 13, 9.5f, kMuted, FontStyleBold);
	Pen divider(Color(255, 45, 62, 56), 1);
	graphics.DrawLine(&divider, 16.0f, 59.5f, 344.0f, 59.5f);
	graphics.DrawLine(&divider, 16.0f, 255.5f, 344.0f, 255.5f);
	label(graphics, L"ACROSS APPS", 236, 73, 108, 18, 10, kMuted, FontStyleBold, StringAlignmentFar);
	label(graphics, L"ESTIMATED SPEED", 17, 168, 173, 16, 10, kMuted, FontStyleBold);
	label(graphics, L"LAST 15 SECONDS", 205, 99, 139, 16, 10, kMuted, FontStyleBold);
	label(graphics, L"Key counts only  \u00b7  no text saved", 17, 185, 327, 15, 10.5f, kMuted);

	// Fifteen one-second columns; the data are counts, never typed characters.
	for (std::size_t i = 0; i < app.snapshot.seconds.size(); ++i) {
		const unsigned int count = app.snapshot.seconds[i];
		const float h = count == 0 ? 4.0f : static_cast<float>(std::min(34U, 5U + count * 4U));
		const float x = 205.0f + static_cast<float>(i) * 9.2f;
		rounded(graphics, x, 154.0f - h, 6.0f, h, 3.0f,
			count == 0 ? Color(255, 52, 73, 64) : Color(255, 150, 226, 180));
	}
	(void)width;
	(void)height;
}

void paint_control(const DRAWITEMSTRUCT &item)
{
	Graphics graphics(item.hDC);
	graphics.Clear(kBackground);
	configure(graphics);
	const bool focused = (item.itemState & ODS_FOCUS) != 0;
	const bool pressed = (item.itemState & ODS_SELECTED) != 0;
	const float w = static_cast<float>(item.rcItem.right - item.rcItem.left) * 96.0f / app.dpi;
	const float h = static_cast<float>(item.rcItem.bottom - item.rcItem.top) * 96.0f / app.dpi;
	switch (item.CtlID) {
	case kStatus: {
		const Color dot = app.tracking ? kAccent
				  : app.failed ? Color(255, 245, 159, 145)
					       : Color(255, 189, 173, 129);
		SolidBrush brush(dot);
		graphics.FillEllipse(&brush, 1.0f, 6.0f, 8.0f, 8.0f);
		const wchar_t *name = app.tracking      ? L"LIVE"
				      : app.failed      ? L"UNAVAILABLE"
				      : app.has_started ? L"PAUSED"
							: L"READY";
		label(graphics, name, 15, 0, w - 15, h, 10.5f, app.tracking ? kAccent : kMuted, FontStyleBold);
		break;
	}
	case kValue: {
		const std::wstring number = app.tracking ? std::to_wstring(app.snapshot.wpm) : L"\u2014";
		const float size = number.size() >= 4 ? 48.0f : 59.0f;
		FontFamily family(L"Segoe UI");
		Font font(&family, size, FontStyleBold, UnitPixel);
		RectF bounds;
		graphics.MeasureString(number.c_str(), -1, &font, PointF(0, 0), &bounds);
		label(graphics, number, 0, 0, 155, h, size, kText, FontStyleBold);
		const float suffix_x = std::min(145.0f, std::max(38.0f, bounds.Width + 1.0f));
		label(graphics, L"WPM", suffix_x, 39, w - suffix_x, 21, 11, kMuted, FontStyleBold);
		break;
	}
	case kCount:
		label(graphics, std::to_wstring(app.snapshot.keys) + L" keys counted", 0, 0, w, h, 11, kMuted);
		break;
	case kStart:
	case kReset: {
		Color fill = item.CtlID == kStart ? (pressed ? Color(255, 151, 218, 175) : kAccent)
						  : (pressed ? Color(255, 54, 78, 67) : kButton);
		rounded(graphics, 0.5f, 0.5f, w - 1, h - 1, 9, fill, focused ? &kText : nullptr);
		wchar_t text[80]{};
		GetWindowTextW(item.hwndItem, text, static_cast<int>(sizeof(text) / sizeof(text[0])));
		label(graphics, text, 4, 0, w - 8, h, 13, item.CtlID == kStart ? kAccentInk : kText, FontStyleBold,
		      StringAlignmentCenter);
		break;
	}
	case kPin: {
		if (pressed)
			rounded(graphics, 0, 0, w, h, 6, kButton);
		if (focused)
			rounded(graphics, 0.5f, 0.5f, w - 1, h - 1, 6, kBackground, &kMuted);
		Pen ring(app.topmost ? kAccent : kMuted, 1.3f);
		graphics.DrawEllipse(&ring, 1.5f, 9.0f, 9.0f, 9.0f);
		if (app.topmost) {
			SolidBrush brush(kAccent);
			graphics.FillEllipse(&brush, 4.5f, 12.0f, 3.0f, 3.0f);
		}
		label(graphics, app.topmost ? L"Pinned on top" : L"Pin on top", 17, 0, w - 17, h, 11.5f, kText,
		      FontStyleBold);
		break;
	}
	case kPrivacy:
		if (pressed)
			rounded(graphics, 0, 0, w, h, 6, kButton);
		if (focused)
			rounded(graphics, 0.5f, 0.5f, w - 1, h - 1, 6, kBackground, &kMuted);
		label(graphics, L"Privacy", 0, 0, w, h, 11.5f, kMuted, FontStyleBold, StringAlignmentFar);
		break;
	case kClose: {
		if (pressed)
			rounded(graphics, 1, 1, w - 2, h - 2, 7, kButton);
		if (focused)
			rounded(graphics, 0.5f, 0.5f, w - 1, h - 1, 7, kBackground, &kMuted);
		Pen pen(kMuted, 1.6f);
		graphics.DrawLine(&pen, 10.0f, 10.0f, 20.0f, 20.0f);
		graphics.DrawLine(&pen, 20.0f, 10.0f, 10.0f, 20.0f);
		break;
	}
	}
}

LRESULT CALLBACK keyboard_hook(int code, WPARAM message, LPARAM data)
{
	if (code == HC_ACTION && app.tracking) {
		const bool down = message == WM_KEYDOWN || message == WM_SYSKEYDOWN;
		const bool up = message == WM_KEYUP || message == WM_SYSKEYUP;
		if (down || up) {
			const auto *event = reinterpret_cast<const KBDLLHOOKSTRUCT *>(data);
			app.modifiers.observe(event->vkCode, down);
			DWORD process = 0;
			const HWND foreground = GetForegroundWindow();
			if (foreground)
				GetWindowThreadProcessId(foreground, &process);
			// Widget controls and its dialogs must not affect the user's speed.
			// The virtual key is classified, then discarded; no text is decoded.
			if (down && process != 0 && process != GetCurrentProcessId() &&
			    typebloom::is_typing_key(event->vkCode, app.modifiers.current())) {
				app.meter.record(GetTickCount64());
			}
		}
	}
	return CallNextHookEx(nullptr, code, message, data); // never swallow or alter input
}

void pause_tracking()
{
	app.tracking = false;
	if (app.hook) {
		UnhookWindowsHookEx(app.hook);
		app.hook = nullptr;
	}
	app.meter.clear();
	app.modifiers = {};
	update_ui();
}

void start_tracking()
{
	if (!app.consent) {
		const int answer = MessageBoxW(
			app.window,
			L"TypeBloom counts typing-like key presses in other apps to estimate speed. "
			L"It does not save or send typed text.\n\n"
			L"Password fields are not detected: their key presses may still affect the visible count. "
			L"Pause TypeBloom before entering anything sensitive.\n\n"
			L"Start tracking on this computer?",
			L"Start tracking?", MB_OKCANCEL | MB_ICONINFORMATION | MB_DEFBUTTON2);
		if (answer != IDOK)
			return;
		app.consent = true;
	}
	app.meter.clear();
	// Seed keys already held when tracking starts, then use hook transitions.
	app.modifiers = {};
	for (const int vk : {VK_LCONTROL, VK_RCONTROL, VK_LMENU, VK_RMENU, VK_LWIN, VK_RWIN}) {
		app.modifiers.observe(static_cast<unsigned int>(vk), (GetAsyncKeyState(vk) & 0x8000) != 0);
	}
	app.hook = SetWindowsHookExW(WH_KEYBOARD_LL, keyboard_hook, app.instance, 0);
	if (!app.hook) {
		app.failed = true;
		update_ui();
		MessageBoxW(app.window,
			    L"Windows could not start the keyboard monitor. Tracking is off. "
			    L"You can try again, or close the widget.",
			    L"TypeBloom", MB_OK | MB_ICONWARNING);
		return;
	}
	app.failed = false;
	app.has_started = true;
	app.tracking = true;
	update_ui();
}

void show_privacy()
{
	MessageBoxW(
		app.window,
		L"TypeBloom starts paused. When you choose to start, it observes typing-like key presses "
		L"across ordinary desktop apps. The hook is removed when paused, minimized or closed.\n\n"
		L"No typed characters, key codes, window titles, or history are stored or sent. "
		L"Only recent timing/counts are held in memory; they are cleared on pause, reset, and exit. "
		L"Window position and pin preference are saved in your Windows user profile.\n\n"
		L"Password fields are not detected. Their keystrokes can affect the visible number, "
		L"so pause before entering sensitive information. Use only on a device you have permission to monitor.\n\n"
		L"WPM is an estimate: five counted keys per word, in a rolling 15-second window. "
		L"Shortcuts are generally excluded; backspace, editing keys, IMEs and some layouts are not fully measured.",
		L"TypeBloom Privacy", MB_OK | MB_ICONINFORMATION);
}

bool read_dword(HKEY key, const wchar_t *name, DWORD &result)
{
	DWORD type = 0, size = sizeof(result);
	return RegQueryValueExW(key, name, nullptr, &type, reinterpret_cast<BYTE *>(&result), &size) == ERROR_SUCCESS &&
	       type == REG_DWORD && size == sizeof(result);
}

struct Settings {
	bool has_position = false;
	int x = 0;
	int y = 0;
	bool topmost = true;
};

Settings load_settings()
{
	Settings settings;
	HKEY key;
	if (RegOpenKeyExW(HKEY_CURRENT_USER, kRegistryPath, 0, KEY_QUERY_VALUE, &key) == ERROR_SUCCESS) {
		DWORD x, y, topmost;
		if (read_dword(key, L"X", x) && read_dword(key, L"Y", y)) {
			settings.x = static_cast<int>(static_cast<std::int32_t>(x));
			settings.y = static_cast<int>(static_cast<std::int32_t>(y));
			settings.has_position = true;
		}
		if (read_dword(key, L"AlwaysOnTop", topmost))
			settings.topmost = topmost != 0;
		RegCloseKey(key);
	}
	return settings;
}

void save_settings()
{
	if (!app.window)
		return;
	HKEY key;
	if (RegCreateKeyExW(HKEY_CURRENT_USER, kRegistryPath, 0, nullptr, 0, KEY_SET_VALUE, nullptr, &key, nullptr) !=
	    ERROR_SUCCESS)
		return;
	const DWORD topmost = app.topmost ? 1 : 0;
	RegSetValueExW(key, L"AlwaysOnTop", 0, REG_DWORD, reinterpret_cast<const BYTE *>(&topmost), sizeof(topmost));
	if (!IsIconic(app.window)) {
		RECT rect;
		if (GetWindowRect(app.window, &rect)) {
			const DWORD x = static_cast<DWORD>(static_cast<std::int32_t>(rect.left));
			const DWORD y = static_cast<DWORD>(static_cast<std::int32_t>(rect.top));
			RegSetValueExW(key, L"X", 0, REG_DWORD, reinterpret_cast<const BYTE *>(&x), sizeof(x));
			RegSetValueExW(key, L"Y", 0, REG_DWORD, reinterpret_cast<const BYTE *>(&y), sizeof(y));
		}
	}
	RegCloseKey(key);
}

RECT fit_to_monitor(RECT rect)
{
	MONITORINFO info{};
	info.cbSize = sizeof(info);
	if (!GetMonitorInfoW(MonitorFromRect(&rect, MONITOR_DEFAULTTONEAREST), &info))
		return rect;
	const int width = rect.right - rect.left;
	const int height = rect.bottom - rect.top;
	const int max_x = std::max(static_cast<int>(info.rcWork.left), static_cast<int>(info.rcWork.right) - width);
	const int max_y = std::max(static_cast<int>(info.rcWork.top), static_cast<int>(info.rcWork.bottom) - height);
	rect.left = std::clamp(static_cast<int>(rect.left), static_cast<int>(info.rcWork.left), max_x);
	rect.top = std::clamp(static_cast<int>(rect.top), static_cast<int>(info.rcWork.top), max_y);
	rect.right = rect.left + width;
	rect.bottom = rect.top + height;
	return rect;
}

void set_region(int width, int height)
{
	if (width == app.region_width && height == app.region_height)
		return;
	app.region_width = width;
	app.region_height = height;
	HRGN region = CreateRoundRectRgn(0, 0, width + 1, height + 1, px(18), px(18));
	if (region && !SetWindowRgn(app.window, region, TRUE))
		DeleteObject(region);
}

HWND child(const wchar_t *type, const wchar_t *text, DWORD style, int id)
{
	return CreateWindowExW(0, type, text, WS_CHILD | WS_VISIBLE | style, 0, 0, 1, 1, app.window,
			       reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)), app.instance, nullptr);
}

LRESULT CALLBACK window_proc(HWND window, UINT message, WPARAM wparam, LPARAM lparam)
{
	switch (message) {
	case WM_CREATE:
		app.window = window;
		app.dpi = GetDpiForWindow(window);
		if (!app.dpi)
			app.dpi = 96;
		app.start = child(L"BUTTON", L"Start tracking", WS_TABSTOP | BS_OWNERDRAW, kStart);
		app.reset = child(L"BUTTON", L"Reset", WS_TABSTOP | BS_OWNERDRAW, kReset);
		app.pin = child(L"BUTTON", L"Always on top: on", WS_TABSTOP | BS_OWNERDRAW, kPin);
		app.privacy = child(L"BUTTON", L"Privacy and how it works", WS_TABSTOP | BS_OWNERDRAW, kPrivacy);
		app.close = child(L"BUTTON", L"Close TypeBloom", WS_TABSTOP | BS_OWNERDRAW, kClose);
		app.status = child(L"STATIC", L"Ready - tracking is off", SS_OWNERDRAW, kStatus);
		app.value = child(L"STATIC", L"\u2014 WPM", SS_OWNERDRAW, kValue);
		app.count = child(L"STATIC", L"0 keys in the last 15 seconds", SS_OWNERDRAW, kCount);
		if (!app.start || !app.reset || !app.pin || !app.privacy || !app.close || !app.status || !app.value ||
		    !app.count)
			return -1;
		layout();
		update_ui();
		SetTimer(window, kTimer, 250, nullptr);
		// Lock/disconnect notifications are best-effort; manual pause remains available.
		WTSRegisterSessionNotification(window, NOTIFY_FOR_THIS_SESSION);
		return 0;
	case WM_SIZE:
		if (wparam == SIZE_MINIMIZED) {
			// Do not continue observing keys when the widget is minimized.
			if (app.tracking)
				pause_tracking();
		} else {
			set_region(LOWORD(lparam), HIWORD(lparam));
			layout();
		}
		return 0;
	case WM_DPICHANGED: {
		app.dpi = HIWORD(wparam);
		if (!app.dpi)
			app.dpi = 96;
		const RECT *suggested = reinterpret_cast<const RECT *>(lparam);
		RECT rect{suggested->left, suggested->top, suggested->left + px(kWidth), suggested->top + px(kHeight)};
		rect = fit_to_monitor(rect);
		SetWindowPos(window, nullptr, rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top,
			     SWP_NOACTIVATE | SWP_NOZORDER);
		layout();
		return 0;
	}
	case WM_DISPLAYCHANGE: {
		if (!IsIconic(window)) {
			RECT rect;
			GetWindowRect(window, &rect);
			const RECT fitted = fit_to_monitor(rect);
			if (rect.left != fitted.left || rect.top != fitted.top)
				SetWindowPos(window, nullptr, fitted.left, fitted.top, 0, 0,
					     SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
		}
		return 0;
	}
	case WM_WTSSESSION_CHANGE:
		if (wparam == WTS_SESSION_LOCK || wparam == WTS_REMOTE_DISCONNECT)
			pause_tracking();
		return 0;
	case WM_POWERBROADCAST:
		if (wparam == PBT_APMSUSPEND)
			pause_tracking();
		return TRUE;
	case WM_EXITSIZEMOVE:
		save_settings();
		return 0;
	case WM_NCHITTEST: {
		RECT rect;
		GetWindowRect(window, &rect);
		const int x = GET_X_LPARAM(lparam) - rect.left;
		const int y = GET_Y_LPARAM(lparam) - rect.top;
		if (x >= 0 && x < px(304) && y >= 0 && y < px(58))
			return HTCAPTION;
		return HTCLIENT;
	}
	case WM_ERASEBKGND:
		return 1;
	case WM_PAINT: {
		PAINTSTRUCT ps;
		HDC dc = BeginPaint(window, &ps);
		RECT client;
		GetClientRect(window, &client);
		HDC memory = CreateCompatibleDC(dc);
		HBITMAP bitmap = CreateCompatibleBitmap(dc, client.right, client.bottom);
		HGDIOBJ old = SelectObject(memory, bitmap);
		draw_background(memory, client.right, client.bottom);
		BitBlt(dc, 0, 0, client.right, client.bottom, memory, 0, 0, SRCCOPY);
		SelectObject(memory, old);
		DeleteObject(bitmap);
		DeleteDC(memory);
		EndPaint(window, &ps);
		return 0;
	}
	case WM_DRAWITEM:
		if (lparam) {
			paint_control(*reinterpret_cast<DRAWITEMSTRUCT *>(lparam));
			return TRUE;
		}
		break;
	case WM_TIMER:
		if (wparam == kTimer)
			update_ui();
		return 0;
	case WM_COMMAND:
		if (HIWORD(wparam) == BN_CLICKED) {
			switch (LOWORD(wparam)) {
			case kStart:
				if (app.tracking)
					pause_tracking();
				else
					start_tracking();
				return 0;
			case kReset:
				app.meter.clear();
				update_ui();
				return 0;
			case kPin:
				app.topmost = !app.topmost;
				SetWindowPos(window, app.topmost ? HWND_TOPMOST : HWND_NOTOPMOST, 0, 0, 0, 0,
					     SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
				save_settings();
				update_ui();
				return 0;
			case kPrivacy:
				show_privacy();
				return 0;
			case kClose:
				DestroyWindow(window);
				return 0;
			}
		}
		break;
	case WM_CLOSE:
		DestroyWindow(window);
		return 0;
	case WM_DESTROY:
		KillTimer(window, kTimer);
		WTSUnRegisterSessionNotification(window);
		pause_tracking();
		save_settings();
		app.window = nullptr;
		PostQuitMessage(0);
		return 0;
	}
	return DefWindowProcW(window, message, wparam, lparam);
}
} // namespace

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int)
{
	HANDLE singleton = CreateMutexW(nullptr, FALSE, L"Local\\TypeBloom.Widget.1");
	if (!singleton) {
		MessageBoxW(nullptr, L"TypeBloom could not start. Please try again.", L"TypeBloom",
			    MB_OK | MB_ICONERROR);
		return 1;
	}
	if (GetLastError() == ERROR_ALREADY_EXISTS) {
		HWND existing = FindWindowW(kClass, nullptr);
		if (existing) {
			ShowWindow(existing, SW_RESTORE);
			SetForegroundWindow(existing);
		}
		CloseHandle(singleton);
		return 0;
	}

	GdiplusStartupInput input;
	ULONG_PTR gdiplus = 0;
	if (GdiplusStartup(&gdiplus, &input, nullptr) != Ok) {
		CloseHandle(singleton);
		return 1;
	}
	app.instance = instance;
	const Settings settings = load_settings();
	app.topmost = settings.topmost;
	app.dpi = GetDpiForSystem();
	if (!app.dpi)
		app.dpi = 96;
	WNDCLASSEXW wc{};
	wc.cbSize = sizeof(wc);
	wc.hInstance = instance;
	wc.lpfnWndProc = window_proc;
	wc.lpszClassName = kClass;
	wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
	wc.hIcon = LoadIconW(instance, MAKEINTRESOURCEW(101));
	wc.hIconSm = wc.hIcon;
	if (!RegisterClassExW(&wc)) {
		GdiplusShutdown(gdiplus);
		CloseHandle(singleton);
		return 1;
	}

	RECT work{};
	SystemParametersInfoW(SPI_GETWORKAREA, 0, &work, 0);
	const int width = px(kWidth), height = px(kHeight);
	const int x = settings.has_position ? settings.x : static_cast<int>(work.right) - width - px(24);
	const int y = settings.has_position ? settings.y : static_cast<int>(work.top) + px(24);
	RECT rect{x, y, x + width, y + height};
	rect = fit_to_monitor(rect);
	HWND window = CreateWindowExW(WS_EX_APPWINDOW | WS_EX_CONTROLPARENT | (app.topmost ? WS_EX_TOPMOST : 0), kClass,
				      L"TypeBloom", WS_POPUP | WS_SYSMENU | WS_MINIMIZEBOX | WS_CLIPCHILDREN, rect.left,
				      rect.top, width, height, nullptr, nullptr, instance, nullptr);
	if (!window) {
		MessageBoxW(nullptr, L"TypeBloom could not create its window.", L"TypeBloom", MB_OK | MB_ICONERROR);
		GdiplusShutdown(gdiplus);
		CloseHandle(singleton);
		return 1;
	}
	// The saved monitor can use a different DPI than the primary monitor.
	app.dpi = GetDpiForWindow(window);
	if (!app.dpi)
		app.dpi = 96;
	GetWindowRect(window, &rect);
	rect.right = rect.left + px(kWidth);
	rect.bottom = rect.top + px(kHeight);
	rect = fit_to_monitor(rect);
	SetWindowPos(window, nullptr, rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top,
		     SWP_NOZORDER | SWP_NOACTIVATE);
	layout();
	ShowWindow(window, SW_SHOW);
	UpdateWindow(window);
	MSG msg;
	while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
		if (!IsDialogMessageW(window, &msg)) {
			TranslateMessage(&msg);
			DispatchMessageW(&msg);
		}
	}
	GdiplusShutdown(gdiplus);
	CloseHandle(singleton);
	return static_cast<int>(msg.wParam);
}
