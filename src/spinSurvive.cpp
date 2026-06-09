#include "spinSurvive.h"
#include <math.h>
#include <stdio.h>

#define FONT14 &lv_font_montserrat_14

typedef enum { MENU, SAFE_CRACKER, SPIN_SURVIVE } ActiveGame_t;
extern ActiveGame_t activeGame;
extern lv_obj_t *scr_menu;

static lv_obj_t *scr_ss         = NULL;
static lv_obj_t *lbl_round      = NULL;
static lv_obj_t *lbl_instr      = NULL;
static lv_obj_t *overlay        = NULL;
static lv_obj_t *lbl_result     = NULL;
static lv_obj_t *btn_restart    = NULL;
static lv_obj_t *arc_sectors[6];

static SpinSurviveState_t ss_state = SS_IDLE;
static int    current_round        = 1;
static float  barrel_angle         = 0.0f;
static uint32_t stop_timer_ms      = 0;
static bool   was_moving           = false;
bool          ss_button_pressed    = false;

static lv_point_precise_t needle_pts[2];
static lv_obj_t *needle            = NULL;

static void drawSectors(int deadCount)
{
    for (int i = 0; i < 6; i++) {
        lv_color_t col = (i < deadCount)
            ? lv_color_hex(0xC0392B)
            : lv_color_hex(0x27AE60);
        lv_obj_set_style_arc_color(arc_sectors[i], col, LV_PART_MAIN);
    }
}

static void updateBarrel(float angleDeg)
{
    for (int i = 0; i < 6; i++) {
        lv_arc_set_rotation(arc_sectors[i], (uint16_t)angleDeg);
    }
}

static void evaluateResult(void);
void SS_ShowScreen(void);

static void ss_btn_action_cb(lv_event_t *e)
{
    if (ss_state == SS_VICTORY) {
        activeGame = MENU;
        lv_scr_load(scr_menu);
    } else {
        SS_ShowScreen();
    }
}

static void evaluateResult(void)
{
    int effective = (int)((360.0f - fmodf(barrel_angle, 360.0f)) / 60.0f) % 6;
    bool is_dead = (effective < current_round);

    lv_obj_remove_flag(overlay, LV_OBJ_FLAG_HIDDEN);

    if (is_dead) {
        static char buf[64];
        snprintf(buf, sizeof(buf), "MORT\nRound %d / 4", current_round);
        lv_label_set_text(lbl_result, buf);
        lv_obj_set_style_bg_color(overlay, lv_color_hex(0x7B0000), 0);
        lv_obj_set_style_bg_opa(overlay, LV_OPA_90, 0);
        lv_label_set_text(lv_obj_get_child(btn_restart, 0), "REJOUER");
        ss_state = SS_DEAD;
        lv_timer_create([](lv_timer_t *t) {
            lv_obj_remove_flag(btn_restart, LV_OBJ_FLAG_HIDDEN);
            lv_timer_delete(t);
        }, 1500, NULL);
    } else {
        if (current_round == 4) {
            lv_label_set_text(lbl_result,
                "SURVIVANT!\nTous les rounds passés.");
            lv_obj_set_style_bg_color(overlay, lv_color_hex(0x1A5C2A), 0);
            lv_obj_set_style_bg_opa(overlay, LV_OPA_90, 0);
            lv_label_set_text(lv_obj_get_child(btn_restart, 0), "MENU");
            ss_state = SS_VICTORY;
            lv_timer_create([](lv_timer_t *t) {
                lv_obj_remove_flag(btn_restart, LV_OBJ_FLAG_HIDDEN);
                lv_timer_delete(t);
            }, 1500, NULL);
        } else {
            current_round++;
            static char buf[80];
            snprintf(buf, sizeof(buf),
                "SURVIE!\nRound suivant: %d balles", current_round);
            lv_label_set_text(lbl_result, buf);
            lv_obj_set_style_bg_color(overlay, lv_color_hex(0x1A3A5C), 0);
            lv_obj_set_style_bg_opa(overlay, LV_OPA_90, 0);
            ss_state = SS_RESULT;
            lv_timer_create([](lv_timer_t *t) {
                lv_timer_delete(t);
                drawSectors(current_round);
                static char rbuf[48];
                snprintf(rbuf, sizeof(rbuf),
                    "Round %d / 4  -  %d balles",
                    current_round, current_round);
                lv_label_set_text(lbl_round, rbuf);
                lv_obj_add_flag(overlay, LV_OBJ_FLAG_HIDDEN);
                barrel_angle = 0.0f;
                was_moving = false;
                stop_timer_ms = 0;
                ss_state = SS_IDLE;
            }, 2000, NULL);
        }
    }
}

void SS_CreateScreen()
{
    scr_ss = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(scr_ss, lv_color_hex(0x0D0D0D), 0);
    lv_obj_set_style_pad_all(scr_ss, 0, 0);
    lv_obj_set_style_border_width(scr_ss, 0, 0);
    lv_obj_remove_flag(scr_ss, LV_OBJ_FLAG_SCROLLABLE);

    for (int i = 0; i < 6; i++) {
        arc_sectors[i] = lv_arc_create(scr_ss);
        lv_obj_set_size(arc_sectors[i], 220, 220);
        lv_obj_align(arc_sectors[i], LV_ALIGN_CENTER, 0, 10);
        lv_arc_set_bg_angles(arc_sectors[i], i * 60, i * 60 + 58);
        lv_arc_set_rotation(arc_sectors[i], 0);
        lv_obj_set_style_arc_opa(arc_sectors[i], LV_OPA_TRANSP, LV_PART_INDICATOR);
        lv_obj_set_style_arc_opa(arc_sectors[i], LV_OPA_TRANSP, LV_PART_KNOB);
        lv_obj_set_style_arc_width(arc_sectors[i], 0, LV_PART_KNOB);
        lv_obj_set_style_arc_width(arc_sectors[i], 40, LV_PART_MAIN);
        lv_obj_remove_flag(arc_sectors[i], LV_OBJ_FLAG_CLICKABLE);
    }
    drawSectors(1);

    needle_pts[0].x = 240;
    needle_pts[0].y = 146;
    needle_pts[1].x = 240;
    needle_pts[1].y = 58;
    needle = lv_line_create(scr_ss);
    lv_line_set_points(needle, needle_pts, 2);
    lv_obj_set_style_line_width(needle, 3, 0);
    lv_obj_set_style_line_color(needle, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_line_rounded(needle, true, 0);

    static lv_point_precise_t tri[4];
    tri[0].x = 240; tri[0].y = 52;
    tri[1].x = 235; tri[1].y = 62;
    tri[2].x = 245; tri[2].y = 62;
    tri[3].x = 240; tri[3].y = 52;
    lv_obj_t *indicator = lv_line_create(scr_ss);
    lv_line_set_points(indicator, tri, 4);
    lv_obj_set_style_line_color(indicator, lv_color_hex(0xE74C3C), 0);
    lv_obj_set_style_line_width(indicator, 2, 0);

    lbl_round = lv_label_create(scr_ss);
    lv_label_set_text(lbl_round, "Round 1 / 4  -  1 balle");
    lv_obj_set_style_text_font(lbl_round, FONT14, 0);
    lv_obj_set_style_text_color(lbl_round, lv_color_hex(0xFFFFFF), 0);
    lv_obj_align(lbl_round, LV_ALIGN_TOP_MID, 0, 8);

    lbl_instr = lv_label_create(scr_ss);
    lv_label_set_text(lbl_instr, "Tournez le capteur, puis relâchez");
    lv_obj_set_style_text_font(lbl_instr, FONT14, 0);
    lv_obj_set_style_text_color(lbl_instr, lv_color_hex(0x888888), 0);
    lv_obj_align(lbl_instr, LV_ALIGN_TOP_MID, 0, 26);

    overlay = lv_obj_create(scr_ss);
    lv_obj_set_size(overlay, 480, 272);
    lv_obj_set_pos(overlay, 0, 0);
    lv_obj_set_style_radius(overlay, 0, 0);
    lv_obj_set_style_border_width(overlay, 0, 0);
    lv_obj_set_style_bg_color(overlay, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(overlay, LV_OPA_80, 0);
    lv_obj_add_flag(overlay, LV_OBJ_FLAG_HIDDEN);

    lbl_result = lv_label_create(overlay);
    lv_label_set_text(lbl_result, "");
    lv_obj_set_style_text_font(lbl_result, FONT14, 0);
    lv_obj_set_style_text_color(lbl_result, lv_color_hex(0xFFFFFF), 0);
    lv_label_set_long_mode(lbl_result, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(lbl_result, 380);
    lv_obj_align(lbl_result, LV_ALIGN_CENTER, 0, -20);

    btn_restart = lv_button_create(overlay);
    lv_obj_set_size(btn_restart, 160, 36);
    lv_obj_align(btn_restart, LV_ALIGN_CENTER, 0, 40);
    lv_obj_set_style_bg_color(btn_restart, lv_color_hex(0x333333), 0);
    lv_obj_add_flag(btn_restart, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_event_cb(btn_restart, ss_btn_action_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *lbl_btn = lv_label_create(btn_restart);
    lv_label_set_text(lbl_btn, "REJOUER");
    lv_obj_set_style_text_font(lbl_btn, FONT14, 0);
    lv_obj_center(lbl_btn);
}

void SS_ShowScreen()
{
    if (!scr_ss) {
        SS_CreateScreen();
    }
    current_round  = 1;
    ss_state       = SS_IDLE;
    barrel_angle   = 0.0f;
    stop_timer_ms  = 0;
    was_moving     = false;
    ss_button_pressed = false;
    drawSectors(1);
    lv_label_set_text(lbl_round, "Round 1 / 4  -  1 balle");
    lv_obj_add_flag(overlay, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(btn_restart, LV_OBJ_FLAG_HIDDEN);
    updateBarrel(0.0f);
    lv_scr_load(scr_ss);
}

void SS_Update(float angleDeg, float speedDegPerSec)
{
    switch (ss_state) {

    case SS_IDLE:
        barrel_angle = angleDeg;
        updateBarrel(barrel_angle);

        if (speedDegPerSec > 10.0f) {
            was_moving = true;
            stop_timer_ms = 0;
        } else if (was_moving) {
            stop_timer_ms += 30;
            if (stop_timer_ms >= 300) {
                ss_state = SS_RESULT;
                evaluateResult();
            }
        }
        break;

    case SS_SPINNING:
    case SS_RESULT:
    case SS_DEAD:
    case SS_VICTORY:
        break;
    }
}
