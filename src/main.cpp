#include "lvgl.h"
#include <Arduino.h>
#include <math.h>

#define AS5047P_CS       9
#define AS5047P_MOSI     7
#define AS5047P_MISO     8
#define AS5047P_SCK      6
#define AS5047P_NOP      0x0000
#define AS5047P_ANGLECOM 0x3FFE

static uint16_t softwareSPI_transfer16(uint16_t value) {
    uint16_t out = 0;
    for (int i = 15; i >= 0; i--) {
        digitalWrite(AS5047P_MOSI, (value & (1 << i)) ? HIGH : LOW);
        delayMicroseconds(5);
        digitalWrite(AS5047P_SCK, HIGH);
        delayMicroseconds(5);
        digitalWrite(AS5047P_SCK, LOW);
        delayMicroseconds(5);
        if (digitalRead(AS5047P_MISO)) out |= (1 << i);
    }
    return out;
}

static uint16_t readAS5047P() {
    uint16_t command = 0x4000 | AS5047P_ANGLECOM;
    digitalWrite(AS5047P_CS, LOW);
    delayMicroseconds(5);
    softwareSPI_transfer16(command);
    digitalWrite(AS5047P_CS, HIGH);
    delayMicroseconds(10);
    digitalWrite(AS5047P_CS, LOW);
    delayMicroseconds(5);
    uint16_t response = softwareSPI_transfer16(AS5047P_NOP);
    digitalWrite(AS5047P_CS, HIGH);
    return (response & 0x3FFF);
}

static lv_obj_t *screen_home       = NULL;
static lv_obj_t *screen_game       = NULL;
static lv_obj_t *screen_victory    = NULL;
static lv_obj_t *arc_cadran        = NULL;
static lv_obj_t *needle_line       = NULL;
static lv_obj_t *label_statut      = NULL;
static lv_obj_t *label_angle       = NULL;
static lv_obj_t *fond_jeu          = NULL;
static lv_obj_t *bar_prox          = NULL;
static lv_obj_t *bar_hold          = NULL;
static lv_obj_t *led_open          = NULL;
static lv_obj_t *label_step        = NULL;
static lv_obj_t *dot_widgets[3]    = {NULL, NULL, NULL};
static lv_obj_t *combo_cells[3]    = {NULL, NULL, NULL};
static lv_obj_t *v_combo_labels[3] = {NULL, NULL, NULL};
static lv_obj_t *screen_boot  = NULL;
static lv_obj_t *boot_label   = NULL;
static int32_t   boot_step    = 0;
static lv_obj_t *v_title_lbl   = NULL;
static lv_obj_t *v_sub_lbl     = NULL;
static lv_obj_t *v_cell_bg[3]  = {NULL, NULL, NULL};
static lv_obj_t *v_btn_play    = NULL;
static lv_obj_t *v_btn_home    = NULL;
static lv_obj_t *v_flash_obj   = NULL;
static lv_point_precise_t needle_pts[2] = {{115, 115}, {115, 60}};

static int32_t  secret_angles[3]  = {90, 180, 270};
static int32_t  current_step      = 0;
static uint32_t hold_timer        = 0;
static bool     is_unlocked       = false;
static const int32_t TOLERANCE_DEG = 10;
static const int32_t CLOSE_DEG     = 30;

void createHomeScreen();
void createGameScreen();
void createVictoryScreen();
void updateGame(int32_t val);
void generateNewGame();
void playVictoryAnimation();

static void btn_start_cb(lv_event_t *e) {
    lv_screen_load_anim(screen_game, LV_SCR_LOAD_ANIM_FADE_IN, 500, 0, false);
}
static void btn_accueil_cb(lv_event_t *e) {
    lv_screen_load_anim(screen_home, LV_SCR_LOAD_ANIM_FADE_IN, 400, 0, false);
}
static void btn_rejouer_cb(lv_event_t *e) {
    generateNewGame();
    lv_screen_load_anim(screen_game, LV_SCR_LOAD_ANIM_FADE_IN, 400, 0, false);
}

void generateNewGame() {
    secret_angles[0] = random(0, 360);
    secret_angles[1] = random(0, 360);
    secret_angles[2] = random(0, 360);
    current_step = 0;
    hold_timer   = 0;
    is_unlocked  = false;
    if (!arc_cadran) return;
    lv_arc_set_value(arc_cadran, 0);
    lv_obj_set_style_arc_color(arc_cadran, lv_color_hex(0x3a3a3a), LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(fond_jeu, lv_color_hex(0x0e0e0e), 0);
    lv_bar_set_value(bar_prox, 5, LV_ANIM_OFF);
    lv_bar_set_value(bar_hold, 0, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(bar_prox, lv_color_hex(0x2a2a2a), LV_PART_INDICATOR);
    lv_led_off(led_open);
    lv_label_set_text(label_statut, "VERROUILLE");
    lv_obj_set_style_text_color(label_statut, lv_color_hex(0xFFFFFF), 0);
    lv_label_set_text(label_step, "ANGLE 1/3");
    for (int i = 0; i < 3; i++) {
        lv_label_set_text(combo_cells[i], "???");
        lv_obj_set_style_text_color(combo_cells[i], lv_color_hex(0xFFFFFF), 0);
        lv_obj_set_style_bg_color(dot_widgets[i], lv_color_hex(0x161616), 0);
        lv_obj_set_style_border_color(dot_widgets[i], lv_color_hex(0x2a2a2a), 0);
    }
    lv_obj_set_style_bg_color(dot_widgets[0], lv_color_hex(0x8B6914), 0);
    lv_obj_set_style_border_color(dot_widgets[0], lv_color_hex(0xC8860A), 0);
    if (v_title_lbl)  lv_obj_set_style_opa(v_title_lbl, 0, 0);
    if (v_sub_lbl)    lv_obj_set_style_opa(v_sub_lbl, 0, 0);
    if (v_btn_play)   lv_obj_set_style_opa(v_btn_play, 0, 0);
    if (v_btn_home)   lv_obj_set_style_opa(v_btn_home, 0, 0);
    if (v_flash_obj)  lv_obj_set_style_opa(v_flash_obj, 0, 0);
    for (int i = 0; i < 3; i++)
        if (v_cell_bg[i]) lv_obj_set_style_opa(v_cell_bg[i], 0, 0);
}

void updateGame(int32_t val) {
    lv_arc_set_value(arc_cadran, val);
    float rad = val * (float)M_PI / 180.0f;
    needle_pts[1].x = (lv_value_precise_t)(115.0f + 52.0f * sinf(rad));
    needle_pts[1].y = (lv_value_precise_t)(115.0f - 52.0f * cosf(rad));
    lv_line_set_points(needle_line, needle_pts, 2);
    char buf[8];
    lv_snprintf(buf, sizeof(buf), "%d\xc2\xb0", (int)val);
    lv_label_set_text(label_angle, buf);
    if (is_unlocked) return;
    int32_t target   = secret_angles[current_step];
    int32_t distance = val - target;
    if (distance < 0)   distance = -distance;
    if (distance > 180) distance = 360 - distance;
    if (distance < TOLERANCE_DEG) {
        lv_obj_set_style_arc_color(arc_cadran, lv_color_hex(0x2ECC71), LV_PART_INDICATOR);
        lv_obj_set_style_bg_color(fond_jeu, lv_color_hex(0x051208), 0);
        lv_label_set_text(label_statut, "MAINTENEZ !");
        lv_obj_set_style_text_color(label_statut, lv_color_hex(0x2ECC71), 0);
        lv_obj_set_style_bg_color(bar_prox, lv_color_hex(0x2ECC71), LV_PART_INDICATOR);
        lv_bar_set_value(bar_prox, 100, LV_ANIM_OFF);
        lv_led_on(led_open);
        hold_timer++;
        lv_bar_set_value(bar_hold, (int32_t)(hold_timer * 2), LV_ANIM_OFF);
        if (hold_timer > 50) {
            lv_label_set_text_fmt(combo_cells[current_step], "%d\xc2\xb0", (int)target);
            lv_obj_set_style_text_color(combo_cells[current_step], lv_color_hex(0x2ECC71), 0);
            lv_obj_set_style_bg_color(dot_widgets[current_step], lv_color_hex(0x2ECC71), 0);
            lv_obj_set_style_border_color(dot_widgets[current_step], lv_color_hex(0x2ECC71), 0);
            hold_timer = 0;
            lv_bar_set_value(bar_hold, 0, LV_ANIM_OFF);
            current_step++;
            if (current_step >= 3) {
                is_unlocked = true;
                for (int i = 0; i < 3; i++)
                    lv_label_set_text_fmt(v_combo_labels[i], "%d\xc2\xb0", (int)secret_angles[i]);
                lv_screen_load(screen_victory);
                playVictoryAnimation();
            } else {
                char sbuf[12];
                lv_snprintf(sbuf, sizeof(sbuf), "ANGLE %d/3", (int)(current_step + 1));
                lv_label_set_text(label_step, sbuf);
                lv_obj_set_style_bg_color(dot_widgets[current_step], lv_color_hex(0x8B6914), 0);
                lv_obj_set_style_border_color(dot_widgets[current_step], lv_color_hex(0xC8860A), 0);
                lv_led_off(led_open);
            }
        }
    } else if (distance < CLOSE_DEG) {
        lv_obj_set_style_arc_color(arc_cadran, lv_color_hex(0xE67E22), LV_PART_INDICATOR);
        lv_obj_set_style_bg_color(fond_jeu, lv_color_hex(0x100800), 0);
        lv_label_set_text(label_statut, "APPROCHE...");
        lv_obj_set_style_text_color(label_statut, lv_color_hex(0xE67E22), 0);
        int32_t prox = 100 - (distance * 100 / CLOSE_DEG);
        lv_obj_set_style_bg_color(bar_prox, lv_color_hex(0xE67E22), LV_PART_INDICATOR);
        lv_bar_set_value(bar_prox, prox, LV_ANIM_OFF);
        lv_bar_set_value(bar_hold, 0, LV_ANIM_OFF);
        lv_led_off(led_open);
        hold_timer = 0;
    } else {
        lv_obj_set_style_arc_color(arc_cadran, lv_color_hex(0x3a3a3a), LV_PART_INDICATOR);
        lv_obj_set_style_bg_color(fond_jeu, lv_color_hex(0x0e0e0e), 0);
        lv_label_set_text(label_statut, "VERROUILLE");
        lv_obj_set_style_text_color(label_statut, lv_color_hex(0x888888), 0);
        lv_obj_set_style_bg_color(bar_prox, lv_color_hex(0x2a2a2a), LV_PART_INDICATOR);
        lv_bar_set_value(bar_prox, 5, LV_ANIM_OFF);
        lv_bar_set_value(bar_hold, 0, LV_ANIM_OFF);
        lv_led_off(led_open);
        hold_timer = 0;
    }
}

static void home_arc_pulse_cb(void *var, int32_t v) {
    lv_arc_set_value((lv_obj_t *)var, v);
}

static const char *boot_lines[] = {
    "SAFE CRACKER v1.0\n",
    "SAFE CRACKER v1.0\n> INIT SYSTEME.............. OK\n",
    "SAFE CRACKER v1.0\n> INIT SYSTEME.............. OK\n> CAPTEUR AS5047D........... OK\n",
    "SAFE CRACKER v1.0\n> INIT SYSTEME.............. OK\n> CAPTEUR AS5047D........... OK\n> GENERATION COMBINAISON.... OK\n",
    "SAFE CRACKER v1.0\n> INIT SYSTEME.............. OK\n> CAPTEUR AS5047D........... OK\n> GENERATION COMBINAISON.... OK\n> INTERFACE LVGL............. OK\n",
    "SAFE CRACKER v1.0\n> INIT SYSTEME.............. OK\n> CAPTEUR AS5047D........... OK\n> GENERATION COMBINAISON.... OK\n> INTERFACE LVGL............. OK\n\n>>> SYSTEME PRET"
};

static void boot_timer_cb(lv_timer_t *t) {
    boot_step++;
    if (boot_step < 6) {
        lv_label_set_text(boot_label, boot_lines[boot_step]);
    } else {
        lv_timer_delete(t);
        lv_screen_load_anim(screen_home, LV_SCR_LOAD_ANIM_FADE_IN, 1000, 0, false);
    }
}

void createBootScreen() {
    screen_boot = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(screen_boot, lv_color_hex(0x000000), 0);
    lv_obj_set_style_pad_all(screen_boot, 0, 0);
    lv_obj_set_style_border_width(screen_boot, 0, 0);

    boot_label = lv_label_create(screen_boot);
    lv_label_set_text(boot_label, boot_lines[0]);
    lv_label_set_long_mode(boot_label, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(boot_label, 440);
    lv_obj_set_style_text_font(boot_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(boot_label, lv_color_hex(0x00FF41), 0);
    lv_obj_align(boot_label, LV_ALIGN_TOP_LEFT, 20, 20);
}

void createHomeScreen() {
    screen_home = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(screen_home, lv_color_hex(0x0e0e0e), 0);
    lv_obj_set_style_pad_all(screen_home, 0, 0);
    lv_obj_set_style_border_width(screen_home, 0, 0);
    lv_obj_t *top_bar = lv_obj_create(screen_home);
    lv_obj_set_size(top_bar, 480, 24);
    lv_obj_set_pos(top_bar, 0, 0);
    lv_obj_set_style_bg_color(top_bar, lv_color_hex(0x161616), 0);
    lv_obj_set_style_radius(top_bar, 0, 0);
    lv_obj_set_style_border_side(top_bar, LV_BORDER_SIDE_BOTTOM, 0);
    lv_obj_set_style_border_color(top_bar, lv_color_hex(0x2a2a2a), 0);
    lv_obj_set_style_border_width(top_bar, 1, 0);
    lv_obj_set_style_pad_all(top_bar, 0, 0);
    lv_obj_t *lbl_sys = lv_label_create(top_bar);
    lv_label_set_text(lbl_sys, "PROJET INTERFACAGE \xC2\xB7 CIMSIT S6");
    lv_obj_set_style_text_font(lbl_sys, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(lbl_sys, lv_color_hex(0xFFFFFF), 0);
    lv_obj_align(lbl_sys, LV_ALIGN_LEFT_MID, 12, 0);
    lv_obj_t *lbl_state = lv_label_create(top_bar);
    lv_label_set_text(lbl_state, "\xE2\x97\x8F VERROUILLE");
    lv_obj_set_style_text_font(lbl_state, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(lbl_state, lv_color_hex(0x8B6914), 0);
    lv_obj_align(lbl_state, LV_ALIGN_RIGHT_MID, -12, 0);
    const int cx[4] = {8,458,8,458};
    const int cy[4] = {30,30,228,228};
    const lv_border_side_t cs[4] = {
        (lv_border_side_t)(LV_BORDER_SIDE_TOP|LV_BORDER_SIDE_LEFT),
        (lv_border_side_t)(LV_BORDER_SIDE_TOP|LV_BORDER_SIDE_RIGHT),
        (lv_border_side_t)(LV_BORDER_SIDE_BOTTOM|LV_BORDER_SIDE_LEFT),
        (lv_border_side_t)(LV_BORDER_SIDE_BOTTOM|LV_BORDER_SIDE_RIGHT)
    };
    for (int i = 0; i < 4; i++) {
        lv_obj_t *c = lv_obj_create(screen_home);
        lv_obj_set_size(c, 14, 14);
        lv_obj_set_pos(c, cx[i], cy[i]);
        lv_obj_set_style_bg_opa(c, LV_OPA_TRANSP, 0);
        lv_obj_set_style_radius(c, 0, 0);
        lv_obj_set_style_border_color(c, lv_color_hex(0x3a3a3a), 0);
        lv_obj_set_style_border_width(c, 2, 0);
        lv_obj_set_style_border_side(c, cs[i], 0);
        lv_obj_set_style_pad_all(c, 0, 0);
    }
    lv_obj_t *deco = lv_arc_create(screen_home);
    lv_obj_set_size(deco, 90, 90);
    lv_obj_align(deco, LV_ALIGN_CENTER, 0, -22);
    lv_arc_set_range(deco, 0, 100);
    lv_arc_set_value(deco, 65);
    lv_arc_set_bg_angles(deco, 0, 360);
    lv_obj_set_style_arc_color(deco, lv_color_hex(0x8B6914), LV_PART_INDICATOR);
    lv_obj_set_style_arc_width(deco, 4, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(deco, lv_color_hex(0x1e1e1e), LV_PART_MAIN);
    lv_obj_set_style_arc_width(deco, 4, LV_PART_MAIN);
    lv_obj_set_style_arc_width(deco, 0, LV_PART_KNOB);
    lv_obj_set_style_pad_all(deco, 0, LV_PART_KNOB);
    lv_obj_remove_flag(deco, LV_OBJ_FLAG_CLICKABLE);
    lv_anim_t a_pulse;
    lv_anim_init(&a_pulse);
    lv_anim_set_var(&a_pulse, deco);
    lv_anim_set_exec_cb(&a_pulse, home_arc_pulse_cb);
    lv_anim_set_values(&a_pulse, 5, 95);
    lv_anim_set_duration(&a_pulse, 2500);
    lv_anim_set_playback_duration(&a_pulse, 2500);
    lv_anim_set_repeat_count(&a_pulse, LV_ANIM_REPEAT_INFINITE);
    lv_anim_set_path_cb(&a_pulse, lv_anim_path_ease_in_out);
    lv_anim_start(&a_pulse);
    lv_obj_t *dot = lv_obj_create(screen_home);
    lv_obj_set_size(dot, 10, 10);
    lv_obj_set_style_radius(dot, 5, 0);
    lv_obj_set_style_bg_color(dot, lv_color_hex(0x8B6914), 0);
    lv_obj_set_style_border_width(dot, 0, 0);
    lv_obj_align(dot, LV_ALIGN_CENTER, 0, -22);
    lv_obj_t *title = lv_label_create(screen_home);
    lv_label_set_text(title, "SAFE CRACKER");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(title, lv_color_hex(0xFFFFFF), 0);
    lv_obj_align(title, LV_ALIGN_CENTER, 0, 46);
    lv_obj_t *sub = lv_label_create(screen_home);
    lv_label_set_text(sub, "COMBINAISON A 3 ANGLES \xC2\xB7 AS5047D");
    lv_obj_set_style_text_font(sub, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(sub, lv_color_hex(0xFFFFFF), 0);
    lv_obj_align(sub, LV_ALIGN_CENTER, 0, 68);
    lv_obj_t *btn = lv_button_create(screen_home);
    lv_obj_set_size(btn, 180, 44);
    lv_obj_align(btn, LV_ALIGN_CENTER, 0, 104);
    lv_obj_set_style_bg_color(btn, lv_color_hex(0x0e0e0e), 0);
    lv_obj_set_style_bg_color(btn, lv_color_hex(0x1e1e1e), LV_STATE_PRESSED);
    lv_obj_set_style_border_color(btn, lv_color_hex(0x8B6914), 0);
    lv_obj_set_style_border_width(btn, 1, 0);
    lv_obj_set_style_radius(btn, 0, 0);
    lv_obj_set_style_shadow_width(btn, 0, 0);
    lv_obj_add_event_cb(btn, btn_start_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *btn_lbl = lv_label_create(btn);
    lv_label_set_text(btn_lbl, "\xE2\x96\xBA NOUVELLE PARTIE");
    lv_obj_set_style_text_font(btn_lbl, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(btn_lbl, lv_color_hex(0xC8860A), 0);
    lv_obj_center(btn_lbl);
    lv_obj_t *warn = lv_obj_create(screen_home);
    lv_obj_set_size(warn, 480, 5);
    lv_obj_align(warn, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_bg_color(warn, lv_color_hex(0x8B6914), 0);
    lv_obj_set_style_bg_opa(warn, 80, 0);
    lv_obj_set_style_border_width(warn, 0, 0);
    lv_obj_set_style_radius(warn, 0, 0);
    lv_obj_t *serial = lv_label_create(screen_home);
    lv_label_set_text(serial, "IUT-CACHAN \xC2\xB7 2025");
    lv_obj_set_style_text_font(serial, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(serial, lv_color_hex(0xAAAAAA), 0);
    lv_obj_align(serial, LV_ALIGN_BOTTOM_RIGHT, -12, -10);
}

void createGameScreen() {
    screen_game = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(screen_game, lv_color_hex(0x0e0e0e), 0);
    lv_obj_set_style_pad_all(screen_game, 0, 0);
    lv_obj_set_style_border_width(screen_game, 0, 0);
    fond_jeu = lv_obj_create(screen_game);
    lv_obj_set_size(fond_jeu, 480, 272);
    lv_obj_set_pos(fond_jeu, 0, 0);
    lv_obj_set_style_bg_color(fond_jeu, lv_color_hex(0x0e0e0e), 0);
    lv_obj_set_style_border_width(fond_jeu, 0, 0);
    lv_obj_set_style_radius(fond_jeu, 0, 0);
    lv_obj_set_style_pad_all(fond_jeu, 0, 0);
    static lv_style_prop_t trans_bg_props[] = {LV_STYLE_BG_COLOR, (lv_style_prop_t)0};
    static lv_style_transition_dsc_t trans_bg_dsc;
    lv_style_transition_dsc_init(&trans_bg_dsc, trans_bg_props,
                                  lv_anim_path_ease_in_out, 350, 0, NULL);
    static lv_style_t style_bg_trans;
    lv_style_init(&style_bg_trans);
    lv_style_set_transition(&style_bg_trans, &trans_bg_dsc);
    lv_obj_add_style(fond_jeu, &style_bg_trans, 0);
    lv_obj_t *bot = lv_obj_create(screen_game);
    lv_obj_set_size(bot, 480, 20);
    lv_obj_align(bot, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_bg_color(bot, lv_color_hex(0x161616), 0);
    lv_obj_set_style_radius(bot, 0, 0);
    lv_obj_set_style_border_side(bot, LV_BORDER_SIDE_TOP, 0);
    lv_obj_set_style_border_color(bot, lv_color_hex(0x1e1e1e), 0);
    lv_obj_set_style_border_width(bot, 1, 0);
    lv_obj_set_style_pad_all(bot, 0, 0);
    lv_obj_t *hint = lv_label_create(bot);
    lv_label_set_text(hint, "\xE2\x96\xA0 TROUVER LES 3 ANGLES POUR DEVERROUILLER LE COFFRE");
    lv_obj_set_style_text_font(hint, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(hint, lv_color_hex(0xAAAAAA), 0);
    lv_obj_align(hint, LV_ALIGN_CENTER, 0, 0);
    lv_obj_t *sep = lv_obj_create(screen_game);
    lv_obj_set_size(sep, 1, 252);
    lv_obj_set_pos(sep, 258, 0);
    lv_obj_set_style_bg_color(sep, lv_color_hex(0x1e1e1e), 0);
    lv_obj_set_style_border_width(sep, 0, 0);
    lv_obj_set_style_radius(sep, 0, 0);
    arc_cadran = lv_arc_create(screen_game);
    lv_obj_set_size(arc_cadran, 228, 228);
    lv_obj_align(arc_cadran, LV_ALIGN_LEFT_MID, 12, -10);
    lv_arc_set_range(arc_cadran, 0, 360);
    lv_arc_set_value(arc_cadran, 0);
    lv_arc_set_bg_angles(arc_cadran, 0, 360);
    lv_obj_set_style_arc_color(arc_cadran, lv_color_hex(0x3a3a3a), LV_PART_INDICATOR);
    lv_obj_set_style_arc_width(arc_cadran, 14, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(arc_cadran, lv_color_hex(0x1a1a1a), LV_PART_MAIN);
    lv_obj_set_style_arc_width(arc_cadran, 14, LV_PART_MAIN);
    lv_obj_set_style_arc_width(arc_cadran, 0, LV_PART_KNOB);
    lv_obj_set_style_pad_all(arc_cadran, 0, LV_PART_KNOB);
    lv_obj_set_style_opa(arc_cadran, LV_OPA_TRANSP, LV_PART_KNOB);
    lv_obj_remove_flag(arc_cadran, LV_OBJ_FLAG_CLICKABLE);
    static lv_style_prop_t trans_arc_props[] = {LV_STYLE_ARC_COLOR, (lv_style_prop_t)0};
    static lv_style_transition_dsc_t trans_arc_dsc;
    lv_style_transition_dsc_init(&trans_arc_dsc, trans_arc_props,
                                  lv_anim_path_ease_in_out, 350, 0, NULL);
    static lv_style_t style_arc_trans;
    lv_style_init(&style_arc_trans);
    lv_style_set_transition(&style_arc_trans, &trans_arc_dsc);
    lv_obj_add_style(arc_cadran, &style_arc_trans, LV_PART_INDICATOR);
    lv_obj_t *sc = lv_scale_create(screen_game);
    lv_obj_set_size(sc, 228, 228);
    lv_obj_align(sc, LV_ALIGN_LEFT_MID, 12, -10);
    lv_scale_set_mode(sc, LV_SCALE_MODE_ROUND_INNER);
    lv_scale_set_total_tick_count(sc, 61);
    lv_scale_set_major_tick_every(sc, 5);
    lv_scale_set_label_show(sc, false);
    lv_scale_set_range(sc, 0, 360);
    lv_scale_set_angle_range(sc, 360);
    lv_scale_set_rotation(sc, 270);
    lv_obj_set_style_line_color(sc, lv_color_hex(0x2a2a2a), LV_PART_INDICATOR);
    lv_obj_set_style_line_width(sc, 2, LV_PART_INDICATOR);
    lv_obj_set_style_length(sc, 10, LV_PART_INDICATOR);
    lv_obj_set_style_line_color(sc, lv_color_hex(0x161616), LV_PART_ITEMS);
    lv_obj_set_style_line_width(sc, 1, LV_PART_ITEMS);
    lv_obj_set_style_length(sc, 5, LV_PART_ITEMS);
    lv_obj_set_style_arc_width(sc, 0, LV_PART_MAIN);
    lv_obj_set_style_arc_opa(sc, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_remove_flag(sc, LV_OBJ_FLAG_CLICKABLE);
    needle_line = lv_line_create(arc_cadran);
    lv_line_set_points(needle_line, needle_pts, 2);
    lv_obj_set_style_line_color(needle_line, lv_color_hex(0xD4CFBF), 0);
    lv_obj_set_style_line_width(needle_line, 2, 0);
    lv_obj_set_style_line_rounded(needle_line, true, 0);
    lv_obj_t *cdot = lv_obj_create(arc_cadran);
    lv_obj_set_size(cdot, 12, 12);
    lv_obj_set_style_radius(cdot, 6, 0);
    lv_obj_set_style_bg_color(cdot, lv_color_hex(0x161616), 0);
    lv_obj_set_style_border_color(cdot, lv_color_hex(0x8B6914), 0);
    lv_obj_set_style_border_width(cdot, 2, 0);
    lv_obj_align(cdot, LV_ALIGN_CENTER, 0, 0);
    lv_obj_t *phdr = lv_obj_create(screen_game);
    lv_obj_set_size(phdr, 222, 28);
    lv_obj_set_pos(phdr, 258, 0);
    lv_obj_set_style_bg_color(phdr, lv_color_hex(0x161616), 0);
    lv_obj_set_style_radius(phdr, 0, 0);
    lv_obj_set_style_border_side(phdr, LV_BORDER_SIDE_BOTTOM, 0);
    lv_obj_set_style_border_color(phdr, lv_color_hex(0x1e1e1e), 0);
    lv_obj_set_style_border_width(phdr, 1, 0);
    lv_obj_set_style_pad_all(phdr, 0, 0);
    lv_obj_t *lhdr = lv_label_create(phdr);
    lv_label_set_text(lhdr, "DASHBOARD");
    lv_obj_set_style_text_font(lhdr, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(lhdr, lv_color_hex(0xAAAAAA), 0);
    lv_obj_align(lhdr, LV_ALIGN_LEFT_MID, 8, 0);
    lv_obj_t *bbk = lv_button_create(phdr);
    lv_obj_set_size(bbk, 72, 22);
    lv_obj_align(bbk, LV_ALIGN_RIGHT_MID, -4, 0);
    lv_obj_set_style_bg_color(bbk, lv_color_hex(0x161616), 0);
    lv_obj_set_style_bg_color(bbk, lv_color_hex(0x2a2a2a), LV_STATE_PRESSED);
    lv_obj_set_style_border_color(bbk, lv_color_hex(0x2a2a2a), 0);
    lv_obj_set_style_border_width(bbk, 1, 0);
    lv_obj_set_style_radius(bbk, 0, 0);
    lv_obj_set_style_shadow_width(bbk, 0, 0);
    lv_obj_add_event_cb(bbk, btn_accueil_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *lbk = lv_label_create(bbk);
    lv_label_set_text(lbk, "\xe2\x8c\x82 ACCUEIL");
    lv_obj_set_style_text_font(lbk, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(lbk, lv_color_hex(0xFFFFFF), 0);
    lv_obj_center(lbk);
    led_open = lv_led_create(screen_game);
    lv_obj_set_size(led_open, 8, 8);
    lv_obj_align(led_open, LV_ALIGN_TOP_RIGHT, -10, 10);
    lv_led_set_color(led_open, lv_color_hex(0x2ECC71));
    lv_led_off(led_open);
    int32_t dx[3] = {-28,-18,-8};
    for (int i = 0; i < 3; i++) {
        dot_widgets[i] = lv_obj_create(screen_game);
        lv_obj_set_size(dot_widgets[i], 10, 10);
        lv_obj_set_style_radius(dot_widgets[i], 5, 0);
        lv_obj_set_style_bg_color(dot_widgets[i], lv_color_hex(0x161616), 0);
        lv_obj_set_style_border_color(dot_widgets[i], lv_color_hex(0x2a2a2a), 0);
        lv_obj_set_style_border_width(dot_widgets[i], 1, 0);
        lv_obj_set_style_pad_all(dot_widgets[i], 0, 0);
        lv_obj_align(dot_widgets[i], LV_ALIGN_TOP_RIGHT, dx[i], 36);
    }
    lv_obj_set_style_bg_color(dot_widgets[0], lv_color_hex(0x8B6914), 0);
    lv_obj_set_style_border_color(dot_widgets[0], lv_color_hex(0xC8860A), 0);
    label_step = lv_label_create(screen_game);
    lv_label_set_text(label_step, "ANGLE 1/3");
    lv_obj_set_style_text_font(label_step, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(label_step, lv_color_hex(0xFFFFFF), 0);
    lv_obj_align(label_step, LV_ALIGN_TOP_RIGHT, -95, 33);
    int32_t cx2[3] = {-148,-84,-20};
    for (int i = 0; i < 3; i++) {
        lv_obj_t *cb = lv_obj_create(screen_game);
        lv_obj_set_size(cb, 56, 22);
        lv_obj_align(cb, LV_ALIGN_TOP_RIGHT, cx2[i], 54);
        lv_obj_set_style_bg_color(cb, lv_color_hex(0x161616), 0);
        lv_obj_set_style_border_color(cb, lv_color_hex(0x2a2a2a), 0);
        lv_obj_set_style_border_width(cb, 1, 0);
        lv_obj_set_style_radius(cb, 0, 0);
        lv_obj_set_style_pad_all(cb, 0, 0);
        combo_cells[i] = lv_label_create(cb);
        lv_label_set_text(combo_cells[i], "???");
        lv_obj_set_style_text_font(combo_cells[i], &lv_font_montserrat_14, 0);
        lv_obj_set_style_text_color(combo_cells[i], lv_color_hex(0xFFFFFF), 0);
        lv_obj_align(combo_cells[i], LV_ALIGN_CENTER, 0, 0);
    }
    label_statut = lv_label_create(screen_game);
    lv_label_set_text(label_statut, "VERROUILLE");
    lv_obj_set_style_text_font(label_statut, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(label_statut, lv_color_hex(0xFFFFFF), 0);
    lv_obj_align(label_statut, LV_ALIGN_TOP_RIGHT, -10, 84);
    lv_obj_t *lp = lv_label_create(screen_game);
    lv_label_set_text(lp, "PROXIMITE CIBLE");
    lv_obj_set_style_text_font(lp, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(lp, lv_color_hex(0xFFFFFF), 0);
    lv_obj_align(lp, LV_ALIGN_TOP_RIGHT, -10, 106);
    bar_prox = lv_bar_create(screen_game);
    lv_obj_set_size(bar_prox, 200, 5);
    lv_obj_align(bar_prox, LV_ALIGN_TOP_RIGHT, -10, 122);
    lv_bar_set_range(bar_prox, 0, 100);
    lv_bar_set_value(bar_prox, 5, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(bar_prox, lv_color_hex(0x161616), LV_PART_MAIN);
    lv_obj_set_style_border_color(bar_prox, lv_color_hex(0x1e1e1e), LV_PART_MAIN);
    lv_obj_set_style_border_width(bar_prox, 1, LV_PART_MAIN);
    lv_obj_set_style_radius(bar_prox, 0, LV_PART_MAIN);
    lv_obj_set_style_bg_color(bar_prox, lv_color_hex(0x2a2a2a), LV_PART_INDICATOR);
    lv_obj_set_style_radius(bar_prox, 0, LV_PART_INDICATOR);
    lv_obj_t *lh = lv_label_create(screen_game);
    lv_label_set_text(lh, "MAINTIEN");
    lv_obj_set_style_text_font(lh, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(lh, lv_color_hex(0xFFFFFF), 0);
    lv_obj_align(lh, LV_ALIGN_TOP_RIGHT, -10, 136);
    bar_hold = lv_bar_create(screen_game);
    lv_obj_set_size(bar_hold, 200, 5);
    lv_obj_align(bar_hold, LV_ALIGN_TOP_RIGHT, -10, 152);
    lv_bar_set_range(bar_hold, 0, 100);
    lv_bar_set_value(bar_hold, 0, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(bar_hold, lv_color_hex(0x161616), LV_PART_MAIN);
    lv_obj_set_style_border_color(bar_hold, lv_color_hex(0x1e1e1e), LV_PART_MAIN);
    lv_obj_set_style_border_width(bar_hold, 1, LV_PART_MAIN);
    lv_obj_set_style_radius(bar_hold, 0, LV_PART_MAIN);
    lv_obj_set_style_bg_color(bar_hold, lv_color_hex(0x2ECC71), LV_PART_INDICATOR);
    lv_obj_set_style_radius(bar_hold, 0, LV_PART_INDICATOR);
    lv_obj_t *ap = lv_obj_create(screen_game);
    lv_obj_set_size(ap, 200, 40);
    lv_obj_align(ap, LV_ALIGN_BOTTOM_RIGHT, -10, -28);
    lv_obj_set_style_bg_color(ap, lv_color_hex(0x161616), 0);
    lv_obj_set_style_border_color(ap, lv_color_hex(0x1e1e1e), 0);
    lv_obj_set_style_border_width(ap, 1, 0);
    lv_obj_set_style_radius(ap, 0, 0);
    lv_obj_set_style_pad_all(ap, 0, 0);
    label_angle = lv_label_create(ap);
    lv_label_set_text(label_angle, "000\xC2\xB0");
    lv_obj_set_style_text_font(label_angle, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(label_angle, lv_color_hex(0xFFFFFF), 0);
    lv_obj_align(label_angle, LV_ALIGN_CENTER, 0, 0);
}

static void opa_cb(void *var, int32_t v) {
    lv_obj_set_style_opa((lv_obj_t *)var, (lv_opa_t)v, 0);
}

static void anim_timeline_done_cb(lv_anim_timeline_t *tl) {
    lv_anim_timeline_delete(tl);
}

void playVictoryAnimation() {
    lv_anim_timeline_t *tl = lv_anim_timeline_create();
    lv_anim_t a;

    lv_anim_init(&a);
    lv_anim_set_var(&a, v_flash_obj);
    lv_anim_set_exec_cb(&a, opa_cb);
    lv_anim_set_values(&a, 0, 180);
    lv_anim_set_duration(&a, 180);
    lv_anim_set_playback_duration(&a, 180);
    lv_anim_timeline_add(tl, 0, &a);

    lv_anim_init(&a);
    lv_anim_set_var(&a, v_title_lbl);
    lv_anim_set_exec_cb(&a, opa_cb);
    lv_anim_set_values(&a, 0, 255);
    lv_anim_set_duration(&a, 600);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_out);
    lv_anim_timeline_add(tl, 250, &a);

    lv_anim_init(&a);
    lv_anim_set_var(&a, v_sub_lbl);
    lv_anim_set_exec_cb(&a, opa_cb);
    lv_anim_set_values(&a, 0, 255);
    lv_anim_set_duration(&a, 500);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_out);
    lv_anim_timeline_add(tl, 650, &a);

    for (int i = 0; i < 3; i++) {
        lv_anim_init(&a);
        lv_anim_set_var(&a, v_cell_bg[i]);
        lv_anim_set_exec_cb(&a, opa_cb);
        lv_anim_set_values(&a, 0, 255);
        lv_anim_set_duration(&a, 400);
        lv_anim_set_path_cb(&a, lv_anim_path_ease_out);
        lv_anim_timeline_add(tl, 950 + i * 200, &a);
    }

    lv_anim_init(&a);
    lv_anim_set_var(&a, v_btn_play);
    lv_anim_set_exec_cb(&a, opa_cb);
    lv_anim_set_values(&a, 0, 255);
    lv_anim_set_duration(&a, 500);
    lv_anim_timeline_add(tl, 1700, &a);

    lv_anim_init(&a);
    lv_anim_set_var(&a, v_btn_home);
    lv_anim_set_exec_cb(&a, opa_cb);
    lv_anim_set_values(&a, 0, 255);
    lv_anim_set_duration(&a, 500);
    lv_anim_timeline_add(tl, 1850, &a);

    lv_anim_timeline_start(tl);
    lv_anim_timeline_set_completed_cb(tl, anim_timeline_done_cb);
}

void createVictoryScreen() {
    screen_victory = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(screen_victory, lv_color_hex(0x0c0800), 0);
    lv_obj_set_style_pad_all(screen_victory, 0, 0);
    lv_obj_set_style_border_width(screen_victory, 0, 0);
    v_flash_obj = lv_obj_create(screen_victory);
    lv_obj_set_size(v_flash_obj, 480, 272);
    lv_obj_set_pos(v_flash_obj, 0, 0);
    lv_obj_set_style_bg_color(v_flash_obj, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_border_width(v_flash_obj, 0, 0);
    lv_obj_set_style_radius(v_flash_obj, 0, 0);
    lv_obj_set_style_opa(v_flash_obj, 0, 0);
    lv_obj_t *gl = lv_obj_create(screen_victory);
    lv_obj_set_size(gl, 480, 3);
    lv_obj_set_pos(gl, 0, 0);
    lv_obj_set_style_bg_color(gl, lv_color_hex(0xC8860A), 0);
    lv_obj_set_style_border_width(gl, 0, 0);
    lv_obj_set_style_radius(gl, 0, 0);
    v_title_lbl = lv_label_create(screen_victory);
    lv_obj_t *vt = v_title_lbl;
    lv_label_set_text(vt, "\xe2\x9c\xa6 OUVERT \xe2\x9c\xa6");
    lv_obj_set_style_text_font(vt, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(vt, lv_color_hex(0xC8860A), 0);
    lv_obj_align(vt, LV_ALIGN_CENTER, 0, -70);
    v_sub_lbl = lv_label_create(screen_victory);
    lv_obj_t *vs = v_sub_lbl;
    lv_label_set_text(vs, "COMBINAISON TROUVEE");
    lv_obj_set_style_text_font(vs, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(vs, lv_color_hex(0xFFFFFF), 0);
    lv_obj_align(vs, LV_ALIGN_CENTER, 0, -48);
    int32_t vo[3] = {-66,0,66};
    for (int i = 0; i < 3; i++) {
        v_cell_bg[i] = lv_obj_create(screen_victory);
        lv_obj_t *vc = v_cell_bg[i];
        lv_obj_set_size(vc, 58, 34);
        lv_obj_align(vc, LV_ALIGN_CENTER, vo[i], -8);
        lv_obj_set_style_bg_color(vc, lv_color_hex(0x161616), 0);
        lv_obj_set_style_border_color(vc, lv_color_hex(0x8B6914), 0);
        lv_obj_set_style_border_width(vc, 1, 0);
        lv_obj_set_style_radius(vc, 0, 0);
        lv_obj_set_style_pad_all(vc, 0, 0);
        v_combo_labels[i] = lv_label_create(vc);
        lv_label_set_text(v_combo_labels[i], "---");
        lv_obj_set_style_text_font(v_combo_labels[i], &lv_font_montserrat_14, 0);
        lv_obj_set_style_text_color(v_combo_labels[i], lv_color_hex(0xC8860A), 0);
        lv_obj_align(v_combo_labels[i], LV_ALIGN_CENTER, 0, 0);
        lv_obj_set_style_opa(v_cell_bg[i], 0, 0);
    }
    v_btn_play = lv_button_create(screen_victory);
    lv_obj_t *br = v_btn_play;
    lv_obj_set_size(br, 150, 44);
    lv_obj_align(br, LV_ALIGN_CENTER, -86, 54);
    lv_obj_set_style_bg_color(br, lv_color_hex(0x0c0800), 0);
    lv_obj_set_style_bg_color(br, lv_color_hex(0x1a1200), LV_STATE_PRESSED);
    lv_obj_set_style_border_color(br, lv_color_hex(0x8B6914), 0);
    lv_obj_set_style_border_width(br, 1, 0);
    lv_obj_set_style_radius(br, 0, 0);
    lv_obj_set_style_shadow_width(br, 0, 0);
    lv_obj_add_event_cb(br, btn_rejouer_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *lr = lv_label_create(br);
    lv_label_set_text(lr, "\xe2\x86\xba REJOUER");
    lv_obj_set_style_text_font(lr, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(lr, lv_color_hex(0xC8860A), 0);
    lv_obj_center(lr);
    v_btn_home = lv_button_create(screen_victory);
    lv_obj_t *ba = v_btn_home;
    lv_obj_set_size(ba, 150, 44);
    lv_obj_align(ba, LV_ALIGN_CENTER, 86, 54);
    lv_obj_set_style_bg_color(ba, lv_color_hex(0x0c0800), 0);
    lv_obj_set_style_bg_color(ba, lv_color_hex(0x161616), LV_STATE_PRESSED);
    lv_obj_set_style_border_color(ba, lv_color_hex(0x2a2a2a), 0);
    lv_obj_set_style_border_width(ba, 1, 0);
    lv_obj_set_style_radius(ba, 0, 0);
    lv_obj_set_style_shadow_width(ba, 0, 0);
    lv_obj_add_event_cb(ba, btn_accueil_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *la = lv_label_create(ba);
    lv_label_set_text(la, "\xe2\x8c\x82 ACCUEIL");
    lv_obj_set_style_text_font(la, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(la, lv_color_hex(0xFFFFFF), 0);
    lv_obj_center(la);
    lv_obj_t *wv = lv_obj_create(screen_victory);
    lv_obj_set_size(wv, 480, 5);
    lv_obj_align(wv, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_bg_color(wv, lv_color_hex(0xC8860A), 0);
    lv_obj_set_style_bg_opa(wv, 80, 0);
    lv_obj_set_style_border_width(wv, 0, 0);
    lv_obj_set_style_radius(wv, 0, 0);
    lv_obj_set_style_opa(v_title_lbl, 0, 0);
    lv_obj_set_style_opa(v_sub_lbl, 0, 0);
    lv_obj_set_style_opa(v_btn_play, 0, 0);
    lv_obj_set_style_opa(v_btn_home, 0, 0);
}

#ifdef ARDUINO
#include "lvglDrivers.h"

void mySetup() {
    Serial.begin(115200);
    pinMode(AS5047P_CS,   OUTPUT);
    pinMode(AS5047P_SCK,  OUTPUT);
    pinMode(AS5047P_MOSI, OUTPUT);
    pinMode(AS5047P_MISO, INPUT);
    digitalWrite(AS5047P_CS,  HIGH);
    digitalWrite(AS5047P_SCK, LOW);
    delay(10);
    randomSeed(analogRead(A0) + millis());
    createBootScreen();
    createHomeScreen();
    createGameScreen();
    createVictoryScreen();
    generateNewGame();
    lv_screen_load(screen_boot);
    lv_timer_create(boot_timer_cb, 600, NULL);
}

void loop() {}

void myTask(void *pvParameters) {
    TickType_t xLastWakeTime = xTaskGetTickCount();
    while (1) {
        uint16_t raw = readAS5047P();
        int32_t  val = (int32_t)(raw * 360.0f / 16384.0f);
        Serial.println(val);
        lvglLock(portMAX_DELAY);
            updateGame(val);
        lvglUnlock();
        vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(30));
    }
}

#else
int main(void) { return 0; }
#endif