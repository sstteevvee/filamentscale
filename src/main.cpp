#include <Arduino.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <HX711.h>
#include <EEPROM.h>
// #include <Fonts/FreeMonoBold24pt7b.h>
#include <Fonts/FreeSans24pt7b.h>

struct
{
  uint8_t Version;
  uint32_t Offset;
  uint16_t Reel;
  double Scale;
} config;

const int N = 5;
bool first = true;
float data[N];
int p = 0;

#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels

const int LOADCELL_DOUT_PIN = 2;
const int LOADCELL_SCK_PIN = 3;
#define OLED_ADDR 0x3C

HX711 scale;

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1); // -1 = no reset pin : not used on 4-pin OLED module

void printConfig()
{
  Serial.print(" Offset ");
  Serial.println(config.Offset);
  Serial.print(" Scale ");
  Serial.println(config.Scale, 2);
  Serial.print(" Reel ");
  Serial.println(config.Reel);
}

void loadSettings(bool defaults)
{
  Serial.println("Load Settings");
  EEPROM.get(0, config);
  if (defaults || config.Version != 2)
  {
    Serial.println(" Using defaults");
    config.Version = 2;
    config.Offset = 59000;
    config.Scale = 407.12;
    config.Reel = 240;
  }

  printConfig();

  scale.set_scale(config.Scale);
  scale.set_offset(config.Offset);
}

void saveSettings()
{
  Serial.println("Save Settings");

  config.Version = 2;
  config.Offset = scale.get_offset();
  config.Scale = scale.get_scale();

  printConfig();

  EEPROM.put(0, config);
}

void setup()
{
  EEPROM.read(0);

  Serial.begin(9600);
  Serial.println("Filament Scale");

  scale.begin(LOADCELL_DOUT_PIN, LOADCELL_SCK_PIN);

  loadSettings(false);

  display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR);
  display.clearDisplay();
  display.display();

  display.setFont(&FreeSans24pt7b);
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE, SSD1306_BLACK);
}

float averager(float value)
{
  if (first)
  {
    first = false;
    for (int i = 0; i < N; i++)
    {
      data[i] = value;
    }
    p = 1;
  }
  else
  {
    data[p] = value;
    p = (p + 1) % N;
  }

  float result = 0.0;
  for (int i = 0; i < N; i++)
  {
    result += data[i];
  }

  return result / N;
}

void doSerial()
{
  if (Serial.available() == 0)
  {
    return;
  }

  int c = Serial.read();

  Serial.print((char)c);

  static int enteredValue = 0;

  if (c == 8)
  {
    Serial.print(' ');
    Serial.print((char)8);
    enteredValue /= 10;
    return;
  }

  if (c >= '0' && c <= '9')
  {
    enteredValue = enteredValue * 10 + (c - '0');
    return;
  }

  Serial.println();

  switch (c)
  {
  case '?':
  {
    Serial.println("Z - Zero, [value]C - Calibrate, [value]R - Set reel weight, S - save, L - load, D - defaults");
    break;
  }
  case 'z':
  {
    scale.tare();
    config.Offset = scale.get_offset();

    Serial.print("Offset set to ");
    Serial.println(config.Offset);
    first = true;
    break;
  }
  case 's':
  {
    saveSettings();
    break;
  }
  case 'l':
  {
    loadSettings(false);
    break;
  }
  case 'd':
  {
    loadSettings(true);
    break;
  }
  case 'c':
  {
    float value = scale.get_value(10) / enteredValue;

    scale.set_scale(value);
    first = true;

    Serial.print("Scale for ");
    Serial.print(enteredValue);
    Serial.print(" => ");
    Serial.println(value, 2);

    enteredValue = 0;
    break;
  }
  case 'r':
  {
    config.Reel = enteredValue;
    Serial.print("Reel weight set to ");
    Serial.println(enteredValue);

    enteredValue = 0;
    break;
  }
  }
}

void loop()
{
  const float density = 1.24;  // grams/cc
  const float diameter = 1.75; // mm
  const float radius = diameter / 2.0;
  const float area = M_PI * radius * radius; // cross sectional area
  const float gramsPerMetre = area * density;

  const int maxWeight = 1000;
  float grams = averager(scale.get_units(1));
  float filamentGrams = grams - config.Reel;
  float distance = filamentGrams / gramsPerMetre;

  display.clearDisplay();

  int numLen = (int)ceil(log10(abs(distance)));
  if (distance < 0)
  {
    numLen += 1;
  }

  int xpos = (128 - (28 * (numLen + 1))) / 2;
  display.setCursor(xpos, 44);
  display.print(distance, 0);
  display.println("m");

  const int barY = 63 - 8;
  const int barW = 124;
  int barLength = (barW * filamentGrams + maxWeight / 2) / maxWeight;
  if (barLength < 0)
  {
    barLength = 0;
  }
  else if (barLength > barW)
  {
    barLength = barW;
  }

  display.drawRect(0, barY, 128, 8, SSD1306_WHITE);
  display.fillRect(2, barY + 2, barLength, 4, SSD1306_WHITE);

  display.display();

  doSerial();
}
