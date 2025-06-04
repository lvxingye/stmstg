#ifndef _game_h_
#define _game_h_

#include "arm_math.h"
#include "arm_math_types.h"
#include "u8g2.h"
#include <stdint.h>

//
#define DANMUKU_POOL_SIZE 10
#define BULLET_POOL_SIZE 700
#define ENEMY_RAD   6
#define PLAYER_RAD  2
#define BULLET_RAD 1
#define DANMUKU_COUNT 1
#define ENEMY_BULLET_DAMAGE 1
#define PLAYER_BULLET_DAMAGE 0.7F
#define STAGE_STARTED 0
#define STAGE_START 1
#define STAGE_CLEAR 2
#define STAGE_TIME_UP 3
#define STAGE_CLEAR_INIT 4
#define STAGE_TIME_UP_INIT 5
#define STAGE_START_INIT 6
#define STAGE_END 7
#define STAGE_END_INIT 8
// ui layout
#define GAME_SECTION_START_X 0
#define GAME_SECTION_END_X 80
#define GAME_SECTION_START_Y 5
#define GAME_SECTION_END_Y 58
#define PROGRESS_SECTION_START_X GAME_SECTION_START_X
#define PROGRESS_SECTION_END_X GAME_SECTION_END_X
#define PROGRESS_SECTION_START_Y 0
#define PROGRESS_SECTION_END_Y (GAME_SECTION_START_Y - 1)
#define INFO_SECTION_START_X (GAME_SECTION_END_X + 1)
#define INFO_SECTION_END_X 127
#define INFO_SECTION_START_Y 0
#define INFO_SECTION_END_Y 63
#define HEALTH_SECTION_START_X GAME_SECTION_START_X
#define HEALTH_SECTION_END_X GAME_SECTION_END_X
#define HEALTH_SECTION_START_Y (GAME_SECTION_END_Y+1)
#define HEALTH_SECTION_END_Y 63


typedef struct bullet_struct {
    // float32_t volocity_vert, volocity_tang, accel_vert, accel_tang;
    float32_t shoot_time;
    float32_t damage;
    float32_t direction[2], pos[2],init_pos[2];
    uint8_t   owner;
    uint8_t (*pos_update_callback)(float32_t time,float32_t dir[2],float32_t pos_start[2],float32_t pos_out[2],uint8_t arg1,void* arg2);
} bullet_typedef;

typedef struct danmuku_struct {
    char*           name;
    bullet_typedef* bullets[500];
    float32_t       bullet_ang;
    float32_t       fire_intv, duration, cur_time;
    float32_t       translation_v, translation_a, ang_v, ang_a;
    float32_t (*centers)[2], (*trans_dir)[2];
    uint8_t  center_cnt, shots_per_fire;
    uint16_t bullet_cnt;
    bullet_typedef* (*bullet_init_callback)(uint8_t fire_cnt,void *param);
    uint8_t (*bullet_update_callback)(bullet_typedef* bullet,float32_t time,uint8_t arg1,void* arg2);
    void *param;
} danmuku_typedef;

typedef struct enemy_struct {
    char*            name;
    danmuku_typedef* cur_danmuku;
    float32_t        health;
    float32_t        pos[2];
    uint8_t (*danmuku_update_callback)(danmuku_typedef* danmuku,float32_t time,uint8_t arg1,void* arg2);
} enemy_typedef;

typedef struct player_struct {
    char*            name;
    danmuku_typedef* cur_danmuku;
    float32_t        health;
    float32_t        pos[2];
    float32_t        speed_fast;
    float32_t        speed_slow;
} player_typedef;

void game_init(void);
void fix_update(void);
void update(void);

#endif
