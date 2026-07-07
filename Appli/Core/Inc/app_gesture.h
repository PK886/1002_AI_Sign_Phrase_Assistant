/**
 ****************************************************************************************************
 * @file        app_gesture.h
 * @brief       Hand gesture semantic parser.
 ****************************************************************************************************
 */

#ifndef __APP_GESTURE_H
#define __APP_GESTURE_H

#include <stdint.h>
#include "ld.h"

typedef enum {
    APP_GESTURE_NONE = 0,
    APP_GESTURE_PINCH,
    APP_GESTURE_OK,
    APP_GESTURE_SWIPE_LEFT,
    APP_GESTURE_SWIPE_RIGHT,
    APP_GESTURE_SWIPE_UP,
    APP_GESTURE_SWIPE_DOWN,
    APP_GESTURE_ADJUST_UP,
    APP_GESTURE_ADJUST_DOWN,
    APP_GESTURE_OPEN_PALM,
    APP_GESTURE_FIST,
    APP_GESTURE_VICTORY,
    APP_GESTURE_THREE,
    APP_GESTURE_THUMB_UP,
    APP_GESTURE_THUMB_DOWN,
} app_gesture_type_t;

typedef enum {
    APP_CONTROL_NONE = 0,
    APP_CONTROL_SELECT,
    APP_CONTROL_RELEASE,
    APP_CONTROL_PREVIOUS,
    APP_CONTROL_NEXT,
    APP_CONTROL_SCROLL_UP,
    APP_CONTROL_SCROLL_DOWN,
    APP_CONTROL_VALUE_UP,
    APP_CONTROL_VALUE_DOWN,
    APP_CONTROL_ALL_ON,
    APP_CONTROL_ALL_OFF,
    APP_CONTROL_RECORD_TOGGLE,
    APP_CONTROL_SCENE_DEMO,
    APP_CONTROL_SCENE_DATASET,
    APP_CONTROL_SCENE_ENERGY,
    APP_CONTROL_SCENE_SAFE,
} app_control_cmd_t;

typedef struct {
    app_gesture_type_t gesture;
    app_control_cmd_t command;
    uint8_t is_pinching;
    float pinch_distance;
    float motion_x;
    float motion_y;
} app_gesture_result_t;

typedef struct {
    uint8_t initialized;
    uint8_t is_pinching;
    uint32_t last_event_ms;
    uint32_t pinch_start_ms;
    uint32_t pose_start_ms;
    app_gesture_type_t pose_candidate;
    app_gesture_type_t pose_latched;
    ld_point_t ref_point;
    ld_point_t prev_point;
} app_gesture_state_t;

typedef enum {
    APP_SIGN_NONE = 0,
    APP_SIGN_G01_HELLO,
    APP_SIGN_G02_THANKS,
    APP_SIGN_G03_NEED_HELP,
    APP_SIGN_G04_UNCOMFORTABLE,
    APP_SIGN_G05_WAIT,
    APP_SIGN_G06_WATER,
    APP_SIGN_G07_CONTACT_FAMILY,
    APP_SIGN_G08_YES,
    APP_SIGN_G09_NO,
    APP_SIGN_G10_EMERGENCY,
    APP_SIGN_COUNT
} app_sign_phrase_t;

typedef struct {
    app_sign_phrase_t candidate;
    app_sign_phrase_t phrase;
    uint8_t confirmed;
    uint32_t hold_ms;
    uint8_t progress;
    uint8_t cooldown;
} app_sign_result_t;

typedef struct {
    app_sign_phrase_t candidate;
    app_sign_phrase_t last_phrase;
    app_sign_phrase_t latched;
    uint32_t candidate_since_ms;
    uint32_t last_confirm_ms;
} app_sign_state_t;

void app_gesture_init(app_gesture_state_t *state);
void app_gesture_reset(app_gesture_state_t *state);
app_gesture_result_t app_gesture_update(app_gesture_state_t *state,
                                        const ld_point_t landmarks[LD_LANDMARKS_NB],
                                        uint32_t now_ms);
const char *app_gesture_name(app_gesture_type_t gesture);
const char *app_control_cmd_name(app_control_cmd_t command);

void app_sign_init(app_sign_state_t *state);
void app_sign_reset(app_sign_state_t *state);
app_sign_result_t app_sign_update(app_sign_state_t *state,
                                  const ld_point_t landmarks[LD_LANDMARKS_NB],
                                  uint32_t now_ms);
const char *app_sign_phrase_id(app_sign_phrase_t phrase);
const char *app_sign_phrase_text(app_sign_phrase_t phrase);
const char *app_sign_gesture_name(app_sign_phrase_t phrase);
uint32_t app_sign_confirm_hold_ms(void);
uint32_t app_sign_repeat_cooldown_ms(void);

#endif
