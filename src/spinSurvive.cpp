// Module Spin & Survive — roulette russe : 4 manches, secteurs mortels croissants
// Carte : STM32F746NG-DISCO  |  Auteur : [étudiant]  |  Date : 2026
//
// Machine d'états :
//   SS_IDLE     : barillet = angle brut du capteur (mis à jour en temps réel)
//   SS_RESULT   : pause 2 s après un survie (affichage résultat, puis round suivant)
//   SS_DEAD     : mort — overlay rouge, bouton "Rejouer" affiché après 1,5 s
//   SS_VICTORY  : victoire — overlay vert, bouton "Menu" affiché après 1,5 s
//
// Détection d'arrêt :
//   Le barillet s'arrête quand speedDegPerSec < 10°/s pendant 300 ms (10 ticks × 30 ms).
//   La vitesse est calculée dans myTask() à partir du delta angulaire réel du capteur.
//
// Mapping secteur → mort (evaluateResult) :
//   Le disque est divisé en 6 secteurs de 60° (numérotés 0 à 5 dans le sens trigonométrique).
//   effective = (int)((360 - fmod(angle, 360)) / 60) % 6
//   is_dead   = (effective < current_round)
//   Manche 1 → 1 secteur mortel, manche 2 → 2, ..., manche 4 → 4 secteurs mortels sur 6.
#include "spinSurvive.h"
#include <math.h>
#include <stdio.h>

#define FONT14 &lv_font_montserrat_14

typedef enum { MENU, SAFE_CRACKER, SPIN_SURVIVE } ActiveGame_t;
extern ActiveGame_t activeGame;
extern lv_obj_t *scr_menu;

// Widgets LVGL du jeu
static lv_obj_t *scr_ss         = NULL;
static lv_obj_t *lbl_round      = NULL;
static lv_obj_t *lbl_instr      = NULL;
static lv_obj_t *overlay        = NULL;
static lv_obj_t *lbl_result     = NULL;
static lv_obj_t *btn_restart    = NULL;
static lv_obj_t *arc_sectors[6]; // 6 arcs représentant les secteurs du barillet

// État interne du jeu
static SpinSurviveState_t ss_state = SS_IDLE;
static int    current_round        = 1;    // manche en cours (1 à 4)
static float  barrel_angle         = 0.0f; // angle courant du barillet en degrés
static uint32_t stop_timer_ms      = 0;    // durée de l'arrêt détecté (ms)
static bool   was_moving           = false; // vrai si le capteur a déjà été en mouvement
bool          ss_button_pressed    = false;

// Points de la ligne-aiguille (pivot bas = centre barillet, pointe haute)
static lv_point_precise_t needle_pts[2];
static lv_obj_t *needle            = NULL;

// Met à jour la couleur des secteurs : rouge pour les i < deadCount, vert sinon
static void drawSectors(int deadCount)
{
    for (int i = 0; i < 6; i++) {
        lv_color_t col = (i < deadCount)
            ? lv_color_hex(0xC0392B)  // rouge = mortel
            : lv_color_hex(0x27AE60); // vert  = sûr
        lv_obj_set_style_arc_color(arc_sectors[i], col, LV_PART_MAIN);
    }
}

// Fait pivoter les 6 arcs d'un même angle pour simuler la rotation du barillet
static void updateBarrel(float angleDeg)
{
    for (int i = 0; i < 6; i++) {
        lv_arc_set_rotation(arc_sectors[i], (uint16_t)angleDeg);
    }
}

static void evaluateResult(void);
void SS_ShowScreen(void);

// Callback du bouton "Rejouer" / "Menu" : relance le jeu ou retourne au menu
static void ss_btn_action_cb(lv_event_t *e)
{
    if (ss_state == SS_VICTORY) {
        activeGame = MENU;
        lv_scr_load(scr_menu);
    } else {
        SS_ShowScreen();
    }
}

// Détermine si l'angle d'arrêt est mortel, met à jour l'affichage et fait avancer l'état
static void evaluateResult(void)
{
    // Calcul du secteur effectif (sens horaire depuis le haut)
    // effective ∈ [0,5] : les secteurs 0..current_round-1 sont mortels
    int effective = (int)((360.0f - fmodf(barrel_angle, 360.0f)) / 60.0f) % 6;
    bool is_dead = (effective < current_round);

    lv_obj_remove_flag(overlay, LV_OBJ_FLAG_HIDDEN);

    if (is_dead) {
        static char buf[64];
        snprintf(buf, sizeof(buf), "MORT\nRound %d / 4", current_round);
        lv_label_set_text(lbl_result, buf);
        lv_obj_set_style_bg_color(overlay, lv_color_hex(0x7B0000), 0);
        lv_obj_set_style_bg_opa(overlay, LV_OPA_90, 0);
        lv_label_set_text(lv_obj_get_child(btn_restart, 0), "REJOUER"); // btn_restart a un lbl enfant
        ss_state = SS_DEAD;
        // Affiche le bouton après 1,5 s pour laisser le joueur lire le résultat
        lv_timer_create([](lv_timer_t *t) {
            lv_obj_remove_flag(btn_restart, LV_OBJ_FLAG_HIDDEN);
            lv_timer_delete(t);
        }, 1500, NULL);
    } else {
        if (current_round == 4) {
            // Victoire : tous les rounds passés
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
            // Survie : passage au round suivant après 2 s
            current_round++;
            static char buf[80];
            snprintf(buf, sizeof(buf),
                "SURVIE!\nRound suivant: %d balles", current_round);
            lv_label_set_text(lbl_result, buf);
            lv_obj_set_style_bg_color(overlay, lv_color_hex(0x1A3A5C), 0);
            lv_obj_set_style_bg_opa(overlay, LV_OPA_90, 0);
            ss_state = SS_RESULT;
            // Après 2 s : mise à jour des secteurs et retour à SS_IDLE
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

// Crée tous les widgets LVGL de l'écran Spin & Survive (init paresseuse : appelé une seule fois)
void SS_CreateScreen()
{
    scr_ss = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(scr_ss, lv_color_hex(0x0D0D0D), 0);
    lv_obj_set_style_pad_all(scr_ss, 0, 0);
    lv_obj_set_style_border_width(scr_ss, 0, 0);
    lv_obj_remove_flag(scr_ss, LV_OBJ_FLAG_SCROLLABLE);

    // 6 arcs formant le barillet ; centrés à (240, 146) avec décalage y=+10 px
    for (int i = 0; i < 6; i++) {
        arc_sectors[i] = lv_arc_create(scr_ss);
        lv_obj_set_size(arc_sectors[i], 220, 220);
        lv_obj_align(arc_sectors[i], LV_ALIGN_CENTER, 0, 10);
        lv_arc_set_bg_angles(arc_sectors[i], i * 60, i * 60 + 58); // 58° par secteur, 2° de séparation
        lv_arc_set_rotation(arc_sectors[i], 0);
        lv_obj_set_style_arc_opa(arc_sectors[i], LV_OPA_TRANSP, LV_PART_INDICATOR);
        lv_obj_set_style_arc_opa(arc_sectors[i], LV_OPA_TRANSP, LV_PART_KNOB);
        lv_obj_set_style_arc_width(arc_sectors[i], 0, LV_PART_KNOB);
        lv_obj_set_style_arc_width(arc_sectors[i], 40, LV_PART_MAIN);
        lv_obj_remove_flag(arc_sectors[i], LV_OBJ_FLAG_CLICKABLE);
    }
    drawSectors(1); // manche 1 : 1 secteur mortel

    // Aiguille fixe pointant vers le haut (pivot au centre du barillet, y=146)
    needle_pts[0].x = 240;
    needle_pts[0].y = 146; // pivot = centre barillet (alignement LV_ALIGN_CENTER, 0, 10)
    needle_pts[1].x = 240;
    needle_pts[1].y = 58;  // pointe de l'aiguille
    needle = lv_line_create(scr_ss);
    lv_line_set_points(needle, needle_pts, 2);
    lv_obj_set_style_line_width(needle, 3, 0);
    lv_obj_set_style_line_color(needle, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_line_rounded(needle, true, 0);

    // Petite flèche indicatrice au bout de l'aiguille
    static lv_point_precise_t tri[4];
    tri[0].x = 240; tri[0].y = 52;
    tri[1].x = 235; tri[1].y = 62;
    tri[2].x = 245; tri[2].y = 62;
    tri[3].x = 240; tri[3].y = 52;
    lv_obj_t *indicator = lv_line_create(scr_ss);
    lv_line_set_points(indicator, tri, 4);
    lv_obj_set_style_line_color(indicator, lv_color_hex(0xE74C3C), 0);
    lv_obj_set_style_line_width(indicator, 2, 0);

    // Étiquette de round en haut de l'écran
    lbl_round = lv_label_create(scr_ss);
    lv_label_set_text(lbl_round, "Round 1 / 4  -  1 balle");
    lv_obj_set_style_text_font(lbl_round, FONT14, 0);
    lv_obj_set_style_text_color(lbl_round, lv_color_hex(0xFFFFFF), 0);
    lv_obj_align(lbl_round, LV_ALIGN_TOP_MID, 0, 8);

    // Instruction joueur
    lbl_instr = lv_label_create(scr_ss);
    lv_label_set_text(lbl_instr, "Tournez le capteur, puis relâchez");
    lv_obj_set_style_text_font(lbl_instr, FONT14, 0);
    lv_obj_set_style_text_color(lbl_instr, lv_color_hex(0x888888), 0);
    lv_obj_align(lbl_instr, LV_ALIGN_TOP_MID, 0, 26);

    // Overlay de résultat (mort / survie / victoire), masqué par défaut
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

    // Bouton d'action (Rejouer / Menu), masqué jusqu'après 1,5 s
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

// Réinitialise l'état du jeu et charge l'écran (crée les widgets si nécessaire — init paresseuse)
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

// Met à jour le jeu à chaque tick de 30 ms (appelée depuis myTask() dans lvglLock).
// angleDeg  : angle courant du capteur en degrés (0–360)
// speedDegPerSec : vitesse angulaire absolue en °/s (calculée dans myTask)
void SS_Update(float angleDeg, float speedDegPerSec)
{
    switch (ss_state) {

    case SS_IDLE:
        // Barillet miroir du capteur en temps réel
        barrel_angle = angleDeg;
        updateBarrel(barrel_angle);

        // Détection d'arrêt : vitesse < 10°/s pendant 300 ms consécutifs
        if (speedDegPerSec > 10.0f) {
            was_moving = true;
            stop_timer_ms = 0;
        } else if (was_moving) {
            stop_timer_ms += 30; // +30 ms par tick
            if (stop_timer_ms >= 300) {
                ss_state = SS_RESULT;
                evaluateResult();
            }
        }
        break;

    // États passifs : le jeu est géré par les callbacks LVGL (timers, boutons)
    case SS_SPINNING:
    case SS_RESULT:
    case SS_DEAD:
    case SS_VICTORY:
        break;
    }
}
