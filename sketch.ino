// Prabhashana M.R.M.
// 210486F


// Include libraries
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <DHTesp.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <ESP32Servo.h>


// Define OLED Parameters
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define SCREEN_ADDRESS 0x3c

#define BUZZER 5
#define LED_1 15
#define PB_CANCEL 25
#define PB_OK 32
#define PB_UP 33
#define PB_DOWN 26
#define DHTPIN 12
#define LIGHT_SENSOR_PIN1 34
#define LIGHT_SENSOR_PIN2 35
#define SERVO_PIN 19

#define NTP_SERVER     "pool.ntp.org"
#define UTC_OFFSET     0
#define UTC_OFFSET_DST 0

// Declare Objects
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
DHTesp dhtSensor;
WiFiClient espClient;
PubSubClient mqttClient(espClient);
Servo myServo;


// Global Variables
// Time Parameters
int days = 0;
int hours = 0;
int minutes = 0;
int seconds = 0;
String dayName = "";

// Alarm Parameters
bool alarm_enabled = true;
int n_alarms = 3;
int alarm_hours[] = {0, 1, 2};
int alarm_minutes[] = {1, 10, 20};
bool alarm_triggered[] = {false, false, false};

// Buzzer Parameters
int n_notes = 8;
int C = 262;
int D = 294;
int E = 330;
int F = 349;
int G = 392;
int A = 440;
int B = 494;
int C_H = 523;
int notes[] = {C, D, E, F, G, A, B, C_H};
char leftLDRArr[6];
char rightLDRArr[6];
char tempArr[6];
char humidityArr[6];
char currentServoAngle[6];

// Menu Options
int current_mode = 0;
int max_modes = 5;
String modes[] = {"1 - Change Time Offset", "2 - Set Alarm 1", "3 - Set Alarm 2", "4 - Set Alarm 3",  "5 - Disable Alarms"};

// Dashboard Parameters
float min_angle = 30;
float controlling_factor = 0.75;
float max_intensity = 0;
float val_D = 0;



void setup() {
  // put your setup code here, to run once:

  pinMode(BUZZER, OUTPUT);
  pinMode(LED_1, OUTPUT);
  pinMode(PB_CANCEL, INPUT);
  pinMode(PB_OK, INPUT);
  pinMode(PB_UP, INPUT);
  pinMode(PB_DOWN, INPUT);


  dhtSensor.setup(DHTPIN, DHTesp::DHT22);
  myServo.attach(SERVO_PIN);

  Serial.begin(9600);
  if (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println(F("SSD1306 allocation is failed"));
    for (;;);
  }

  display.display();
  delay(500);

  setupWiFi();
  setupMqtt();

  display.clearDisplay();
  configTime(UTC_OFFSET, UTC_OFFSET_DST, NTP_SERVER);

  print_line("Welcome to MediBox!", 0, 0, 2);
  display.clearDisplay();
  delay(500);
}



void loop() {
  if (!mqttClient.connected()) {
    connectToBroker();
  }

  mqttClient.loop();

  display.clearDisplay();
  update_time_with_check_alarm();
  check_temp();
  
  check_light();
  mqttClient.publish("MEDI-LEFT-LDR", leftLDRArr);
  mqttClient.publish("MEDI-RIGHT-LDR", rightLDRArr);
  mqttClient.publish("MEDI-TEMP", tempArr);
  mqttClient.publish("MEDI-HUMIDITY", humidityArr);
  mqttClient.publish("MEDI-CURRENT-SERVOANGLE", currentServoAngle);
  adjustServo();

  if (digitalRead(PB_OK) == LOW) {
    delay(200);
    go_to_menu();
  }
}


// Function to establish the wifi connection
void setupWiFi() {
  display.clearDisplay();

  WiFi.begin("Wokwi-GUEST", "", 6);
  while (WiFi.status() != WL_CONNECTED) {
    delay(250);
    display.clearDisplay();
    print_line("Connecting...", 0, 0, 2);
  }

  display.clearDisplay();
  print_line("Connected to WIFI", 0, 0, 2);
  delay(500);
}


// Function to establish the MQTT connection to NodeRed
void setupMqtt() {
  mqttClient.setServer("test.mosquitto.org", 1883);
  mqttClient.setCallback(receiveCallback);
}

void connectToBroker() {
  while (!mqttClient.connected()) {
    Serial.print("Attempting MQTT Connection...");

    if (mqttClient.connect("ESP32-345464")) {
      Serial.println("Connected");
      mqttClient.subscribe("MEDI-MIN-ANGLE");
      mqttClient.subscribe("MEDI-CF");
      mqttClient.subscribe("MEDI-MAX-INTENSITY");
      mqttClient.subscribe("MEDI-VAL-D");

    } else {
      Serial.print("Failed");
      Serial.println(mqttClient.state());
      delay(5000);
    }
  }
}


void receiveCallback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");

  char payloadCharAr[length];

  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
    payloadCharAr[i] = (char)payload[i];
  }

  Serial.println();
  float value = atof(payloadCharAr);

  if (strcmp(topic, "MEDI-MIN-ANGLE") == 0) {
    min_angle = value;
  } else if (strcmp(topic, "MEDI-CF") == 0) {
    controlling_factor = value;
  } else if (strcmp(topic, "MEDI-MAX-INTENSITY") == 0) {
    max_intensity = value;
  } else if (strcmp(topic, "MEDI-VAL-D") == 0) {
    val_D = value;
  }
  
}



void print_line(String text, int column, int row, int text_size) {
  // Change the parameters of text
  display.setTextSize(text_size);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(column, row);
  display.println(text);

  // Display the text
  display.display();
}


void update_time_with_check_alarm(void) {
  update_time();
  print_time_now();

  if (alarm_enabled == true) {
    for (int i = 0; i < n_alarms; i++) {
      if (alarm_triggered[i] == false && alarm_hours[i] == hours && alarm_minutes[i] == minutes) {
        ring_alarm();
        alarm_triggered[i] = true;
      }
    }
  }
}

void update_time() {
  struct tm timeinfo;
  getLocalTime(&timeinfo);

  char timeHour[3];
  strftime(timeHour, 3, "%H", &timeinfo);
  hours = atoi(timeHour);

  char timeMinute[3];
  strftime(timeMinute, 3, "%M", &timeinfo);
  minutes = atoi(timeMinute);

  char timeSecond[3];
  strftime(timeSecond, 3, "%S", &timeinfo);
  seconds = atoi(timeSecond);

  char timeDay[3];
  strftime(timeDay, 3, "%d", &timeinfo);
  days = atoi(timeDay);

  char timeDayName[5];
  strftime(timeDayName, sizeof(timeDayName), "%a", &timeinfo);
  dayName = timeDayName;
}



void print_time_now(void) {
  String date = dayName + ". " + String(days);
  String currentTIme = date + "\n" + String(hours) + ":" + String(minutes) + ":" + String(seconds);
  print_line(currentTIme, 0, 0, 2);
}



void ring_alarm() {
  bool break_alarm = false;

  // Display Message
  display.clearDisplay();
  print_line("MEDICINE TIME!", 0, 0, 2);

  // Turn on the LED
  digitalWrite(LED_1, HIGH);


  // Ring the Buzzer
  while (break_alarm == false && digitalRead(PB_CANCEL) == HIGH) {
    for (int i = 0; i < n_notes; i++) {
      if (digitalRead(PB_CANCEL) == LOW) {
        delay(200);
        break_alarm = true;
        break;
      }

      tone(BUZZER, notes[i]);
      delay(500);
      noTone(BUZZER);
      delay(2);
    }
  }

  // Turn off the LED
  digitalWrite(LED_1, LOW);
  display.clearDisplay();
}



void go_to_menu() {
  while (digitalRead(PB_CANCEL) == HIGH) {
    delay(200);
    display.clearDisplay();
    print_line(modes[current_mode], 0, 0, 2);
    delay(1000);

    int pressed = wait_for_button_press();

    if (pressed == PB_UP) {
      delay(200);
      current_mode += 1;
      current_mode = current_mode % max_modes;
    }
    else if (pressed == PB_DOWN) {
      delay(200);
      current_mode -= 1;
      if (current_mode < 0) {
        current_mode = max_modes - 1;
      }
    }
    else if (pressed == PB_OK) {
      delay(200);
      run_mode(current_mode);
    }
    else if (pressed == PB_CANCEL) {
      delay(200);
      break;
    }
  }
}


// Button pressing signal
int wait_for_button_press() {
  while (true) {
    if (digitalRead(PB_UP) == LOW) {
      delay(200);
      return PB_UP;
    }
    else if (digitalRead(PB_DOWN) == LOW) {
      delay(200);
      return PB_DOWN;
    }
    else if (digitalRead(PB_OK) == LOW) {
      delay(200);
      return PB_OK;
    }
    else if (digitalRead(PB_CANCEL) == LOW) {
      delay(200);
      return PB_CANCEL;
    }

    update_time();
  }
}


void run_mode(int mode) {
  if (mode == 0) {
    set_Offset_time();
  }
  else if (mode == 1 || mode == 2 || mode == 3) {
    set_alarm(mode - 1);
  }
  else if (mode == 4) {
    alarm_enabled = false;
    display.clearDisplay();
    print_line("Alarms Disabled", 0, 0, 2);
    delay(500);
  }
}


void set_Offset_time() {
  int temp_hours = 0;
  int temp_minutes = 0;
  String temp_sign = "+";
  bool isCancel = false;

  while (true && !isCancel) {
    display.clearDisplay();
    print_line("Enter Offset sign: " + temp_sign, 0, 0, 2);

    int pressed = wait_for_button_press();
    if (pressed == PB_UP || pressed == PB_DOWN) {
      delay(200);
      if (temp_sign == "+") {
        temp_sign = "-";
      } else if (temp_sign == "-") {
        temp_sign = "+";
      }
    }
    else if (pressed == PB_OK) {
      delay(200);
      break;
    }
    else if (pressed == PB_CANCEL) {
      delay(200);
      isCancel = true;
      break;
    }
  }

  while (true && !isCancel) {
    display.clearDisplay();
    print_line("Enter Offset hours: " + String(temp_hours), 0, 0, 2);

    int pressed = wait_for_button_press();
    if (pressed == PB_UP) {
      delay(200);
      temp_hours += 1;
      if (temp_hours > 12) {
        temp_hours = -12;
      }
    }
    else if (pressed == PB_DOWN) {
      delay(200);
      temp_hours -= 1;
      if (temp_hours < -12) {
        temp_hours = 12;
      }
    }
    else if (pressed == PB_OK) {
      delay(200);
      break;
    }
    else if (pressed == PB_CANCEL) {
      delay(200);
      isCancel = true;
      break;
    }
  }

  while (true && !isCancel) {
    display.clearDisplay();
    print_line("Enter Offset minutes: " + String(temp_minutes), 0, 0, 2);

    int pressed = wait_for_button_press();
    if (pressed == PB_UP) {
      delay(200);
      temp_minutes += 15;
      temp_minutes = temp_minutes % 60;
    }
    else if (pressed == PB_DOWN) {
      delay(200);
      temp_minutes -= 15;
      if (temp_minutes < 0) {
        temp_minutes = 45;
      }
    }
    else if (pressed == PB_OK) {
      delay(200);
      int offset = temp_hours * 3600 + temp_minutes * 60;

      if (temp_sign == "-") {
        offset = 0 - offset;
      } else {
        offset = offset;
      }

      configTime(offset, UTC_OFFSET_DST, NTP_SERVER);
      display.clearDisplay();
      print_line("Time is set", 0, 0, 2);
      delay(1000);
      break;
    }
    else if (pressed == PB_CANCEL) {
      delay(200);
      break;
    }
  }

  if (isCancel) {
    isCancel = false;
  }
}


void set_alarm(int alarm) {
  int temp_hours = alarm_hours[alarm];
  int temp_minutes = alarm_minutes[alarm];
  bool isCancel = false;
  alarm_triggered[alarm] = false;

  while (true && !isCancel) {
    display.clearDisplay();
    print_line("Enter hours: " + String(temp_hours), 0, 0, 2);

    int pressed = wait_for_button_press();
    if (pressed == PB_UP) {
      delay(200);
      temp_hours += 1;
      temp_hours = temp_hours % 24;
    }
    else if (pressed == PB_DOWN) {
      delay(200);
      temp_hours -= 1;
      if (temp_hours < 0) {
        temp_hours = 23;
      }
    }
    else if (pressed == PB_OK) {
      delay(200);
      alarm_hours[alarm] = temp_hours;
      break;
    }
    else if (pressed == PB_CANCEL) {
      delay(200);
      isCancel = true;
      break;
    }
  }

  while (true && !isCancel) {
    display.clearDisplay();
    print_line("Enter minutes: " + String(temp_minutes), 0, 0, 2);

    int pressed = wait_for_button_press();
    if (pressed == PB_UP) {
      delay(200);
      temp_minutes += 1;
      temp_minutes = temp_minutes % 60;
    }
    else if (pressed == PB_DOWN) {
      delay(200);
      temp_minutes -= 1;
      if (temp_minutes < 0) {
        temp_minutes = 59;
      }
    }
    else if (pressed == PB_OK) {
      delay(200);
      alarm_minutes[alarm] = temp_minutes;
      alarm_enabled = true;   // Alarms should be enabled for ringing.
      display.clearDisplay();
      print_line("Alarm " + String(alarm + 1) + " is set", 0, 0, 2);
      delay(500);
      break;
    }
    else if (pressed == PB_CANCEL) {
      delay(200);
      break;
    }
  }
  if (isCancel) {
    isCancel = false;
  }
}


void alarmWarning(int freq) {
  tone(BUZZER, notes[freq]);
  delay(100);
  noTone(BUZZER);
  delay(100);
}


void check_temp() {
  TempAndHumidity data = dhtSensor.getTempAndHumidity();
  float temp = data.temperature;
  float humidity = data.humidity;

  String(temp).toCharArray(tempArr, 6);
  String(humidity).toCharArray(humidityArr, 6);

  String tempCondtion = "";
  String humidityCondtion = "";

  if (data.temperature > 35) {
    tempCondtion = "TEMP HIGH";
    alarmWarning(0);
  }
  else if (data.temperature < 25) {
    tempCondtion = "TEMP LOW";
    alarmWarning(0);
  }

  if (data.humidity > 40) {
    humidityCondtion = "HUMIDITY HIGH";
    alarmWarning(5);
  }
  else if (data.humidity < 20) {
    humidityCondtion = "HUMIDITY LOW";
    alarmWarning(5);
  }

  print_line(tempCondtion + "\n" + humidityCondtion, 0, 40, 1);
  delay(400);
}


void check_light() {
  // reads the input on analog pin (value between 0 and 4095)
  int LDR_LEFT = analogRead(LIGHT_SENSOR_PIN1);
  int LDR_RIGHT = analogRead(LIGHT_SENSOR_PIN2);

  String(LDR_LEFT).toCharArray(leftLDRArr, 6);
  String(LDR_RIGHT).toCharArray(rightLDRArr, 6);
}


void adjustServo() {
  float motor_angle = min_angle * val_D + (180 - min_angle) * max_intensity * controlling_factor;

  if (motor_angle > 180) {
    motor_angle = 180;
  }

  String(motor_angle).toCharArray(currentServoAngle, 6);
  myServo.write((int) motor_angle);
  Serial.print("Servo angle set to ");
  Serial.println(motor_angle);
}