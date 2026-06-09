#include "spinSurvive.h"
#include <math.h>
#include <stdio.h>

#define FONT14 &lv_font_montserrat_14

typedef enum { MENU, SAFE_CRACKER, SPIN_SURVIVE } ActiveGame_t;
extern ActiveGame_t activeGame;
extern lv_obj_t *scr_menu;

static lv_obj_t *scr_ss        = NULL;
static lv_obj_t *lbl_round     = NULL;
static lv_obj_t *lbl_instr     = NULL;
static lv_obj_t *overlay       = NULL;
static lv_obj_t *lbl_result    = NULL;
static lv_obj_t *btn_restart   = NULL;
static lv_obj_t *needle        = NULL;
static lv_obj_t *arc_sectors[6];

static SpinSurviveState_t ss_state  = SS_IDLE;
static int    current_round         = 1;
static float  needle_angle          = 0.0f;
static float  needle_speed          = 0.0f;
bool          ss_button_pressed     = false;

static lv_point_precise_t needle_pts[2];

static void drawSectors(int deadCount)
{
    for (int i = 0; i < 6; i++) {
        int start_angle = i * 60;
        int end_angle   = start_angle + 58;
        lv_arc_set_bg_angles(arc_sectors[i], start_angle, end_angle);
        lv_color_t col = (i < deadCount)
            ? lv_color_hex(0xC0392B)
            : lv_color_hex(0x27AE60);
        lv_obj_set_style_arc_color(arc_sectors[i], col, LV_PART_MAIN);
    }
}

static void updateNeedle(float angleDeg)
{
    float rad = angleDeg * (float)M_PI / 180.0f;
    needle_pts[0].x = 240;
    needle_pts[0].y = 136;
    needle_pts[1].x = (lv_value_precise_t)(240 + (int)(90.0f * cosf(rad)));
    needle_pts[1].y = (lv_value_precise_t)(136 + (int)(90.0f * sinf(rad)));
    lv_line_set_points(needle, needle_pts, 2);
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
    int sector = (int)(needle_angle / 60.0f) % 6;
    bool is_dead = (sector < current_round);

    lv_obj_remove_flag(overlay, LV_OBJ_FLAG_HIDDEN);

    if (is_dead) {
        static char buf[64];
        snprintf(buf, sizeof(buf), "YOU DIED\nRound %d / 4", current_round);
        lv_label_set_text(lbl_result, buf);
        lv_obj_set_style_bg_color(overlay, lv_color_hex(0x7B0000), 0);
        lv_obj_set_style_bg_opa(overlay, LV_OPA_90, 0);
        lv_label_set_text(
            lv_obj_get_child(btn_restart, 0), "PLAY AGAIN");
        ss_state = SS_DEAD;
        lv_timer_create([](lv_timer_t *t) {
            lv_obj_remove_flag(btn_restart, LV_OBJ_FLAG_HIDDEN);
            lv_timer_delete(t);
        }, 1500, NULL);
    } else {
        if (current_round == 4) {
            lv_label_set_text(lbl_result,
                "SURVIVOR!\nAll 4 rounds cleared.");
            lv_obj_set_style_bg_color(overlay, lv_color_hex(0x1A5C2A), 0);
            lv_obj_set_style_bg_opa(overlay, LV_OPA_90, 0);
            lv_label_set_text(
                lv_obj_get_child(btn_restart, 0), "BACK TO MENU");
            ss_state = SS_VICTORY;
            lv_timer_create([](lv_timer_t *t) {
                lv_obj_remove_flag(btn_restart, LV_OBJ_FLAG_HIDDEN);
                lv_timer_delete(t);
            }, 1500, NULL);
        } else {
            current_round++;
            static char buf[80];
            snprintf(buf, sizeof(buf),
                "SURVIVED!\nNext round: %d bullets", current_round);
            lv_label_set_text(lbl_result, buf);
            lv_obj_set_style_bg_color(overlay, lv_color_hex(0x1A3A5C), 0);
            lv_obj_set_style_bg_opa(overlay, LV_OPA_90, 0);
            ss_state = SS_RESULT;
            lv_timer_create([](lv_timer_t *t) {
                lv_timer_delete(t);
                drawSectors(current_round);
                static char rbuf[48];
                snprintf(rbuf, sizeof(rbuf),
                    "Round %d / 4  —  %d bullets",
                    current_round, current_round);
                lv_label_set_text(lbl_round, rbuf);
                lv_obj_add_flag(overlay, LV_OBJ_FLAG_HIDDEN);
                needle_speed = 0.0f;
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
        lv_arc_set_rotation(arc_sectors[i], 0);
        lv_arc_set_bg_angles(arc_sectors[i], i * 60, i * 60 + 58);
        lv_obj_set_style_arc_opa(arc_sectors[i], LV_OPA_TRANSP,
                                 LV_PART_INDICATOR);
        lv_obj_set_style_opa(lv_obj_get_child(arc_sectors[i], 0),
                             LV_OPA_TRANSP, 0);
        lv_obj_set_style_arc_width(arc_sectors[i], 40, LV_PART_MAIN);
        lv_obj_remove_flag(arc_sectors[i], LV_OBJ_FLAG_CLICKABLE);
    }
    drawSectors(1);

    needle_pts[0].x = 240;
    needle_pts[0].y = 136;
    needle_pts[1].x = 330;
    needle_pts[1].y = 136;
    needle = lv_line_create(scr_ss);
    lv_line_set_points(needle, needle_pts, 2);
    lv_obj_set_style_line_width(needle, 3, 0);
    lv_obj_set_style_line_color(needle, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_line_rounded(needle, true, 0);

    lbl_round = lv_label_create(scr_ss);
    lv_label_set_text(lbl_round, "Round 1 / 4  —  1 bullet");
    lv_obj_set_style_text_font(lbl_round, FONT14, 0);
    lv_obj_set_style_text_color(lbl_round, lv_color_hex(0xFFFFFF), 0);
    lv_obj_align(lbl_round, LV_ALIGN_TOP_MID, 0, 8);

    lbl_instr = lv_label_create(scr_ss);
    lv_label_set_text(lbl_instr, "SPIN fast, then press USER BUTTON");
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
    lv_label_set_text(lbl_btn, "PLAY AGAIN");
    lv_obj_set_style_text_font(lbl_btn, FONT14, 0);
    lv_obj_center(lbl_btn);
}

void SS_ShowScreen()
{
    current_round  = 1;
    ss_state       = SS_IDLE;
    needle_angle   = 0.0f;
    needle_speed   = 0.0f;
    ss_button_pressed = false;
    drawSectors(1);
    lv_label_set_text(lbl_round, "Round 1 / 4  —  1 bullet");
    lv_obj_add_flag(overlay, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(btn_restart, LV_OBJ_FLAG_HIDDEN);
    updateNeedle(0.0f);
    lv_scr_load(scr_ss);
}

void SS_Update(float angleDeg, float speedDegPerSec)
{
    switch (ss_state) {

    case SS_IDLE:
        updateNeedle(angleDeg);
        if (ss_button_pressed) {
            ss_button_pressed = false;
            needle_speed = speedDegPerSec;
            if (needle_speed < 50.0f)    needle_speed = 50.0f;
            if (needle_speed > 1440.0f)  needle_speed = 1440.0f;
            ss_state = SS_SPINNING;
        }
        break;

    case SS_SPINNING:
        needle_speed *= 0.97f;
        needle_angle += needle_speed * 0.03f;
        while (needle_angle >= 360.0f) needle_angle -= 360.0f;
        updateNeedle(needle_angle);
        if (needle_speed < 1.5f) {
            ss_state = SS_RESULT;
            evaluateResult();
        }
        break;

    case SS_RESULT:
    case SS_DEAD:
    case SS_VICTORY:
        break;
    }
}
