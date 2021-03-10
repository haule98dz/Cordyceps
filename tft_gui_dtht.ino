#include <MCUFRIEND_kbv.h>
#include <Adafruit_GFX.h>    // Core graphics library
#include <C:\font\FontConvert\roboto24pt.h>
#include <C:\font\FontConvert\roboto-black-8pt.h>
#include <C:\font\FontConvert\hand16pt.h>
#include <C:\font\FontConvert\test.h>  // hand24pt
#include <C:\font\FontConvert\ss10pt.h>
#include <C:\font\FontConvert\ss13pt.h>
#include <C:\font\FontConvert\ss16pt.h>
#include <C:\font\FontConvert\ss20pt.h>
#include <avr/pgmspace.h>
#include <TouchScreen.h>
//#include <arduino-timer.h>
#include "img.h"

#include <Adafruit_TFTLCD.h> // Hardware-specific library
#include <SD.h>
#include <SPI.h>

#define LCD_CS A3 // Chip Select goes to Analog 3
#define LCD_CD A2 // Command/Data goes to Analog 2
#define LCD_WR A1 // LCD Write goes to Analog 1
#define LCD_RD A0 // LCD Read goes to Analog 0
#define PIN_SD_CS 10 // Adafruit SD shields and modules: pin 10 
//
#define LCD_RESET A4 // Can alternately just connect to Arduino's reset pin
#define LCD_CS A3   // Chip Select goes to Analog 3
#define LCD_CD A2  // Command/Data goes to Analog 2
#define LCD_WR A1  // LCD Write goes to Analog 1
#define LCD_RD A0 // LCD Read goes to Analog 0

#define YP A3  // must be an analog pin, use "An" notation!
#define XM A2  // must be an analog pin, use "An" notation!
#define YM 9   // can be a digital pin
#define XP 8   // can be a digital pin

//Param For 3.5"
#define TS_MINX 118
#define TS_MAXX 906

#define TS_MINY 92
#define TS_MAXY 951

// Assign human-readable names to some common 16-bit color values:
#define BLACK   0x0000
#define BLUE    0x001F
#define RED     0xF800
#define GREEN   0x07E0
#define CYAN    0x07FF
#define MAGENTA 0xF81F
#define YELLOW  0xFFE0
#define WHITE   0xFFFF
#define Transparent GREEN
#define Backcolor 0x967

#define MAX 4   //gradient max stop point

#define MAX_BMP         10                      // bmp file num
#define FILENAME_LEN    20                      // max file name length

#define MINPRESSURE 10
#define MAXPRESSURE 1000

#define MAIN 0
#define SETUP 1
#define ENVIR 2
#define HT_INPUT 3
#define WIFI_PASSWORD 4
#define WIFI_LIST 5
#define WIFI_STATUS 6

#define AUTO 0
#define MANUAL 1
#define Backcolor24 {8, 44, 54}
#define topBarBackground 0x1a6b

MCUFRIEND_kbv tft;
TouchScreen ts = TouchScreen(XP, YP, XM, YM, 300);  //300 Ohm
int wscr = 480;
int hscr = 320;
int tx, ty, tz;
bool touched, modeChangedRecently = false;
uint8_t mode = AUTO;

float temperature = 19, humidity = 80, temperatureSP = 18, humiditySP = 90;
bool lightOn;

class Screen {
  public:
    bool Changed = false;
    uint8_t history[10], prev = MAIN;
    int8_t i = -1;
    void add(uint8_t scr) {
      if (i != -1) prev = history[i];
      i++;
      history[i] = scr;
      //changeScreen(scr);
      Changed = true;
    }
    void pop() {
      prev = history[i];
      i--;
      //changeScreen(history[i]);
      Changed = true;
    }
    uint8_t current() {
      return history[i];
    }
    uint8_t previous() {
      return history[max(i - 1, 0)];
    }
    uint8_t at(uint8_t to) {
      return history[to];
    }
    void truncate(uint8_t to) {
      prev = history[i];
      i = to;
      //changeScreen(history[i]);
      Changed = true;
    }
    void reset() {
      prev = history[i];
      i = 0;
      //changeScreen(history[0]);
      Changed = true;
    }

    bool hasJustChanged() {
      bool btemp = Changed;
      Changed = false;
      return btemp;
    }

    //    void changeScreen(uint8_t md) {
    //      switch (current()) {
    //        case MAIN:
    //          drawMainScreen();
    //          break;
    //        case SETUP:
    //          drawSetupScreen();
    //          break;
    //        case ENVIR:
    //          drawEnvirScreen();
    //          break;
    //        case HT_INPUT:
    //          drawEnvirInputs();
    //          break;
    //        case ENTER_PASSWORD_SCREEN:
    //          drawEnterPasswordScreen();
    //          break;
    //      }
    //    }
};
Screen screen;

#define N_COMMAND 2
class Command {
  public:
    const String cmd[N_COMMAND] = {"list", "status"};
    const uint8_t ncmd = N_COMMAND;
    bool done = false;
    String buffer = "";
    String param;

    int read() {
      while (Serial3.available() > 0) {
        char b = Serial3.read();
        if (b == '\n') return -1;
        if (b == ';') {
          String temp = buffer;
          buffer = "";
          int dot = temp.indexOf('.');
          if (dot != -1) {
            String tempcmd = temp.substring(0, dot);
            for (int i = 0; i < ncmd; i++) {
              if (tempcmd == cmd[i]) {
                if (dot + 1 < temp.length()) param = temp.substring(dot + 1);
                return i;
              }
            }
          } else {
            for (int i = 0; i < ncmd; i++) {
              if (temp == cmd[i]) {
                return i;
              }
            }
          }
        } else {
          buffer += String(b);
        }
      }
      return -1;
    }
};
Command cmd;

//------------------------------------------------------For showing bitmap image------------------------------------------------------
File file;

uint16_t read16(File f)
{
  uint16_t d;
  uint8_t b;
  b = f.read();
  d = f.read();
  d <<= 8;
  d |= b;
  return d;
}

uint32_t read32(File f)
{
  uint32_t d;
  uint16_t b;

  b = read16(f);
  d = read16(f);
  d <<= 16;
  d |= b;
  return d;
}

class Image {
  public:
    String fileName;
    uint16_t w, h, bufflen;
    boolean down;
    uint8_t bitDepth, bmpOffset;
    int x, y;

    Image(String fname, uint16_t buffl = 0) {
      bufflen = buffl;
      fileName = fname;
    }
    void init() {
      file = SD.open(fileName);
      if (!file) {
        Serial.println("Cannot find " + fileName);
      } else {
        //--- read header--
        read16(file); //magic byte
        read32(file);// read file size
        read32(file);// read and ignore creator bytes
        bmpOffset = read32(file); //where to start to read color data
        // read DIB header
        read32(file);          //img size
        w = read32(file);             //get w, h
        h = read32(file);
        read16(file);
        bitDepth = read16(file);//bit depth
      }
      file.close();
    }

    void draw(int xx, int yy, bool downn = false) {
      down = downn;
      x = xx; y = yy;
      file = SD.open(fileName);
      if (!file) {
        Serial.println("Cannot find at the second time" + fileName);
      } else {
        if (bufflen == 0) bufflen = w;
        int bufflenB = 2 * bufflen;

        uint8_t sdbuffer[200];   // 50 px  max
        if (!down) {
          file.seek(bmpOffset);
          for (int i = h - 1; i >= 0; i--) { //reversed
            for (int j = 0; j + bufflenB <= 2 * w; j += bufflenB) {
              file.read(sdbuffer, bufflenB);
              uint8_t offset_x = j >> 1; // j/2   ^^
              //uint16_t cl;
              for (int k = 0; k < bufflen; k++) {
                //cl = (sdbuffer[(k << 1) + 1 ] << 8) | sdbuffer[k << 1];
                tft.drawPixel(x + k + offset_x, y + i, (sdbuffer[(k << 1) + 1 ] << 8) | sdbuffer[k << 1]);
              }
            }
          }
        } else {
          uint32_t endpoint = bmpOffset + 2 * w * h;
          for (int i = 0; i < h; i++) {
            for (int j = 0; j + bufflenB <= 2 * w; j += bufflenB) {
              file.seek(endpoint - bufflenB * (i + 1));
              file.read(sdbuffer, bufflenB);
              uint8_t offset_x = j >> 1; // j/2   ^^
              //uint16_t cl;
              for (int k = 0; k < bufflen; k++) {
                //cl = (sdbuffer[(k << 1) + 1 ] << 8) | sdbuffer[k << 1];
                tft.drawPixel(x + k + offset_x, y + i, (sdbuffer[(k << 1) + 1 ] << 8) | sdbuffer[k << 1]);
              }
            }
          }
        }
        file.close();
      }
    }

    bool pressed = false, clicked = false, released = false, prev;
    unsigned long t = millis(), td = t;
    void onclicked(void (*f)()) {
      prev = pressed;
      pressed = touched && (tx >= x && tx < x + w && ty >= y && ty < y + h);
      if (!prev && pressed) { //pressed
        td = millis();
      }
      if (prev && !pressed) { //released
        if (millis() - t > 500 && millis() - td > 20) {
          (*f)();
        }
        t = millis();
      }
    }
};

class ImageFromSRAM {
  public:
    uint16_t* arr;
    uint16_t w, h;

    ImageFromSRAM(uint16_t* dt, uint16_t ww, uint16_t hh) {
      arr = dt;
      w = ww, h = hh;
    }
    void draw(int x, int y) {
      uint32_t i = 0;
      for (int row = h - 1; row >= 0; row--) {
        for (int c = 0; c < w; c++) {
          tft.drawPixel(x + c, y + row, pgm_read_word_near(arr + i));
          i++;
        }
      }
    }
};

//---------------------------------------------------------------Graphics: Gradient-----------------------------------------------------
typedef struct {
  uint8_t r, g, b;
} Color;

class Grad
{
  public:
    int p[MAX];
    int n;
    Color color[MAX]; //Maximum 2, can be changed, at least 2
};

class GradRectH: public Grad {
  public:
    int x, len;
    void draw() {
      for (uint8_t i = 0; i < n - 1; i++) {
        for (int row = p[i]; row < p[i + 1]; row++) {
          uint8_t r, g, b;
          r = map(row, p[i], p[i + 1], color[i].r, color[i + 1].r);
          g = map(row, p[i], p[i + 1], color[i].g, color[i + 1].g);
          b = map(row, p[i], p[i + 1], color[i].b, color[i + 1].b);
          tft.drawFastHLine(x, row, len, tft.color565(r, g, b));
        }
      }
    }
};

class GradRectV: public Grad {
  public:
    int y, len;
    void draw() {
      for (uint8_t i = 0; i < n - 1; i++) {
        for (int column = p[i]; column < p[i + 1]; column++) {
          uint8_t r, g, b;
          r = map(column, p[i], p[i + 1], color[i].r, color[i + 1].r);
          g = map(column, p[i], p[i + 1], color[i].g, color[i + 1].g);
          b = map(column, p[i], p[i + 1], color[i].b, color[i + 1].b);
          tft.drawFastVLine(column, y, len, tft.color565(r, g, b));
        }
      }
    }
};

class GradCircle: public Grad {
  public:
    int x, y;
    // p[] is an array of radius in this case
    void draw() {
      for (uint8_t i = 0; i < n - 1; i++) {
        for (int radius = p[i]; radius < p[i + 1]; radius++) {
          uint8_t r, g, b;
          r = map(radius, p[i], p[i + 1], color[i].r, color[i + 1].r);
          g = map(radius, p[i], p[i + 1], color[i].g, color[i + 1].g);
          b = map(radius, p[i], p[i + 1], color[i].b, color[i + 1].b);
          tft.drawCircle(x, y, radius, tft.color565(radius, g, b));
        }
      }
    }
};

class GradLineH: public GradRectV {
  public:
    GradLineH(int xx, int yy, int lenn, int lw, Color c1, Color c2) {
      y = yy;
      p[0] = xx; p[1] = xx + lenn / 4; p[2] = xx + 3 * lenn / 4; p[3] = xx + lenn;
      len = lw; n = 4;
      color[0] = color[3] = c2;
      color[1] = color[2] = c1;
    }
};
//-----------------------------------------------------------Graphics Elements---------------------------------------------------------------
class GroupBox {
  public:
    String name;
    uint16_t x, y, x2, y2, border, fill, hideLine, nameBackColor = Backcolor, nameForeColor;
    uint8_t r, namepxh = 16, nameTxtSize;

    GroupBox(String NAME, int X, int  Y, int X2, int Y2, uint16_t BORDER = WHITE, uint8_t R = 5, uint16_t FILL = Transparent) {
      name = NAME;
      x = X;
      y = Y;
      x2 = X2;
      y2 = Y2;
      r = R;
      border = BORDER;
      fill = FILL;
      r = R;
      hideLine = 90;
      nameForeColor = WHITE;
      nameTxtSize = 1;
    }
    void draw() {
      if (r == 0) {
        tft.drawRect(x, y, x2 - x, y2 - y, border);
        if (fill != Transparent) tft.fillRect(x, y, x2 - x, y2 - y, fill);
      } else {
        tft.drawRoundRect(x, y, x2 - x, y2 - y, r, border);
        if (fill != Transparent) tft.fillRoundRect(x, y, x2 - x, y2 - y, r, fill);
      }
      //draw label
      if (hideLine != 0) tft.drawFastHLine(x + 5, y, hideLine, nameBackColor);
      text(name, x + 10, y + namepxh / 2, nameTxtSize, nameForeColor, &hand16pt);
    }
};

class Wifi {
  public:
    uint8_t spacing = 2, hmax = 20, w = 3;
    uint8_t bar = 4;
    bool stt = false, receivedWifiList, printedWifiList;
    String ssid[10];
    signed int rssi[10];
    int n = 0;
    String password, name;

    void drawSignal(int x, int y) {
      uint16_t color[] = {RED, YELLOW, GREEN, GREEN};
      for (char i = 0; i < 4; i++) {
        int h = (i + 1) * hmax / 4;
        tft.fillRect(x + i * (w + spacing), y - h, w, h, i < bar ? color[bar - 1] : topBarBackground);
      }
    }
};

class Switch {
  public:
    bool st;
    uint16_t x, y;
    uint8_t w, h, r, pos;
    uint16_t onColor = tft.color565(75, 215, 100);
    uint16_t offColor = tft.color565(215, 220, 222);
    Switch(uint16_t X, uint16_t Y, uint16_t W = 60, uint16_t H = 30) {
      x = X;
      y = Y;
      w = W;
      h = H;
      r = (uint8_t)(0.46 * h);
    }
    void draw(bool stt) {
      st = stt;
      if (st) {
        tft.fillRoundRect(x, y, w, h, h / 2, onColor);
        tft.fillCircle(x + w - h / 2 + pos, y + h / 2, r, WHITE);
      } else {
        tft.fillRoundRect(x, y, w, h, h / 2, offColor);
        tft.fillCircle(x + h / 2, y + h / 2, r, WHITE);
      }
    }
};

class Clock {
  public:
    uint8_t hour, minute, day, month;
    uint16_t year;

    void set(uint8_t _hour, uint8_t _minute, uint8_t _day, uint8_t _month, uint16_t _year) {
      hour = _hour;
      minute = _minute;
      day = _day;
      month = _month;
      year = _year;
    }

    String getTimeString() {
      return form(hour) + String(":") + form(minute);
    }
    String getDateString() {
      return form(day) + String("/") + form(month) + String("/") + String(year);
    }

    String form(int x) {
      if (x < 10) {
        return String("0") + String(x);
      } else {
        return String(x);
      }
    }
};
Clock clk;

class Message {
  public:
    String s;
    int w = 100, h = 20, x, y, xText = 3, yText = 3;
    uint16_t color = WHITE;
    const GFXfont *font = NULL;

    void draw(String _s, int xx, int yy) {
      s = _s;
      x = xx;
      y = yy;
      tft.fillRect(x, y, w, h, RED);
      text(s, x + xText, y + yText, 1, color, font);
    }
};
Message msg;

class Btn {     //Button
  public:
    String btnText;
    uint16_t border = BLACK, back = WHITE, fore = BLACK;
    GradRectH gr;
    int x, y, w, h, xText = 7;

    Btn(String s, int ww = 200, int hh = 30) {
      btnText = s;
      w = ww;
      h = hh;
    }

    void draw(int xx, int yy) {
      x = xx; y = yy;
      tft.fillRoundRect(x, y, w, h, 4, back);
      tft.drawRoundRect(x, y, w, h, 4, border);
      //      if (clicked) {
      //        tft.drawRoundRect(x + 1, y + 1, w - 2, h - 2, 4, border);
      //        tft.drawRect(x + 2, y + 2, w - 4, h - 4, border);
      //      }

      gr.x = x + 2;  gr.len = w - 4;
      gr.n = 2;

      gr.p[1] = y + h - 2;     //end gradient
      gr.color[0] = {255, 255, 255};  //begin color
      //      if (clicked) {
      //        gr.p[0] = y + h / 3 + 2; //begin gradient
      //        gr.color[1] = {50, 50, 50};  //end color
      //      } else {
      gr.p[0] = y + h / 4 + 2; //begin gradient
      gr.color[1] = {150, 150, 150};  //end color
      //      }
      gr.draw();

      text(btnText, x + xText, y + h - 10, 1, fore, &ss10pt);
    }

    bool pressed = false, clicked = false, released = false, prev;
    unsigned long tu = millis(), td = tu;
    void onclicked(void (*f)()) {
      prev = pressed;
      pressed = touched && (tx >= x && tx < x + w && ty >= y && ty < y + h);
      if (!prev && pressed ) { //pressed down
        if (millis() - td > 700 && millis() - tu > 400) {
          clicked = true;
          draw(x, y);
          (*f)();
          clicked = false;
        }
        td = millis();
      }
      if (prev && !pressed) { //released up
        if (millis() - tu > 700 && millis() - td > 400) {
          clicked = false;
        }
        tu = millis();
      }
    }
};

class SimpleBtn {     //Button
  public:
    String btnText;
    uint16_t border = BLACK, back = BLACK, fore = WHITE, backInvert = YELLOW;
    int x, y, w, h, xText, yText;
    uint8_t textSize = 3;

    void init(String s, int xx, int yy, int ww = 200, int hh = 30) {
      btnText = s;
      x = xx; y = yy;
      w = ww;
      h = hh;
    }

    void draw(bool invert = false) {
      tft.fillRect(x, y, w, h, invert ? backInvert : back);
      tft.drawRect(x, y, w, h, border);
      text(btnText, x + xText, y + yText, textSize, fore/*, &ss10pt*/);
    }

    bool pressed = false, prev = false;
    unsigned long tu = millis(), td = tu;
    bool clicked() {
      bool temp = false;
      prev = pressed;
      pressed = touched && (tx >= x && tx < x + w && ty >= y && ty < y + h);
      if (!prev && pressed) { //pressed down
        if (millis() - td > 100 && millis() - tu > 100) {
          draw(true);
          temp = true;
        }
        td = millis();
      }
      if (prev && !pressed) { //released up
        //if (millis() - tu > 100 && millis() - td > 100) {
        draw();
        //}
        tu = millis();
      }
      return temp;
    }
};

class Keypad {
  public:
    SimpleBtn button[12];
    int x, y, w, h, wBtn, hBtn, spacing;
    void init(int xx, int yy, int ww = 180, int hh = 180, int spc = 2) {
      x = xx;
      y = yy;
      w = ww;
      h = hh;
      wBtn = w / 3;
      hBtn = h / 4;
      spacing = spc;
      for (int i = 0; i < 12; i ++) {
        button[i].btnText = KeypadSource[i];
      }

      button[11].back = 0x7000;
      button[11].xText = (wBtn - spacing) / 2 - 16;
      int i = 0;
      for (int r = 0; r < 4; r ++) {
        for (int c = 0; c < 3; c ++) {
          button[i].x = x + c * wBtn + spacing / 2;
          button[i].y = y + r * hBtn + spacing / 2;
          button[i].w = wBtn - spacing;
          button[i].h = hBtn - spacing;
          button[i].yText = button[i].h / 2 + 7;
          if (i != 11) {
            button[i].xText = button[i].w / 2 - 5;
          }
          i++;
        }
      }
    }
    void draw() {
      uint8_t padding = 2;
      tft.drawRect(x - padding, y - padding, w + 2 * padding, h + 2 * padding, BLACK);
      for (int i = 0; i < 12; i ++) {
        button[i].draw();
      }
    }

    String getPressed() {
      String key = "";
      for (int i = 0; i < 12; i ++) {
        if (button[i].clicked()) {
          if (i == 11) key = "del";
          else key = button[i].btnText;
        }
      }
      return key;
    }
};

class KeyboardTouch {
  public:
    int x = 10, y = 100, w = 460, h = 200, wBtn, hBtn, padding = 2;
    uint8_t mode = 0;
    bool caps = false;
    SimpleBtn key[30], cmdKey[5];

    void init() {
      wBtn = w / 10;
      hBtn = h / 4;

      int i = 0;
      for (int r = 0; r < 3; r++) {
        for (int c = 0; c < 10; c++) {
          char kt;
          switch (mode) {
            case 0:
              if (caps) kt = pgm_read_byte(&CapsLeterKeys[r][c]);
              else kt = pgm_read_byte_near(&LeterKeys[r][c]);
              break;
            case 1:
              kt = pgm_read_byte(&NumKeys[r][c]);
              break;
            case 2:
              kt = pgm_read_byte(&SymKeys[r][c]);
              break;
          }

          key[i].x = x + c * wBtn + padding;
          key[i].y = y + r * hBtn + padding;
          key[i].w = wBtn - 2 * padding;
          key[i].h = hBtn - 2 * padding;
          key[i].xText = wBtn / 2 - 8;
          key[i].yText = hBtn / 2 - 8;
          key[i].btnText = kt;
          key[i].border = BLACK;
          key[i].back = WHITE;
          key[i].fore = BLACK;
          i++;
        }
      }

      int xk = 0;
      for (int i = 0; i < 5; i++) {
        cmdKey[i].textSize = 2;
        cmdKey[i].btnText = KeyboardCmd[i];
        cmdKey[i].y = y + 3 * hBtn + padding;
        cmdKey[i].w = KeyboardCmdW[i] * wBtn - 2 * padding;
        if (i > 0) xk += cmdKey[i - 1].w + 2 * padding;
        cmdKey[i].x = x + xk + padding;
        cmdKey[i].h = hBtn - 2 * padding;
        cmdKey[i].xText = cmdKey[i].w / 2 - cmdKey[i].btnText.length() * 6;
        cmdKey[i].yText = hBtn / 2 - 5;
      }
      cmdKey[2].back = WHITE;
      cmdKey[2].fore = BLACK;
    }

    void draw() {
      for (int i = 0; i < 30; i++) {
        key[i].draw();
      }
      for (int i = 0; i < 5; i++) {
        cmdKey[i].draw();
      }
    }

    void redraw() {
      init();
      for (int i = 0; i < 30; i++) {
        key[i].draw();
      }
    }

    String getPressed() {
      String kt = "";
      if (cmdKey[2].clicked()) {
        return " ";
      }
      if (cmdKey[3].clicked()) {
        return "del";
      }
      if (cmdKey[4].clicked()) {
        return "ok";
      }
      if (cmdKey[0].clicked()) {
        mode = mode == 2 ? 0 : mode + 1;
        redraw();
      }
      if (cmdKey[1].clicked()) {
        caps = !caps;
        if (mode == 0) redraw();
      }

      for (int i = 0; i < 30; i++) {
        if (key[i].clicked()) {
          kt = key[i].btnText;
          break;
        }
      }

      return kt;
    }
};

class Input {
  public:
    int x, y, w = 100, h = 35, xText = 10, yText = 25;
    String value = "";
    uint16_t backColor = WHITE, foreColor = BLACK, borderColor = BLACK;
    bool selected = false;

    void draw(int xx, int yy) {
      x = xx;
      y = yy;
      tft.fillRect(x, y, w, h, backColor);
      tft.drawRect(x, y, w, h, borderColor);
      if (value != "") {
        text(value, x + xText, y + yText, 1, foreColor, &ss13pt);
      }
    }

    void redraw(bool refresh = false) {
      if (refresh) tft.fillRect(x + 1, y + 1, w - 2, h - 2, backColor);
      if (value != "") {
        text(value, x + 10, y + 25, 1, foreColor, &ss13pt);
      }
    }

    void typeIn(String s) {
      if (s == "del") {
        if (value != "") {
          value.remove(value.length() - 1, 1);
          redraw(true);
        }
      } else {
        value = value + s;
        redraw();
      }
    }

    void select() {
      selected = true;
      tft.drawRect(x - 3, y - 3, w + 6, h + 6, WHITE);
    }

    void deselect() {
      selected = false;
      tft.drawRect(x - 3, y - 3, w + 6, h + 6, Backcolor);
    }

    bool pressed = false, prev;
    unsigned long tu = millis(), td = tu;
    void onclicked(void (*f)()) {
      prev = pressed;
      pressed = touched && (ty >= y && ty < y + h && tx >= x && tx < x + w);
      if (!prev && pressed ) { //pressed down
        if (millis() - td > 500 && millis() - tu > 200) {
          Serial.println("input selected");
          select();
          (*f)();
        }
        td = millis();
      }
      if (prev && !pressed) { //released up
        //        if (millis() - tu > 500 && millis() - td > 200) {
        //        }
        tu = millis();
      }
    }
};

//class KeyboardTouch {
//  public:
//
//}

class Menu {
    class Element {
      public:
        uint8_t id = 0;
        String lb;
        int ystart = 115, y, h = 45;
        bool last = false;
        Color border = {156, 198, 255}, selectedColor{10, 82, 102};
        bool defaultFont = false;

        void draw() {
          y = ystart + id * h;
          GradLineH(0, y, wscr, 1, border, Backcolor24).draw();
          if (defaultFont) {
            text(lb, 60, y + h / 3, 2, WHITE);
          } else {
            text(lb, 60, y + 2 * h / 3, 1, WHITE, &ss16pt);
          }
          if (last) GradLineH(0, y + h, wscr, 1, border, Backcolor24).draw();
        }

        bool pressed = false, clicked = false, prev;
        unsigned long tu = millis(), td = tu;
        bool onclicked(void (*f)()) {
          bool temp = false;
          prev = pressed;
          pressed = touched && (ty >= y && ty < y + h);
          if (!prev && pressed ) { //pressed down
            if (millis() - td > 500 && millis() - tu > 200) {
              clicked = true;
              tft.drawRect(0, y + 1, wscr, h - 2, Backcolor);
              if (defaultFont) {
                text(lb, 60, y + h / 3, 2, 0x7e0);
              } else {
                text(lb, 60, y + 2 * h / 3, 1, 0x7e0, &ss16pt);
              }
              temp = true;
              (*f)();
              clicked = false;
            }
            td = millis();
          }
          if (prev && !pressed) { //released up
            if (millis() - tu > 500 && millis() - td > 200) {
              clicked = false;
            }
            tu = millis();
          }
          return temp;
        }
    };
  public:
    Element list[10];
    uint8_t n;

    Menu(uint8_t nn) {
      n = nn;
      for (int i = 0; i < n; i++) {
        list[i].id = i;
      }
      list[n - 1].last = true;
    }
    void draw() {
      for (int i = 0; i < n; i++) {
        list[i].draw();
      }
    }
};

class Header {
  public:
    String lb;
    Image backBtn = Image("goback.bmp"), exitBtn = Image("exit.bmp");
    int xlb = 180;
    Header(String lbl) {
      lb = lbl;
    }
    void draw() {
      xlb = wscr / 2 - 9 * lb.length();;
      backBtn.init();
      exitBtn.init();
      text(lb, xlb, 80, 1 , WHITE, &ss20pt);
      if (!ignoreIconHeader()) {
        backBtn.draw(30, 50, true);
        exitBtn.draw(420, 50, true);
      } else {
        backBtn.x = 30;
        backBtn.y = 50;
        exitBtn.x = 420;
        exitBtn.y = 50;
      }
    }
    void onclicked() {
      exitBtn.onclicked([] {screen.reset();});
      backBtn.onclicked([] {screen.pop(); });
    }
};

//--------------------------------------------------------------Declaration and functions----------------------------------------------------
GradRectH gr;
GradCircle grc;
Image cordycepsBox("box.bmp"), tIcon("tprt.bmp"), hIcon("Humidity.bmp"), lIcon("light.bmp"), automode("auto.bmp", 100), manualmode("manual.bmp", 100) ;
Input tInput, hInput, pwInput;
Btn inputOk("Ok");

Header header("");

//auto timer = timer_create_default();

uintptr_t task;
Wifi wifi;

void text(String s , int x, int y, int txtSize = 1, uint16_t cl = WHITE, const GFXfont *txtFont = NULL) {
  if (txtFont != NULL) tft.setFont(txtFont);
  else tft.setFont();
  tft.setCursor(x, y);
  tft.setTextColor(cl);
  tft.setTextSize(txtSize);
  tft.println(s);
}

String trimFloat(float f, int n = 2) {
  String s = String(f, n);
  uint8_t comma = s.lastIndexOf('.'), zero = -1;
  if (comma != -1) {
    for (int i = s.length() - 1; i > comma; i--) {
      if (s.charAt(i) == '0') zero = i;
      else break;
    }
    if (zero != -1) {
      if (zero == comma + 1) zero = comma;
      s = s.substring(0, zero);
    }
  }
  return s;
}

void switchMode() {
  switch (mode) {
    case AUTO:
      mode = MANUAL;
      break;
    case MANUAL:
      mode = AUTO;
      break;
  }
}

// -----------------------screens-------------
void clr() {
  if (ignoreIconHeader()) {
    tft.fillRect(70, gr.p[2], wscr - 140, 130, Backcolor);
    tft.fillRect(0, 110, wscr, hscr - 110, Backcolor);
  } else {
    tft.fillRect(0, gr.p[2], wscr, hscr - gr.p[2], Backcolor);
  }
}

bool ignoreIconHeader() {
  return hasHeader(screen.prev) && hasHeader(screen.current());
}

bool hasHeader(uint8_t scr) {
  return (scr == SETUP || scr == ENVIR || scr == HT_INPUT || scr == WIFI_LIST);
}

void drawTopBar() {
  gr.x = 0;  gr.len = wscr;
  gr.n = 3;
  gr.p[0] = 30;   gr.p[1] = 33; gr.p[2] = 35;
  gr.color[0] = {24, 78, 93};
  gr.color[1] = {75, 141, 160};
  gr.color[2] = {8, 44, 54};
  tft.fillRect(0, 0, wscr, gr.p[0], tft.color565(24, 78, 93));
  gr.draw();
  text(clk.getTimeString(), 10, 8, 2);
  wifi.drawSignal(440, 27);
}

void screenMain() {
  GroupBox thongso("Th\u00a6ng s\u00a8:", 10, 55, 280, 310), groupBox2("", 295, 150, 470, 310);
  Btn gotoSetupBtn("T\u00aei b\u0082ng \u001fi\u0097u khi\u0099n");

  groupBox2.hideLine = 0;
  tft.fillRect(0, gr.p[2], wscr, hscr - gr.p[2], Backcolor);
  //clock
  int xt = 300, yt = 90;
  text(clk.getTimeString(), xt + 30, yt, 1, WHITE, &Roboto_Light24pt7b);
  text(clk.getDateString(), xt + 40, yt + 30, 1, WHITE, &Roboto_Black8pt7b);
  tft.drawFastVLine(xt - 5, yt - 40, 85, WHITE);
  tft.drawFastHLine(xt - 5, yt + 45, 170, WHITE);
  //GradLineH(295, 125, 180, 1, {255, 255, 255}, {8, 44, 54}).draw();
  thongso.draw();
  xt = thongso.x + 70, yt = thongso.y + 60;
  tft.fillCircle(xt - 31, yt - 15, 20, WHITE);
  lIcon.draw(xt - 45, yt - 29);
  text(String("\u001cnh s\u0081ng: ") + String(lightOn ? "B\u0090t" : "T\u0087t"), xt, yt, 1, WHITE, &test); //LIGHT
  yt += 50;
  tft.fillCircle(xt - 31, yt - 15, 20, WHITE);
  hIcon.draw(xt - 41, yt - 30);
  text(String("\u001e\u00ab \u008em: ") + round(humidity) + String("%"), xt, yt, 1, WHITE, &test);   //HUMIDITY
  yt += 50;
  tft.fillCircle(xt - 31, yt - 15, 20, WHITE);
  tIcon.draw(xt - 45, yt - 29);
  text(String("Nhi\u009bt \u001f\u00ab: ") + round(temperature) + String("\u001d"), xt, yt, 1, WHITE, &test); //TEMPERATURE

  GradLineH(20, 240, 250, 1, {255, 255, 255}, {8, 44, 54}).draw();
  gotoSetupBtn.draw(45, 260);

  groupBox2.draw();
  xt = groupBox2.x + 40;
  yt = groupBox2.y;
  String s;

  uint8_t temp = 99;
  while (true) {
    checkTouch();
    if (mode != temp) {
      temp = mode;
      switch (mode) {
        case AUTO:
          s = String("T\u00bd \u001f\u00abng");
          tft.fillRect(xt - 26, yt + 117, 157, 28, Backcolor);
          text(String("Ch\u0098 \u001f\u00ab: ") + s, xt - 25, yt + 135, 1, WHITE, &ss10pt);
          automode.draw(xt, yt + 15);
          break;
        case MANUAL:
          s = String("B\u0086ng tay");
          tft.fillRect(xt - 26, yt + 117, 157, 28, Backcolor);
          text(String("Ch\u0098 \u001f\u00ab: ") + s, xt - 25, yt + 135, 1, WHITE, &ss10pt);
          manualmode.draw(xt, yt + 15);
      }
    }

    automode.onclicked(switchMode);
    gotoSetupBtn.onclicked([] {screen.add(SETUP);});
    if (screen.hasJustChanged()) break;
  }
}

void screenSetup() {
  Menu menuSetup(2);
  menuSetup.list[1].lb = F("C\u0080i \u001f\u008at m\u0084ng");
  menuSetup.list[0].lb = F("Ch\u009enh th\u00a6ng s\u00a8 m\u00a6i tr\u00b8\u00adng");

  clr();
  header.lb = "C\u001cI \u001e\u001dT";
  header.draw();
  menuSetup.draw();

  while (true) {
    checkTouch();
    header.onclicked();
    menuSetup.list[1].onclicked([] {
      if (!wifi.stt) {
        Serial3.print("scan;");
        Serial.println("sent scan cmd");
        wifi.receivedWifiList = false;
        wifi.printedWifiList = false;
        screen.add(WIFI_LIST);
      } else {
        screen.add(WIFI_STATUS);
      }
    });
    menuSetup.list[0].onclicked([] {screen.add(ENVIR);});
    if (screen.hasJustChanged()) break;
  }
}

void screenEnvirScreen() {
  Menu menuEnvir(3);

  menuEnvir.list[0].lb = String("\u001e\u0091n chi\u0098u s\u0081ng: ") + (lightOn ? String("B\u0090t") : String("T\u0087t"));
  menuEnvir.list[1].lb = F("Nhi\u009bt \u001f\u00ab");
  menuEnvir.list[2].lb = F("\u001e\u00ab \u008em");

  clr();
  header.lb = "M\u00a6i tr\u00b8\u00adng";
  header.draw();
  menuEnvir.draw();

  while (true) {
    checkTouch();
    header.onclicked();
    menuEnvir.list[1].onclicked([] {screen.add(HT_INPUT);});
    if (screen.hasJustChanged()) break;
  }
}

void screenHTInputs() {
  Keypad keypad;

  keypad.init(280, 120);
  clr();
  header.lb = "T,H";
  header.draw();
  tInput.value = trimFloat(temperatureSP, 2);
  hInput.value = trimFloat(humiditySP, 2);
  int yt = 150;
  text("Nhi\u009bt \u001f\u00ab:", 10, yt, 1, WHITE, &ss13pt);
  tInput.draw(130, yt - 25);
  tInput.select();
  yt += 50;
  text("Do am:", 10, yt, 1, WHITE, &ss13pt);
  hInput.draw(130, yt - 25);
  inputOk.w = 60;
  inputOk.xText = 15;
  inputOk.draw(160, yt + 40);
  keypad.draw();

  while (true) {
    checkTouch();
    header.onclicked();
    tInput.onclicked([] {hInput.deselect();});
    hInput.onclicked([] {tInput.deselect();});
    String key = keypad.getPressed();
    if (key != "") {
      if (tInput.selected) tInput.typeIn(key);
      if (hInput.selected) hInput.typeIn(key);
    }
    inputOk.onclicked([] {
      float t = tInput.value.toFloat();
      float h = hInput.value.toFloat();
      if (h <= 100) {
        temperatureSP = t;
        humiditySP = h;
        int xt = 30, yt = 260;
        tft.fillRect(xt, yt, 80, 20, Backcolor);
        text("\u001e\u0083 l\u00b8u!", xt, yt, 1, GREEN, &ss10pt);
      }
    });
    if (screen.hasJustChanged()) break;
  }
}

void screenWifiList() {
  Menu menuWifi(1);

  clr();
  header.lb = "Wifi";
  header.draw();

  if (wifi.receivedWifiList) {
    menuWifi = Menu(wifi.n);
    for (int i = 0; i < wifi.n; i++) {
      menuWifi.list[i].lb = String(i + 1) + ". " + wifi.ssid[i] + "  " + wifi.rssi[i];
      menuWifi.list[i].h = 30;
      menuWifi.list[i].defaultFont = true;
    }
    menuWifi.draw();
    wifi.printedWifiList = true;
  }

  while (true) {
    checkTouch();
    header.onclicked();

    if (!wifi.receivedWifiList) {
      msg.draw("Scanning...", 200, 200);
    } else {
      if (!wifi.printedWifiList) break;
    }

    for (int i = 0; i < wifi.n; i++) {
      if (menuWifi.list[i].onclicked([] {
      screen.add(WIFI_PASSWORD);
      })) {
        wifi.name = wifi.ssid[i];
        break;
      }
    }

    if (screen.hasJustChanged()) break;
  }
}

void screenWifiPassword() {
  KeyboardTouch keyboard;
  keyboard.init();

  clr();
  keyboard.draw();
  text("Password:", 20, 80, 1, WHITE, &ss13pt);
  pwInput.draw(150, 50);

  while (true) {
    checkTouch();

    String key = keyboard.getPressed();
    if (key != "") {
      if (key == "ok") {
        wifi.password = pwInput.value;
        Serial3.print("connect." + wifi.name + "::" + wifi.password + ";");
        screen.pop(); screen.pop();
        screen.add(WIFI_STATUS);
        break;
      }

      pwInput.typeIn(key);
    }
    if (screen.hasJustChanged()) break;
  }
}

void screenWifiStatus() {
  Btn disconnect("Ng\u0087t k\u0098t n\u00a8i");
  
  clr();
  header.lb = "Wifi";
  header.draw();
  bool flag = false;

  unsigned long t = millis();

  if (wifi.stt) {
    text("\u001e\u0083 k\u0098t n\u00a8i:", 50, 140, 1, WHITE, &ss13pt);
    text(wifi.name, 70, 170, 1, WHITE, &ss13pt);
    flag = true;
    disconnect.w = 150;    
    disconnect.draw(100, 270);
  }

  bool failed =  false;

  while (true) {
    checkTouch();
    header.onclicked();
    
    if (!flag) {
      if (failed) {
        msg.draw("Failed.", 200, 200);    // failed to connect
      } else {
        if (!wifi.stt) {
          msg.draw("Connecting...", 200, 200);  // connecting
        } else {
          msg.draw("Connected.", 200, 200);      // connected
          Serial.print("Connected.");
          break;
        }
      }
    }

    disconnect.onclicked([] {
      Serial3.print("disconnect;");
      screen.pop();
    });

    if (screen.hasJustChanged()) break;
  }
}

//-----------------------------------------------------------------Main program--------------------------------------------------------------
void setup(void) {
  pinMode(11, INPUT);
  pinMode(12, INPUT);
  pinMode(13, INPUT);
  pinMode(53, OUTPUT);
  Serial.begin(9600);
  Serial3.begin(9600);

  Serial3.print("flush;");

  uint16_t identifier = tft.readID();
  tft.begin(identifier);

  // Init SD_Card
  pinMode(10, OUTPUT);
  if (!SD.begin(10 )) {
    tft.setCursor(0, 0);
    tft.setTextColor(WHITE);
    tft.setTextSize(1);
    tft.println("SD Card Init fail.");
    Serial.println("SD Card Init fail.");
  }

  cordycepsBox.init();
  tIcon.init();
  hIcon.init();
  lIcon.init();
  automode.init();
  manualmode.init();

  pwInput.w = 250;

  clk.set(11, 28, 1, 1, 2011);
  tft.setRotation(1);
  drawTopBar();
  screen.add(MAIN);
  screen.hasJustChanged();
}

void loop(void) {
  // tap events
  switch (screen.current()) {
    case MAIN:
      screenMain();
      break;
    case SETUP:
      screenSetup();
      break;
    case ENVIR:
      screenEnvirScreen();
      break;
    case WIFI_PASSWORD:
      screenWifiPassword();
      break;
    case HT_INPUT:
      screenHTInputs();
      break;
    case WIFI_LIST:
      screenWifiList();
      break;
    case WIFI_STATUS:
      screenWifiStatus();
      break;
  }
}

void checkTouch() {
  digitalWrite(13, HIGH);
  TSPoint p = ts.getPoint();
  digitalWrite(13, LOW);

  //pinMode(XP, OUTPUT);
  pinMode(XM, OUTPUT);
  pinMode(YP, OUTPUT);
  //pinMode(YM, OUTPUT);

  touched = (p.z > MINPRESSURE);
  if (touched) {
    ty = map(p.x, TS_MINX, TS_MAXX, tft.height(), 0);
    tx = map(p.y, TS_MINY, TS_MAXY, tft.width(), 0);
  }
  serial();
}

void serial() {
  int icmd = cmd.read();
  if (icmd == 1) {
    String s = cmd.param;
    
    if (s == "1") {
      wifi.stt = true;
      Serial.println("connected");
    } else {
      wifi.stt = false;
      Serial.println("disconnected");
    }
  }

  switch (icmd) {
    case 0:  // receive wifi list
      Serial.println("Receive list of wifi:");
      String s = cmd.param;
      wifi.n = 0;
      if (s == "") break;
      for (int i = 0; i < 10; i++) {
        int sp = s.indexOf("\\\\");
        String s2;
        if (sp != -1) {
          s2 = s.substring(0, sp);
        } else {
          s2 = s;
        }
        int sp2 = s.indexOf("::");
        wifi.ssid[i] = s2.substring(0, sp2);
        wifi.rssi[i] = s2.substring(sp2 + 2, s2.length()).toInt();
        wifi.n = wifi.n + 1;
        if (sp != -1) s = s.substring(sp + 2, s.length());
        else break;
      }

      wifi.receivedWifiList = true;

      for (int i = 0; i < wifi.n; i++) {
        Serial.print(wifi.ssid[i]);
        Serial.println(wifi.rssi[i]);
      }

      break;
    case 1:
      break;
  }
}
