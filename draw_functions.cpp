#include <Arduino.h>
#include "draw_functions.h"
#include <Adafruit_GFX.h>    // Core graphics library
#include <Adafruit_ST7735.h> // Hardware-specific library for ST7735
#include <Adafruit_ST7789.h> // Hardware-specific library for ST7789
#include <SPI.h>

#include "ble_functions.h"

#define TFT_CS         12
#define TFT_RST        13
#define TFT_DC         14
#define SPI_COPI      26
#define SPI_SCK       27

Adafruit_ST7735 *tft;

uint16_t rgb2cl(uint8_t r, uint8_t g, uint8_t b)
{
  uint16_t res = 0;
  res |= (r>>3)<<11;
  res |= (g>>2)<<5;
  res |= b>>3;
  return res;
}

void draw_batt(int uecg_batt_mv)
{
  int bat_sz = 20;
  int bat_h = 9;
  int bat_x = 105;
  int bat_y = 1;
  int bat_lvl = (uecg_batt_mv - 3300) / 9;
  if(bat_lvl < 0) bat_lvl = 0;
  if(bat_lvl > 99) bat_lvl = 99;
  uint16_t bat_cl = rgb2cl(0, 250, 0);
  if(bat_lvl < 70) bat_cl = rgb2cl(0, 200, 150);
  if(bat_lvl < 30) bat_cl = rgb2cl(150, 200, 0);
  if(bat_lvl < 15) bat_cl = rgb2cl(250, 50, 0);
  if(bat_lvl < 5)
  {
    bat_cl = rgb2cl(250, 0, 0);
    if((millis()%1000) < 500) bat_cl = 0;
  }
  tft->drawRect(bat_x, bat_y, bat_sz, bat_h, bat_cl);
  
//  tft->drawLine(bat_x, bat_y, bat_x + bat_sz, bat_y, bat_cl);
//  tft->drawLine(bat_x, bat_y+bat_h, bat_x + bat_sz, bat_y+bat_h, bat_cl);
//  tft->drawLine(bat_x, bat_y, bat_x, bat_y + bat_h, bat_cl);
//  tft->drawLine(bat_x + bat_sz, bat_y, bat_x + bat_sz, bat_y + bat_h, bat_cl);
  int fill_w = bat_sz * bat_lvl / 100 + 1;
  tft->fillRect(bat_x, bat_y, fill_w, bat_h, bat_cl);
}

int bpm2zone(int bpm)
{
  float max_bpm = 182;
  float rest_bpm = 65;
  int zone = 0;
  if(bpm > rest_bpm + (max_bpm - rest_bpm)*0.5) zone = 1;
  if(bpm > rest_bpm + (max_bpm - rest_bpm)*0.6) zone = 2;
  if(bpm > rest_bpm + (max_bpm - rest_bpm)*0.7) zone = 3;
  if(bpm > rest_bpm + (max_bpm - rest_bpm)*0.8) zone = 4;
  if(bpm > rest_bpm + (max_bpm - rest_bpm)*0.9) zone = 5;
  return zone;
}
int zone2color(int zone, int cdiv)
{
  int cl = rgb2cl(0, 30/cdiv, 140/cdiv);
  if(zone == 0) cl = rgb2cl(192/cdiv, 192/cdiv, 192/cdiv);
  if(zone == 1) cl = rgb2cl(0, 128/cdiv, 255/cdiv);
  if(zone == 2) cl = rgb2cl(0, 255/cdiv, 0);
  if(zone == 3) cl = rgb2cl(255/cdiv, 255/cdiv, 64/cdiv);
  if(zone == 4) cl = rgb2cl(255/cdiv, 64/cdiv, 64/cdiv);
  if(zone == 5)
  {
    cl = rgb2cl(255/cdiv, 0, 200/cdiv);
    if((millis()%1000) > 500) cl = rgb2cl(255/cdiv, 0, 0);
  }
  return cl;
}

void draw_ECG(int16_t *ecg_buf, int ecg_buf_pos, int ecg_buf_len, int BPM)
{
  int BPM_zone = bpm2zone(BPM);
  
  int prev_x = 0;
  int prev_y = 0;
  int min_y = 123456;
  int max_y = -123456;
  for(int ep = 0; ep < 128; ep++)
  {
    int ecg_bp = ecg_buf_pos - 128 + ep;
    if(ecg_bp < 0) ecg_bp += ecg_buf_len;
    int x = ep;
    int y = ecg_buf[ecg_bp];
    if(y < min_y) min_y = y;
    if(y > max_y) max_y = y;
  }
  max_y++;
  int ecg_cl = zone2color(BPM_zone, 1);
  
  for(int ep = 0; ep < 128; ep++)
  {
    int ecg_bp = ecg_buf_pos - 128 + ep;
    if(ecg_bp < 0) ecg_bp += ecg_buf_len;
    int x = ep;
    int y = 50 - (ecg_buf[ecg_bp] - min_y) * 50 / (max_y - min_y);
    if(ep > 0) tft->drawLine(prev_x, prev_y, x, y, ecg_cl);
    prev_x = x;
    prev_y = y;
  }
}

void draw_time_charts(uint8_t *bpm_history, uint8_t *speed_history, int history_pos)
{
  int SW = 128;
  int time_range = history_pos;
//  if(time_range > 30*60) time_range = 30*60;
//  if(time_range < SW) time_range = SW;
  if(time_range > 30) time_range = 30;
//  if(time_range < SW) time_range = SW;
  float pix2time = (float)time_range / (float)SW;
  if(pix2time > 0.99 && pix2time < 1.01) pix2time = 1;
  int min_bpm = 12345;
  int max_bpm = 0;
  int min_speed = 12345;
  int max_speed = 0;
  for(int x = 0; x < SW; x++)
  {
    int hp = history_pos-1 - SW*pix2time + x*pix2time;
    int bpm_val = -1, speed_val = -1;
    if(hp < 0) hp = 0;
    else
    {
      bpm_val = bpm_history[hp];
      speed_val = speed_history[hp];
      if(bpm_val < min_bpm) min_bpm = bpm_val;
      if(bpm_val > max_bpm) max_bpm = bpm_val;
      if(speed_val < min_speed) min_speed = speed_val;
      if(speed_val > max_speed) max_speed = speed_val;
    }
  }
  int draw_bpm = 1, draw_speed = 1;
  if(max_bpm == 0) draw_bpm = 0;
  if(max_speed == 0) draw_speed = 0;
  int bpm_H = 50;
  int speed_H = 50;
  int bpm_dy = 0;
  int speed_dy = 50;
  min_bpm -= 10;
  max_bpm += 10;
  min_speed -= 10;
  max_speed += 10;
  if(min_speed < 0) min_speed = 0;
  tft->startWrite();
  if(draw_bpm)
  {
    int prev_zone = -1;
    int prev_zone_start = 0;
    for(int y = 0; y < bpm_H; y++)
    {
      int bpm_y = min_bpm + y * (max_bpm - min_bpm) / bpm_H;
      int bpm_z = bpm2zone(bpm_y);
      int cl = zone2color(bpm_z, 2);
      int yy = bpm_dy + bpm_H - 1 - y;
      tft->writeFastHLine(0, yy, SW, cl);
      if(0)if(bpm_z != prev_zone)
      {
        if(prev_zone >= 0)
        {
          tft->writeFillRect(0, yy, SW, yy - prev_zone_start, cl);
        }
        prev_zone_start = yy;
        prev_zone = bpm_z;
      }
    }
    int cl = zone2color(prev_zone, 1);
//    tft->writeFillRect(0, bpm_dy, SW, bpm_dy - prev_zone_start, cl);
//    tft->endWrite();
  }
  int prev_x = 0;
  int prev_y_bpm = -1;
  int prev_y_speed = -1;

  for(int x = 0; x < SW; x++)
  {
    int hp = history_pos-1 - SW*pix2time + x*pix2time;
    int bpm_val = -1, speed_val = -1;
    if(hp < 0) hp = 0;
    else
    {
      bpm_val = bpm_history[hp];
      speed_val = speed_history[hp];
      int bpm_y = bpm_dy + bpm_H - 1 - (bpm_val - min_bpm) * bpm_H / (max_bpm - min_bpm);
      int speed_y = speed_dy + speed_H - 1 - (speed_val - min_speed) * speed_H / (max_speed - min_speed);
      
      int bpm_cl = rgb2cl(50, 0, 0);
      int speed_cl = rgb2cl(100, 200, 180);
      int zcl = zone2color(bpm2zone(bpm_val), 1);
      int izcl = zcl;//(255 - zcl&0xFF) | ((255 - ((zcl>>8)&0xFF))<<8) | ((255 - ((zcl>>16)&0xFF))<<16);
      if(prev_y_bpm >= 0 && draw_bpm)
      {
        tft->writeLine(prev_x, prev_y_bpm, x, bpm_y, izcl);//bpm_cl);
        if(bpm_y > 1 && prev_y_bpm > 1)
          tft->writeLine(prev_x, prev_y_bpm-1, x, bpm_y-1, izcl);//bpm_cl);
      }
      if(prev_y_speed >= 0 && draw_speed) tft->writeLine(prev_x, prev_y_speed, x, speed_y, speed_cl);
      prev_y_bpm = bpm_y;
      prev_y_speed = speed_y;
      prev_x = x;
    }
  }
  tft->endWrite();
}

void draw_BPM(int BPM)
{
  tft->setTextColor(rgb2cl(0, 200, 250));
  tft->setCursor(15, 50);
  tft->setTextSize(1);
  tft->print("BPM");
  tft->setCursor(1, 62);
  tft->setTextSize(3);
  tft->print(BPM);
}

void draw_distance(float cur_distance)
{
  tft->setTextColor(rgb2cl(100, 200, 150));
  tft->setCursor(95, 55);
  tft->setTextSize(1);
  tft->print("km");

  float dist = cur_distance / 1000.0f;

  int dist_pd = dist;
  int dist_ad = (dist*10 - dist_pd*10);
  tft->setCursor(75, 70);
  tft->setTextSize(2);
  tft->print(dist_pd);
  tft->print('.');
  tft->print(dist_ad);
}

void draw_speed(float avg_speed)
{
  tft->setTextColor(rgb2cl(150, 250, 50));
  float speed = avg_speed * 3.6f;
  int speed_pd = speed;
  int speed_ad = (speed*10 - speed_pd*10);
  tft->setCursor(25, 100);
  tft->setTextSize(2);
  tft->print(speed_pd);
  tft->print('.');
  tft->print(speed_ad);
  tft->setTextSize(1);
  tft->print(" km/h");
}

void draw_time(int history_pos)
{
  tft->setTextColor(rgb2cl(150, 250, 50));
  int time_sec = history_pos%60;
  int time_min = history_pos / 60;
  tft->setCursor(1, 100);
  tft->setTextSize(2);
  tft->print(time_min);
  tft->print(':');
  if(time_sec < 10)
    tft->print('0');
  tft->print(time_sec);
}

void draw_calories(float cur_calories)
{
  tft->setTextColor(rgb2cl(250, 150, 250));
  int cal_pd = cur_calories;
  int cal_ad = cur_calories*10 - cal_pd*10;
  tft->setCursor(65, 100);
  tft->setTextSize(2);
  tft->print(cal_pd);
  tft->setTextSize(1);
  tft->print('.');
  tft->print(cal_ad);
  tft->print(" cal");
}

void draw_init()
{
  SPI.begin(SPI_SCK, 22, SPI_COPI);
  SPIClass *spi = &SPI;
  tft = new Adafruit_ST7735(spi, TFT_CS, TFT_DC, TFT_RST);
  tft->initR(INITR_144GREENTAB);
  delay(100);
  tft->fillScreen(0);
  tft->setTextWrap(false);
  tft->setCursor(10, 10);
  tft->setTextColor(ST77XX_RED);
  tft->setTextSize(1);
  tft->println("init ok");
}

void draw_cycle(float avg_speed, float cur_distance, float cur_calories, uint8_t *bpm_history, uint8_t *speed_history, int history_pos)
{
  int BPM = get_BPM();
  int batt = get_uecg_batt();
  int16_t* ecg_buf = get_ecg_buf();
  int ecg_bp = get_ecg_buf_pos();
  int ecg_blen = get_ecg_buf_len();
  
  uint32_t ms = millis();
  
  if((ms%10000) < 5000)
  {
    tft->fillScreen(0);
    draw_batt(batt);
    draw_BPM(BPM);
    draw_speed(avg_speed);
    draw_distance(cur_distance);
    draw_ECG(ecg_buf, ecg_bp, ecg_blen, BPM);
  }
  else
  {
    tft->startWrite();
    tft->writeFillRect(0, 50, 128, 78, 0);
    tft->endWrite();
    draw_time_charts(bpm_history, speed_history, history_pos);
    draw_time(history_pos);
    draw_calories(cur_calories);
  }
}

