// ** display_setup.cpp **

#include "display_setup.h" 

bool setupDisplayAndLVGL(LilyGo_RGBPanel &p) {
    // 1. T-RGB Display initialisieren (Typ bitte bei Bedarf anpassen)
    bool rslt = p.begin(LILYGO_T_RGB_2_1_INCHES_FULL_CIRCLE); 

    if (!rslt) {
        return false; 
    }
    
    // 2. LVGL initialisieren
    beginLvglHelper(p);
    
    // LVGL zum Zeichnen zwingen (Hilfreich bei der Initialisierung)
    lv_disp_trig_activity(lv_disp_get_default());
    
    // Hintergrundfarbe des Bildschirms setzen
    lv_obj_set_style_bg_color(lv_scr_act(), lv_color_hex(0x000000), LV_PART_MAIN);

    return true;
}