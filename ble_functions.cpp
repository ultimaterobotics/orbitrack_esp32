#include "ble_functions.h"
#include <NimBLEDevice.h>
#include <Arduino.h>

int uECG_found = 0;
int need_scan = 1;
uint32_t last_ecg_data = 0;

void scanEndedCB(NimBLEScanResults results);

static NimBLEAdvertisedDevice* advDevice;

class AdvertisedDeviceCallbacks: public NimBLEAdvertisedDeviceCallbacks 
{
  void onResult(NimBLEAdvertisedDevice* advertisedDevice) {
//        Serial.print("Advertised Device found: ");
//        Serial.println(advertisedDevice->toString().c_str());
    if(advertisedDevice->haveName())
    {
      const char *nm = advertisedDevice->getName().c_str();
      if(nm[0] == 'u' && nm[1] == 'E' && nm[2] == 'C' && nm[3] == 'G')
      {
        Serial.println("uECG found");
        NimBLEDevice::getScan()->stop();
        advDevice = advertisedDevice;
        need_scan = 0;
        last_ecg_data = millis();
      }
    }
  };
};

enum
{
  pack_ecg_short = 1,
  pack_imu_rr_short,
  pack_hrv_short,
  pack_ecg_imu,
  pack_ecg_rr,
  pack_ecg_hrv
};

int BPM = 0;
int RMSSD = 0;
int cur_RR = 0;
int uecg_batt_mv = 0;

uint16_t prev_data_id = 0;
uint32_t real_data_id = 0;

int16_t ecg_buf[256];
int ecg_buf_pos = 0;
int ecg_buf_len = 256;

void parse_ecg_pack(uint8_t *buf, int length)
{
  int dat_id = (buf[0]<<8) | buf[1];
  if(real_data_id == 0)
  {
    real_data_id = dat_id;
    prev_data_id = dat_id;
  }
  int dp = dat_id - prev_data_id;
  Serial.println(dat_id);
  if(dp < 0) dp += 65536;
  real_data_id += dp;
  if(dp > 13) dp = 13;
  int16_t ecg_vals[16];
  int pp = 2;
  int scale = buf[pp++];
  if(scale > 100) scale = 100 + (scale-100)*4;
  int16_t v0 = (buf[pp]<<8) | buf[pp+1];
  pp += 2;
  int16_t prev_v = v0;
  for(int n = 0; n < 13; n++)
  {
    int dv = buf[pp++] - 128;
    dv *= scale;
    ecg_vals[n] = prev_v + dv;
    prev_v = ecg_vals[n];
//    Serial.println(prev_v);
  }
  int max_diff = 0;
  for(int d = 0; d < 13-dp; d++)
  {
    int ecg_comp_pos = ecg_buf_pos - (13-dp) + d; 
    if(ecg_comp_pos < 0) ecg_comp_pos += ecg_buf_len;
    int dv = ecg_buf[ecg_comp_pos] - ecg_vals[d];
    if(dv < 0) dv = -dv;
    if(dv > max_diff) max_diff = dv;
  }
  Serial.println(max_diff);
  int is_bad = 0;
  if(max_diff > 10) is_bad = 1;
  static int conseq_bad = 0;
  if(is_bad) conseq_bad++;
  else conseq_bad = 0;

  if(conseq_bad > 5) is_bad = 0; //stream lost, can't rely on this
  
  if(!is_bad)
  {
    prev_data_id = dat_id;

    for(int d = 0; d < dp; d++)
    {
      ecg_buf[ecg_buf_pos] = ecg_vals[13 - dp + d];
      ecg_buf_pos++;
      if(ecg_buf_pos >= ecg_buf_len) ecg_buf_pos = 0;
    }
  }
}
void parse_imu_rr_pack(uint8_t *buf, int length)
{
  float T = buf[9] + 200;
  T /= 10.0;

  int rr_id = buf[12];
  int rr1 = (buf[13]<<4) | (buf[14]>>4);
  int rr2 = ((buf[14]&0xF)<<8) | buf[15];
  BPM = buf[16];
  cur_RR = rr2;
  Serial.print("BPM: ");
  Serial.print(BPM);
  Serial.print(" RRs: ");
  Serial.print(rr1);
  Serial.print(" ");
  Serial.println(rr2);
//  buf[pp++] = ecg_params.skin_parameter>>8;
//  buf[pp++] = ecg_params.skin_parameter;
  //19 bytes used 
  
}
void parse_hrv_pack(uint8_t *buf, int length)
{
  int pp = 15;
  int SDRR = buf[pp++]<<4;
  SDRR += buf[pp]>>4;
  RMSSD = (buf[pp++]&0xF)<<8;
  RMSSD += buf[pp++];
  uecg_batt_mv = buf[pp]*10 + 2000;
}

void parse_uecg_data(uint8_t *buf, int length)
{
  if(buf[0] == pack_ecg_short) parse_ecg_pack(buf+1, length);
  if(buf[0] == pack_imu_rr_short) parse_imu_rr_pack(buf+1, length);
  if(buf[0] == pack_hrv_short) parse_hrv_pack(buf+1, length);
  last_ecg_data = millis();
  need_scan = 0;
  uECG_found = 1;
}

void notifyCB(NimBLERemoteCharacteristic* pRemoteCharacteristic, uint8_t* pData, size_t length, bool isNotify)
{
  parse_uecg_data(pData, length);
}
void scanEndedCB(NimBLEScanResults results)
{
  Serial.println("Scan Ended");
}
//static ClientCallbacks clientCB;

int connectToServer() 
{
  NimBLEClient* pClient = nullptr;
  if(NimBLEDevice::getClientListSize() >= NIMBLE_MAX_CONNECTIONS) {
      Serial.println("Max clients reached - no more connections available");
      return 0;
  }
  pClient = NimBLEDevice::createClient();
  Serial.println("New client created");

//  pClient->setClientCallbacks(&clientCB, false);
  /** Set initial connection parameters: These settings are 15ms interval, 0 latency, 120ms timout.
   *  These settings are safe for 3 clients to connect reliably, can go faster if you have less
   *  connections. Timeout should be a multiple of the interval, minimum is 100ms.
   *  Min interval: 12 * 1.25ms = 15, Max interval: 12 * 1.25ms = 15, 0 latency, 51 * 10ms = 510ms timeout
   */
  pClient->setConnectionParams(8,12,0,51);
  /** Set how long we are willing to wait for the connection to complete (seconds), default is 30. */
  pClient->setConnectTimeout(5);


  if (!pClient->connect(advDevice)) {
      /** Created a client but failed to connect, don't need to keep it as it has no data */
    NimBLEDevice::deleteClient(pClient);
    Serial.println("Failed to connect, deleted client");
    need_scan = 1;
    return 0;
  }

  if(!pClient->isConnected()) {
      if (!pClient->connect(advDevice)) {
          Serial.println("Failed to connect");
          return false;
      }
  }

  Serial.print("Connected to: ");
  Serial.println(pClient->getPeerAddress().toString().c_str());
  Serial.print("RSSI: ");
  Serial.println(pClient->getRssi());

  /** Now we can read/write/subscribe the charateristics of the services we are interested in */
  NimBLERemoteService* pSvc = nullptr;
  NimBLERemoteCharacteristic* pChr = nullptr;
  NimBLERemoteDescriptor* pDsc = nullptr;

  Serial.println("Services:");
  std::vector<NimBLERemoteService *> *svcs = pClient->getServices(true);
  for(std::vector<NimBLERemoteService *>::iterator svc = svcs->begin(); svc != svcs->end(); ++svc)
  {
    Serial.println((*svc)->toString().c_str());
  }

  pSvc = pClient->getService(NimBLEUUID("93375900-F229-8B49-B397-44B5899B8600"));
  if(pSvc)     /** make sure it's not null */
  {
    uECG_found = 1;
    pChr = pSvc->getCharacteristic(NimBLEUUID("FC7A850D-C1A5-F61F-0DA7-9995621FBD00"));

    if(pChr) /** make sure it's not null */
    {
      if(pChr->canNotify()) {
          //if(!pChr->registerForNotify(notifyCB)) {
          if(!pChr->subscribe(true, notifyCB)) {
              /** Disconnect if subscribe failed */
              pClient->disconnect();
              return 0;
          }
      }
      else if(pChr->canIndicate()) 
      {
        /** Send false as first argument to subscribe to indications instead of notifications */
        if(!pChr->subscribe(false, notifyCB)) {
          /** Disconnect if subscribe failed */
          pClient->disconnect();
          return 0;
        }
      }
    }
  } 
  else
  {
    need_scan = 1;
    Serial.println("service not found, scanning again");
  }

  Serial.println("attempt ended");
  return 1;
}

uint32_t last_scan_start = 0;
NimBLEScan* pScan;

void ble_functions_init()
{
  NimBLEDevice::init("");
  NimBLEDevice::setSecurityAuth(false, false, false);
  NimBLEDevice::setPower(ESP_PWR_LVL_P9); /** +9db */
  pScan = NimBLEDevice::getScan();
  pScan->setAdvertisedDeviceCallbacks(new AdvertisedDeviceCallbacks());
  pScan->setInterval(16);
  pScan->setWindow(15);
  pScan->setActiveScan(true);
  pScan->start(2);
  need_scan = 1;
  last_scan_start = millis();
}

void scan_end_cb()
{
  
}
void ble_cycle()
{
  uint32_t ms = millis();
  if(need_scan && !uECG_found)
  {
    if(ms - last_scan_start > 2200)
    {
      pScan->start(2, scan_end_cb);
      last_scan_start = ms;
    }
  }
    /** Found a device we want to connect to, do it now */
  if(!need_scan && !uECG_found)
  {
    if(connectToServer()) Serial.println("Connected");
    else Serial.println("Failed to connect, starting scan");
  }

  if(uECG_found && ms - last_ecg_data > 35000)
  {
    need_scan = 1;
    uECG_found = 0;
  }
}

int get_BPM()
{
  return BPM;
}
int get_RR()
{
  return cur_RR;
}
int get_uecg_batt()
{
  return uecg_batt_mv;
}
int16_t* get_ecg_buf()
{
  return ecg_buf;
}
int get_ecg_buf_pos()
{
  return ecg_buf_pos;
}
int get_ecg_buf_len()
{
  return ecg_buf_len;
}


