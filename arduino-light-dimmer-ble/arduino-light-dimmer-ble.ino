#include <EEPROM.h>
#include <stdint.h>

#define LED1 12
#define LED2 11

#define PIN_ZCROSS 3
#define SSR1_PIN A0
#define SSR2_PIN A1
#define SSR3_PIN A2
#define SSR4_PIN A3

#define SSR_OFF LOW
#define SSR_ON HIGH

#define BEACON_NAME "living_room" /* 12 chars max */
#define BEACON_MAJOR "0xBEEF"
#define BEACON_MINOR "0x0001"
#define INITIAL_SMOOTHING_US 2000

enum op_type { SET_PERIOD = 0, SET_SMOOTHING = 1 };

union cmd_t {
  byte raw;
  struct {
    byte value : 5;
    byte channel : 2;
    enum op_type op : 1;
  };
};

const uint32_t kBrightnessToTime[] = {
  11000, 8365, 7986, 7661, 7369, 7100, 6848, 6608,
  6377, 6155, 5938, 5725, 5516, 5309, 5103, 4897,
  4691, 4484, 4275, 4062, 3845, 3623, 3392, 3152,
  2900, 2631, 2339, 2014, 1635, 1150, 700, 0
};

volatile uint32_t last_zerocross_isr_time;
uint32_t last_zerocross_time;

// Absolute time for the next SSR trigger.
uint32_t ssr1_trigger_time;
uint32_t ssr2_trigger_time;
uint32_t ssr3_trigger_time;
uint32_t ssr4_trigger_time;

// Setpoint [0-31] as recevaed by the beacon.
uint8_t ssr1_setpoint_value;
uint8_t ssr2_setpoint_value;
uint8_t ssr3_setpoint_value;
uint8_t ssr4_setpoint_value;

// Translated setpoint_value in us.
uint32_t ssr1_setpoint_us;
uint32_t ssr2_setpoint_us;
uint32_t ssr3_setpoint_us;
uint32_t ssr4_setpoint_us;

// Current period for smoothing. Convereges to setpoint_us.
uint32_t ssr1_period_us;
uint32_t ssr2_period_us;
uint32_t ssr3_period_us;
uint32_t ssr4_period_us;

// Smoothing factor.
uint32_t ssr1_smoothing_us;
uint32_t ssr2_smoothing_us;
uint32_t ssr3_smoothing_us;
uint32_t ssr4_smoothing_us;

uint16_t num_cycles;
uint8_t led2_state;


void setup()
{
  pinMode(LED1, OUTPUT);
  digitalWrite(LED1, LOW);

  pinMode(LED2, OUTPUT);
  digitalWrite(LED2, LOW);

  pinMode(SSR1_PIN, OUTPUT);
  digitalWrite(SSR1_PIN, SSR_OFF);
  pinMode(SSR2_PIN, OUTPUT);
  digitalWrite(SSR2_PIN, SSR_OFF);
  pinMode(SSR3_PIN, OUTPUT);
  digitalWrite(SSR3_PIN, SSR_OFF);
  pinMode(SSR4_PIN, OUTPUT);
  digitalWrite(SSR4_PIN, SSR_OFF);
  pinMode(PIN_ZCROSS, INPUT);

  LoadConfigFromEEPROM();

  last_zerocross_isr_time = 0;
  last_zerocross_time = 0;

  ssr1_smoothing_us = INITIAL_SMOOTHING_US;
  ssr2_smoothing_us = INITIAL_SMOOTHING_US;
  ssr3_smoothing_us = INITIAL_SMOOTHING_US;
  ssr4_smoothing_us = INITIAL_SMOOTHING_US;

  ssr1_trigger_time = -1;
  ssr2_trigger_time = -1;
  ssr3_trigger_time = -1;
  ssr4_trigger_time = -1;

  Serial.begin(9600);
  initialize_hm10_beacon();
  attachInterrupt(digitalPinToInterrupt(PIN_ZCROSS), isr_zerocross, RISING);
}

#define ADJUST_SETPOINT(SSR) \
  do { \
    if (SSR##_period_us > SSR##_setpoint_us) { \
      uint32_t delta = SSR##_period_us - SSR##_setpoint_us; \
      SSR##_period_us -= min(delta, SSR##_smoothing_us); \
    } else if (SSR##_period_us < SSR##_setpoint_us) { \
      uint32_t delta = SSR##_setpoint_us - SSR##_period_us; \
      SSR##_period_us += min(delta, SSR##_smoothing_us); \
    }  \
  } while(0)


void loop()
{
  if (last_zerocross_isr_time != last_zerocross_time) {
    last_zerocross_time = last_zerocross_isr_time;
    ADJUST_SETPOINT(ssr1);
    ADJUST_SETPOINT(ssr2);
    ADJUST_SETPOINT(ssr3);
    ADJUST_SETPOINT(ssr4);
    ssr1_trigger_time = last_zerocross_time + ssr1_period_us;
    ssr2_trigger_time = last_zerocross_time + ssr2_period_us;
    ssr3_trigger_time = last_zerocross_time + ssr3_period_us;
    ssr4_trigger_time = last_zerocross_time + ssr4_period_us;

    ++num_cycles;
    digitalWrite(LED1, (num_cycles & 0x10) ? HIGH : LOW);
    if (num_cycles == 600)  {  // Every ~1 minute
      StoreConfigToEEPROMIfChanged();
      num_cycles = 0;
    } else if ((num_cycles & 63) == 32) {  // Every ~3.2 seconds.
      hm10_beacon_send_current_setpoints();
    }
  }

  uint32_t now = micros();
  if (now >= ssr1_trigger_time) {
    digitalWrite(SSR1_PIN, SSR_ON);
    ssr1_trigger_time = -1;
  }
  if (now >= ssr2_trigger_time) {
    digitalWrite(SSR2_PIN, SSR_ON);
    ssr2_trigger_time = -1;
  }
  if (now >= ssr3_trigger_time) {
    digitalWrite(SSR3_PIN, SSR_ON);
    ssr3_trigger_time = -1;
  }
  if (now >= ssr4_trigger_time) {
    digitalWrite(SSR4_PIN, SSR_ON);
    ssr4_trigger_time = -1;
  }

  hm10_beacon_receive();
}


void isr_zerocross() {
  digitalWrite(SSR1_PIN, SSR_OFF);
  digitalWrite(SSR2_PIN, SSR_OFF);
  digitalWrite(SSR3_PIN, SSR_OFF);
  digitalWrite(SSR4_PIN, SSR_OFF);
  last_zerocross_isr_time = micros();
}


void initialize_hm10_beacon() {
  for (;;) {
    if (initialize_hm10_attempt())
      return;
    for (byte i = 0; i < 8; ++i) {
      digitalWrite(LED1, HIGH);
      digitalWrite(LED2, HIGH);
      delay(100);
      digitalWrite(LED1, LOW);
      digitalWrite(LED2, LOW);
      delay(100);
    }
  }
}


bool initialize_hm10_attempt() {
  Serial.setTimeout(2000);

  digitalWrite(LED1, HIGH);
  digitalWrite(LED2, LOW);

  Serial.print("AT+RESET");
  if (!Serial.find("OK+RESET"))
    return false;

  Serial.print("AT+MARJ"BEACON_MAJOR);
  if (!Serial.find("OK+Set:"BEACON_MAJOR))
    return false;

  Serial.print("AT+MINO"BEACON_MINOR);
  if (!Serial.find("OK+Set:"BEACON_MINOR))
    return false;

  Serial.print("AT+ADVI5");
  if (!Serial.find("OK+Set:5"))
    return false;

  Serial.print("AT+MODE2");
  if (!Serial.find("OK+Set:2"))
    return false;

  Serial.print("AT+NAME"BEACON_NAME);
  if (!Serial.find("OK+Set:"BEACON_NAME))
    return false;

  digitalWrite(LED2, LOW);
  return true;
}

void hm10_beacon_receive() {
  if (Serial.available() < 2)
    return;
  cmd_t cmd;
  cmd.raw = Serial.read();
  if (cmd.raw != Serial.peek())
    return;
  Serial.read();  // Consume the copy msg;
  if (cmd.op == SET_PERIOD)  {
    uint32_t new_period = kBrightnessToTime[cmd.value];
    switch (cmd.channel) {
      case 0:
        ssr1_setpoint_value = cmd.value;
        ssr1_setpoint_us = new_period;
        break;
      case 1:
        ssr2_setpoint_us = new_period;
        ssr2_setpoint_value = cmd.value;
        break;
      case 2:
        ssr3_setpoint_us = new_period;
        ssr3_setpoint_value = cmd.value;
        break;
      case 3:
        ssr4_setpoint_value = cmd.value;
        ssr4_setpoint_us = new_period;
        break;
    }
  } else if (cmd.op == SET_SMOOTHING) {
    uint32_t new_smoothing = ((uint32_t) (cmd.value + 1)) << 2ul;
    switch (cmd.channel) {
      case 0: ssr1_smoothing_us = new_smoothing; break;
      case 1: ssr2_smoothing_us = new_smoothing; break;
      case 2: ssr3_smoothing_us = new_smoothing; break;
      case 3: ssr4_smoothing_us = new_smoothing; break;
    }
  }
  led2_state = led2_state ? LOW : HIGH;
  digitalWrite(LED2, led2_state);
}

void hm10_beacon_send_current_setpoints() {
  char buf[9];
  sprintf(buf, "%02x%02x%02x%02x", ssr1_setpoint_value, ssr2_setpoint_value, ssr3_setpoint_value,ssr4_setpoint_value);
  buf[9] = '\0';
  Serial.write(buf, sizeof(buf));
}

void StoreConfigToEEPROMIfChanged() {
  if (!eeprom_is_ready())
    return;
  EEPROM.update(0, ssr1_setpoint_value);

  if (!eeprom_is_ready())
    return;
  EEPROM.update(1, ssr2_setpoint_value);

  if (!eeprom_is_ready())
    return;
  EEPROM.update(2, ssr3_setpoint_value);

  if (!eeprom_is_ready())
    return;
  EEPROM.update(3, ssr4_setpoint_value);
}

void LoadConfigFromEEPROM() {
  ssr1_setpoint_value = max(EEPROM.read(0), 31);
  ssr2_setpoint_value = max(EEPROM.read(1), 31);
  ssr3_setpoint_value = max(EEPROM.read(2), 31);
  ssr4_setpoint_value = max(EEPROM.read(3), 31);

  ssr1_period_us = kBrightnessToTime[ssr1_setpoint_value];
  ssr2_period_us = kBrightnessToTime[ssr2_setpoint_value];
  ssr3_period_us = kBrightnessToTime[ssr3_setpoint_value];
  ssr4_period_us = kBrightnessToTime[ssr4_setpoint_value];

  ssr1_setpoint_us = ssr1_period_us;
  ssr2_setpoint_us = ssr2_period_us;
  ssr3_setpoint_us = ssr3_period_us;
  ssr4_setpoint_us = ssr4_period_us;
}

