#pragma once

// Persistent settings and identity storage.

#include <Arduino.h>

#include "app_state.h"

String defaultDeviceHostName();
String defaultDeviceLabel();
String sanitizeHostName(const String &text);

bool readStoredWifiCredentials(StoredWifiCredentials &credentials);
bool saveStoredWifiCredentials(const StoredWifiCredentials &credentials);
void eraseStoredWifiCredentials();

void loadOrCreateDeviceConfig();
void saveDeviceConfigToNVS();
void saveSettingsToNVS();
void loadSettingsFromNVS();
void loadOrCreateBridgeSuffix();
void loadOrCreatePairingCode();
