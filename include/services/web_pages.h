#pragma once

#include <Arduino.h>

// ---------------------------------------------------------------------------
// Shared CSS — used by both admin and provisioning pages
// ---------------------------------------------------------------------------
static const char SHARED_CSS[] PROGMEM = R"css(
:root{color-scheme:dark;--bg:#0c0f10;--surface:#171b1d;--surface-2:#111517;--border:#2a2f33;--border-strong:#40484d;--text:#f4f4f4;--muted:#c7cccf;--subtle:#98a1a6;--input:#0f1315;--primary:#68724D;--secondary:#273038;--danger:#3a2020;--msg:#1f2d24;--msg-text:#d7f2df;--error:#3a2020;--error-text:#ffd6d6;--shadow:0 18px 50px rgba(0,0,0,.22);}
@media(prefers-color-scheme:light){:root{color-scheme:light;--bg:#f5f2eb;--surface:#fffdf8;--surface-2:#f4f0e7;--border:#ded8cc;--border-strong:#bdb5a6;--text:#171b1d;--muted:#4b5257;--subtle:#737b80;--input:#ffffff;--primary:#68724D;--secondary:#e8e1d3;--msg:#e4efdc;--msg-text:#28361f;--error:#f2d7d4;--error-text:#5b1e18;--shadow:0 18px 50px rgba(74,63,44,.14);}}
*{transition:background-color .18s ease,border-color .18s ease,color .18s ease,box-shadow .18s ease,transform .18s ease;}
body{font-family:-apple-system,BlinkMacSystemFont,'Segoe UI',sans-serif;background:var(--bg);color:var(--text);margin:0;padding:24px;}
.wrap{max-width:720px;margin:0 auto;}
.card{background:var(--surface);border:1px solid var(--border);border-radius:18px;padding:20px;margin-bottom:18px;box-shadow:0 0 0 rgba(0,0,0,0);}
.card:hover{border-color:var(--border-strong);box-shadow:var(--shadow);transform:translateY(-1px);}
h1,h2{margin:0 0 12px 0;}p{color:var(--muted);line-height:1.45;}
label{display:block;margin:14px 0 6px 0;font-weight:600;}
input,select{width:100%;box-sizing:border-box;border-radius:12px;border:1px solid var(--border-strong);background:var(--input);color:var(--text);padding:14px;font-size:16px;}
input:hover,select:hover{border-color:var(--primary);}input:focus,select:focus,button:focus-visible,summary:focus-visible{outline:2px solid color-mix(in srgb,var(--primary) 70%,white);outline-offset:2px;}
.row{display:flex;gap:12px;flex-wrap:wrap;}.row > div{flex:1 1 220px;}
.actions{display:flex;gap:12px;flex-wrap:wrap;margin-top:18px;}
button{border:0;border-radius:999px;padding:12px 18px;font-size:15px;font-weight:600;cursor:pointer;}
button:hover{transform:translateY(-1px);filter:brightness(1.08);}button:active{transform:translateY(0) scale(.98);}
.primary{background:var(--primary);color:#fff;}.secondary{background:var(--secondary);color:var(--text);}.danger{background:var(--danger);color:var(--text);}
.msg{margin-bottom:16px;padding:12px 14px;border-radius:12px;background:var(--msg);color:var(--msg-text);}
.msg.error{background:var(--error);color:var(--error-text);}
.subtle{color:var(--subtle);font-size:14px;}
.mono{font-family:ui-monospace,SFMono-Regular,Menlo,monospace;font-size:14px;color:var(--muted);overflow-wrap:anywhere;word-break:break-word;}
.status{display:grid;grid-template-columns:minmax(140px,180px) 1fr;gap:8px 12px;}
.status strong{color:var(--text);}
.status span{min-width:0;overflow-wrap:anywhere;word-break:break-word;}
.status-link{color:var(--text);text-decoration:none;border-bottom:1px solid var(--border-strong);}
.status-link:hover{color:var(--primary);border-color:var(--primary);}
.pairing-code{color:var(--text);}
.checkbox-label{display:flex;align-items:center;gap:10px;margin:10px 0;font-weight:500;}
.checkbox-label input{width:auto;flex:0 0 auto;padding:0;margin:0;}
.toggle-item{margin:12px 0 16px 0;}
.toggle-item .subtle{margin:2px 0 0 0;}
.toggle-item .checkbox-label + .subtle{margin-left:34px;}
.qr{display:flex;gap:20px;align-items:flex-start;flex-wrap:wrap;}
.qr img{width:220px;height:220px;background:#fff;border-radius:18px;padding:10px;box-sizing:border-box;}
.setting-block{margin-bottom:20px;}
.setting-block:last-of-type{margin-bottom:10px;}
.stepper{display:flex;gap:10px;align-items:center;}
.stepper input{flex:1 1 auto;margin:0;}
.step-btn{min-width:52px;padding:12px 0;line-height:1;font-size:22px;}
.color-picker-wrap{position:relative;}
input[type="color"]{appearance:none;-webkit-appearance:none;padding:6px;height:58px;background:transparent;cursor:pointer;}
input[type="color"]::-webkit-color-swatch-wrapper{padding:0;}
input[type="color"]::-webkit-color-swatch{border:1px solid var(--border-strong);border-radius:10px;}
input[type="color"]::-moz-color-swatch{border:1px solid var(--border-strong);border-radius:10px;}
.color-picker-plus{position:absolute;inset:0;display:flex;align-items:center;justify-content:center;pointer-events:none;font-size:34px;font-weight:500;color:var(--text);text-shadow:0 1px 4px var(--bg),0 0 10px var(--bg);}
.swatches{display:flex;gap:10px;flex-wrap:wrap;margin-top:12px;}
.swatch{width:42px;height:42px;border-radius:999px;border:2px solid var(--border-strong);padding:0;background-clip:padding-box;box-shadow:inset 0 0 0 1px rgba(0,0,0,.18);}
.swatch:hover{transform:translateY(-2px) scale(1.04);box-shadow:0 8px 18px rgba(0,0,0,.18);}
.swatch.active{border-color:var(--text);}
.section-card{padding:0;overflow:hidden;}
.section-card summary{list-style:none;cursor:pointer;padding:20px;font-size:18px;font-weight:700;}
.section-card summary:hover{background:color-mix(in srgb,var(--primary) 10%,transparent);}
.section-card summary::-webkit-details-marker{display:none;}
.section-card summary::after{content:'';border:solid var(--subtle);border-width:0 2px 2px 0;display:inline-block;padding:3px;transform:rotate(-45deg);transition:transform .2s ease;float:right;margin-top:6px;}
.section-card[open]>summary::after{transform:rotate(45deg);}
.section-body{padding:0 20px 20px 20px;border-top:1px solid var(--border);}
.section-card .manual-location summary{padding:5px 10px;font-size:12px;font-weight:400;line-height:1.25;color:var(--subtle);}
.section-card .manual-location summary:hover{color:var(--text);background:color-mix(in srgb,var(--secondary) 32%,transparent);}
.section-card .manual-location summary::after{content:'';border:solid var(--subtle);border-width:0 1.5px 1.5px 0;display:inline-block;padding:2px;transform:rotate(-45deg);transition:transform .2s ease;float:right;margin-top:2px;}
.section-card .manual-location[open] summary::after{transform:rotate(45deg);}
.search-results{display:flex;flex-direction:column;gap:10px;margin-top:16px;}
.result-form{margin:0;}
.result-btn{width:100%;text-align:left;background:var(--surface-2);border:1px solid var(--border);border-radius:14px;padding:14px 16px;color:var(--text);}
.result-btn:hover{border-color:var(--primary);box-shadow:0 10px 24px color-mix(in srgb,var(--primary) 16%,transparent);}
.result-title{display:block;font-size:16px;font-weight:700;margin-bottom:4px;}
.result-meta{display:block;color:var(--muted);font-size:14px;line-height:1.35;}
.location-search{display:flex;gap:10px;align-items:stretch;}
.location-search input{flex:1 1 auto;min-width:0;}
.pin-btn{flex:0 0 54px;border-radius:12px;padding:0;background:var(--secondary);color:var(--text);display:flex;align-items:center;justify-content:center;}
.pin-btn svg{width:20px;height:20px;display:block;}
.manual-location{margin-top:10px;border:1px solid color-mix(in srgb,var(--border) 58%,transparent);border-radius:10px;background:transparent;overflow:hidden;}
.manual-location summary{padding:5px 10px;font-size:12px;font-weight:400;color:var(--subtle);cursor:pointer;line-height:1.25;}
.manual-location summary:hover{color:var(--text);background:color-mix(in srgb,var(--secondary) 32%,transparent);}
.manual-location .manual-body{padding:0 10px 10px 10px;border-top:1px solid color-mix(in srgb,var(--border) 58%,transparent);}
.manual-location .manual-body label{font-size:13px;font-weight:500;}
.manual-location .manual-body input,.manual-location .manual-body select{padding:10px 11px;font-size:14px;}
.hidden-form{display:none;}
.weather-location-hint{display:none;margin-top:10px;padding:10px 12px;border-radius:12px;background:var(--error);color:var(--error-text);font-size:14px;line-height:1.4;}
.weather-location-hint.show{display:block;}
.brand-footer{text-align:center;margin:34px 0 10px 0;color:var(--text);}
.brand-footer strong{display:block;font-size:34px;line-height:1;font-weight:800;letter-spacing:-.04em;}
.brand-footer span{display:block;margin-top:6px;font-size:13px;color:var(--subtle);letter-spacing:.02em;}
.brand-footer a{display:inline-flex;align-items:center;gap:7px;margin-top:14px;color:var(--subtle);text-decoration:none;font-size:13px;}
.brand-footer a:hover{color:var(--text);}
.brand-footer svg{width:17px;height:17px;display:block;}
@media(max-width:520px){body{padding:14px;}.card{padding:16px;}.status{grid-template-columns:1fr;gap:4px;}.status strong{margin-top:8px;}}
)css";

// ---------------------------------------------------------------------------
// Provisioning page additional CSS
// ---------------------------------------------------------------------------
static const char PROVISIONING_CSS[] PROGMEM = R"css(
.scan-results{display:flex;flex-direction:column;gap:8px;margin-top:10px;}
.scan-item{display:flex;justify-content:space-between;gap:12px;background:var(--surface-2);border:1px solid var(--border);border-radius:14px;padding:12px 14px;color:var(--text);}
.scan-item:hover{border-color:var(--primary);box-shadow:0 10px 24px color-mix(in srgb,var(--primary) 16%,transparent);transform:translateY(-1px);}
.scan-panel{margin:8px 0 14px 0;border:1px solid var(--border);border-radius:14px;background:var(--surface-2);overflow:hidden;}
.scan-panel summary{list-style:none;cursor:pointer;padding:12px 14px;color:var(--subtle);font-size:14px;font-weight:600;}
.scan-panel summary:hover{color:var(--text);background:color-mix(in srgb,var(--primary) 10%,transparent);}
.scan-panel summary::-webkit-details-marker{display:none;}
.scan-panel summary::after{content:'';border:solid var(--text);border-width:0 2px 2px 0;display:inline-block;padding:3px;transform:rotate(-45deg);transition:transform .18s ease;float:right;margin-top:4px;}
.scan-panel[open] summary::after{transform:rotate(45deg);}
.scan-panel[open] summary{border-bottom:1px solid var(--border);}
@media(max-width:520px){body{padding:14px;}.card{padding:16px;}.scan-item{flex-direction:column;}}
)css";

// ---------------------------------------------------------------------------
// Admin page templates
// ---------------------------------------------------------------------------
static const char ADMIN_HEAD_OPEN[] PROGMEM = R"html(
<!doctype html><html><head><meta charset="utf-8">
<link rel="icon" href="data:,">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>HOMEsmthng Admin</title>
<style>)html";

static const char ADMIN_HEAD_CLOSE[] PROGMEM = R"html(
</style></head><body><div class="wrap">)html";

static const char ADMIN_STATUS_CARD_START[] PROGMEM = R"html(
<div class="card"><h1>)html";

static const char ADMIN_STATUS_CARD_INTRO[] PROGMEM = R"html(
</h1><p>Admin UI for device setup, Wi-Fi, display settings, weather and Apple Home.</p>
<div class="status">
<strong>Hostname</strong><span class="mono">)html";

static const char ADMIN_STATUS_CARD_WIFI[] PROGMEM = R"html(
.local</span>
<strong>Wi-Fi</strong><span>)html";

static const char ADMIN_STATUS_CARD_MDNS[] PROGMEM = R"html(
</span>
<strong>Primary URL</strong><span class="mono">)html";

static const char ADMIN_STATUS_CARD_DIRECT[] PROGMEM = R"html(
</span>
<strong>Alternative URL</strong><span class="mono">)html";

static const char ADMIN_STATUS_CARD_SETUP[] PROGMEM = R"html(
</span>
<strong>Setup Portal</strong><span class="mono">)html";

static const char ADMIN_STATUS_CARD_HOMEKIT[] PROGMEM = R"html(
</span>
<strong>HomeKit</strong><span><a class="status-link" href="/?section=homekit">)html";

static const char ADMIN_STATUS_CARD_END[] PROGMEM = R"html(
</a></span>
</div></div>)html";

// Section card wrappers
static const char ADMIN_SECTION_OPEN[] PROGMEM = R"html(
<details class="card section-card")html";

static const char ADMIN_SECTION_OPEN_WEATHER[] PROGMEM = R"html(
<details class="card section-card" id="weather-section")html";

static const char ADMIN_SECTION_CLOSE[] PROGMEM = R"html(
</div></details>)html";

// HomeKit
static const char ADMIN_SECTION_HOMEKIT_HEAD[] PROGMEM = R"html(
><summary>Apple Home</summary><div class="section-body">)html";

static const char ADMIN_SECTION_HOMEKIT_PAIRED[] PROGMEM = R"html(
<p>The accessory is already paired to Apple Home. Remove it from the Home app if you want to pair again.</p>)html";

static const char ADMIN_SECTION_HOMEKIT_UNPAIRED_HEAD[] PROGMEM = R"html(
<p>Open the Home app and scan this QR code, or enter the pairing code manually.</p>
<div class="qr">
<img src="/homekit/qr.svg" alt="HomeKit QR code">
<div><p><strong>Pairing code</strong><br><span class="mono pairing-code">)html";

static const char ADMIN_SECTION_HOMEKIT_UNPAIRED_MID[] PROGMEM = R"html(
</span></p>
<p><strong>Setup payload</strong><br><span class="mono">)html";

static const char ADMIN_SECTION_HOMEKIT_UNPAIRED_END[] PROGMEM = R"html(
</span></p></div>
</div>)html";

// Device section
static const char ADMIN_SECTION_DEVICE[] PROGMEM = R"html(
><summary>Device</summary><div class="section-body">
<p>Change the local device label and the mDNS hostname. A reboot is required so Bonjour and the web UI stay consistent.</p>
<form method="POST" action="/device/save">
<input type="hidden" name="section" value="device">
<label for="device_label">Device label</label>
<input id="device_label" name="device_label" value=")html";

static const char ADMIN_SECTION_DEVICE_MID[] PROGMEM = R"html(
" maxlength="48">
<label for="device_host">Hostname</label>
<input id="device_host" name="device_host" value=")html";

static const char ADMIN_SECTION_DEVICE_END[] PROGMEM = R"html(
" maxlength="48" spellcheck="false">
<p class="subtle">Allowed characters: lowercase letters, numbers and hyphens. If you install multiple devices, give each one a distinct hostname.</p>
<div class="actions"><button class="primary" type="submit">Save and reboot</button></div>
</form></div></details>)html";

// Wi-Fi section
static const char ADMIN_SECTION_WIFI[] PROGMEM = R"html(
><summary>Wi-Fi</summary><div class="section-body">
<p>Update the stored Wi-Fi credentials or reset the device back into setup mode.</p>
<form method="POST" action="/wifi/save">
<input type="hidden" name="section" value="wifi">
<label for="wifi_ssid">Wi-Fi network</label>
<input id="wifi_ssid" name="ssid" value=")html";

static const char ADMIN_SECTION_WIFI_END[] PROGMEM = R"html(
" maxlength="32" spellcheck="false" autocapitalize="none" autocomplete="username">
<label for="wifi_pwd">Wi-Fi password</label>
<input id="wifi_pwd" name="pwd" type="password" value="" maxlength="64" placeholder="Enter a new password only if it changed" autocomplete="current-password" autocapitalize="none" spellcheck="false">
<p class="subtle">Saving Wi-Fi restarts the device. Leaving the password empty keeps the current password for the same SSID, or uses an open network for a new SSID.</p>
<div class="actions"><button class="primary" type="submit">Save Wi-Fi and reboot</button></div>
</form>
<form method="POST" action="/wifi/reset" class="actions">
<input type="hidden" name="section" value="wifi">
<button class="danger" type="submit">Reset Wi-Fi and enter setup mode</button>
</form></div></details>)html";

// Display section — head
static const char ADMIN_SECTION_DISPLAY_HEAD[] PROGMEM = R"html(
><summary>Appearance &amp; Display</summary><div class="section-body">
<form method="POST" action="/settings/display">
<input type="hidden" name="section" value="display">
<input type="hidden" name="color_idx" id="color_idx" value="">
<div class="setting-block"><label for="brightness_scale">Display Brightness</label>
<div class="stepper">
<button class="secondary step-btn" type="button" onclick="adjustBrightness(-1)" aria-label="Dunkler">&minus;</button>
<input id="brightness_scale" name="brightness_scale" type="number" min="0" max=")html";

static const char ADMIN_SECTION_DISPLAY_BRIGHTNESS_MID[] PROGMEM = R"html(" step="1" value=")html";

static const char ADMIN_SECTION_DISPLAY_BRIGHTNESS_END[] PROGMEM = R"html(">
<button class="secondary step-btn" type="button" onclick="adjustBrightness(1)" aria-label="Heller">+</button>
</div>
<p class="subtle">Use a simple scale from 0 (darkest) to 10 (brightest).</p>
</div><div class="setting-block"><label for="color_picker">Switch ON Color</label>
<div class="color-picker-wrap"><input class="color-input" id="color_picker" name="color_picker" type="color" value=")html";

static const char ADMIN_SECTION_DISPLAY_COLOR_MID[] PROGMEM = R"html(">
<span class="color-picker-plus" aria-hidden="true">+</span></div>
<p class="subtle">Tap the color field or choose a quick preset.</p>
<div class="swatches">)html";

static const char ADMIN_SECTION_DISPLAY_SWATCHES_END[] PROGMEM = R"html(
</div><label for="color_hex">Hex Color</label>
<input id="color_hex" name="color_hex" value=")html";

static const char ADMIN_SECTION_DISPLAY_HEX_END[] PROGMEM = R"html(
" maxlength="7" spellcheck="false">
<p class="subtle">Enter a value like #68724D.</p></div>)html";

static const char ADMIN_SECTION_DISPLAY_CHECKBOXES[] PROGMEM = R"html(
<div class="checkboxes">)html";

static const char ADMIN_SECTION_DISPLAY_PXSHIFT_CHECKED[] PROGMEM = R"html(
<div class="toggle-item"><label class="checkbox-label"><input type="checkbox" name="px_shift" checked)html";

static const char ADMIN_SECTION_DISPLAY_PXSHIFT_UNCHECKED[] PROGMEM = R"html(
<div class="toggle-item"><label class="checkbox-label"><input type="checkbox" name="px_shift")html";

static const char ADMIN_SECTION_DISPLAY_PXSHIFT_LABEL[] PROGMEM = R"html(
>Pixel Shift</label>
<p class="subtle">Moves the UI by a few pixels every so often. This helps reduce uneven OLED aging and image retention on static screens.</p></div>)html";

static const char ADMIN_SECTION_DISPLAY_CLOCKBTN_CHECKED[] PROGMEM = R"html(
<div class="toggle-item"><label class="checkbox-label"><input type="checkbox" name="clock_btn" checked)html";

static const char ADMIN_SECTION_DISPLAY_CLOCKBTN_UNCHECKED[] PROGMEM = R"html(
<div class="toggle-item"><label class="checkbox-label"><input type="checkbox" name="clock_btn")html";

static const char ADMIN_SECTION_DISPLAY_CLOCKBTN_LABEL[] PROGMEM = R"html(
>Clock Button Screen</label>
<p class="subtle">Shows the separate full-screen clock button page below the large single-button screen.</p></div>)html";

static const char ADMIN_SECTION_DISPLAY_SCREEN_ORDER_BIG[] PROGMEM = R"html(
<div class="toggle-item"><label for="screen_order">Screen order</label>
<select id="screen_order" name="screen_order">
<option value="big_first" selected>Large B1 button first</option>
<option value="clock_first">Clock button first</option>
</select><p class="subtle">Choose which B1 page appears first below Screen 1. Restart to apply the new order.</p></div>)html";

static const char ADMIN_SECTION_DISPLAY_SCREEN_ORDER_CLOCK[] PROGMEM = R"html(
<div class="toggle-item"><label for="screen_order">Screen order</label>
<select id="screen_order" name="screen_order">
<option value="big_first">Large B1 button first</option>
<option value="clock_first" selected>Clock button first</option>
</select><p class="subtle">Choose which B1 page appears first below Screen 1. Restart to apply the new order.</p></div>)html";

static const char ADMIN_SECTION_DISPLAY_EXPERIMENTAL_HEAD[] PROGMEM = R"html(
<details class="manual-location"><summary>Experimental</summary><div class="manual-body">
<label for="display_rotation">Display UI orientation</label>
<select id="display_rotation" name="display_rotation">)html";

static const char ADMIN_SECTION_DISPLAY_ROTATION_OPTION_SELECTED[] PROGMEM = R"html(
<option value=")html";

static const char ADMIN_SECTION_DISPLAY_ROTATION_OPTION[] PROGMEM = R"html(
<option value=")html";

static const char ADMIN_SECTION_DISPLAY_EXPERIMENTAL_END[] PROGMEM = R"html(
</select>
<p class="subtle">Rotates the on-device display UI and touch input in 90 degree steps. Non-zero rotations can show panel artifacts on some devices.</p>
</div></details>
<div class="actions">
<button class="primary" type="submit">Save display settings</button>
<button class="secondary" type="submit" name="restart_after_save" value="1">Save &amp; restart</button>
</div></form></div></details>)html";

// Time section
static const char ADMIN_SECTION_TIME[] PROGMEM = R"html(
><summary>Time</summary><div class="section-body">
<form method="POST" action="/settings/time">
<input type="hidden" name="section" value="time">
<label for="tz_idx">Timezone</label>
<select id="tz_idx" name="tz_idx">)html";

static const char ADMIN_SECTION_TIME_OPTION_SELECTED[] PROGMEM = R"html(
<option value=")html";

static const char ADMIN_SECTION_TIME_OPTION[] PROGMEM = R"html(
<option value=")html";

static const char ADMIN_SECTION_TIME_END[] PROGMEM = R"html(
</select><div class="actions"><button class="primary" type="submit">Save timezone</button></div></form></div></details>)html";

// Weather section
static const char ADMIN_SECTION_WEATHER_HEAD[] PROGMEM = R"html(
><summary>Weather</summary><div class="section-body">)html";

static const char ADMIN_SECTION_WEATHER_VISIBILITY_FORM[] PROGMEM = R"html(
<form method="POST" action="/weather/save#weather-section">
<input type="hidden" name="section" value="weather">
<input type="hidden" name="weather_visibility" value="1">
<input type="hidden" name="name" value=")html";

static const char ADMIN_SECTION_WEATHER_VISIBILITY_LAT[] PROGMEM = R"html(
">
<input type="hidden" name="latitude" value=")html";

static const char ADMIN_SECTION_WEATHER_VISIBILITY_LON[] PROGMEM = R"html(
">
<input type="hidden" name="longitude" value=")html";

static const char ADMIN_SECTION_WEATHER_VISIBILITY_TOGGLE_CHECKED[] PROGMEM = R"html(
">
<div class="toggle-item"><label class="checkbox-label"><input type="checkbox" name="weather_enabled" checked)html";

static const char ADMIN_SECTION_WEATHER_VISIBILITY_TOGGLE_UNCHECKED[] PROGMEM = R"html(
">
<div class="toggle-item"><label class="checkbox-label"><input type="checkbox" name="weather_enabled")html";

static const char ADMIN_SECTION_WEATHER_VISIBILITY_END[] PROGMEM = R"html(
>Show weather on clock views</label>
<p class="subtle">Applies to both the standby Clock Saver and the clock button screen. Setting a location turns this on automatically.</p></div>
<div class="actions"><button class="secondary" type="submit">Save weather visibility</button></div>
</form>)html";

static const char ADMIN_SECTION_WEATHER_INFO[] PROGMEM = R"html(
<p><strong>Weather:</strong> )html";

static const char ADMIN_SECTION_WEATHER_INFO_ENABLED[] PROGMEM = R"html(Enabled)html";
static const char ADMIN_SECTION_WEATHER_INFO_DISABLED[] PROGMEM = R"html(Disabled)html";

static const char ADMIN_SECTION_WEATHER_INFO_SOURCE[] PROGMEM = R"html(
<br>
<strong>Current source:</strong> )html";

static const char ADMIN_SECTION_WEATHER_INFO_COORDS[] PROGMEM = R"html(
<br>
<strong>Coordinates:</strong> <span class="mono">)html";

static const char ADMIN_SECTION_WEATHER_INFO_CURRENT[] PROGMEM = R"html(
</span><br>
<strong>Current weather:</strong> )html";

static const char ADMIN_SECTION_WEATHER_INFO_CLOSE[] PROGMEM = R"html(
</p>)html";

// Search form
static const char ADMIN_SECTION_WEATHER_SEARCH_FORM[] PROGMEM = R"html(
<form method="GET" action="/weather/search#weather-section">
<input type="hidden" name="section" value="weather">
<label for="query">Find city or village</label>
<div class="location-search"><input id="query" name="query" value=")html";

static const char ADMIN_SECTION_WEATHER_SEARCH_PINBTN[] PROGMEM = R"html(
" placeholder="e.g. Berlin, Tegernsee, Winterberg">
<button class="pin-btn" type="button" title="Use current browser location" aria-label="Use current browser location" onclick="useBrowserLocation(true)">
<svg viewBox="0 0 384 512" aria-hidden="true" focusable="false"><path fill="currentColor" d="M192 0C86 0 0 86 0 192c0 77 27 99 172 310 10 14 30 14 40 0 145-211 172-233 172-310C384 86 298 0 192 0zm0 272a80 80 0 1 1 0-160 80 80 0 0 1 0 160z"/></svg>
</button></div><div id="location_permission_hint" class="weather-location-hint"></div>
<div class="actions"><button class="primary" type="submit">Search place</button></div></form>)html";

static const char ADMIN_SECTION_WEATHER_BROWSER_FORM[] PROGMEM = R"html(
<form id="browser_location_form" class="hidden-form" method="POST" action="/weather/save#weather-section">
<input type="hidden" name="section" value="weather">
<input type="hidden" id="browser_location_name" name="name" value="Browser location">
<input type="hidden" id="browser_latitude" name="latitude" value="">
<input type="hidden" id="browser_longitude" name="longitude" value="">
</form>
<p class="subtle">Search uses Open-Meteo geocoding and needs an active Wi-Fi internet connection. Tap the pin to use this browser's current location.</p>)html";

// Weather manual coordinates
static const char ADMIN_SECTION_WEATHER_MANUAL[] PROGMEM = R"html(
<details class="manual-location"><summary>Manual coordinates</summary><div class="manual-body">
<form method="POST" action="/weather/save#weather-section">
<input type="hidden" name="section" value="weather">
<p class="subtle">Advanced: enter exact coordinates manually. Saving coordinates turns weather on automatically.</p>
<label for="name">Location name</label>
<input id="name" name="name" value=")html";

static const char ADMIN_SECTION_WEATHER_MANUAL_MID[] PROGMEM = R"html(
" placeholder="e.g. Munich Home">
<div class="row"><div><label for="latitude">Latitude</label>
<input id="latitude" name="latitude" value=")html";

static const char ADMIN_SECTION_WEATHER_MANUAL_LAT_END[] PROGMEM = R"html(
" inputmode="decimal">
</div><div><label for="longitude">Longitude</label>
<input id="longitude" name="longitude" value=")html";

static const char ADMIN_SECTION_WEATHER_MANUAL_END[] PROGMEM = R"html(
" inputmode="decimal">
</div></div><div class="actions">
<button class="primary" type="submit">Save location</button>
<button class="secondary" type="button" onclick="useBrowserLocation(false)">Fill with browser location</button>
</div></form></div></details>)html";

static const char ADMIN_SECTION_WEATHER_CLEAR[] PROGMEM = R"html(
<form method="POST" action="/weather/reset#weather-section" class="actions">
<input type="hidden" name="section" value="weather">
<button class="danger" type="submit">Clear weather location</button>
</form>)html";

// Search results message
static const char ADMIN_SECTION_WEATHER_SEARCH_MSG[] PROGMEM = R"html(
<div class="msg)html";

static const char ADMIN_SECTION_WEATHER_SEARCH_MSG_END[] PROGMEM = R"html(
">)html";

static const char ADMIN_SECTION_WEATHER_SEARCH_RESULTS_OPEN[] PROGMEM = R"html(
<div class="search-results">)html";

static const char ADMIN_SECTION_WEATHER_SEARCH_RESULT_FORM[] PROGMEM = R"html(
<form method="POST" action="/weather/save#weather-section" class="result-form">
<input type="hidden" name="section" value="weather">
<input type="hidden" name="name" value=")html";

static const char ADMIN_SECTION_WEATHER_SEARCH_RESULT_LAT[] PROGMEM = R"html(
">
<input type="hidden" name="latitude" value=")html";

static const char ADMIN_SECTION_WEATHER_SEARCH_RESULT_LON[] PROGMEM = R"html(
">
<input type="hidden" name="longitude" value=")html";

static const char ADMIN_SECTION_WEATHER_SEARCH_RESULT_BTN[] PROGMEM = R"html(
">
<button class="result-btn" type="submit">
<span class="result-title">)html";

static const char ADMIN_SECTION_WEATHER_SEARCH_RESULT_META[] PROGMEM = R"html(
</span>
<span class="result-meta mono">)html";

static const char ADMIN_SECTION_WEATHER_SEARCH_RESULT_CLOSE[] PROGMEM = R"html(
</span>
</button></form>)html";

static const char ADMIN_SECTION_WEATHER_SEARCH_RESULTS_CLOSE[] PROGMEM = R"html(
</div>)html";

// Battery section
static const char ADMIN_SECTION_BATTERY[] PROGMEM = R"html(
><summary>Battery</summary><div class="section-body">)html";

static const char ADMIN_SECTION_BATTERY_NO_BACKEND[] PROGMEM = R"html(
<p>This device does not have a battery.</p>)html";

static const char ADMIN_SECTION_BATTERY_NO_TELEMETRY[] PROGMEM = R"html(
<p>Battery telemetry is not available on this device.</p>)html";

static const char ADMIN_SECTION_BATTERY_STATUS_OPEN[] PROGMEM = R"html(
<div class="status" style="margin-top:12px">
<strong>Status</strong><span>)html";

static const char ADMIN_SECTION_BATTERY_CONDITION[] PROGMEM = R"html(
</span>
<strong>Condition</strong><span>)html";

static const char ADMIN_SECTION_BATTERY_VOLTAGE[] PROGMEM = R"html(
</span>
<strong>Voltage</strong><span class="mono">)html";

static const char ADMIN_SECTION_BATTERY_WARNING[] PROGMEM = R"html(
</span>
<strong style="color:var(--error-text);">Warning</strong><span style="color:var(--error-text);">)html";

static const char ADMIN_SECTION_BATTERY_STATUS_CLOSE[] PROGMEM = R"html(
</span>
</div>)html";

static const char ADMIN_SECTION_BATTERY_APPROX[] PROGMEM = R"html(
<p class="subtle">Percentage is estimated from voltage.</p>)html";

static const char ADMIN_SECTION_BATTERY_END[] PROGMEM = R"html(
</div></details>)html";

// Admin JS + footer
static const char ADMIN_JS[] PROGMEM = R"js(
<script>
function normalizeHexColor(value){
let hex=(value||'').trim().toUpperCase();
if(!hex.startsWith('#')){hex='#'+hex;}
return /^#[0-9A-F]{6}$/.test(hex)?hex:'';
}
function updatePresetSelection(hex,presetIndex){
document.querySelectorAll('.swatch').forEach(btn=>btn.classList.remove('active'));
if(typeof presetIndex==='number'&&presetIndex>=0){
const preset=document.querySelector('.swatch[data-preset-index="'+presetIndex+'"]');
if(preset){preset.classList.add('active');}
return;
}
const normalized=normalizeHexColor(hex);
if(!normalized){return;}
document.querySelectorAll('.swatch').forEach(btn=>{
if(normalizeHexColor(btn.dataset.color)===normalized){btn.classList.add('active');}
});
}
function setPresetColor(color,index){
document.getElementById('color_picker').value=color;
document.getElementById('color_hex').value=color.toUpperCase();
document.getElementById('color_idx').value=String(index);
updatePresetSelection(color,index);
}
function adjustBrightness(delta){
const input=document.getElementById('brightness_scale');
if(!input){return;}
const min=parseInt(input.min||'0',10);
const max=parseInt(input.max||'10',10);
const current=parseInt(input.value||'0',10);
const next=Math.min(max,Math.max(min,(isNaN(current)?min:current)+delta));
input.value=String(next);
}
function adjustStandbyBrightness(delta){
const input=document.getElementById('standby_brightness_scale');
if(!input){return;}
const min=parseInt(input.min||'0',10);
const max=parseInt(input.max||'10',10);
const current=parseInt(input.value||'0',10);
const next=Math.min(max,Math.max(min,(isNaN(current)?min:current)+delta));
input.value=String(next);
}
function updateStandbyBrightnessVisibility(){
const mode=document.getElementById('standby_mode');
const wrap=document.getElementById('standby_brightness_wrap');
if(!mode||!wrap){return;}
wrap.hidden=!(mode.value==='dim'||mode.value==='clock');
}
function showLocationHint(message){
const hint=document.getElementById('location_permission_hint');
if(hint){hint.textContent=message;hint.classList.add('show');}
else{alert(message);}
}
function useBrowserLocation(saveImmediately){
if(window.isSecureContext===false){
showLocationHint('Browser location is blocked because this page is opened over HTTP. Safari only asks for location permission on secure pages. Please search for your place manually.');
return;
}
if(!navigator.geolocation){showLocationHint('Geolocation is not supported by this browser. Please search for your place manually.');return;}
navigator.geolocation.getCurrentPosition(function(pos){
const hint=document.getElementById('location_permission_hint');
if(hint){hint.classList.remove('show');}
const lat=pos.coords.latitude.toFixed(6);
const lon=pos.coords.longitude.toFixed(6);
if(saveImmediately){
document.getElementById('browser_latitude').value=lat;
document.getElementById('browser_longitude').value=lon;
document.getElementById('browser_location_form').submit();
return;
}
document.getElementById('latitude').value=lat;
document.getElementById('longitude').value=lon;
const name=document.getElementById('name');
if(!name.value){name.value='Browser location';}
},function(err){showLocationHint('Unable to read your location: '+err.message);},{enableHighAccuracy:true,timeout:10000,maximumAge:60000});
}
document.addEventListener('DOMContentLoaded',function(){
const picker=document.getElementById('color_picker');
const hex=document.getElementById('color_hex');
const preset=document.getElementById('color_idx');
const standbyMode=document.getElementById('standby_mode');
if(standbyMode){
standbyMode.addEventListener('change',updateStandbyBrightnessVisibility);
updateStandbyBrightnessVisibility();
}
if(picker&&hex){
picker.addEventListener('input',function(){
hex.value=picker.value.toUpperCase();
preset.value='';
updatePresetSelection(picker.value,-1);
});
hex.addEventListener('input',function(){
const normalized=normalizeHexColor(hex.value);
preset.value='';
if(normalized){picker.value=normalized;updatePresetSelection(normalized,-1);}
});
}
const params=new URLSearchParams(window.location.search);
const weather=document.getElementById('weather-section');
if(weather&&(window.location.pathname.indexOf('/weather')===0||params.get('section')==='weather'||window.location.hash==='#weather-section')){
setTimeout(function(){weather.scrollIntoView({block:'start'});},50);
}
});
</script>)js";

static const char ADMIN_FOOTER[] PROGMEM = R"html(
<div class="brand-footer"><strong>HOMEsmthng</strong><span>by modsmthng</span>
<a href="https://github.com/modsmthng/homesmthng" target="_blank" rel="noopener noreferrer">
<svg viewBox="0 0 496 512" aria-hidden="true" focusable="false"><path fill="currentColor" d="M165.9 397.4c0 2-2.3 3.6-5.2 3.6-3.3 .3-5.6-1.3-5.6-3.6 0-2 2.3-3.6 5.2-3.6 3-.3 5.6 1.3 5.6 3.6zm-31.1-4.5c-.7 2 1.3 4.3 4.3 4.9 2.6 1 5.6 0 6.2-2s-1.3-4.3-4.3-5.2c-2.6-.7-5.5 .3-6.2 2.3zm44.2-1.7c-2.9 .7-4.9 2.6-4.6 4.9 .3 2 2.9 3.3 5.9 2.6 2.9-.7 4.9-2.6 4.6-4.6-.3-1.9-3-3.2-5.9-2.9zM244.8 8C106.1 8 0 113.3 0 252c0 110.9 69.8 205.8 169.5 239.2 12.8 2.3 17.3-5.6 17.3-12.1 0-6.2-.3-40.4-.3-61.4 0 0-70 15-84.7-29.8 0 0-11.4-29.1-27.8-36.6 0 0-22.9-15.7 1.6-15.4 0 0 24.9 2 38.6 25.8 21.9 38.6 58.6 27.5 72.9 20.9 2.3-16 8.8-27.1 16-33.7-55.9-6.2-112.3-14.3-112.3-110.5 0-27.5 7.6-41.3 23.6-58.9-2.6-6.5-11.1-33.3 2.6-67.9 20.9-6.5 69 27 69 27 20-5.6 41.5-8.5 62.8-8.5s42.8 2.9 62.8 8.5c0 0 48.1-33.6 69-27 13.7 34.6 5.2 61.4 2.6 67.9 16 17.7 25.8 31.5 25.8 58.9 0 96.5-58.9 104.2-114.8 110.5 9.2 7.9 17 22.9 17 46.4 0 33.7-.3 75.4-.3 83.6 0 6.5 4.6 14.7 17.3 12.1C428.2 457.8 496 362.9 496 252 496 113.3 383.5 8 244.8 8z"/></svg>
GitHub</a></div></div></body></html>)html";

// ---------------------------------------------------------------------------
// Provisioning page templates
// ---------------------------------------------------------------------------
static const char PROVISIONING_HEAD_OPEN[] PROGMEM = R"html(
<!doctype html><html><head><meta charset="utf-8">
<link rel="icon" href="data:,">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>HOMEsmthng Setup</title>
<style>)html";

static const char PROVISIONING_HEAD_CLOSE[] PROGMEM = R"html(
</style></head><body><div class="wrap">
<div class="card"><h1>HOMEsmthng Setup</h1>
<p>Connect this device to your home Wi-Fi. After saving the network, the device restarts and the admin UI moves to the local network.</p>
<p><strong>Setup Wi-Fi</strong><br><span class="mono">)html";

static const char PROVISIONING_AP_END[] PROGMEM = R"html(
</span><br><strong>Portal</strong><br><span class="mono">http://192.168.4.1</span></p></div>)html";

static const char PROVISIONING_FORM_HEAD[] PROGMEM = R"html(
<div id="wifi-setup" class="card"><h2>Provision Device</h2>
<form method="POST" action="/wifi/save">
<label for="device_label">Device label</label>
<input id="device_label" name="device_label" value=")html";

static const char PROVISIONING_FORM_MID[] PROGMEM = R"html(
" maxlength="48">
<label for="device_host">Hostname</label>
<input id="device_host" name="device_host" value=")html";

static const char PROVISIONING_FORM_TAIL[] PROGMEM = R"html(
" maxlength="48" spellcheck="false">
<p class="subtle">Each device should have its own hostname, for example <span class="mono">homesmthng-abcd</span>.</p>
<label for="ssid">Wi-Fi network</label>
<input id="ssid" name="ssid" list="ssid-list" placeholder="Choose or type a Wi-Fi network" maxlength="32" required spellcheck="false" autocapitalize="none" autocomplete="username">
<datalist id="ssid-list"></datalist>
<details id="scan-panel" class="scan-panel"><summary id="scan-summary">Scan networks</summary><div id="scan-results" class="scan-results"></div></details>
<label for="pwd">Wi-Fi password</label>
<input id="pwd" name="pwd" type="password" maxlength="64" placeholder="Leave empty only for open networks" autocomplete="current-password" autocapitalize="none" spellcheck="false">
<div class="actions">
<button class="primary" type="submit">Save Wi-Fi and reboot</button>
</div></form>
</div>)html";

static const char PROVISIONING_SCRIPT[] PROGMEM = R"js(
<script>
let scanRunning=false;
async function refreshNetworks(){
if(scanRunning){return;}
scanRunning=true;
const list=document.getElementById('ssid-list');
const panel=document.getElementById('scan-panel');
const summary=document.getElementById('scan-summary');
const results=document.getElementById('scan-results');
summary.textContent='Scanning networks...';
list.innerHTML='';results.innerHTML='<div class="scan-item"><span>Scanning Wi-Fi networks...</span></div>';
try{const response=await fetch('/wifi/scan',{cache:'no-store'});
const items=await response.json();
results.innerHTML='';
summary.textContent=items.length?('Scanned networks ('+items.length+')'):'Scanned networks';
if(!items.length){results.innerHTML='<div class="scan-item"><span>No Wi-Fi networks found.</span></div>';return;}
items.forEach(item=>{
const option=document.createElement('option');option.value=item.ssid;list.appendChild(option);
const row=document.createElement('button');row.type='button';row.className='scan-item';
row.innerHTML='<span>'+item.ssid+'</span><span>'+item.rssi+' dBm'+(item.secure?' · secured':' · open')+'</span>';
row.onclick=()=>{document.getElementById('ssid').value=item.ssid;panel.open=false;summary.textContent='Selected: '+item.ssid;document.getElementById('pwd').focus();};
results.appendChild(row);
});
}catch(err){summary.textContent='Scan networks';results.innerHTML='<div class="scan-item"><span>Wi-Fi scan failed. Try again.</span></div>';}
finally{scanRunning=false;}}
document.addEventListener('DOMContentLoaded',()=>{const target=document.getElementById('wifi-setup');if(target){setTimeout(()=>target.scrollIntoView({block:'start'}),150);}});
document.addEventListener('DOMContentLoaded',()=>{const panel=document.getElementById('scan-panel');if(panel){panel.addEventListener('toggle',()=>{if(panel.open){refreshNetworks();}});}});
</script></div></body></html>)js";
