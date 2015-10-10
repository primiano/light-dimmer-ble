#include <EEPROM.h>
#include <stdint.h>

#define BEACON_NAME "living_room" /* 12 chars max */
#define BEACON_MAJOR "0xBEEF"
#define BEACON_MINOR "0x0001"

#define LED1 12
#define LED2 11

#define PIN_ZCROSS 3
#define SSR1_PIN A3
#define SSR2_PIN A0
#define SSR3_PIN A1
#define SSR4_PIN A2
#define INITIAL_SMOOTHING_US 2000

// Maps a brigthness value [0-31] to a duty-cycle value in the range
// [0-10000] ms (for 50 Hz mains), keeping into account the energy
// distribution of the mains sinewave.
const uint16_t kBrightnessDutyCycleMap[] = {
  11000, 8365, 7986, 7661, 7369, 7100, 6848, 6608,
  6377, 6155, 5938, 5725, 5516, 5309, 5103, 4897,
  4691, 4484, 4275, 4062, 3845, 3623, 3392, 3152,
  2900, 2631, 2339, 2014, 1635, 1150, 700, 0
};

// Data type for the messages exhanged via Bluetooth.
enum op_type_t { SET_PERIOD = 0, SET_SMOOTHING = 1 };
union cmd_t {
  byte raw;
  struct {
    byte value : 5;
    byte channel : 2;
    op_type_t op : 1;
  };
};


volatile uint8_t g_zerocross_isr_flag;
volatile uint32_t g_zerocross_time;
uint32_t g_now;
uint16_t g_num_cycles;
uint8_t g_led2_state;
uint8_t g_mins_since_last_remote_command;


template<unsigned int PIN>
class DimmerChannel {
  public:
    void initialize() {
      pinMode(PIN, OUTPUT);
      digitalWrite(PIN, LOW);
      next_trigger_time_ = -1;
      brightness_ = EEPROM.read(PIN);
      if (brightness_ > 31)
        brightness_ = 0;
      target_duty_cycle_us_ = kBrightnessDutyCycleMap[brightness_];
      current_duty_cycle_us_ = kBrightnessDutyCycleMap[0];
      duty_cycle_smoothing_rate_us_ = INITIAL_SMOOTHING_US;
    }

    void on_zero_cross() {
      digitalWrite(PIN, LOW);

      // Perform smoothing adjustement to converge towards the
      // |target_duty_cycle_us| set-point.
      if (current_duty_cycle_us_ > target_duty_cycle_us_) {
        uint16_t delta = current_duty_cycle_us_ - target_duty_cycle_us_;
        current_duty_cycle_us_ -= min(delta, duty_cycle_smoothing_rate_us_);
      } else if (current_duty_cycle_us_ < target_duty_cycle_us_) {
        uint16_t delta = target_duty_cycle_us_ - current_duty_cycle_us_;
        current_duty_cycle_us_ += min(delta, duty_cycle_smoothing_rate_us_);
      }

      // Calculate the absolute time for the next SSR trigger.
      next_trigger_time_ = g_now + current_duty_cycle_us_;
    }

    void store_brightness_in_eeprom() {
      // Bail out if the EEPROM is busy and the write would block due to a prior
      // pending write. We cannot stall the SSR triggers for ~3ms (which is the
      // time that an EEPROM write cycle takes) as it would generate a visible
      // glitch.
      if (!eeprom_is_ready())
        return;
      EEPROM.update(PIN, brightness_);
    }

    void trigger_ssr_if_time() {
      if (g_now >= next_trigger_time_) {
        digitalWrite(PIN, HIGH);
        next_trigger_time_ = -1;
      }
    }

    inline uint8_t brightness() const {
       return brightness_;
    }

    void set_brightness(uint8_t value) {
      brightness_ = min(value, 31);
      target_duty_cycle_us_ = kBrightnessDutyCycleMap[brightness_];
    }

    inline uint16_t duty_cycle_smoothing_rate_us() const {
       return duty_cycle_smoothing_rate_us_;
    }

    void set_duty_cycle_smoothing_rate(uint8_t value) {
      duty_cycle_smoothing_rate_us_ = ((uint16_t) (value + 1)) << 2;
    }

  private:
    // Absolute time (w.r.t. micros()) for the next SSR trigger.
    uint32_t next_trigger_time_;

    // At every mains cycle |current_duty_cycle_us| is adjusted of at most
    // +- |duty_cycle_smoothing_rate_us| in order to converge towards
    // |target_duty_cycle_us|.
    uint16_t duty_cycle_smoothing_rate_us_;
    uint16_t current_duty_cycle_us_;

    // |target_duty_cycle_us| is |brightness| after the logarithmic
    // transformation to get the equivalent duty cycle [0-10.000] ms
    // in the mains sinewave.
    uint16_t target_duty_cycle_us_;
    uint8_t brightness_;
};

DimmerChannel<SSR1_PIN> ch1;
DimmerChannel<SSR2_PIN> ch2;
DimmerChannel<SSR3_PIN> ch3;
DimmerChannel<SSR4_PIN> ch4;


void setup()
{
  pinMode(LED1, OUTPUT);
  digitalWrite(LED1, LOW);

  pinMode(LED2, OUTPUT);
  digitalWrite(LED2, LOW);

  pinMode(PIN_ZCROSS, INPUT);

  ch1.initialize();
  ch2.initialize();
  ch3.initialize();
  ch4.initialize();

  g_mins_since_last_remote_command = 0;
  Serial.begin(9600);
  initialize_hm10_beacon();

  g_zerocross_isr_flag = 0;
  g_now = micros();
  attachInterrupt(digitalPinToInterrupt(PIN_ZCROSS), isr_zerocross, RISING);
}

void loop()
{
  if (g_zerocross_isr_flag) {
    g_now = g_zerocross_time;
    g_zerocross_isr_flag = 0;
    ch1.on_zero_cross();
    ch2.on_zero_cross();
    ch3.on_zero_cross();
    ch4.on_zero_cross();

    ++g_num_cycles;
    digitalWrite(LED1, (g_num_cycles & 0x10) ? HIGH : LOW);
    if (g_num_cycles == 6000)  {  // Every ~10 mins
      ++g_mins_since_last_remote_command;
      if (g_mins_since_last_remote_command >= 24)  // Every ~4 hours.
        turn_all_off_smoothly();
      ch1.store_brightness_in_eeprom();
      ch2.store_brightness_in_eeprom();
      ch3.store_brightness_in_eeprom();
      ch4.store_brightness_in_eeprom();
      g_num_cycles = 0;
    } else if ((g_num_cycles & 63) == 32) {  // Every ~3.2 seconds.
      hm10_beacon_send_current_brightness();
    }
  }

  g_now = micros();
  ch1.trigger_ssr_if_time();
  ch2.trigger_ssr_if_time();
  ch3.trigger_ssr_if_time();
  ch4.trigger_ssr_if_time();
  hm10_beacon_receive();
}

void isr_zerocross() {
  g_zerocross_time = micros();
  g_zerocross_isr_flag = 1;
}

void turn_all_off_smoothly() {
  ch1.set_duty_cycle_smoothing_rate(1);
  ch1.set_brightness(0);
  ch2.set_duty_cycle_smoothing_rate(1);
  ch2.set_brightness(0);
  ch3.set_duty_cycle_smoothing_rate(1);
  ch3.set_brightness(0);
  ch4.set_duty_cycle_smoothing_rate(1);
  ch4.set_brightness(0);
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
    switch (cmd.channel) {
      case 0: ch1.set_brightness(cmd.value); break;
      case 1: ch2.set_brightness(cmd.value); break;
      case 2: ch3.set_brightness(cmd.value); break;
      case 3: ch4.set_brightness(cmd.value); break;
    }
  } else { /* cmd.op == SET_SMOOTHING */
    switch (cmd.channel) {
      case 0: ch1.set_duty_cycle_smoothing_rate(cmd.value); break;
      case 1: ch2.set_duty_cycle_smoothing_rate(cmd.value); break;
      case 2: ch3.set_duty_cycle_smoothing_rate(cmd.value); break;
      case 3: ch4.set_duty_cycle_smoothing_rate(cmd.value); break;
    }
  }
  g_led2_state = g_led2_state ? LOW : HIGH;
  digitalWrite(LED2, g_led2_state);
  g_mins_since_last_remote_command = 0;
}

void hm10_beacon_send_current_brightness() {
  char buf[9];
  sprintf(buf,
          "%02x%02x%02x%02x",
          ch1.brightness(),
          ch2.brightness(),
          ch3.brightness(),
          ch4.brightness());
  buf[9] = '\0';
  Serial.write(buf, sizeof(buf));
}

