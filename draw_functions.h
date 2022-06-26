#include "Arduino.h"

uint16_t rgb2cl(uint8_t r, uint8_t g, uint8_t b);
void draw_batt(int uecg_batt_mv);
int bpm2zone(int bpm);
int zone2color(int zone, int cdiv);
void draw_ECG(int *ecg_buf, int ecg_buf_pos, int ecg_buf_len, int BPM);
void draw_time_charts(uint8_t *bpm_history, uint8_t *speed_history, int history_pos);
void draw_BPM(int BPM);
void draw_distance(float cur_distance);
void draw_speed(float avg_speed);
void draw_time();
void draw_calories(float cur_calories);
void draw_init();
void draw_cycle(float avg_speed, float cur_distance, float cur_calories, uint8_t *bpm_history, uint8_t *speed_history, int history_pos);

