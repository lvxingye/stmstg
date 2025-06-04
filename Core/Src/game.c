#include "game.h"
#include "arm_math_types.h"
#include "dsp/fast_math_functions.h"
#include "main.h"
#include "stm32f4xx_hal_def.h"
#include "stm32f4xx_hal_tim.h"
#include "u8g2.h"
#include "u8x8.h"
#include "usb_host.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// #define sqrt_2_div_2 0.7071067811865475f

extern I2C_HandleTypeDef hi2c1;
extern DMA_HandleTypeDef hdma_i2c1_tx;
extern TIM_HandleTypeDef htim6;
// extern void MX_I2C1_Init(void);

u8g2_t          dsp;
enemy_typedef   enemy;
player_typedef  player;
float32_t       danmuku_time                         = 0.0f;
float32_t       game_time                            = 0.0f;
uint8_t         key_codes                            = 0;
bullet_typedef  bullet_pool[BULLET_POOL_SIZE]        = {0};
uint8_t         bullet_pool_flag[BULLET_POOL_SIZE]   = {0};
danmuku_typedef danmuku_pool[DANMUKU_POOL_SIZE]      = {0};
uint8_t         danmuku_pool_flag[DANMUKU_POOL_SIZE] = {0};
uint16_t        danmuku_finish_cnt                   = 0;
uint16_t        score                                = 0;
uint8_t is_stage_finished = STAGE_START_INIT;   // 0:not finished; 1: stage clear; 2: time up

// private functions
uint8_t dsp_hw_iic_msg_callback(u8x8_t* u8x8, uint8_t msg, uint8_t arg_int, void* arg_ptr);
uint8_t dsp_gpio_delay_msg_callback(u8x8_t* u8x8, uint8_t msg, uint8_t arg_int, void* arg_ptr);
uint8_t is_inter_game_section(u8g2_t* u8g2, int16_t x0, int16_t y0, int16_t x1, int16_t y1);
bullet_typedef*  get_new_bullet(void);
void             remove_bullet(bullet_typedef* _bullet);
danmuku_typedef* get_new_danmuku(uint8_t is_random);
uint8_t          is_contact(float32_t pos1[2], float32_t pos2[2]);
void             clear_danmuku(uint8_t type);

// game functions
// enemy danmuku init
void danmuku_init(danmuku_typedef* danmuku) {
    danmuku[0].name           = "境符「波与粒的境界」";
    danmuku[0].centers        = NULL;
    danmuku[0].center_cnt     = 1;
    danmuku[0].fire_intv      = 0.1f;
    danmuku[0].shots_per_fire = 8;
    danmuku[0].bullet_ang     = 45.0f * PI / 180.0f;
    danmuku[0].ang_a          = -150 * PI / 180.0f;
    danmuku[0].ang_v          = 0;
    danmuku[0].translation_a  = 0;
    danmuku[0].translation_v  = 0;
    danmuku[0].duration       = 60.0f;
    danmuku[0].cur_time       = 0;
    danmuku[0].bullet_cnt     = 0;
    danmuku[0].trans_dir      = NULL;
    memset((bullet_typedef*)(danmuku->bullets), 0x00, 500 * sizeof(bullet_typedef*));
}
void enemy_init(enemy_typedef* enemy) {
    enemy->name        = "博丽灵梦";
    enemy->pos[0]      = 40;
    enemy->pos[1]      = 15;
    enemy->health      = 100.0f;
    enemy->cur_danmuku = danmuku_pool + 0;
}
void player_init(player_typedef* player) {
    danmuku_typedef* danmuku = (danmuku_typedef*)malloc(sizeof(danmuku_typedef));
    float32_t(*center)[2]    = (float32_t(*)[2])malloc(3 * sizeof(float32_t[2]));

    player->name       = "雾雨魔理沙";
    player->speed_fast = 40.0f;
    player->speed_slow = 15.0f;
    player->pos[0]     = 40;
    player->pos[1]     = 53;
    player->health     = 100.0f;

    center[0][0] = player->pos[0];
    center[0][1] = player->pos[1] - 5.0f;
    center[1][0] = player->pos[0] - 3.0f;
    center[1][1] = player->pos[1] - 5.0f;
    center[2][0] = player->pos[0] + 3.0f;
    center[2][1] = player->pos[1] - 5.0f;

    danmuku->name           = "?";
    danmuku->centers        = center;
    danmuku->center_cnt     = 3;
    danmuku->fire_intv      = 0.2f;
    danmuku->shots_per_fire = 1;
    danmuku->bullet_ang     = 0.0f;
    danmuku->ang_a          = 0.0f;
    danmuku->ang_v          = 0;
    danmuku->translation_a  = 0;
    danmuku->translation_v  = 0.0f;
    danmuku->duration       = 0.0f;
    danmuku->cur_time       = 0;
    danmuku->bullet_cnt     = 0;
    danmuku->trans_dir      = NULL;
    memset((bullet_typedef*)(danmuku->bullets), 0x00, 500 * sizeof(bullet_typedef*));

    player->cur_danmuku = danmuku;

    // return;
}
void game_init(void) {
    u8g2_Setup_ssd1306_i2c_128x64_noname_f(
        &dsp, U8G2_R0, u8x8_byte_sw_i2c, dsp_gpio_delay_msg_callback);
    u8g2_InitDisplay(&dsp);       // 根据所选的芯片进行初始化工作，初始化完成后，显示器处于关闭状态
    u8g2_SetPowerSave(&dsp, 0);   // 打开显示器
    u8g2_SetFontPosTop(&dsp);
    u8g2_ClearBuffer(&dsp);
    u8g2_SendBuffer(&dsp);
    danmuku_init(danmuku_pool);
    enemy_init(&enemy);
    player_init(&player);
    HAL_TIM_Base_Start_IT(&htim6);
}

void fix_update(void) {
    static float32_t shoot_time_enemy = 0.0f, shoot_time_player = 0.0f, d_angle = 0.0f;
    bullet_typedef * bullet               = NULL, *_bullet;
    float32_t        normalization_coef   = 0.0f;
    float32_t        player_directions[2] = {0.0f, 0.0f};
    float32_t(*center)[2]                 = player.cur_danmuku->centers;
    uint8_t is_player_shoot               = 0;

    // MX_USB_HOST_Process();
    // /* USER CODE BEGIN 3 */
    key_codes = query_keys();

    // player update
    if (!is_stage_finished) {
        player_directions[0] += key_codes & RIGHT_KEY ? 1 : 0;   // player position update
        player_directions[0] -= key_codes & LEFT_KEY ? 1 : 0;
        player_directions[1] += key_codes & DOWN_KEY ? 1 : 0;
        player_directions[1] -= key_codes & UP_KEY ? 1 : 0;

        is_player_shoot = key_codes & Z_KEY;

        arm_sqrt_f32(player_directions[0] * player_directions[0] +
                         player_directions[1] * player_directions[1],
                     &normalization_coef);
        if (normalization_coef) {   // normalization
            player_directions[0] /= normalization_coef;
            player_directions[1] /= normalization_coef;
        }
        player.pos[0] += player_directions[0] *
                         ((key_codes & SHIFT_KEY) ? player.speed_slow : player.speed_fast) *
                         FIX_UPDATE_TIME;
        player.pos[1] += player_directions[1] *
                         ((key_codes & SHIFT_KEY) ? player.speed_slow : player.speed_fast) *
                         FIX_UPDATE_TIME;
        // position restriction
        player.pos[0] - 2 < GAME_SECTION_START_X ? (player.pos[0] = GAME_SECTION_START_X + 2) : 0;
        player.pos[0] + 2 > GAME_SECTION_END_X ? (player.pos[0] = GAME_SECTION_END_X - 2) : 0;
        player.pos[1] - 2 < GAME_SECTION_START_Y ? (player.pos[1] = GAME_SECTION_START_Y + 2) : 0;
        player.pos[1] + 2 > GAME_SECTION_END_Y ? (player.pos[1] = GAME_SECTION_END_Y - 2) : 0;

        center[0][0] = player.pos[0];
        center[0][1] = player.pos[1] - 5.0f;
        center[1][0] = player.pos[0] - 3.0f;
        center[1][1] = player.pos[1] - 5.0f;
        center[2][0] = player.pos[0] + 3.0f;
        center[2][1] = player.pos[1] - 5.0f;

        // simplified, only upwards bullets; 3 centers, 1 shot per center
        for (uint16_t i = 0; i < player.cur_danmuku->bullet_cnt; i++) {   // update previous bullets
            _bullet = player.cur_danmuku->bullets[i];
            if (_bullet) {

                _bullet->volocity_vert += _bullet->accel_vert * FIX_UPDATE_TIME;
                _bullet->direction[0] = 0.0f;
                _bullet->direction[1] = -1.0f;

                _bullet->pos[1] += _bullet->volocity_vert * FIX_UPDATE_TIME * -1.0f;

                if (!is_inter_game_section(&dsp,
                                           (int16_t)(_bullet->pos[0] - BULLET_RAD),
                                           (int16_t)(_bullet->pos[1] - BULLET_RAD),
                                           (int16_t)(_bullet->pos[0] + BULLET_RAD + 1),
                                           (int16_t)(_bullet->pos[1] + BULLET_RAD + 1))) {
                    remove_bullet(_bullet);   // remove bullet which is out of the display
                                              // and free the memeory
                    player.cur_danmuku->bullets[i] = NULL;
                    continue;
                }

                // damage detection
                if (is_contact(_bullet->pos, enemy.pos)) {
                    enemy.health -= _bullet->damage;
                    if (enemy.health <= 0) {
                        enemy.health = 100.0f;
                        danmuku_finish_cnt++;
                        danmuku_time      = 0.0f;
                        is_stage_finished = STAGE_CLEAR_INIT;
                        score += 100;
                        // change_danmuku();
                    }
                    score++;
                    remove_bullet(_bullet);   // remove bullet which is out of the display
                                              // and free the memeory
                    player.cur_danmuku->bullets[i] = NULL;
                    continue;
                }
            }
        }
        if (!is_stage_finished && is_player_shoot &&
            shoot_time_player >= player.cur_danmuku->fire_intv) {   // generate new bullets
            shoot_time_player = 0.0f;
            // shoot_time = 0.0f;
            for (int i = 0; i < player.cur_danmuku->center_cnt; i++) {   // bullet init
                bullet                = get_new_bullet();
                bullet->accel_tang    = 0.0f;
                bullet->accel_vert    = 0.0f;
                bullet->volocity_tang = 0.0f;
                bullet->volocity_vert = 30.0f;
                bullet->damage        = 0.7f;
                bullet->owner         = 2;
                bullet->pos[0]        = player.cur_danmuku->centers[i][0];
                bullet->pos[1]        = player.cur_danmuku->centers[i][1];
                bullet->direction[0]  = 0;
                bullet->direction[1]  = -1.0f;

                player.cur_danmuku->bullets[player.cur_danmuku->bullet_cnt++] = bullet;
            }
        }

        // enemy update
        for (uint16_t i = 0; i < enemy.cur_danmuku->bullet_cnt; i++) {   // update previous bullets
            _bullet = enemy.cur_danmuku->bullets[i];
            if (_bullet) {
                arm_sqrt_f32(_bullet->direction[0] * _bullet->direction[0] +
                                 _bullet->direction[1] * _bullet->direction[1],
                             &normalization_coef);
                if (normalization_coef) {   // normalization
                    _bullet->direction[0] /= normalization_coef;
                    _bullet->direction[1] /= normalization_coef;
                }
                _bullet->volocity_tang += _bullet->accel_tang * FIX_UPDATE_TIME;
                _bullet->volocity_vert += _bullet->accel_vert * FIX_UPDATE_TIME;
                _bullet->direction[0] =
                    _bullet->volocity_vert * FIX_UPDATE_TIME * _bullet->direction[0] -
                    _bullet->volocity_tang * FIX_UPDATE_TIME * _bullet->direction[1];
                _bullet->direction[1] =
                    _bullet->volocity_vert * FIX_UPDATE_TIME * _bullet->direction[1] +
                    _bullet->volocity_tang * FIX_UPDATE_TIME * _bullet->direction[0];
                _bullet->pos[0] += _bullet->direction[0];
                _bullet->pos[1] += _bullet->direction[1];

                if (!is_inter_game_section(&dsp,
                                           (int16_t)(_bullet->pos[0] - BULLET_RAD),
                                           (int16_t)(_bullet->pos[1] - BULLET_RAD),
                                           (int16_t)(_bullet->pos[0] + BULLET_RAD + 1),
                                           (int16_t)(_bullet->pos[1] + BULLET_RAD + 1))) {
                    remove_bullet(_bullet);   // remove bullet which is out of the display
                                              // and free the memeory
                    enemy.cur_danmuku->bullets[i] = NULL;
                }

                // damage detection
                if (is_contact(_bullet->pos, player.pos)) {
                    player.health -= _bullet->damage;
                    if (player.health <= 0) {
                        player.health = 0.0f;
                        danmuku_time      = 0.0f;
                        is_stage_finished = STAGE_END_INIT;
                        // change_danmuku();
                    }
                    remove_bullet(_bullet);   // remove bullet which is out of the display
                                              // and free the memeory
                    enemy.cur_danmuku->bullets[i] = NULL;
                    continue;
                }
            }
        }
        if (!is_stage_finished &&
            shoot_time_enemy >= enemy.cur_danmuku->fire_intv) {   // generate new bullets
            shoot_time_enemy = 0.0f;
            for (int i = 0; i < enemy.cur_danmuku->shots_per_fire; i++) {   // bullet init
                bullet = get_new_bullet();   //(bullet_typedef*)malloc(sizeof(bullet_typedef));
                bullet->accel_tang    = 0.0f;
                bullet->accel_vert    = -5.0f;
                bullet->volocity_tang = 0.0f;
                bullet->volocity_vert = 30.0f;
                bullet->damage        = 0;
                bullet->owner         = 1;
                bullet->pos[0]        = enemy.pos[0];
                bullet->pos[1]        = enemy.pos[1];
                bullet->direction[0] =
                    arm_cos_f32(enemy.cur_danmuku->bullet_ang * (float32_t)(i) + d_angle);
                bullet->direction[1] =
                    arm_sin_f32(enemy.cur_danmuku->bullet_ang * (float32_t)(i) + d_angle);

                enemy.cur_danmuku->bullets[enemy.cur_danmuku->bullet_cnt++] = bullet;
            }
        }


        enemy.cur_danmuku->ang_v += enemy.cur_danmuku->ang_a * FIX_UPDATE_TIME;
        if (enemy.cur_danmuku->ang_v > 270 * PI / 180.0f) {
            enemy.cur_danmuku->ang_a = -150.0f * PI / 180.0f;
        } else if (enemy.cur_danmuku->ang_v < -270 * PI / 180.0f) {
            enemy.cur_danmuku->ang_a = 150.0f * PI / 180.0f;
        }
        enemy.cur_danmuku->translation_v += enemy.cur_danmuku->translation_a * FIX_UPDATE_TIME;
        d_angle += enemy.cur_danmuku->ang_v * FIX_UPDATE_TIME;

        if (!is_stage_finished) {
            shoot_time_enemy += FIX_UPDATE_TIME;   // update time
            shoot_time_player += FIX_UPDATE_TIME;
            danmuku_time += FIX_UPDATE_TIME;
        }
        if (danmuku_time >= enemy.cur_danmuku->duration) {
            danmuku_time      = 0.0f;
            is_stage_finished = STAGE_TIME_UP_INIT;
            danmuku_finish_cnt++;
        }
    }
    game_time += FIX_UPDATE_TIME;
}

void update(uint8_t _key_codes) {
    static uint8_t   page_num   = 0;
    static float32_t start_time = 0.0F;
    bullet_typedef*  _bullet    = NULL;
    uint16_t         j          = 0;
    char             _text[100] = {0};

    // key_codes = _key_codes;
    if (page_num == 0) {
        u8g2_FirstPage(&dsp);
    }
    u8g2_ClearBuffer(&dsp);

    u8g2_SetFont(&dsp, u8g2_font_spleen8x16_mf);

    for (uint16_t i = 0; i < player.cur_danmuku->bullet_cnt; i++) {
        // display bullets and remove bullets not in the display
        _bullet                            = player.cur_danmuku->bullets[i];
        player.cur_danmuku->bullets[i - j] = player.cur_danmuku->bullets[i];
        if (_bullet) {
            u8g2_DrawFilledEllipse(&dsp,
                                   (uint16_t)(_bullet->pos[0]),
                                   (uint16_t)(_bullet->pos[1]),
                                   BULLET_RAD,
                                   BULLET_RAD,
                                   U8G2_DRAW_ALL);
        } else {
            j++;
        }
    }
    player.cur_danmuku->bullet_cnt -= j;

    j = 0;
    for (uint16_t i = 0; i < enemy.cur_danmuku->bullet_cnt; i++) {
        // display bullets and remove bullets not in the display
        _bullet                           = enemy.cur_danmuku->bullets[i];
        enemy.cur_danmuku->bullets[i - j] = enemy.cur_danmuku->bullets[i];
        if (_bullet) {
            u8g2_DrawFilledEllipse(&dsp,
                                   (uint16_t)(_bullet->pos[0]),
                                   (uint16_t)(_bullet->pos[1]),
                                   BULLET_RAD,
                                   BULLET_RAD,
                                   U8G2_DRAW_ALL);
        } else {
            j++;
        }
    }
    enemy.cur_danmuku->bullet_cnt -= j;

    // draw player
    //  if (is_show)
    u8g2_DrawFilledEllipse(&dsp,
                           (u8g2_uint_t)(player.pos[0]),
                           (u8g2_uint_t)(player.pos[1]),
                           PLAYER_RAD,
                           PLAYER_RAD,
                           U8G2_DRAW_ALL);
    // is_transparent = !is_transparent;   // half light??
    // draw enemy
    u8g2_DrawFilledEllipse(&dsp,
                           (u8g2_uint_t)(enemy.pos[0]),
                           (u8g2_uint_t)(enemy.pos[1]),
                           ENEMY_RAD,
                           ENEMY_RAD,
                           U8G2_DRAW_ALL);
    if (is_stage_finished) {
        switch (is_stage_finished) {
        case STAGE_START_INIT:
            start_time = game_time;
            clear_danmuku(0);
            clear_danmuku(1);   // NO BREAK!!!
        case STAGE_START:
            is_stage_finished = STAGE_START;
            enemy.health      = 100.0f;
            if ((game_time - start_time) <= 1.0f) {
                u8g2_DrawStr(&dsp,
                             GAME_SECTION_END_X / 2 - 20,
                             (GAME_SECTION_END_Y + GAME_SECTION_START_Y) / 2 - 16,
                             "STAGE");
                sprintf(_text, "%5d", danmuku_finish_cnt + 1);
                u8g2_DrawStr(&dsp,
                             GAME_SECTION_END_X / 2 - 20,
                             (GAME_SECTION_END_Y + GAME_SECTION_START_Y) / 2,
                             _text);
            } else {
                is_stage_finished = STAGE_STARTED;
            }
            break;
        case STAGE_CLEAR_INIT: start_time = game_time;   // NO BREAK!!!
        case STAGE_CLEAR:
            is_stage_finished = STAGE_CLEAR;
            if ((game_time - start_time) <= 1.0f) {
                u8g2_DrawStr(&dsp,
                             GAME_SECTION_END_X / 2 - 20,
                             (GAME_SECTION_END_Y + GAME_SECTION_START_Y) / 2 - 16,
                             "STAGE");
                u8g2_DrawStr(&dsp,
                             GAME_SECTION_END_X / 2 - 20,
                             (GAME_SECTION_END_Y + GAME_SECTION_START_Y) / 2,
                             "CLEAR");
            } else if ((game_time - start_time) <= 1.5f) {
            } else if ((game_time - start_time) <= 2.5f) {
                u8g2_DrawStr(&dsp,
                             GAME_SECTION_END_X / 2 - 20,
                             (GAME_SECTION_END_Y + GAME_SECTION_START_Y) / 2 - 24,
                             "BONUS");
                u8g2_DrawStr(&dsp,
                             GAME_SECTION_END_X / 2 - 4,
                             (GAME_SECTION_END_Y + GAME_SECTION_START_Y) / 2 - 8,
                             "GET");
                u8g2_DrawStr(&dsp,
                             GAME_SECTION_END_X / 2 - 12,
                             (GAME_SECTION_END_Y + GAME_SECTION_START_Y) / 2 + 8,
                             "+100");
            } else {
                is_stage_finished = STAGE_START_INIT;
            }
            break;
        case STAGE_TIME_UP_INIT: start_time = game_time;   // NO BREAK!!!
        case STAGE_TIME_UP:
            is_stage_finished = STAGE_TIME_UP;
            if ((game_time - start_time) <= 1.0f) {
                u8g2_DrawStr(&dsp,
                             GAME_SECTION_END_X / 2 - 16,
                             (GAME_SECTION_END_Y + GAME_SECTION_START_Y) / 2 - 16,
                             "TIME");
                u8g2_DrawStr(&dsp,
                             GAME_SECTION_END_X / 2 - 8,
                             (GAME_SECTION_END_Y + GAME_SECTION_START_Y) / 2,
                             "UP");
            } else if ((game_time - start_time) <= 1.5f) {
            } else {
                is_stage_finished = STAGE_START_INIT;
            }
            break;
        case STAGE_END_INIT:start_time = game_time;   // NO BREAK!!!
        case STAGE_END:            is_stage_finished = STAGE_END;
            if (1||(game_time - start_time) <= 1.0f) {
                u8g2_DrawStr(&dsp,
                             GAME_SECTION_END_X / 2 - 16,
                             (GAME_SECTION_END_Y + GAME_SECTION_START_Y) / 2 - 16,
                             "GAME");
                u8g2_DrawStr(&dsp,
                             GAME_SECTION_END_X / 2 - 8,
                             (GAME_SECTION_END_Y + GAME_SECTION_START_Y) / 2,
                             "END");
            } else if ((game_time - start_time) <= 1.5f) {
            }
            break;
        default: break;
        }
    }
    // ui
    u8g2_DrawHLine(   // width :2  ->exec 2 times
        &dsp,
        PROGRESS_SECTION_START_X,
        1,
        (u8g2_uint_t)((PROGRESS_SECTION_END_X - PROGRESS_SECTION_START_X) * (enemy.health) /
                      100.0f));
    u8g2_DrawHLine(&dsp,
                   PROGRESS_SECTION_START_X,
                   2,
                   (u8g2_uint_t)((PROGRESS_SECTION_END_X - PROGRESS_SECTION_START_X) *
                                 (enemy.health) / 100.0f));
    u8g2_DrawHLine(   // width :2  ->exec 2 times
        &dsp,
        HEALTH_SECTION_START_X,
        61,
        (u8g2_uint_t)((HEALTH_SECTION_END_X - HEALTH_SECTION_START_X) * (player.health) /
                      100.0f));
    u8g2_DrawHLine(&dsp,
                   HEALTH_SECTION_START_X,
                   62,
                   (u8g2_uint_t)((HEALTH_SECTION_END_X - HEALTH_SECTION_START_X) *
                                 (player.health) / 100.0f));
    u8g2_DrawFrame(&dsp,
                   INFO_SECTION_START_X,
                   INFO_SECTION_START_Y,
                   INFO_SECTION_END_X - INFO_SECTION_START_X,
                   INFO_SECTION_END_Y - INFO_SECTION_START_Y);
    // u8g2_SetFontMode(&dsp,U8G2_FONT_MODE_SOLID);
    // sprintf(_text, "%f", time);
    sprintf(_text,
            "%02d.%01d",
            (uint16_t)(enemy.cur_danmuku->duration - danmuku_time),
            (uint16_t)((enemy.cur_danmuku->duration - danmuku_time) * 10) % 10);
    u8g2_DrawStr(&dsp, 83, 3, _text);
    sprintf(_text, "%4d", score);
    u8g2_DrawStr(&dsp, 83, 19, _text);
    sprintf(_text, "%4d", danmuku_finish_cnt);
    u8g2_DrawStr(&dsp, 83, 35, _text);
    u8g2_NextPage(&dsp);
    page_num += 1;
    if (page_num & 0x08) {
        page_num = 0;
    }
}
/*
find and return the first bullet not in use
*/
bullet_typedef* get_new_bullet(void) {
    static uint16_t i   = 0;
    uint16_t        cnt = 0;
    //
    while (bullet_pool_flag[i]) {
        i++;
        cnt++;
        if (i >= BULLET_POOL_SIZE) {
            i = 0;
        }
        if (cnt >= BULLET_POOL_SIZE) {
            return NULL;
        }
    }
    bullet_pool_flag[i] = 1;
    return bullet_pool + i;
}

void remove_bullet(bullet_typedef* _bullet) {
    bullet_pool_flag[(uint16_t)(_bullet - bullet_pool)] = 0;
}

danmuku_typedef* get_new_danmuku(uint8_t is_random) {

    return NULL;
}
uint8_t is_inter_game_section(u8g2_t* u8g2, int16_t x0, int16_t y0, int16_t x1, int16_t y1) {
    UNUSED(u8g2);
#if !defined(GAME_SECTION_START_X) || !defined(GAME_SECTION_END_X) || \
    !defined(GAME_SECTION_START_Y) || !defined(GAME_SECTION_END_Y)
    return u8g2_IsIntersection(
        u8g2, (u8g2_uint_t)x0, (u8g2_uint_t)y0, (u8g2_uint_t)x1, (u8g2_uint_t)y1);
#endif

    if (x0 > GAME_SECTION_END_X || x0 < GAME_SECTION_START_X) {
        return 0;
    }
    if (x1 > GAME_SECTION_END_X || x1 < GAME_SECTION_START_X) {
        return 0;
    }
    if (y0 > GAME_SECTION_END_Y || y0 < GAME_SECTION_START_Y) {
        return 0;
    }
    if (y1 > GAME_SECTION_END_Y || y1 < GAME_SECTION_START_Y) {
        return 0;
    }
    return 1;
}

uint8_t is_contact(float32_t pos1[2], float32_t pos2[2]) {
    return ((pos2[0] - pos1[0]) * (pos2[0] - pos1[0]) +
            (pos2[1] - pos1[1]) * (pos2[1] - pos1[1])) <=
           (BULLET_RAD + ENEMY_RAD) * (BULLET_RAD + ENEMY_RAD);
}

// type:  0: enemy   1:player;
void clear_danmuku(uint8_t type) {
    uint16_t        bullet_cnt;
    bullet_typedef* bullet;
    if (type) {
        bullet_cnt = enemy.cur_danmuku->bullet_cnt;
        for (int i = 0; i < bullet_cnt; i++) {
            bullet = enemy.cur_danmuku->bullets[i];
            remove_bullet(bullet);
            enemy.cur_danmuku->bullets[i] = NULL;
        }
    } else {
        bullet_cnt = player.cur_danmuku->bullet_cnt;
        for (int i = 0; i < bullet_cnt; i++) {
            bullet = player.cur_danmuku->bullets[i];
            remove_bullet(bullet);
            player.cur_danmuku->bullets[i] = NULL;
        }
    }
}
// hardware driver functions
uint8_t dsp_hw_iic_msg_callback(u8x8_t* u8x8, uint8_t msg, uint8_t arg_int, void* arg_ptr) {
    uint8_t* data = (uint8_t*)arg_ptr;
    // uint8_t *data = (uint8_t *)arg_ptr;
    switch (msg) {
    case U8X8_MSG_BYTE_SEND:
        while (arg_int > 0) {
            HAL_I2C_Master_Transmit(&hi2c1, u8x8_GetI2CAddress(u8x8), data, 1, 100);
            data++;
            arg_int--;
            while (HAL_I2C_GetState(&hi2c1) != HAL_I2C_STATE_READY);
        }
        // HAL_I2C_Master_Transmit (&hi2c1, u8x8_GetI2CAddress (u8x8),
        //                          (uint8_t *)arg_ptr, arg_int, 100);

        break;
    case U8X8_MSG_BYTE_INIT:
        /* add your custom code to init i2c subsystem */
        // MX_I2C1_Init();
        // break;
    case U8X8_MSG_BYTE_SET_DC:
        /* ignored for i2c */
        // break;
    case U8X8_MSG_BYTE_START_TRANSFER:   // break;
    case U8X8_MSG_BYTE_END_TRANSFER: break;
    default: return 0;
    }
    return 1;
}

uint8_t dsp_gpio_delay_msg_callback(u8x8_t* u8x8, uint8_t msg, uint8_t arg_int, void* arg_ptr) {
    UNUSED(arg_ptr);
    UNUSED(u8x8);

    switch (msg) {

    case U8X8_MSG_DELAY_MILLI:   // delay arg_int * 1 milli second
        HAL_Delay(arg_int * 1);
        break;

    case U8X8_MSG_GPIO_I2C_CLOCK:   // arg_int=0: Output low at I2C clock

        HAL_GPIO_WritePin(GPIOB, GPIO_PIN_4, arg_int ? GPIO_PIN_SET : GPIO_PIN_RESET);
        break;                     // arg_int=1: Input dir with pullup high for I2C clock pin
    case U8X8_MSG_GPIO_I2C_DATA:   // arg_int=0: Output low at I2C data pin
        HAL_GPIO_WritePin(GPIOB, GPIO_PIN_5, arg_int ? GPIO_PIN_SET : GPIO_PIN_RESET);

        break;   // arg_int=1: Input dir with pullup high for I2C data pin

    case U8X8_MSG_GPIO_AND_DELAY_INIT: HAL_Delay(1); break;
    case U8X8_MSG_DELAY_100NANO:   // delay arg_int * 100 nano seconds
                                   // __NOP();
                                   // break;
    case U8X8_MSG_DELAY_10MICRO:   // delay arg_int * 10 micro seconds
                                   // for (uint16_t n = 0; n < 320; n++) __NOP();
                                   // break;
    case U8X8_MSG_DELAY_I2C:       // arg_int is the I2C speed in 100KHz, e.g. 4 =
                                   // 400 KHz
        // for (uint16_t n = 0; n < 50; n++) __NOP();
        break;
    default:
        u8x8_SetGPIOResult(u8x8, 1);   // default return value
        break;
    }
    return 1;
}
