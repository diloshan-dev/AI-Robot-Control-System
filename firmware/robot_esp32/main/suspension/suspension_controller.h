#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    SUSPENSION_CORNER_FRONT_LEFT = 0,
    SUSPENSION_CORNER_FRONT_RIGHT = 1,
    SUSPENSION_CORNER_BACK_LEFT = 2,
    SUSPENSION_CORNER_BACK_RIGHT = 3,
} suspension_corner_id_t;

void suspension_controller_init(void);
void suspension_set_corner(suspension_corner_id_t corner, int angle_deg);
void suspension_level_all(int height_deg);
void suspension_stand_tall(void);
void suspension_crouch(void);
void suspension_tilt_correct(float pitch_deg, float roll_deg);
void suspension_hold(void);
bool suspension_auto_level_enabled(void);
void suspension_auto_level_set(bool enabled);
