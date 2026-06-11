// Module Spin & Survive — roulette russe : 4 manches, balles cachees
// Carte : STM32F746NG-DISCO  |  Auteur : [etudiant]  |  Date : 2026
//
// Machine d'etats :
//   SS_HOME      : ecran intro, attente du bouton JOUER
//   SS_WAIT_SPIN : barillet = capteur ; armement si vitesse > 360 deg/s x 3 ticks consecutifs
//   SS_SPINNING  : arme ; arret si vitesse < 10 deg/s x 10 ticks consecutifs (300 ms)
//   SS_SUSPENSE  : 700 ms "CLIC..." ; revelation a la fin du timer
//   SS_RESULT    : overlay 2200 ms (chambre vide) puis round suivant
//   SS_DEAD      : overlay permanent (balle) + boutons REJOUER / MENU
//   SS_VICTORY   : overlay permanent (victoire) + boutons REJOUER / MENU
//
// Geometrie du barillet (plain lv_obj - pas d'arc) :
//   Centre C = (240, 146)  |  Rayon orbite chambres = 62 px  |  Taille chambre = 52x52
//   Chambre i : x = 240 + 62*cos(theta + i*60) - 26,  y = 146 + 62*sin(theta + i*60) - 26
//   Selection par le marteau (fixe, 12h) :
//     sel = ((int)lroundf((270 - theta) / 60)) % 6 ; si sel < 0 : sel += 6
#include "spinSurvive.h"
#include <Arduino.h>
#include <math.h>
#include <stdio.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define FONT14 &lv_font_montserrat_14

typedef enum { MENU, SAFE_CRACKER, SPIN_SURVIVE } ActiveGame_t;
extern ActiveGame_t activeGame;
extern lv_obj_t *scr_menu;

// --- Ecrans ---
static lv_obj_t *scr_ss_home = NULL;
static lv_obj_t *scr_ss_game = NULL;

// --- Widgets du jeu ---
static lv_obj_t *chambers[6];           // 6 chambres, repositionnees chaque tick
static lv_obj_t *lbl_status      = NULL;
static lv_obj_t *lbl_round_top   = NULL;
static lv_obj_t *overlay         = NULL;
static lv_obj_t *lbl_overlay     = NULL;
static lv_obj_t *btn_rejouer     = NULL;
static lv_obj_t *btn_menu_end    = NULL;
static lv_obj_t *lbl_ss_sensor_error = NULL;

// --- Etat du jeu ---
static SS_State_t ss_state     = SS_HOME;
static int   current_round     = 1;
static float theta             = 0.0f;
static int   loaded_chambers[6];
static int   loaded_count      = 0;
static int   sel               = 0;
static int   arm_ticks         = 0;
static int   stop_ticks        = 0;
static lv_timer_t *pending_timer = NULL; // timer one-shot en cours (NULL si aucun)

// Charge n balles dans n chambres distinctes parmi 6 -- Fisher-Yates partiel
static void loadBullets(int n)
{
    int deck[6] = {0, 1, 2, 3, 4, 5};
    for (int i = 0; i < n; i++) {
        int j = i + random(0, 6 - i);
        int tmp = deck[i]; deck[i] = deck[j]; deck[j] = tmp;
        loaded_chambers[i] = deck[i];
    }
    loaded_count = n;
}

// Remet les 6 chambres a leur apparence neutre et supprime l'eventuel label "X"
// Note : lv_obj_get_child() est ici sur un lv_obj plain (chambre), pas un lv_arc
static void resetChamberStyles(void)
{
    for (int i = 0; i < 6; i++) {
        if (lv_obj_get_child_count(chambers[i]) > 0)
            lv_obj_delete(lv_obj_get_child(chambers[i], 0));
        lv_obj_set_style_bg_color(chambers[i], lv_color_hex(0x0D0D0D), 0);
        lv_obj_set_style_border_color(chambers[i], lv_color_hex(0x4a4a4a), 0);
    }
}

// Repositionne les 6 chambres en coordonnees absolues a chaque tick
// parent = scr_ss_game avec pad_all=0, donc set_pos = coordonnees ecran
static void repositionChambers(float angleDeg)
{
    for (int i = 0; i < 6; i++) {
        float rad = (angleDeg + i * 60.0f) * (float)M_PI / 180.0f;
        lv_obj_set_pos(chambers[i],
            (lv_coord_t)((int)(240.0f + 62.0f * cosf(rad)) - 26),
            (lv_coord_t)((int)(146.0f + 62.0f * sinf(rad)) - 26));
    }
}

// Declarations anticipees des callbacks timer
static void suspense_cb(lv_timer_t *t);
static void dead_overlay_cb(lv_timer_t *t);
static void result_cb(lv_timer_t *t);

// Callback : 700 ms apres l'arret -- revele balle ou chambre vide
static void suspense_cb(lv_timer_t *t)
{
    lv_timer_delete(t);
    pending_timer = NULL;

    bool is_dead = false;
    for (int i = 0; i < loaded_count; i++) {
        if (loaded_chambers[i] == sel) { is_dead = true; break; }
    }

    if (is_dead) {
        // Revele toutes les chambres chargees en rouge
        for (int i = 0; i < loaded_count; i++) {
            lv_obj_set_style_bg_color(chambers[loaded_chambers[i]], lv_color_hex(0xC0392B), 0);
            lv_obj_set_style_border_color(chambers[loaded_chambers[i]], lv_color_hex(0xFF6B5B), 0);
        }
        // Marque la chambre fatale avec un "X"
        lv_obj_t *lbl_x = lv_label_create(chambers[sel]);
        lv_label_set_text(lbl_x, "X");
        lv_obj_set_style_text_font(lbl_x, FONT14, 0);
        lv_obj_set_style_text_color(lbl_x, lv_color_hex(0xFFFFFF), 0);
        lv_obj_center(lbl_x);
        ss_state = SS_DEAD;
        // Overlay apres 1200 ms (laisse voir les chambres revelees)
        pending_timer = lv_timer_create(dead_overlay_cb, 1200, NULL);
    } else {
        // Chambre vide : flash vert sur la chambre selectionnee
        lv_obj_set_style_bg_color(chambers[sel], lv_color_hex(0x27AE60), 0);
        lv_obj_set_style_bg_opa(overlay, LV_OPA_90, 0);
        static char buf[64];
        if (current_round < 4) {
            lv_obj_set_style_bg_color(overlay, lv_color_hex(0x1A3A5C), 0);
            snprintf(buf, sizeof(buf), "CHAMBRE VIDE\nManche %d : %d balle(s)",
                     current_round + 1, current_round + 1);
        } else {
            // Dernier round : annonce la victoire imminente
            lv_obj_set_style_bg_color(overlay, lv_color_hex(0x1A5C2A), 0);
            snprintf(buf, sizeof(buf), "CHAMBRE VIDE\nVictoire !");
        }
        lv_label_set_text(lbl_overlay, buf);
        lv_obj_remove_flag(overlay, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(btn_rejouer, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(btn_menu_end, LV_OBJ_FLAG_HIDDEN);
        ss_state      = SS_RESULT;
        pending_timer = lv_timer_create(result_cb, 2200, NULL);
    }
}

// Callback : 1200 ms apres revelation balle -- affiche l'overlay PERDU
static void dead_overlay_cb(lv_timer_t *t)
{
    lv_timer_delete(t);
    pending_timer = NULL;

    lv_obj_set_style_bg_color(overlay, lv_color_hex(0x7B0000), 0);
    lv_obj_set_style_bg_opa(overlay, LV_OPA_90, 0);
    static char buf[48];
    snprintf(buf, sizeof(buf), "PERDU\nManche %d / 4", current_round);
    lv_label_set_text(lbl_overlay, buf);
    lv_obj_remove_flag(overlay, LV_OBJ_FLAG_HIDDEN);
    lv_obj_remove_flag(btn_rejouer, LV_OBJ_FLAG_HIDDEN);
    lv_obj_remove_flag(btn_menu_end, LV_OBJ_FLAG_HIDDEN);
}

// Callback : 2200 ms apres chambre vide -- passe au round suivant ou victoire
static void result_cb(lv_timer_t *t)
{
    lv_timer_delete(t);
    pending_timer = NULL;

    if (current_round == 4) {
        // 4 manches survecues : victoire totale
        lv_obj_set_style_bg_color(overlay, lv_color_hex(0x1A5C2A), 0);
        lv_obj_set_style_bg_opa(overlay, LV_OPA_90, 0);
        lv_label_set_text(lbl_overlay, "VICTOIRE\n4 manches survecues");
        lv_obj_remove_flag(overlay, LV_OBJ_FLAG_HIDDEN);
        lv_obj_remove_flag(btn_rejouer, LV_OBJ_FLAG_HIDDEN);
        lv_obj_remove_flag(btn_menu_end, LV_OBJ_FLAG_HIDDEN);
        ss_state = SS_VICTORY;
    } else {
        // Survie : prepare le round suivant
        current_round++;
        loadBullets(current_round);
        resetChamberStyles();
        static char rbuf[36];
        snprintf(rbuf, sizeof(rbuf), "MANCHE %d/4 - %d balle(s)", current_round, current_round);
        lv_label_set_text(lbl_round_top, rbuf);
        lv_obj_add_flag(overlay, LV_OBJ_FLAG_HIDDEN);
        lv_label_set_text(lbl_status, "ARMEZ - faites tourner l'aimant");
        arm_ticks  = 0;
        stop_ticks = 0;
        ss_state   = SS_WAIT_SPIN;
    }
}

// Annule le timer en cours et remet le jeu a l'etat initial (round 1, 1 balle)
// Appelee depuis les callbacks boutons (deja dans lvglTask -- pas de lock a ajouter)
static void SS_Reset(void)
{
    if (pending_timer) { lv_timer_delete(pending_timer); pending_timer = NULL; }

    current_round = 1;
    theta         = 0.0f;
    arm_ticks     = 0;
    stop_ticks    = 0;
    sel           = 0;

    loadBullets(1);
    resetChamberStyles();
    repositionChambers(0.0f);

    lv_label_set_text(lbl_round_top, "MANCHE 1/4 - 1 balle");
    lv_label_set_text(lbl_status, "ARMEZ - faites tourner l'aimant");
    lv_obj_add_flag(overlay, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(btn_rejouer, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(btn_menu_end, LV_OBJ_FLAG_HIDDEN);

    ss_state = SS_WAIT_SPIN;
}

// Callback du bouton JOUER sur l'ecran d'accueil
static void ss_home_jouer_cb(lv_event_t *e)
{
    (void)e;
    SS_Reset();
    lv_scr_load(scr_ss_game);
}

// Cree l'ecran d'accueil S&S (init paresseuse -- appele une seule fois)
static void SS_CreateHomeScreen(void)
{
    scr_ss_home = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(scr_ss_home, lv_color_hex(0x0D0D0D), 0);
    lv_obj_set_style_border_width(scr_ss_home, 0, 0);
    lv_obj_set_style_pad_all(scr_ss_home, 0, 0);
    lv_obj_remove_flag(scr_ss_home, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *lbl_title = lv_label_create(scr_ss_home);
    lv_label_set_text(lbl_title, "SPIN & SURVIVE");
    lv_obj_set_style_text_font(lbl_title, FONT14, 0);
    lv_obj_set_style_text_color(lbl_title, lv_color_hex(0xFFFFFF), 0);
    lv_obj_align(lbl_title, LV_ALIGN_TOP_MID, 0, 20);

    // Regles du jeu
    lv_obj_t *lbl_rules = lv_label_create(scr_ss_home);
    lv_label_set_text(lbl_rules,
        "Le barillet suit votre aimant.\n"
        "Donnez une forte impulsion et laissez-le s'arreter.\n"
        "Chambre vide = manche suivante. Balle = perdu.");
    lv_obj_set_style_text_font(lbl_rules, FONT14, 0);
    lv_obj_set_style_text_color(lbl_rules, lv_color_hex(0x888888), 0);
    lv_label_set_long_mode(lbl_rules, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(lbl_rules, 400);
    lv_obj_align(lbl_rules, LV_ALIGN_CENTER, 0, -40);

    lv_obj_t *lbl_hint = lv_label_create(scr_ss_home);
    lv_label_set_text(lbl_hint, "4 manches - 1 a 4 balles");
    lv_obj_set_style_text_font(lbl_hint, FONT14, 0);
    lv_obj_set_style_text_color(lbl_hint, lv_color_hex(0x666666), 0);
    lv_obj_align(lbl_hint, LV_ALIGN_CENTER, 0, 10);

    // Bouton JOUER (style Safe Cracker : fond sombre, bordure doree, radius 0)
    lv_obj_t *btn_jouer = lv_button_create(scr_ss_home);
    lv_obj_set_size(btn_jouer, 160, 40);
    lv_obj_align(btn_jouer, LV_ALIGN_CENTER, 0, 55);
    lv_obj_set_style_bg_color(btn_jouer, lv_color_hex(0x1A1A1A), 0);
    lv_obj_set_style_border_width(btn_jouer, 1, 0);
    lv_obj_set_style_border_color(btn_jouer, lv_color_hex(0x8B6914), 0);
    lv_obj_set_style_radius(btn_jouer, 0, 0);
    lv_obj_add_event_cb(btn_jouer, ss_home_jouer_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *lbl_jouer = lv_label_create(btn_jouer);
    lv_label_set_text(lbl_jouer, "JOUER");
    lv_obj_set_style_text_font(lbl_jouer, FONT14, 0);
    lv_obj_center(lbl_jouer);

    // Bouton MENU (gris, bordure grise)
    lv_obj_t *btn_menu_home = lv_button_create(scr_ss_home);
    lv_obj_set_size(btn_menu_home, 80, 28);
    lv_obj_align(btn_menu_home, LV_ALIGN_CENTER, 0, 105);
    lv_obj_set_style_bg_color(btn_menu_home, lv_color_hex(0x333333), 0);
    lv_obj_set_style_border_width(btn_menu_home, 1, 0);
    lv_obj_set_style_border_color(btn_menu_home, lv_color_hex(0x666666), 0);
    lv_obj_add_event_cb(btn_menu_home, [](lv_event_t *) {
        activeGame = MENU;
        lv_scr_load(scr_menu);
    }, LV_EVENT_CLICKED, NULL);
    lv_obj_t *lbl_menu_h = lv_label_create(btn_menu_home);
    lv_label_set_text(lbl_menu_h, "MENU");
    lv_obj_set_style_text_font(lbl_menu_h, FONT14, 0);
    lv_obj_center(lbl_menu_h);
}

// Cree l'ecran de jeu S&S (init paresseuse -- appele une seule fois)
// Geometrie : centre barillet C=(240,146), rayon orbite=62 px, chambre=52x52
static void SS_CreateGameScreen(void)
{
    scr_ss_game = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(scr_ss_game, lv_color_hex(0x0D0D0D), 0);
    lv_obj_set_style_border_width(scr_ss_game, 0, 0);
    lv_obj_set_style_pad_all(scr_ss_game, 0, 0);
    lv_obj_remove_flag(scr_ss_game, LV_OBJ_FLAG_SCROLLABLE);

    // Barre de navigation haute (480x24, bg tres sombre)
    lv_obj_t *topbar = lv_obj_create(scr_ss_game);
    lv_obj_set_size(topbar, 480, 24);
    lv_obj_set_pos(topbar, 0, 0);
    lv_obj_set_style_bg_color(topbar, lv_color_hex(0x161616), 0);
    lv_obj_set_style_border_width(topbar, 0, 0);
    lv_obj_set_style_pad_all(topbar, 0, 0);
    lv_obj_remove_flag(topbar, LV_OBJ_FLAG_SCROLLABLE);

    // Bouton MENU gauche de la topbar
    lv_obj_t *btn_menu_top = lv_button_create(topbar);
    lv_obj_set_size(btn_menu_top, 56, 20);
    lv_obj_align(btn_menu_top, LV_ALIGN_LEFT_MID, 2, 0);
    lv_obj_set_style_bg_color(btn_menu_top, lv_color_hex(0x2A2A2A), 0);
    lv_obj_set_style_border_width(btn_menu_top, 1, 0);
    lv_obj_set_style_border_color(btn_menu_top, lv_color_hex(0x555555), 0);
    lv_obj_set_style_pad_all(btn_menu_top, 0, 0);
    lv_obj_add_event_cb(btn_menu_top, [](lv_event_t *) {
        SS_Reset();
        activeGame = MENU;
        lv_scr_load(scr_menu);
    }, LV_EVENT_CLICKED, NULL);
    lv_obj_t *lbl_mt = lv_label_create(btn_menu_top);
    lv_label_set_text(lbl_mt, "MENU");
    lv_obj_set_style_text_font(lbl_mt, FONT14, 0);
    lv_obj_center(lbl_mt);

    // Label titre topbar (centre-gauche, apres le bouton MENU)
    lv_obj_t *lbl_top_title = lv_label_create(topbar);
    lv_label_set_text(lbl_top_title, "SPIN & SURVIVE");
    lv_obj_set_style_text_font(lbl_top_title, FONT14, 0);
    lv_obj_set_style_text_color(lbl_top_title, lv_color_hex(0xAAAAAA), 0);
    lv_obj_align(lbl_top_title, LV_ALIGN_LEFT_MID, 64, 0);

    // Label de manche (topbar droite)
    lbl_round_top = lv_label_create(topbar);
    lv_label_set_text(lbl_round_top, "MANCHE 1/4 - 1 balle");
    lv_obj_set_style_text_font(lbl_round_top, FONT14, 0);
    lv_obj_set_style_text_color(lbl_round_top, lv_color_hex(0xCCCCCC), 0);
    lv_obj_align(lbl_round_top, LV_ALIGN_RIGHT_MID, -4, 0);

    // Corps du barillet -- cercle 196x196, centre a (240,146) via LV_ALIGN_CENTER 0,+10
    lv_obj_t *cyl_body = lv_obj_create(scr_ss_game);
    lv_obj_set_size(cyl_body, 196, 196);
    lv_obj_align(cyl_body, LV_ALIGN_CENTER, 0, 10);
    lv_obj_set_style_radius(cyl_body, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(cyl_body, lv_color_hex(0x1A1A1A), 0);
    lv_obj_set_style_border_width(cyl_body, 3, 0);
    lv_obj_set_style_border_color(cyl_body, lv_color_hex(0x3a3a3a), 0);
    lv_obj_set_style_pad_all(cyl_body, 0, 0);
    lv_obj_remove_flag(cyl_body, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_remove_flag(cyl_body, LV_OBJ_FLAG_SCROLLABLE);

    // 6 chambres -- cercles 52x52, positionnees via repositionChambers() a chaque tick
    for (int i = 0; i < 6; i++) {
        chambers[i] = lv_obj_create(scr_ss_game);
        lv_obj_set_size(chambers[i], 52, 52);
        lv_obj_set_style_radius(chambers[i], LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_color(chambers[i], lv_color_hex(0x0D0D0D), 0);
        lv_obj_set_style_border_width(chambers[i], 2, 0);
        lv_obj_set_style_border_color(chambers[i], lv_color_hex(0x4a4a4a), 0);
        lv_obj_set_style_pad_all(chambers[i], 0, 0);
        lv_obj_remove_flag(chambers[i], LV_OBJ_FLAG_CLICKABLE);
        lv_obj_remove_flag(chambers[i], LV_OBJ_FLAG_SCROLLABLE);
    }
    repositionChambers(0.0f);

    // Moyeu central -- cercle 24x24 centre sur C=(240,146) : pos = (228, 134)
    lv_obj_t *hub = lv_obj_create(scr_ss_game);
    lv_obj_set_size(hub, 24, 24);
    lv_obj_set_pos(hub, 228, 134);
    lv_obj_set_style_radius(hub, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(hub, lv_color_hex(0x3a3a3a), 0);
    lv_obj_set_style_border_width(hub, 0, 0);
    lv_obj_set_style_pad_all(hub, 0, 0);
    lv_obj_remove_flag(hub, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_remove_flag(hub, LV_OBJ_FLAG_SCROLLABLE);

    // Marteau fixe -- rectangle 8x26 a 12h, couleur doree (pas d'arc, pas de rotation)
    lv_obj_t *hammer = lv_obj_create(scr_ss_game);
    lv_obj_set_size(hammer, 8, 26);
    lv_obj_set_pos(hammer, 236, 30);
    lv_obj_set_style_bg_color(hammer, lv_color_hex(0xC8860A), 0);
    lv_obj_set_style_border_width(hammer, 0, 0);
    lv_obj_set_style_radius(hammer, 0, 0);
    lv_obj_set_style_pad_all(hammer, 0, 0);
    lv_obj_remove_flag(hammer, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_remove_flag(hammer, LV_OBJ_FLAG_SCROLLABLE);

    // Label de statut (instruction en bas de l'ecran, sous le barillet)
    lbl_status = lv_label_create(scr_ss_game);
    lv_label_set_text(lbl_status, "ARMEZ - faites tourner l'aimant");
    lv_obj_set_style_text_font(lbl_status, FONT14, 0);
    lv_obj_set_style_text_color(lbl_status, lv_color_hex(0x888888), 0);
    lv_obj_align(lbl_status, LV_ALIGN_BOTTOM_MID, 0, -6);

    // Label d'erreur capteur (masque par defaut, active par SS_SetSensorError)
    lbl_ss_sensor_error = lv_label_create(scr_ss_game);
    lv_label_set_text(lbl_ss_sensor_error, "CAPTEUR NON DETECTE");
    lv_obj_set_style_text_font(lbl_ss_sensor_error, FONT14, 0);
    lv_obj_set_style_text_color(lbl_ss_sensor_error, lv_color_hex(0xFF4444), 0);
    lv_obj_align(lbl_ss_sensor_error, LV_ALIGN_CENTER, 0, 0);
    lv_obj_add_flag(lbl_ss_sensor_error, LV_OBJ_FLAG_HIDDEN);

    // Overlay de resultat -- couvre tout l'ecran, masque par defaut
    overlay = lv_obj_create(scr_ss_game);
    lv_obj_set_size(overlay, 480, 272);
    lv_obj_set_pos(overlay, 0, 0);
    lv_obj_set_style_radius(overlay, 0, 0);
    lv_obj_set_style_border_width(overlay, 0, 0);
    lv_obj_set_style_bg_color(overlay, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(overlay, LV_OPA_80, 0);
    lv_obj_set_style_pad_all(overlay, 0, 0);
    lv_obj_remove_flag(overlay, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(overlay, LV_OBJ_FLAG_HIDDEN);

    lbl_overlay = lv_label_create(overlay);
    lv_label_set_text(lbl_overlay, "");
    lv_obj_set_style_text_font(lbl_overlay, FONT14, 0);
    lv_obj_set_style_text_color(lbl_overlay, lv_color_hex(0xFFFFFF), 0);
    lv_label_set_long_mode(lbl_overlay, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(lbl_overlay, 380);
    lv_obj_align(lbl_overlay, LV_ALIGN_CENTER, 0, -30);

    // Bouton REJOUER
    btn_rejouer = lv_button_create(overlay);
    lv_obj_set_size(btn_rejouer, 140, 36);
    lv_obj_align(btn_rejouer, LV_ALIGN_CENTER, 0, 30);
    lv_obj_set_style_bg_color(btn_rejouer, lv_color_hex(0x1A1A1A), 0);
    lv_obj_set_style_border_width(btn_rejouer, 1, 0);
    lv_obj_set_style_border_color(btn_rejouer, lv_color_hex(0x8B6914), 0);
    lv_obj_add_flag(btn_rejouer, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_event_cb(btn_rejouer, [](lv_event_t *) {
        SS_Reset();
        lv_scr_load(scr_ss_game);
    }, LV_EVENT_CLICKED, NULL);
    lv_obj_t *lbl_rj = lv_label_create(btn_rejouer);
    lv_label_set_text(lbl_rj, "REJOUER");
    lv_obj_set_style_text_font(lbl_rj, FONT14, 0);
    lv_obj_center(lbl_rj);

    // Bouton MENU (fin de partie)
    btn_menu_end = lv_button_create(overlay);
    lv_obj_set_size(btn_menu_end, 100, 36);
    lv_obj_align(btn_menu_end, LV_ALIGN_CENTER, 0, 80);
    lv_obj_set_style_bg_color(btn_menu_end, lv_color_hex(0x333333), 0);
    lv_obj_set_style_border_width(btn_menu_end, 1, 0);
    lv_obj_set_style_border_color(btn_menu_end, lv_color_hex(0x666666), 0);
    lv_obj_add_flag(btn_menu_end, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_event_cb(btn_menu_end, [](lv_event_t *) {
        SS_Reset();
        activeGame = MENU;
        lv_scr_load(scr_menu);
    }, LV_EVENT_CLICKED, NULL);
    lv_obj_t *lbl_me = lv_label_create(btn_menu_end);
    lv_label_set_text(lbl_me, "MENU");
    lv_obj_set_style_text_font(lbl_me, FONT14, 0);
    lv_obj_center(lbl_me);
}

// Charge l'ecran d'accueil S&S (init paresseuse des deux ecrans)
// Appelee depuis btn_spinSurvive_cb dans main.cpp (contexte lvglTask)
void SS_ShowHome(void)
{
    if (!scr_ss_home) {
        SS_CreateHomeScreen();
        SS_CreateGameScreen();
    }
    current_round = 1;
    ss_state      = SS_HOME;
    theta         = 0.0f;
    arm_ticks     = 0;
    stop_ticks    = 0;
    if (pending_timer) { lv_timer_delete(pending_timer); pending_timer = NULL; }
    lv_scr_load(scr_ss_home);
}

// Met a jour le jeu a chaque tick de 30 ms (appelee depuis myTask() dans lvglLock)
// angleDeg       : angle courant du capteur (0-360 deg)
// speedDegPerSec : vitesse angulaire absolue en deg/s (calculee avec fabsf dans myTask)
void SS_Update(float angleDeg, float speedDegPerSec)
{
    switch (ss_state) {

    case SS_HOME:
        break;

    case SS_WAIT_SPIN:
        // Barillet miroir du capteur ; armement apres 3 ticks consecutifs > 360 deg/s
        theta = angleDeg;
        repositionChambers(theta);
        if (speedDegPerSec > 360.0f) {
            arm_ticks++;
        } else {
            arm_ticks = 0;
        }
        if (arm_ticks >= 3) {
            ss_state = SS_SPINNING;
            lv_label_set_text(lbl_status, "LANCE - laissez ralentir...");
        }
        break;

    case SS_SPINNING:
        // Toujours miroir du capteur ; arret detecte apres 10 ticks < 10 deg/s (300 ms)
        theta = angleDeg;
        repositionChambers(theta);
        if (speedDegPerSec < 10.0f) {
            stop_ticks++;
        } else {
            stop_ticks = 0;
        }
        if (stop_ticks >= 10) {
            // Gele l'angle et identifie la chambre face au marteau (12h = 270 deg en trigo)
            sel = ((int)lroundf((270.0f - theta) / 60.0f)) % 6;
            if (sel < 0) sel += 6;
            lv_obj_set_style_border_color(chambers[sel], lv_color_hex(0xC8860A), 0);
            lv_label_set_text(lbl_status, "CLIC ...");
            ss_state      = SS_SUSPENSE;
            pending_timer = lv_timer_create(suspense_cb, 700, NULL);
        }
        break;

    // Etats passifs : pilotes par les timers et boutons LVGL (pas de lock ici)
    case SS_SUSPENSE:
    case SS_RESULT:
    case SS_DEAD:
    case SS_VICTORY:
        break;
    }
}

// Affiche ou masque le label "CAPTEUR NON DETECTE" sur l'ecran de jeu
void SS_SetSensorError(bool visible)
{
    if (!lbl_ss_sensor_error) return;
    if (visible) lv_obj_remove_flag(lbl_ss_sensor_error, LV_OBJ_FLAG_HIDDEN);
    else         lv_obj_add_flag(lbl_ss_sensor_error, LV_OBJ_FLAG_HIDDEN);
}
