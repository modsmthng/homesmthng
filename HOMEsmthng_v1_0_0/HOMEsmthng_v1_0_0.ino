/**
 * @file      HOMEsmthng.ino
 * @author    modsmthng
 * @date      10.12.2025 V1.0.1 (UI Cleanup)
**/

#include <Arduino.h>
#include "HomeSpan.h"       
#include <LilyGo_RGBPanel.h>
#include <LV_Helper.h>       
#include "display_setup.h"  
#include "WiFi.h"
#include <math.h> 
#include <Preferences.h> 

// Globale Instanz des Panels
LilyGo_RGBPanel panel;
Preferences preferences; 

// *** VORWÄRTSDEKLARATIONEN VON FUNKTIONEN ***
void updateLVGLState(uint8_t id, bool state);
void updateWiFiSymbol(); 
void safeSetBrightness(int target_brightness); 


// *** 1. GLOBALE KONSTANTEN & VARIABLEN ***
// *****************************************

const int NUM_SWITCHES = 5; 
const char* switch_names[] = {"S1", "S2", "S3", "S4", "B1"}; 

// Variable für den persistenten HomeKit-Code (8-stellig).
String HOMEKIT_PAIRING_CODE_STR = "22446688"; 
const char* HOMEKIT_PAIRING_CODE; 

// Globaler String für den zufälligen 4-stelligen Hex-Suffix des Bridge-Namens
String BRIDGE_SUFFIX_STR; 

// Globale UI & Logik Variablen
lv_obj_t* tileview;
lv_obj_t* ui_switch_buttons[4]; 
lv_obj_t* ui_big_button;      
lv_obj_t* ui_wifi_label;
lv_obj_t* ui_brightness_slider;
lv_obj_t* ui_bl_toggle = NULL; 
lv_obj_t* ui_wifi_setup_msg; // Label für die Setup-Meldung

// Globale Pointer für die Tiles
lv_obj_t * tile1;
lv_obj_t * tile2;
lv_obj_t * tile3;
lv_obj_t * tile4;

// Variablen, die in der gesamten Anwendung verwendet werden:
int global_brightness = 12; 
int current_display_brightness = 0; 
lv_timer_t * screen2_timer = NULL; 
bool screen2_bl_always_on = true; 

// Dynamische UI-Anpassung
uint32_t active_switch_color_hex = 0x68724D; // Standardwert
lv_color_t active_switch_color = lv_color_hex(active_switch_color_hex); 

// Farbschema (HEX-Werte)
const uint32_t color_hex_options[] = {
    0xFF0000, 0xFF9900, 0xCCCCCC, 0x0000FF, 0x68724D, 0xF99963 // << GEÄNDERT: 0xFFFFFF zu 0xCCCCCC
};
const int NUM_COLORS = 6;


// VORWÄRTSDEKLARATIONEN FÜR DIE KLASSE
class MySwitch;
MySwitch* homekit_switches[NUM_SWITCHES]; 


// =========================================================================
// 2. HOMESPAN SERVICE 
// =========================================================================

class MySwitch : public Service::Switch {
  public:
    uint8_t switchId; 
    Characteristic::On *on;

    MySwitch(uint8_t id) : Service::Switch() {
      switchId = id; 
      on = new Characteristic::On(false);
    }

    bool update() { 
      bool newState = on->getNewVal();
      updateLVGLState(switchId, newState);
      
      // HomeSpan speichert den Zustand von Characteristic::On automatisch ab.
      
      return(true);
    }
};


// =========================================================================
// 3. HILFSFUNKTIONEN 
// =========================================================================

/**
 * @brief Setzt die Helligkeit nur, wenn sie sich vom aktuellen Wert unterscheidet.
 */
void safeSetBrightness(int target_brightness) {
    if (target_brightness < 1) target_brightness = 0;
    if (target_brightness > 16) target_brightness = 16;
    
    if (target_brightness != current_display_brightness) {
        panel.setBrightness(target_brightness);
        current_display_brightness = target_brightness;
    }
}


/**
 * @brief Schaltet die Hintergrundbeleuchtung aus, wenn der Timer abläuft 
 * und Screen 2 noch aktiv ist.
 */
void screen2_off_timer_cb(lv_timer_t * timer) { 
    lv_obj_t * active_tile = lv_tileview_get_tile_act(tileview);
    if (!screen2_bl_always_on && active_tile == tile2) { 
        safeSetBrightness(0); 
    } 
    screen2_timer = NULL; 
}

/**
 * @brief Aktualisiert das WiFi-Symbol und die Setup-Meldung auf Tile 1.
 */
void updateWiFiSymbol() {
    wl_status_t wifiStatus = WiFi.status();
    
    // Flag, um zu prüfen, ob der AP-Modus (Provisioning) aktiv ist
    bool isAPModeActive = (wifiStatus == WL_IDLE_STATUS || wifiStatus == WL_NO_SSID_AVAIL || wifiStatus == WL_NO_SHIELD);
    
    if (wifiStatus == WL_CONNECTED) {
        // Zustand: Verbunden und OK
        lv_label_set_text(ui_wifi_label, LV_SYMBOL_WIFI);
        lv_obj_set_style_text_color(ui_wifi_label, lv_palette_main(LV_PALETTE_GREEN), 0);
        
    } else if (isAPModeActive) {
        // Zustand: WiFi Manager / Provisioning wartet auf Konfiguration (AP aktiv)
        lv_label_set_text(ui_wifi_label, LV_SYMBOL_SETTINGS); // Zahnrad-Symbol
        lv_obj_set_style_text_color(ui_wifi_label, lv_palette_main(LV_PALETTE_ORANGE), 0);
        
    } else {
        // Zustand: Verbindungsversuch/Fehler
        lv_label_set_text(ui_wifi_label, LV_SYMBOL_WARNING);
        lv_obj_set_style_text_color(ui_wifi_label, lv_palette_main(LV_PALETTE_RED), 0);
    }
    
    // Logik zur Steuerung der Sichtbarkeit der Setup-Meldung auf Tile 1 (um Flackern zu vermeiden)
    static bool switches_hidden = false;
    lv_obj_t * current_tile = lv_tileview_get_tile_act(tileview);

    if (current_tile == tile1) {
        if (isAPModeActive && !switches_hidden) {
            // AP aktiv UND Switches sind noch sichtbar -> Verstecken und Meldung zeigen
            lv_obj_clear_flag(ui_wifi_setup_msg, LV_OBJ_FLAG_HIDDEN);
            for(int i = 0; i < 4; i++) {
               lv_obj_add_flag(ui_switch_buttons[i], LV_OBJ_FLAG_HIDDEN);
            }
            switches_hidden = true;

        } else if (wifiStatus == WL_CONNECTED && switches_hidden) {
            // Verbunden UND Switches sind versteckt -> Zeigen und Meldung verstecken
            lv_obj_add_flag(ui_wifi_setup_msg, LV_OBJ_FLAG_HIDDEN);
            for(int i = 0; i < 4; i++) {
               lv_obj_clear_flag(ui_switch_buttons[i], LV_OBJ_FLAG_HIDDEN);
            }
            switches_hidden = false;
        }
    }
}

void updateLVGLState(uint8_t id, bool state) {
    if (id < 4) { // S1 bis S4 (Index 0 bis 3)
        if (ui_switch_buttons[id]) { // Safety Check
            if (state) lv_obj_add_state(ui_switch_buttons[id], LV_STATE_CHECKED);
            else lv_obj_clear_state(ui_switch_buttons[id], LV_STATE_CHECKED);
        }
    } else if (id == 4) { // B1 (Index 4)
        if (ui_big_button) { // Safety Check
            if (state) lv_obj_add_state(ui_big_button, LV_STATE_CHECKED);
            else lv_obj_clear_state(ui_big_button, LV_STATE_CHECKED);
        }
    }
}

/**
 * @brief Aktualisiert die aktive Farbe für alle Schalter.
 */
void applyNewColor(lv_color_t new_color) {
    active_switch_color = new_color;
    for (int i = 0; i < 4; i++) {
        if (ui_switch_buttons[i]) {
            lv_obj_set_style_bg_color(ui_switch_buttons[i], active_switch_color, LV_STATE_CHECKED);
        }
    }
    // B1 Button ebenfalls aktualisieren
    if (ui_big_button) {
        lv_obj_set_style_bg_color(ui_big_button, active_switch_color, LV_STATE_CHECKED);
        lv_obj_set_style_bg_color(ui_big_button, active_switch_color, LV_STATE_CHECKED | LV_STATE_PRESSED); 
    }
}


// =========================================================================
// 4. LVGL EVENTS UND NVS-FUNKTIONEN
// =========================================================================

/**
 * @brief Speichert die aktuelle Helligkeit und Farbe im NVS.
 */
void saveSettingsToNVS() {
    preferences.begin("homespan", false);
    preferences.putInt("brightness", global_brightness);
    preferences.putUInt("color_hex", active_switch_color_hex); 
    preferences.end();
}

/**
 * @brief Lädt gespeicherte Helligkeit und Farbe aus dem NVS.
 */
void loadSettingsFromNVS() {
    preferences.begin("homespan", true); // read-only
    
    // Helligkeit laden (Default: 12)
    global_brightness = preferences.getInt("brightness", 12);
    
    // Farbe laden (Default: 0x68724D)
    active_switch_color_hex = preferences.getUInt("color_hex", 0x68724D); 
    preferences.end();
    
    // LVGL-Farbe aktualisieren
    active_switch_color = lv_color_hex(active_switch_color_hex);
}

void small_switch_event_handler(lv_event_t * e) {
    uint8_t id = *(uint8_t*)lv_obj_get_user_data(lv_event_get_target(e)); 
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
        bool newState = lv_obj_has_state(lv_event_get_target(e), LV_STATE_CHECKED);
        
        // HomeKit-Zustand ändern (speichert automatisch den Zustand)
        if (homekit_switches[id]) homekit_switches[id]->on->setVal(newState); 
    }
}

/**
 * @brief Behandelt Klicks auf den großen Button (Screen 2 / B1).
 */
void big_switch_event_handler(lv_event_t * e) {
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
        
        lv_obj_t *btn = lv_event_get_target(e);
        bool newState = lv_obj_has_state(btn, LV_STATE_CHECKED); 
        
        // 1. HOMEKIT: Status von B1 ändern (Index 4) (speichert automatisch den Zustand)
        if (homekit_switches[4]) {
            homekit_switches[4]->on->setVal(newState); 
        }
        
        // 2. DISPLAY: Hintergrundbeleuchtung einschalten (immer)
        safeSetBrightness(global_brightness); 
        
        // 3. TIMER: Nur starten/zurücksetzen, wenn BL-Modus nicht "immer an" ist
        if (!screen2_bl_always_on) { 
            if(screen2_timer) lv_timer_reset(screen2_timer);
            else screen2_timer = lv_timer_create(screen2_off_timer_cb, 5000, lv_obj_get_parent(btn)); 
        } else {
             if (screen2_timer) {
                 lv_timer_del(screen2_timer);
                 screen2_timer = NULL;
             }
        }
    }
}

void brightness_slider_event_cb(lv_event_t * e) {
    lv_obj_t * slider = lv_event_get_target(e);
    global_brightness = (int)lv_slider_get_value(slider);
    
    safeSetBrightness(global_brightness); 
    
    // Einstellungen speichern
    saveSettingsToNVS(); 
}

void color_button_event_handler(lv_event_t * e) {
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
        uint8_t id = *(uint8_t*)lv_obj_get_user_data(lv_event_get_target(e));
        uint32_t new_color_hex = color_hex_options[id]; 
        lv_color_t new_color = lv_color_hex(new_color_hex);
        
        applyNewColor(new_color);
        
        // Globale HEX-Variable aktualisieren und speichern
        active_switch_color_hex = new_color_hex;
        saveSettingsToNVS();
    }
}

/**
 * @brief Event-Handler für den BL-Toggle-Schalter
 */
void bl_toggle_event_handler(lv_event_t * e) {
    if (lv_event_get_code(e) == LV_EVENT_VALUE_CHANGED) {
        bool newState = lv_obj_has_state(lv_event_get_target(e), LV_STATE_CHECKED);
        screen2_bl_always_on = newState;
        
        if (newState && screen2_timer) {
             lv_timer_del(screen2_timer);
             screen2_timer = NULL;
        }
    }
}

/**
 * @brief Generiert einen 4-stelligen Hex-Suffix und speichert ihn im NVS.
 * @return Der generierte Code als String.
 */
String generateAndStoreBridgeSuffix() {
    long random_val = random(0, 65536); 
    char buffer[5];
    sprintf(buffer, "%04X", (unsigned int)random_val); 
    String suffix = buffer;

    preferences.begin("homespan", false);
    preferences.putString("br_suffix", suffix);
    preferences.end();
    
    return suffix; 
}

/**
 * @brief Lädt den gespeicherten Bridge Suffix oder generiert einen neuen.
 */
void loadOrCreateBridgeSuffix() {
    preferences.begin("homespan", false);
    
    String suffix_from_nvs = preferences.getString("br_suffix", "");

    if (suffix_from_nvs.length() == 4) {
        BRIDGE_SUFFIX_STR = suffix_from_nvs; 
    } else {
        BRIDGE_SUFFIX_STR = generateAndStoreBridgeSuffix();
    }
    
    preferences.end();
}

/**
 * @brief Generiert einen 8-stelligen Zufallscode und speichert ihn im NVS.
 * @return Der generierte Code als String.
 */
String generateAndStorePairingCode() {
    String code = "";
    for (int i = 0; i < 8; ++i) {
        code += String(random(0, 10)); 
    }

    preferences.begin("homespan", false);
    // ACHTUNG: Der Pairing Code selbst wird im NVS ohne Formatierung (XXXXYYYY) gespeichert, 
    // HomeSpan braucht den Code auch unformatiert!
    preferences.putString("hk_code", code);
    preferences.end();
    
    return code; 
}

/**
 * @brief Lädt den gespeicherten Pairing-Code oder generiert einen neuen.
 */
void loadOrCreatePairingCode() {
    preferences.begin("homespan", false);
    
    String code_from_nvs = preferences.getString("hk_code", "");

    if (code_from_nvs.length() == 8) {
        HOMEKIT_PAIRING_CODE_STR = code_from_nvs; 
    } else {
        HOMEKIT_PAIRING_CODE_STR = generateAndStorePairingCode();
    }
    
    preferences.end();
}


// =========================================================================
// 5. UI SETUP
// =========================================================================

void setupUI() {
    lv_obj_t * scr = lv_scr_act();
    lv_obj_set_size(scr, 480, 480); 

    lv_coord_t full_size = 480; 
    lv_coord_t center = 240; 

    tileview = lv_tileview_create(scr);
    lv_obj_set_size(tileview, full_size, full_size);
    lv_obj_set_style_bg_color(tileview, lv_color_black(), 0);

    // Tiles erstellen und globale Pointer zuweisen
    tile1 = lv_tileview_add_tile(tileview, 0, 0, LV_DIR_BOTTOM); 
    tile2 = lv_tileview_add_tile(tileview, 0, 1, LV_DIR_TOP | LV_DIR_BOTTOM);
    tile3 = lv_tileview_add_tile(tileview, 0, 2, LV_DIR_TOP | LV_DIR_BOTTOM);
    tile4 = lv_tileview_add_tile(tileview, 0, 3, LV_DIR_TOP); 


    // ---------------- SCREEN 1: CENTRAL SWITCHES ----------------
    
    static uint8_t ids[4]; 
    float angles[] = {270.0, 0.0, 90.0, 180.0}; 
    int R = 120; 
    const int CENTER_X = center; 
    const int CIRCLE_CENTER_Y = center; 
    int btn_size = 120; 
    
    for(int i=0; i<4; i++) { 
        ids[i] = i;
        float rad = PI * angles[i] / 180.0;
        int x = CENTER_X + (int)(R * cos(rad)) - (btn_size / 2); 
        int y = CIRCLE_CENTER_Y + (int)(R * sin(rad)) - (btn_size / 2); 
        
        lv_obj_t *btn = lv_btn_create(tile1);
        lv_obj_set_size(btn, btn_size, btn_size);
        lv_obj_set_style_radius(btn, LV_RADIUS_CIRCLE, 0); 
        lv_obj_set_pos(btn, x, y); 
        lv_obj_set_user_data(btn, &ids[i]);
        lv_obj_add_event_cb(btn, small_switch_event_handler, LV_EVENT_CLICKED, NULL);
        lv_obj_set_style_bg_color(btn, lv_color_make(50,50,50), 0);
        lv_obj_set_style_bg_color(btn, active_switch_color, LV_STATE_CHECKED);
        lv_obj_add_flag(btn, LV_OBJ_FLAG_CHECKABLE);
        ui_switch_buttons[i] = btn;

        // >> LABEL FÜR SWITCHES ENTFERNT <<
    }
    
    // WiFi Setup Message Label
    ui_wifi_setup_msg = lv_label_create(tile1);
    lv_label_set_text(ui_wifi_setup_msg, "Please connect to WLAN HOMEsmthng. \n"
    "If you have already set up the WLAN,\n"
    "please wait a moment or check the reception.\n"
    "\n"
    "(Note: The flickering of the display\n"
    "will stop after connecting.)"); 
    lv_obj_set_width(ui_wifi_setup_msg, full_size);
    lv_obj_align(ui_wifi_setup_msg, LV_ALIGN_CENTER, 0, 0); 
    lv_obj_set_style_text_align(ui_wifi_setup_msg, LV_TEXT_ALIGN_CENTER, 0); 
    lv_obj_set_style_text_font(ui_wifi_setup_msg, &lv_font_montserrat_24, 0); 
    lv_obj_set_style_text_color(ui_wifi_setup_msg, lv_color_white(), 0); 
    lv_obj_add_flag(ui_wifi_setup_msg, LV_OBJ_FLAG_HIDDEN); // Standardmäßig versteckt

    // ---------------- SCREEN 2: MASTER BUTTON (B1) ----------------
    
    ui_big_button = lv_btn_create(tile2);
    lv_obj_set_size(ui_big_button, full_size, full_size); 
    lv_obj_center(ui_big_button);
    lv_obj_set_style_radius(ui_big_button, LV_RADIUS_CIRCLE, 0); 
    lv_obj_set_style_bg_color(ui_big_button, lv_color_black(), 0);
    lv_obj_set_style_bg_color(ui_big_button, active_switch_color, LV_STATE_CHECKED | LV_STATE_PRESSED); 
    lv_obj_set_style_bg_color(ui_big_button, active_switch_color, LV_STATE_CHECKED); 
    lv_obj_add_flag(ui_big_button, LV_OBJ_FLAG_CHECKABLE);
    lv_obj_add_event_cb(ui_big_button, big_switch_event_handler, LV_EVENT_CLICKED, NULL);

    // ---------------- SCREEN 3: SETTINGS ----------------
    
    lv_obj_t * lbl_set = lv_label_create(tile3);
    lv_label_set_text(lbl_set, "Settings"); 
    lv_obj_align(lbl_set, LV_ALIGN_TOP_MID, 0, 20); 
    lv_obj_set_style_text_font(lbl_set, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(lbl_set, lv_color_white(), 0);

    // *** 1. COLOR SELECTION ***
    lv_obj_t * lbl_color = lv_label_create(tile3);
    lv_label_set_text(lbl_color, "Switch ON Color"); 
    lv_obj_set_style_text_color(lbl_color, lv_color_white(), 0);
    lv_obj_align_to(lbl_color, lbl_set, LV_ALIGN_OUT_BOTTOM_MID, 0, 15);

    lv_obj_t * color_panel = lv_obj_create(tile3);
    lv_obj_set_size(color_panel, full_size - 40, 60);
    lv_obj_align_to(color_panel, lbl_color, LV_ALIGN_OUT_BOTTOM_MID, 0, 5);
    lv_obj_set_style_bg_color(color_panel, lv_color_black(), 0);
    lv_obj_set_style_border_width(color_panel, 0, 0);
    lv_obj_set_style_pad_all(color_panel, 0, 0);
    lv_obj_set_style_pad_gap(color_panel, 10, 0); 
    lv_obj_set_flex_flow(color_panel, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(color_panel, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    static uint8_t color_ids[NUM_COLORS];
    for (int i = 0; i < NUM_COLORS; i++) {
        color_ids[i] = i;
        lv_obj_t * color_btn = lv_btn_create(color_panel);
        lv_obj_set_size(color_btn, 40, 40); 
        lv_obj_set_style_bg_color(color_btn, lv_color_hex(color_hex_options[i]), 0);
        lv_obj_set_style_radius(color_btn, 5, 0);
        lv_obj_set_user_data(color_btn, &color_ids[i]);
        lv_obj_add_event_cb(color_btn, color_button_event_handler, LV_EVENT_CLICKED, NULL);
    }
    
    // *** 2. BRIGHTNESS ***
    lv_obj_t * lbl_bri_title = lv_label_create(tile3);
    lv_label_set_text(lbl_bri_title, "Display Brightness"); 
    lv_obj_set_style_text_color(lbl_bri_title, lv_color_white(), 0);
    lv_obj_align_to(lbl_bri_title, color_panel, LV_ALIGN_OUT_BOTTOM_MID, 0, 30); 

    ui_brightness_slider = lv_slider_create(tile3);
    lv_obj_set_width(ui_brightness_slider, 300); 
    lv_obj_align_to(ui_brightness_slider, lbl_bri_title, LV_ALIGN_OUT_BOTTOM_MID, 0, 5);
    lv_slider_set_range(ui_brightness_slider, 1, 16);
    // Slider-Wert auf geladenen Wert setzen
    lv_slider_set_value(ui_brightness_slider, global_brightness, LV_ANIM_OFF); 
    lv_obj_add_event_cb(ui_brightness_slider, brightness_slider_event_cb, LV_EVENT_VALUE_CHANGED, NULL);
    
    
    // *** 3. SCREEN 2 BACKLIGHT SETTING ***
    lv_obj_t * bl_cont = lv_obj_create(tile3);
    lv_obj_set_size(bl_cont, full_size - 40, 75); 
    lv_obj_align_to(bl_cont, ui_brightness_slider, LV_ALIGN_OUT_BOTTOM_MID, 0, 20); 
    lv_obj_set_style_bg_color(bl_cont, lv_color_black(), 0);
    lv_obj_set_style_border_width(bl_cont, 0, 0);
    lv_obj_set_style_pad_all(bl_cont, 0, 0);
    lv_obj_set_style_layout(bl_cont, LV_LAYOUT_FLEX, 0); 
    lv_obj_set_flex_flow(bl_cont, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(bl_cont, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lv_obj_t * bl_lbl = lv_label_create(bl_cont);
    lv_label_set_text(bl_lbl, 
        "Keep Screen 2 backlight ON\n" 
        "(Disable if you want to use the display\n"
        "as a switch without it lighting up)");
    lv_obj_set_style_text_color(bl_lbl, lv_color_white(), 0);
    lv_obj_set_style_text_font(bl_lbl, &lv_font_montserrat_16, 0); 

    ui_bl_toggle = lv_switch_create(bl_cont);
    if (screen2_bl_always_on) {
        lv_obj_add_state(ui_bl_toggle, LV_STATE_CHECKED);
    }
    lv_obj_add_event_cb(ui_bl_toggle, bl_toggle_event_handler, LV_EVENT_VALUE_CHANGED, NULL);
    
    
    // *** 4. HOMEsmthng TEXT & Swipe Hint ***
    
    lv_obj_t * lbl_home_status = lv_label_create(tile3);
    // Zeigt nur HOMEsmthng an (ohne Suffix)
    lv_label_set_text(lbl_home_status, "HOMEsmthng"); 
    lv_obj_set_style_text_font(lbl_home_status, &lv_font_montserrat_28, 0); 
    lv_obj_set_style_text_color(lbl_home_status, lv_color_white(), 0); 
    lv_obj_align_to(lbl_home_status, bl_cont, LV_ALIGN_OUT_BOTTOM_MID, 0, 20); 
    
    lv_obj_t * lbl_subtitle = lv_label_create(tile3);
    lv_label_set_text(lbl_subtitle, "developed with HomeSpan");
    lv_obj_set_style_text_font(lbl_subtitle, &lv_font_montserrat_16, 0); 
    lv_obj_set_style_text_color(lbl_subtitle, lv_color_make(180, 180, 180), 0); 
    lv_obj_align_to(lbl_subtitle, lbl_home_status, LV_ALIGN_OUT_BOTTOM_MID, 0, 5);
    
    lv_obj_t * lbl_swipe_hint_down = lv_label_create(tile3);
    lv_label_set_text(lbl_swipe_hint_down, LV_SYMBOL_DOWN " Pairing Code"); 
    lv_obj_align(lbl_swipe_hint_down, LV_ALIGN_BOTTOM_MID, 0, -10); 
    lv_obj_set_style_text_font(lbl_swipe_hint_down, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(lbl_swipe_hint_down, lv_color_make(180, 180, 180), 0);


    // ---------------- SCREEN 4: PAIRING CODE ----------------
    
    // Formatierung in das Standard-HomeKit-Format XXXX-XXXX
    String formatted_code = HOMEKIT_PAIRING_CODE_STR.substring(0, 4) + "-" + 
                            HOMEKIT_PAIRING_CODE_STR.substring(4, 8);
    
    // Titel
    lv_obj_t * lbl_pairing_title = lv_label_create(tile4);
    lv_label_set_text(lbl_pairing_title, "HomeKit Pairing Code"); 
    lv_obj_set_width(lbl_pairing_title, full_size); 
    lv_obj_align(lbl_pairing_title, LV_ALIGN_TOP_MID, 0, 80); 
    lv_obj_set_style_text_align(lbl_pairing_title, LV_TEXT_ALIGN_CENTER, 0); 
    lv_obj_set_style_text_font(lbl_pairing_title, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(lbl_pairing_title, lv_color_white(), 0);

    // Pairing Code (weiß, zentriert)
    lv_obj_t * lbl_pairing_code = lv_label_create(tile4);
    lv_label_set_text(lbl_pairing_code, formatted_code.c_str()); 
    lv_obj_set_width(lbl_pairing_code, full_size); 
    lv_obj_align_to(lbl_pairing_code, lbl_pairing_title, LV_ALIGN_OUT_BOTTOM_MID, 0, 20); 
    lv_obj_set_style_text_align(lbl_pairing_code, LV_TEXT_ALIGN_CENTER, 0); 
    lv_obj_set_style_text_font(lbl_pairing_code, &lv_font_montserrat_48, 0); 
    lv_obj_set_style_text_color(lbl_pairing_code, lv_color_white(), 0); 

    // Fallback-Hinweis
    lv_obj_t * lbl_fallback = lv_label_create(tile4);
    lv_label_set_text(lbl_fallback, "If pairing code doesn't work,\nplease try: 2244-6688, 4663-7726 or 1234-5678\n"
    "(Try to be as close to your router as possible,\n"
    "as this speeds up the process considerably and\n"
    "also prevents the pairing process from failing)"); 
    lv_obj_set_width(lbl_fallback, full_size); 
    lv_obj_align_to(lbl_fallback, lbl_pairing_code, LV_ALIGN_OUT_BOTTOM_MID, 0, 15);
    lv_obj_set_style_text_align(lbl_fallback, LV_TEXT_ALIGN_CENTER, 0); 
    lv_obj_set_style_text_font(lbl_fallback, &lv_font_montserrat_16, 0); 
    lv_obj_set_style_text_color(lbl_fallback, lv_color_make(180, 180, 180), 0); 
    
    // "Zurück"-Hinweis
    lv_obj_t * lbl_swipe_hint_up = lv_label_create(tile4);
    lv_label_set_text(lbl_swipe_hint_up, LV_SYMBOL_UP " Back to Settings"); 
    lv_obj_align(lbl_swipe_hint_up, LV_ALIGN_BOTTOM_MID, 0, -80); 
    lv_obj_set_style_text_font(lbl_swipe_hint_up, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(lbl_swipe_hint_up, lv_color_make(180, 180, 180), 0);
    
    ui_wifi_label = lv_label_create(tile4);
    lv_label_set_text(ui_wifi_label, LV_SYMBOL_WIFI); 
    lv_obj_set_style_text_font(ui_wifi_label, &lv_font_montserrat_48, 0); 
    lv_obj_align_to(ui_wifi_label, lbl_swipe_hint_up, LV_ALIGN_OUT_BOTTOM_MID, 0, 10); 
    lv_obj_set_style_text_color(ui_wifi_label, lv_color_white(), 0);
}


// =========================================================================
// 6. SETUP & LOOP
// =========================================================================

void setup() {
  Serial.begin(115200);
  
  // 0. Lädt gespeicherte Helligkeit und Farbe
  loadSettingsFromNVS(); 

  // 1. Lädt oder erstellt den persistenten Pairing-Code
  loadOrCreatePairingCode(); 

  // 2. Lädt oder erstellt den persistenten Bridge Suffix
  loadOrCreateBridgeSuffix(); 

  // 3. Setzt HOMEKIT_PAIRING_CODE
  HOMEKIT_PAIRING_CODE = HOMEKIT_PAIRING_CODE_STR.c_str();
  homeSpan.setPairingCode(HOMEKIT_PAIRING_CODE); 

  // Setup Display
  if (!setupDisplayAndLVGL(panel)) {
    while (1) delay(1000);
  }

  // Die UI muss NACH dem Laden der Einstellungen aufgerufen werden
  setupUI();

  // 4. Konstruiert den finalen Bridge-Namen
  String final_bridge_name = "HOMEsmthng " + BRIDGE_SUFFIX_STR;
  homeSpan.begin(Category::Bridges, final_bridge_name.c_str()); 

  // --- WiFi Provisioning mit HomeSpan 2.x ---
  
  // 1. Legt den Namen des Konfigurations-WLAN fest.
  homeSpan.setApSSID("HOMEsmthng"); 

  // 2. Legt KEIN Passwort für den Setup Access Point fest (Offenes WLAN)
  homeSpan.setApPassword(""); 

  // 3. Aktiviert das automatische Starten des APs, wenn keine WLAN-Daten gefunden werden.
  homeSpan.enableAutoStartAP(); 
  
  Serial.println("Starting HomeSpan Provisioning...");

  // HomeKit Accessories
  new SpanAccessory();                            
    new Service::AccessoryInformation();
      new Characteristic::Identify();  
      new Characteristic::Manufacturer("LilyGo");
      new Characteristic::Model("T-RGB MultiPage 480");
      new Characteristic::Name("T-RGB Bridge");

  for(int i = 0; i < NUM_SWITCHES; ++i) { 
      new SpanAccessory(); 
        new Service::AccessoryInformation();
          new Characteristic::Identify();
          new Characteristic::Name(switch_names[i]);     
      homekit_switches[i] = new MySwitch(i); 
  }
  
  // Initialisiert den UI-Zustand basierend auf dem geladenen HomeSpan-Zustand (alle 5 Schalter)
  for(int i = 0; i < NUM_SWITCHES; ++i) { 
      if (homekit_switches[i]) {
          updateLVGLState(i, homekit_switches[i]->on->getVal());
      }
  }

  // Initialer Check, um die Setup-Meldung korrekt zu setzen
  // Der Aufruf von updateWiFiSymbol() in loop() übernimmt dann die Logik.
  delay(100); 
  updateWiFiSymbol(); 

  // Setzt die Helligkeit auf den geladenen NVS-Wert
  safeSetBrightness(global_brightness);
}


void loop() {
  lv_timer_handler();
  homeSpan.poll();
  
  // Regelmäßige UI Updates (WiFi-Symbol und damit auch die Setup-Meldung auf Tile 1)
  static unsigned long lastUpdate = 0;
  if (millis() - lastUpdate > 1000) {
      lastUpdate = millis();
      updateWiFiSymbol();
  }
  
  // Helligkeits-Steuerung: Reagiert auf Tile-Wechsel und Timer-Ablauf
  lv_obj_t * act = lv_tileview_get_tile_act(tileview);
  
  int target_brightness_level = global_brightness; 

  if (tile2 == act) { 
      // Wir sind auf Screen 2 (Master Switch)
      if (!screen2_bl_always_on && screen2_timer == NULL) { 
          // Timer ist abgelaufen und BL soll nicht immer an sein
          target_brightness_level = 0;
      }
      // Ansonsten (Timer läuft oder BL immer an): target_brightness_level bleibt global_brightness
      
  } 
  // Ansonsten (Tile 1, 3 oder 4): target_brightness_level bleibt global_brightness

  safeSetBrightness(target_brightness_level);
  
  delay(5); 
}