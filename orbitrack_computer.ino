#include "Arduino.h"
#include "ble_functions.h"
#include "draw_functions.h"

int pin_rev_count = 25;


typedef struct sActivity
{
  uint8_t user_id;
  uint8_t min_bpm;
  uint8_t max_bpm;
  uint8_t avg_bpm;
  uint8_t max_speed;
  uint8_t avg_speed;
  uint8_t bpm_zones[6];
  uint16_t activity_length;
  uint8_t year;
  uint8_t month;
  uint8_t day;
  uint8_t hour;
  uint8_t minute;
  uint8_t RFU[8];
}sActivity;
void setup() 
{
  Serial.begin(115200);
  pinMode(pin_rev_count, INPUT_PULLUP);
  ble_functions_init();
  draw_init();

  delay(500);
}


float cur_rev_speed = 0;
float avg_speed = 0;
float cur_distance = 0;
float cur_calories = 0;
float rev_dist = 5.55; //for translating into cycling
uint32_t prev_rev_time = 0;
uint32_t prev_change_time = 0;
uint32_t avg_speed_upd = 0;

void measure_speed()
{
//assuming 1 rev / 2 sec = 4.0 MET/h = 10 km/h cycling
//1 MET = 1 kcal/kg per hour
  static uint8_t prev_st = 0;
  uint8_t st = digitalRead(pin_rev_count);
  uint32_t ms = millis();
  if(st == 0 && prev_st != 0 && ms - prev_change_time > 100) //debouncing this way
  {
    if(prev_rev_time == 0) prev_rev_time = ms - 2000; //initial assumption to simplify stuff
    float rev_interval = ms - prev_rev_time;
    rev_interval *= 0.001;
    prev_rev_time = ms;
    cur_rev_speed = rev_dist / rev_interval;
    cur_distance += rev_dist;
    //avg speed: 10 km/h / 3.6 = 2.78 m/s, 4 MET / 2.78 = 1.44 MET per hour / per 1 m/s
    cur_calories += 72.0 * avg_speed * 1.44 * rev_interval / 3600.0;
  }
  if(ms - prev_change_time > 4000) cur_rev_speed = 0;
  if(ms - avg_speed_upd > 300)
  {
    avg_speed_upd = ms;
    avg_speed *= 0.9;
    avg_speed += 0.1*cur_rev_speed;
  }
  if(prev_st != st) prev_change_time = ms;
  prev_st = st;
}

uint32_t last_hist_push_time = 0;
uint32_t last_active_move = 0;
uint8_t bpm_history[7200]; //2 hours max
uint8_t speed_history[7200];
int history_pos = 0;

int push_hist_data(int BPM, float avg_speed)
{
  uint32_t ms = millis();
  if(ms - last_active_move > 60000 && avg_speed < 0.5)
  {
//    if(current_activity_start > 0 && ms - current_activity_start > 60000) append_activity();
    history_pos = 0;
    return 0;
  }
  if(ms - last_hist_push_time > 1000 && (avg_speed > 0.5 || ms - last_active_move < 60000))
  {
    if(avg_speed > 0.5) last_active_move = ms;
    last_hist_push_time = ms;
    bpm_history[history_pos] = BPM;
    speed_history[history_pos] = avg_speed * 15;
    if(avg_speed * 15 > 255) speed_history[history_pos] = 255;
    history_pos++;
    if(history_pos > 7199) history_pos = 0;
  }
  return history_pos;
}

int get_excercise_avg_BPM()
{
  if(history_pos < 65) return 0;
  int avg_bpm = 0, avgz = 0;
  for(int x = 60; x < history_pos; x++)
    avg_bpm += bpm_history[x], avgz++;
  return avg_bpm/avgz;
}
int get_excercise_min_BPM()
{
  if(history_pos < 65) return 0;
  int min_bpm = 1234;
  for(int x = 60; x < history_pos; x++)
    if(bpm_history[x] < min_bpm) min_bpm = bpm_history[x];
  return min_bpm;
}
int get_excercise_max_BPM()
{
  if(history_pos < 65) return 0;
  int max_bpm = 0;
  for(int x = 60; x < history_pos; x++)
    if(bpm_history[x] > max_bpm) max_bpm = bpm_history[x];
  return max_bpm;
}
void get_excercise_zone_percent(uint8_t *zone_res)
{
  int zones[6];
  for(int z = 0; z < 6; z++) zones[z] = 0;
  if(history_pos < 65) return;
  int zone_z = 0;
  for(int x = 30; x < history_pos; x++)
  {
    int z = bpm2zone(bpm_history[x]);
    if(z < 0 || z > 5) continue;
    zones[z]++;
    zone_z++;
  }
  for(int z = 0; z < 6; z++)
  {
    zone_res[z] = (float)zones[z] * 100.0f / (float)zone_z + 0.5f;
  }
}
int get_excercise_time()
{
  return history_pos;
}
float get_excercise_avg_speed()
{
  if(history_pos < 65) return 0;
  float avg_spd = 0, avgz = 0;
  for(int x = 60; x < history_pos; x++)
    avg_spd += speed_history[x], avgz++;
  return avg_spd/avgz;
}
float get_excercise_max_speed()
{
  if(history_pos < 65) return 0;
  float max_spd = 0;
  for(int x = 30; x < history_pos; x++)
    if(speed_history[x] > max_spd) max_spd = speed_history[x];
  return max_spd;
}

uint32_t last_draw_ms = 0;
uint32_t current_activity_start = 0;

void loop() {
  uint32_t ms = millis();
  ble_cycle();
  
  measure_speed();
  
  if(ms - last_draw_ms < 50) return;
  
  int BPM = get_BPM();

  int has_activity = push_hist_data(BPM, avg_speed);
  if(!has_activity)
  {
    current_activity_start = 0;
    cur_distance = 0;
    cur_calories = 0;
  }
  else
  {
    if(current_activity_start == 0)
      current_activity_start = ms;
  }
  last_draw_ms = ms;
  draw_cycle(avg_speed, cur_distance, cur_calories, bpm_history, speed_history, history_pos);
}

