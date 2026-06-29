#pragma once

#include <Arduino.h>

#include "app_state.h"

// Render functions
String renderAdminPage(
    const String &message = "",
    bool is_error = false,
    const String &search_query = "",
    const WeatherSearchResult *search_results = nullptr,
    int search_result_count = 0,
    const String &search_message = "",
    bool search_is_error = false,
    const String &active_section = "");
String renderProvisioningPage(const String &message = "", bool is_error = false);
String renderWebHomePage();
String renderQrSvg(const String &payload);

// Web handlers
void handleWebHome();
void handleWeatherConfigRoot();
void handleWeatherConfigSearch();
void handleWeatherConfigSave();
void handleWeatherConfigReset();
void redirectToWeatherSection();
void handleDisplaySettingsSave();
void handleStandbySettingsSave();
void handleTimezoneSave();
void handleDeviceConfigSave();
void handleWifiConfigSave();
void handleWifiReset();
void handleDeviceReboot();
void handleHomeKitQrSvg();
void handleProvisioningRoot();
void handleProvisioningWifiScan();
void handleProvisioningWifiSave();
void handleProvisioningRedirect();

// Server lifecycle
void startWeatherConfigServer();
void ensureWeatherConfigServer();
void startProvisioningServer();
void startProvisioningMode();

// URL / misc helpers
String adminMdnsUrl();
String adminDirectUrl();
String setupPortalUrl();
String setupAccessPointSsid();
String defaultAdminSection(const String &requested_section);
String formattedPairingCode();
String homeKitSetupPayload();
String weatherConfigStationUrl();
String weatherConfigApUrl();
bool isPairingOnboardingActive();
bool isSetupAccessPointVisible();
void logWeatherConfigUrls();
