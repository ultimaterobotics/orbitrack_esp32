#include "Arduino.h"

void ble_functions_init();
void ble_cycle();

int get_BPM();
int get_uecg_batt();
int16_t* get_ecg_buf();
int get_ecg_buf_pos();
int get_ecg_buf_len();

int get_excercise_avg_BPM();
int get_excercise_min_BPM();
int get_excercise_max_BPM();
void get_excercise_zone_percent(int *zones);
int get_excercise_time();
float get_excercise_avg_speed();
float get_excercise_max_speed();

