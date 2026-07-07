/**
 ****************************************************************************************************
 * @file        app_gesture.c
 * @brief       Hand landmark gesture semantic parser.
 ****************************************************************************************************
 */

#include "app_gesture.h"
#include <math.h>
#include <string.h>

#define GESTURE_EVENT_COOLDOWN_MS       350U
#define GESTURE_PINCH_HOLD_MS           140U
#define GESTURE_MIN_PALM_SCALE          20.0f
#define GESTURE_PINCH_ON_RATIO          0.42f
#define GESTURE_PINCH_OFF_RATIO         0.60f
#define GESTURE_SWIPE_RATIO             0.85f
#define GESTURE_ADJUST_RATIO            0.65f
#define GESTURE_POSE_HOLD_MS            520U
#define GESTURE_POSE_MOTION_RATIO       0.35f
#define GESTURE_FINGER_EXT_RATIO        1.08f
#define GESTURE_THUMB_EXT_RATIO         1.10f

#define SIGN_CONFIRM_HOLD_MS            1200U
#define SIGN_REPEAT_COOLDOWN_MS         1500U

#define GESTURE_FINGER_THUMB            0x01U
#define GESTURE_FINGER_INDEX            0x02U
#define GESTURE_FINGER_MIDDLE           0x04U
#define GESTURE_FINGER_RING             0x08U
#define GESTURE_FINGER_PINKY            0x10U

static float app_gesture_distance(ld_point_t a, ld_point_t b)
{
    float dx = a.x - b.x;
    float dy = a.y - b.y;

    return sqrtf(dx * dx + dy * dy);
}

static ld_point_t app_gesture_control_point(const ld_point_t landmarks[LD_LANDMARKS_NB])
{
    ld_point_t point;

    point.x = (landmarks[4].x + landmarks[8].x) * 0.5f;
    point.y = (landmarks[4].y + landmarks[8].y) * 0.5f;

    return point;
}

static float app_gesture_palm_scale(const ld_point_t landmarks[LD_LANDMARKS_NB])
{
    float scale = app_gesture_distance(landmarks[0], landmarks[9]);

    if (scale < GESTURE_MIN_PALM_SCALE)
    {
        scale = GESTURE_MIN_PALM_SCALE;
    }

    return scale;
}

static uint8_t app_gesture_finger_extended(const ld_point_t landmarks[LD_LANDMARKS_NB],
                                           uint8_t tip,
                                           uint8_t pip)
{
    float tip_distance = app_gesture_distance(landmarks[0], landmarks[tip]);
    float pip_distance = app_gesture_distance(landmarks[0], landmarks[pip]);

    return (uint8_t)(tip_distance > (pip_distance * GESTURE_FINGER_EXT_RATIO));
}

static uint8_t app_gesture_thumb_extended(const ld_point_t landmarks[LD_LANDMARKS_NB])
{
    float tip_distance = app_gesture_distance(landmarks[0], landmarks[4]);
    float mcp_distance = app_gesture_distance(landmarks[0], landmarks[2]);

    return (uint8_t)(tip_distance > (mcp_distance * GESTURE_THUMB_EXT_RATIO));
}

static uint8_t app_gesture_finger_mask(const ld_point_t landmarks[LD_LANDMARKS_NB])
{
    uint8_t mask = 0U;

    if (app_gesture_thumb_extended(landmarks) != 0U)
    {
        mask |= GESTURE_FINGER_THUMB;
    }

    if (app_gesture_finger_extended(landmarks, 8U, 6U) != 0U)
    {
        mask |= GESTURE_FINGER_INDEX;
    }

    if (app_gesture_finger_extended(landmarks, 12U, 10U) != 0U)
    {
        mask |= GESTURE_FINGER_MIDDLE;
    }

    if (app_gesture_finger_extended(landmarks, 16U, 14U) != 0U)
    {
        mask |= GESTURE_FINGER_RING;
    }

    if (app_gesture_finger_extended(landmarks, 20U, 18U) != 0U)
    {
        mask |= GESTURE_FINGER_PINKY;
    }

    return mask;
}

static app_gesture_type_t app_gesture_static_pose(const ld_point_t landmarks[LD_LANDMARKS_NB],
                                                  float pinch_distance,
                                                  float palm_scale)
{
    uint8_t mask = app_gesture_finger_mask(landmarks);
    uint8_t long_fingers = (uint8_t)(mask & (GESTURE_FINGER_INDEX |
                                            GESTURE_FINGER_MIDDLE |
                                            GESTURE_FINGER_RING |
                                            GESTURE_FINGER_PINKY));
    uint8_t thumb_only = (uint8_t)((mask & GESTURE_FINGER_THUMB) != 0U);
    float thumb_dy = landmarks[4].y - landmarks[2].y;
    float thumb_dx = fabsf(landmarks[4].x - landmarks[2].x);

    if ((pinch_distance < (GESTURE_PINCH_ON_RATIO * palm_scale)) &&
        ((mask & (GESTURE_FINGER_MIDDLE | GESTURE_FINGER_RING | GESTURE_FINGER_PINKY)) ==
         (GESTURE_FINGER_MIDDLE | GESTURE_FINGER_RING | GESTURE_FINGER_PINKY)))
    {
        return APP_GESTURE_OK;
    }

    if (long_fingers == (GESTURE_FINGER_INDEX |
                         GESTURE_FINGER_MIDDLE |
                         GESTURE_FINGER_RING |
                         GESTURE_FINGER_PINKY))
    {
        return APP_GESTURE_OPEN_PALM;
    }

    if ((long_fingers == 0U) && (thumb_only == 0U))
    {
        return APP_GESTURE_FIST;
    }

    if (long_fingers == (GESTURE_FINGER_INDEX | GESTURE_FINGER_MIDDLE))
    {
        return APP_GESTURE_VICTORY;
    }

    if (long_fingers == (GESTURE_FINGER_INDEX | GESTURE_FINGER_MIDDLE | GESTURE_FINGER_RING))
    {
        return APP_GESTURE_THREE;
    }

    if ((long_fingers == 0U) && (thumb_only != 0U) && (fabsf(thumb_dy) > (thumb_dx * 1.25f)))
    {
        if (thumb_dy < 0.0f)
        {
            return APP_GESTURE_THUMB_UP;
        }

        return APP_GESTURE_THUMB_DOWN;
    }

    return APP_GESTURE_NONE;
}

static app_control_cmd_t app_gesture_pose_command(app_gesture_type_t gesture)
{
    switch (gesture)
    {
    case APP_GESTURE_OK:
        return APP_CONTROL_SELECT;
    case APP_GESTURE_OPEN_PALM:
        return APP_CONTROL_ALL_ON;
    case APP_GESTURE_FIST:
        return APP_CONTROL_ALL_OFF;
    case APP_GESTURE_VICTORY:
        return APP_CONTROL_RECORD_TOGGLE;
    case APP_GESTURE_THREE:
        return APP_CONTROL_SCENE_ENERGY;
    case APP_GESTURE_THUMB_UP:
        return APP_CONTROL_VALUE_UP;
    case APP_GESTURE_THUMB_DOWN:
        return APP_CONTROL_VALUE_DOWN;
    default:
        return APP_CONTROL_NONE;
    }
}

void app_gesture_init(app_gesture_state_t *state)
{
    memset(state, 0, sizeof(*state));
}

void app_gesture_reset(app_gesture_state_t *state)
{
    state->initialized = 0;
    state->is_pinching = 0;
    state->pinch_start_ms = 0;
    state->pose_start_ms = 0;
    state->pose_candidate = APP_GESTURE_NONE;
    state->pose_latched = APP_GESTURE_NONE;
}

app_gesture_result_t app_gesture_update(app_gesture_state_t *state,
                                        const ld_point_t landmarks[LD_LANDMARKS_NB],
                                        uint32_t now_ms)
{
    app_gesture_result_t result;
    ld_point_t point = app_gesture_control_point(landmarks);
    float palm_scale = app_gesture_palm_scale(landmarks);
    float pinch_distance = app_gesture_distance(landmarks[4], landmarks[8]);
    float pinch_on = GESTURE_PINCH_ON_RATIO * palm_scale;
    float pinch_off = GESTURE_PINCH_OFF_RATIO * palm_scale;
    float dx;
    float dy;
    float frame_motion;
    app_gesture_type_t pose;
    uint8_t can_emit;

    memset(&result, 0, sizeof(result));
    result.pinch_distance = pinch_distance;

    if (state->initialized == 0)
    {
        state->initialized = 1;
        state->ref_point = point;
        state->prev_point = point;
    }

    if (state->is_pinching == 0)
    {
        if (pinch_distance < pinch_on)
        {
            state->is_pinching = 1;
            state->pinch_start_ms = now_ms;
            state->ref_point = point;
            if (app_gesture_static_pose(landmarks, pinch_distance, palm_scale) == APP_GESTURE_OK)
            {
                result.gesture = APP_GESTURE_OK;
            }
            else
            {
                result.gesture = APP_GESTURE_PINCH;
            }
            result.command = APP_CONTROL_SELECT;
        }
    }
    else if (pinch_distance > pinch_off)
    {
        state->is_pinching = 0;
        state->ref_point = point;
        result.command = APP_CONTROL_RELEASE;
    }

    dx = point.x - state->ref_point.x;
    dy = point.y - state->ref_point.y;
    frame_motion = app_gesture_distance(point, state->prev_point);
    result.motion_x = dx;
    result.motion_y = dy;
    result.is_pinching = state->is_pinching;

    can_emit = (uint8_t)((now_ms - state->last_event_ms) > GESTURE_EVENT_COOLDOWN_MS);

    if ((state->is_pinching != 0) &&
        ((now_ms - state->pinch_start_ms) > GESTURE_PINCH_HOLD_MS) &&
        (can_emit != 0))
    {
        if (dy < -GESTURE_ADJUST_RATIO * palm_scale)
        {
            result.gesture = APP_GESTURE_ADJUST_UP;
            result.command = APP_CONTROL_VALUE_UP;
            state->ref_point = point;
            state->last_event_ms = now_ms;
        }
        else if (dy > GESTURE_ADJUST_RATIO * palm_scale)
        {
            result.gesture = APP_GESTURE_ADJUST_DOWN;
            result.command = APP_CONTROL_VALUE_DOWN;
            state->ref_point = point;
            state->last_event_ms = now_ms;
        }
    }
    else if ((state->is_pinching == 0) && (can_emit != 0))
    {
        if (dx > GESTURE_SWIPE_RATIO * palm_scale)
        {
            result.gesture = APP_GESTURE_SWIPE_RIGHT;
            result.command = APP_CONTROL_NEXT;
            state->ref_point = point;
            state->last_event_ms = now_ms;
        }
        else if (dx < -GESTURE_SWIPE_RATIO * palm_scale)
        {
            result.gesture = APP_GESTURE_SWIPE_LEFT;
            result.command = APP_CONTROL_PREVIOUS;
            state->ref_point = point;
            state->last_event_ms = now_ms;
        }
        else if (dy < -GESTURE_SWIPE_RATIO * palm_scale)
        {
            result.gesture = APP_GESTURE_SWIPE_UP;
            result.command = APP_CONTROL_SCROLL_UP;
            state->ref_point = point;
            state->last_event_ms = now_ms;
        }
        else if (dy > GESTURE_SWIPE_RATIO * palm_scale)
        {
            result.gesture = APP_GESTURE_SWIPE_DOWN;
            result.command = APP_CONTROL_SCROLL_DOWN;
            state->ref_point = point;
            state->last_event_ms = now_ms;
        }
    }

    if ((result.command == APP_CONTROL_NONE) &&
        (state->is_pinching == 0U) &&
        (can_emit != 0U) &&
        (frame_motion < (GESTURE_POSE_MOTION_RATIO * palm_scale)))
    {
        pose = app_gesture_static_pose(landmarks, pinch_distance, palm_scale);

        if (pose == APP_GESTURE_NONE)
        {
            state->pose_candidate = APP_GESTURE_NONE;
            state->pose_latched = APP_GESTURE_NONE;
            state->pose_start_ms = now_ms;
        }
        else
        {
            if (pose != state->pose_candidate)
            {
                state->pose_candidate = pose;
                state->pose_start_ms = now_ms;
            }
            else if ((pose != state->pose_latched) &&
                     ((now_ms - state->pose_start_ms) > GESTURE_POSE_HOLD_MS))
            {
                result.gesture = pose;
                result.command = app_gesture_pose_command(pose);
                state->ref_point = point;
                state->last_event_ms = now_ms;
                state->pose_start_ms = now_ms;
                state->pose_latched = pose;
            }
        }
    }
    else if (result.command != APP_CONTROL_NONE)
    {
        state->pose_candidate = APP_GESTURE_NONE;
        state->pose_latched = APP_GESTURE_NONE;
        state->pose_start_ms = now_ms;
    }

    state->prev_point = point;

    return result;
}

const char *app_gesture_name(app_gesture_type_t gesture)
{
    switch (gesture)
    {
    case APP_GESTURE_PINCH:
        return "Pinch";
    case APP_GESTURE_OK:
        return "OK";
    case APP_GESTURE_SWIPE_LEFT:
        return "Swipe L";
    case APP_GESTURE_SWIPE_RIGHT:
        return "Swipe R";
    case APP_GESTURE_SWIPE_UP:
        return "Swipe U";
    case APP_GESTURE_SWIPE_DOWN:
        return "Swipe D";
    case APP_GESTURE_ADJUST_UP:
        return "Adjust +";
    case APP_GESTURE_ADJUST_DOWN:
        return "Adjust -";
    case APP_GESTURE_OPEN_PALM:
        return "Palm";
    case APP_GESTURE_FIST:
        return "Fist";
    case APP_GESTURE_VICTORY:
        return "Victory";
    case APP_GESTURE_THREE:
        return "Three";
    case APP_GESTURE_THUMB_UP:
        return "Thumb +";
    case APP_GESTURE_THUMB_DOWN:
        return "Thumb -";
    default:
        return "None";
    }
}

const char *app_control_cmd_name(app_control_cmd_t command)
{
    switch (command)
    {
    case APP_CONTROL_SELECT:
        return "Select";
    case APP_CONTROL_RELEASE:
        return "Release";
    case APP_CONTROL_PREVIOUS:
        return "Previous";
    case APP_CONTROL_NEXT:
        return "Next";
    case APP_CONTROL_SCROLL_UP:
        return "Scroll Up";
    case APP_CONTROL_SCROLL_DOWN:
        return "Scroll Down";
    case APP_CONTROL_VALUE_UP:
        return "Value +";
    case APP_CONTROL_VALUE_DOWN:
        return "Value -";
    case APP_CONTROL_ALL_ON:
        return "All On";
    case APP_CONTROL_ALL_OFF:
        return "All Off";
    case APP_CONTROL_RECORD_TOGGLE:
        return "Record";
    case APP_CONTROL_SCENE_DEMO:
        return "Demo";
    case APP_CONTROL_SCENE_DATASET:
        return "Dataset";
    case APP_CONTROL_SCENE_ENERGY:
        return "Energy";
    case APP_CONTROL_SCENE_SAFE:
        return "Safe";
    default:
        return "Idle";
    }
}

static app_sign_phrase_t app_sign_detect_phrase(const ld_point_t landmarks[LD_LANDMARKS_NB])
{
    float palm_scale = app_gesture_palm_scale(landmarks);
    float pinch_distance = app_gesture_distance(landmarks[4], landmarks[8]);
    uint8_t mask = app_gesture_finger_mask(landmarks);
    uint8_t thumb = (uint8_t)((mask & GESTURE_FINGER_THUMB) != 0U);
    uint8_t long_fingers = (uint8_t)(mask & (GESTURE_FINGER_INDEX |
                                            GESTURE_FINGER_MIDDLE |
                                            GESTURE_FINGER_RING |
                                            GESTURE_FINGER_PINKY));
    float thumb_dy = landmarks[4].y - landmarks[2].y;
    float thumb_dx = fabsf(landmarks[4].x - landmarks[2].x);
    uint8_t thumb_vertical = (uint8_t)(fabsf(thumb_dy) > (thumb_dx * 1.15f));
    uint8_t ok_pinch = (uint8_t)(pinch_distance < (GESTURE_PINCH_ON_RATIO * palm_scale));

    if ((thumb != 0U) && (long_fingers == 0U) && (thumb_vertical != 0U))
    {
        if (thumb_dy < 0.0f)
        {
            return APP_SIGN_G08_YES;
        }

        return APP_SIGN_G04_UNCOMFORTABLE;
    }

    if ((thumb != 0U) &&
        ((long_fingers & GESTURE_FINGER_PINKY) != 0U) &&
        ((long_fingers & (GESTURE_FINGER_INDEX | GESTURE_FINGER_MIDDLE | GESTURE_FINGER_RING)) == 0U))
    {
        return APP_SIGN_G07_CONTACT_FAMILY;
    }

    if ((ok_pinch != 0U) &&
        ((long_fingers & (GESTURE_FINGER_MIDDLE | GESTURE_FINGER_RING | GESTURE_FINGER_PINKY)) ==
         (GESTURE_FINGER_MIDDLE | GESTURE_FINGER_RING | GESTURE_FINGER_PINKY)))
    {
        return APP_SIGN_G02_THANKS;
    }

    if ((thumb != 0U) &&
        (long_fingers == (GESTURE_FINGER_INDEX | GESTURE_FINGER_MIDDLE |
                          GESTURE_FINGER_RING | GESTURE_FINGER_PINKY)))
    {
        return APP_SIGN_G01_HELLO;
    }

    if ((thumb == 0U) &&
        (long_fingers == (GESTURE_FINGER_INDEX | GESTURE_FINGER_MIDDLE |
                          GESTURE_FINGER_RING | GESTURE_FINGER_PINKY)))
    {
        return APP_SIGN_G10_EMERGENCY;
    }

    if (long_fingers == 0U)
    {
        return APP_SIGN_G03_NEED_HELP;
    }

    if (long_fingers == GESTURE_FINGER_INDEX)
    {
        return APP_SIGN_G05_WAIT;
    }

    if (long_fingers == (GESTURE_FINGER_INDEX | GESTURE_FINGER_MIDDLE))
    {
        return APP_SIGN_G09_NO;
    }

    if (long_fingers == (GESTURE_FINGER_INDEX | GESTURE_FINGER_MIDDLE | GESTURE_FINGER_RING))
    {
        return APP_SIGN_G06_WATER;
    }

    return APP_SIGN_NONE;
}

void app_sign_init(app_sign_state_t *state)
{
    memset(state, 0, sizeof(*state));
}

void app_sign_reset(app_sign_state_t *state)
{
    state->candidate = APP_SIGN_NONE;
    state->latched = APP_SIGN_NONE;
    state->candidate_since_ms = 0U;
}

app_sign_result_t app_sign_update(app_sign_state_t *state,
                                  const ld_point_t landmarks[LD_LANDMARKS_NB],
                                  uint32_t now_ms)
{
    app_sign_result_t result;
    app_sign_phrase_t candidate = app_sign_detect_phrase(landmarks);
    uint32_t hold_ms = 0U;
    uint8_t cooldown_clear;

    memset(&result, 0, sizeof(result));
    result.candidate = candidate;
    result.phrase = state->last_phrase;

    if (candidate == APP_SIGN_NONE)
    {
        state->candidate = APP_SIGN_NONE;
        state->latched = APP_SIGN_NONE;
        state->candidate_since_ms = 0U;
        return result;
    }

    if (candidate != state->candidate)
    {
        state->candidate = candidate;
        state->latched = APP_SIGN_NONE;
        state->candidate_since_ms = now_ms;
    }
    else
    {
        hold_ms = now_ms - state->candidate_since_ms;
    }

    if (state->candidate_since_ms == 0U)
    {
        state->candidate_since_ms = now_ms;
        hold_ms = 0U;
    }

    if (hold_ms > SIGN_CONFIRM_HOLD_MS)
    {
        hold_ms = SIGN_CONFIRM_HOLD_MS;
    }

    result.hold_ms = hold_ms;
    result.progress = (uint8_t)((hold_ms * 100U) / SIGN_CONFIRM_HOLD_MS);

    cooldown_clear = (uint8_t)((state->last_confirm_ms == 0U) ||
                               ((now_ms - state->last_confirm_ms) >= SIGN_REPEAT_COOLDOWN_MS));
    result.cooldown = (uint8_t)(cooldown_clear == 0U);

    if ((hold_ms >= SIGN_CONFIRM_HOLD_MS) &&
        (state->latched != candidate) &&
        (cooldown_clear != 0U))
    {
        state->last_phrase = candidate;
        state->last_confirm_ms = now_ms;
        state->latched = candidate;
        result.phrase = candidate;
        result.confirmed = 1U;
        result.cooldown = 0U;
    }

    return result;
}

const char *app_sign_phrase_id(app_sign_phrase_t phrase)
{
    switch (phrase)
    {
    case APP_SIGN_G01_HELLO:
        return "G01";
    case APP_SIGN_G02_THANKS:
        return "G02";
    case APP_SIGN_G03_NEED_HELP:
        return "G03";
    case APP_SIGN_G04_UNCOMFORTABLE:
        return "G04";
    case APP_SIGN_G05_WAIT:
        return "G05";
    case APP_SIGN_G06_WATER:
        return "G06";
    case APP_SIGN_G07_CONTACT_FAMILY:
        return "G07";
    case APP_SIGN_G08_YES:
        return "G08";
    case APP_SIGN_G09_NO:
        return "G09";
    case APP_SIGN_G10_EMERGENCY:
        return "G10";
    default:
        return "--";
    }
}

const char *app_sign_phrase_text(app_sign_phrase_t phrase)
{
    switch (phrase)
    {
    case APP_SIGN_G01_HELLO:
        return "你好";
    case APP_SIGN_G02_THANKS:
        return "谢谢";
    case APP_SIGN_G03_NEED_HELP:
        return "我需要帮助";
    case APP_SIGN_G04_UNCOMFORTABLE:
        return "我不舒服";
    case APP_SIGN_G05_WAIT:
        return "请稍等";
    case APP_SIGN_G06_WATER:
        return "我想喝水";
    case APP_SIGN_G07_CONTACT_FAMILY:
        return "请联系家人";
    case APP_SIGN_G08_YES:
        return "是的";
    case APP_SIGN_G09_NO:
        return "不是";
    case APP_SIGN_G10_EMERGENCY:
        return "紧急情况，请帮忙";
    default:
        return "未确认";
    }
}

const char *app_sign_gesture_name(app_sign_phrase_t phrase)
{
    switch (phrase)
    {
    case APP_SIGN_G01_HELLO:
        return "Open Palm";
    case APP_SIGN_G02_THANKS:
        return "OK Pinch";
    case APP_SIGN_G03_NEED_HELP:
        return "Fist";
    case APP_SIGN_G04_UNCOMFORTABLE:
        return "Thumb Down";
    case APP_SIGN_G05_WAIT:
        return "Index One";
    case APP_SIGN_G06_WATER:
        return "Three Fingers";
    case APP_SIGN_G07_CONTACT_FAMILY:
        return "Phone Hand";
    case APP_SIGN_G08_YES:
        return "Thumb Up";
    case APP_SIGN_G09_NO:
        return "Victory";
    case APP_SIGN_G10_EMERGENCY:
        return "Four Fingers";
    default:
        return "None";
    }
}

uint32_t app_sign_confirm_hold_ms(void)
{
    return SIGN_CONFIRM_HOLD_MS;
}

uint32_t app_sign_repeat_cooldown_ms(void)
{
    return SIGN_REPEAT_COOLDOWN_MS;
}
