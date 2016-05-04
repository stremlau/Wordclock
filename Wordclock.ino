//#include <Adafruit_GFX.h>

#include <EEPROM.h>
#include <SoftwareSerial.h>
#include <FastLED.h>
#include <DS3232RTC.h>    //http://github.com/JChristensen/DS3232RTC
#include <Time.h>         //https://github.com/PaulStoffregen/Time
#include <Timezone.h>     //https://github.com/JChristensen/Timezone
#include <Wire.h>         //http://arduino.cc/en/Reference/Wire (included with Arduino IDE)

//#include <glcdfont.c>

SoftwareSerial btSerial(8, 9);
CRGB leds[10 * 11 + 4];

const byte ES[] =      {0, 0, 2};
const byte IST[] =     {0, 3, 3};
const byte FUENF[] =   {0, 7, 4};
const byte ZEHN[] =    {1, 0, 4};
const byte ZWANZIG[] = {1, 4, 7};
const byte VIERTEL[] = {2, 4, 7};
const byte NACH[] =    {3, 2, 4};
const byte VOR[] =     {3, 6, 3};
const byte HALB[] =    {4, 0, 4};
const byte UHR[] =     {9, 8, 3};

const byte HOUR[][3] = {
  //EIN       EINS       ZWEI       DREI       VIER       FUENF
  {5, 2, 3}, {5, 2, 4}, {5, 0, 4}, {6, 1, 4}, {7, 7, 4}, {6, 7, 4},
  //SECHS     SIEBEN     ACHT       NEUN       ZEHN       ELF        ZWÖLF
  {9, 1, 5}, {5, 5, 6}, {8, 1, 4}, {7, 3, 4}, {8, 5, 4}, {7, 0, 3}, {4, 5, 5}
};

/**
 * Contains all added words since  the last call of generateWords().
 */
const byte *new_words[6];
byte new_words_length = 0;

/**
 * Contains all removed words since the last call of generateWords().
 */
const byte *old_words[6];
byte old_words_length = 0;

/*
 * Contains all unchanged words since the last call of generateWords()
 */
const byte *const_words[6];
byte const_words_length = 0;

/**
 * Fore- and background color for the letters.
 */
CRGB foreground = CRGB(255, 245, 220);
CRGB background = CRGB(30, 30, 20); //CRGB(0, 255, 0) CRGB(10, 50, 5)

/**
 * Stores the effect number.
 */
byte effect = 2;

/**
 * Hardware version.
 */

byte hwVersion = 0;

/**
 * Should "Es ist" be showed?
 */
bool showEsIst = false;

//TimeChangeRule aEDT = {"AEDT", First, Sun, Oct, 2, 660};    //UTC + 11 hours
//TimeChangeRule aEST = {"AEST", First, Sun, Apr, 3, 600};    //UTC + 10 hours
TimeChangeRule CEST = {"CEST", Last, Sun, Mar, 2, 120};     //Central European Summer Time
TimeChangeRule CET = {"CET ", Last, Sun, Oct, 3, 60};       //Central European Standard Time
//TimeChangeRule BST = {"BST", Last, Sun, Mar, 1, 60};        //British Summer Time
//TimeChangeRule GMT = {"GMT", Last, Sun, Oct, 2, 0};         //Standard Time
//TimeChangeRule usEDT = {"EDT", Second, Sun, Mar, 2, -240};  //Eastern Daylight Time = UTC - 4 hours
//TimeChangeRule usEST = {"EST", First, Sun, Nov, 2, -300};   //Eastern Standard Time = UTC - 5 hours
//TimeChangeRule usCDT = {"CDT", Second, dowSunday, Mar, 2, -300};
//TimeChangeRule usCST = {"CST", First, dowSunday, Nov, 2, -360};
//TimeChangeRule usMDT = {"MDT", Second, dowSunday, Mar, 2, -360};
//TimeChangeRule usMST = {"MST", First, dowSunday, Nov, 2, -420};
//TimeChangeRule usPDT = {"PDT", Second, dowSunday, Mar, 2, -420};
//TimeChangeRule usPST = {"PST", First, dowSunday, Nov, 2, -480};

//Timezone ausET(aEDT, aEST); //Australia Eastern Time Zone (Sydney, Melbourne)
Timezone CE(CEST, CET); //Central European Time (Frankfurt, Paris)
//Timezone UK(BST, GMT); //United Kingdom (London, Belfast)
//Timezone usET(usEDT, usEST); //US Eastern Time Zone (New York, Detroit)
//Timezone usCT(usCDT, usCST); //US Central Time Zone (Chicago, Houston)
//Timezone usMT(usMDT, usMST); //US Mountain Time Zone (Denver, Salt Lake City)
//Timezone usAZ(usMST, usMST); //Arizona is US Mountain Time Zone but does not use DST
//Timezone usPT(usPDT, usPST); //US Pacific Time Zone (Las Vegas, Los Angeles)

Timezone timezones[] = {CE};

byte timezone = 0;

void setup() {
  Serial.begin(9600);

  //setup real time clock
  setSyncProvider(RTC.get);
  if(timeStatus() != timeSet) 
      Serial.println("Unable to sync with the RTC");
  else
      Serial.println("RTC has set the system time");

  //setup FastLED
  delay( 1000 ); // power-up safety delay
  FastLED.addLeds<NEOPIXEL, 7>(leds, 10 * 11 + 4);
  FastLED.setBrightness(255);

  for (int i = 0; i < 10 * 11 + 4; i++) {
    leds[i] = CRGB::Red;
  }
  FastLED.show();
  delay(100);

  btSerial.begin(9600);

  /*showChar(0, 2, 'A', CRGB(255, 255, 255));
  showChar(6, 2, 'O', CRGB(255, 255, 255));
  
  FastLED.show();
  delay(1500); */

  loadSettings();
  
  Serial.println("initialized");
}

void loop()
{
  generateWords();

  handleBluetooth();
  
  showCorners(foreground, background);
  
  switch (effect) {
    case 1: showFade(foreground, background); break;
    case 2: showTypewriter(foreground, background); break;
    case 3: showMatrix(foreground, background); break;
    case 4: showRollDown(foreground, background); break;
    case 5: showParty(foreground, background); break;
    case 0:
    default:
      showSimple(foreground, background);
      break;
  }
  
  FastLED.show();
  delay(100);
}

/**
 * Generates the words to show using the actual time.
 */
void generateWords() {
  time_t local = timezones[timezone].toLocal(now());
  
  clearWords();

  if (showEsIst) {
    addWord(ES);
    addWord(IST);
  }
  
  byte shour = hourFormat12(local);
  switch(minute(local) / 5) {
    case 0:  if (shour == 1) shour = 0;                 addWord(UHR); break;
    case 1:  addWord(FUENF);   addWord(NACH);                         break;
    case 2:  addWord(ZEHN);    addWord(NACH);                         break;
    case 3:  addWord(VIERTEL); addWord(NACH);                         break;
    case 4:  addWord(ZWANZIG); addWord(NACH);                         break;
    case 5:  addWord(FUENF);   addWord(VOR);  addWord(HALB); shour++; break;
    case 6:                                   addWord(HALB); shour++; break;
    case 7:  addWord(FUENF);   addWord(NACH); addWord(HALB); shour++; break;
    case 8:  addWord(ZWANZIG); addWord(VOR);                 shour++; break;
    case 9:  addWord(VIERTEL); addWord(VOR);                 shour++; break;
    case 10: addWord(ZEHN);    addWord(VOR);                 shour++; break;
    case 11: addWord(FUENF);   addWord(VOR);                 shour++; break;
  }
  
  if (shour == 13) shour = 1;
  addWord(HOUR[shour]);
}

void showCorners(CRGB on, CRGB off) {
  byte num = minute() % 5;
  for (byte i = 0; i < 4; i++) {
    leds[10 * 11 + i] = (i < num) ?  on : off;
  }
}

/**
 * Shows all the new and constant words.
 */
void showSimple(CRGB on, CRGB off) {
  fillLeds(off);
  showAllWords(on, new_words, new_words_length);
  showAllWords(on, const_words, const_words_length);
}

/**
 * Fades new words in and old words out.
 */
void showFade(CRGB on, CRGB off) {
  if (new_words_length > 0 || old_words_length > 0) {
    for (int e = 1; e <= 256/8; e++) {
      int i = e * 8 - 1;
      fillLeds(off);
      showAllWords(on, const_words, const_words_length);
      showAllWords(off.lerp8(on, i), new_words, new_words_length);
      showAllWords(on.lerp8(off, i), old_words, old_words_length);
      FastLED.show();
      delay(50);
    }
  }
  
  showSimple(on, off);
}

/**
 * Rolls characters out and in to change display..
 */
void showRollDown(CRGB on, CRGB off) {
  if (new_words_length > 0 || old_words_length > 0) {
    for (int e = 1; e <= 10; e++) {
      fillLeds(off);
      showAllWords(on, const_words, const_words_length, 0, e);
      showAllWords(on, old_words, old_words_length, 0, e);
      FastLED.show();
      delay(80 + (10 - e) * 5);
    }

    for (int e = 10; e >= 0; e--) {
      fillLeds(off);
      showAllWords(on, const_words, const_words_length, 0, -e);
      showAllWords(on, new_words, new_words_length, 0, -e);
      FastLED.show();
      delay(80 + (10 - e) * 8);
    }
  }
  
  showSimple(on, off);
}

/**
 * Shows the "matrix effect" in background color and the time in foreground color.
 */
byte matrix_worms[11] = {-5, -10, -3, -13, -1, 0, -1, -5, -6, -11, -4};
void showMatrix(CRGB on, CRGB off) {
  _fadeall();

  for (byte i = 0; i < 11; i++) {
    if (matrix_worms[i] < 10) {
      setLeds(matrix_worms[i], i, off, 1, false);
    }
    else if (matrix_worms[i] == 10) {
        matrix_worms[i] = -random8(14);
    }
    matrix_worms[i]++;
  }

  showAllWords(on, new_words, new_words_length);
  showAllWords(on, const_words, const_words_length);
}
void _fadeall() { for(int i = 0; i < 10 * 11; i++) { leds[i].nscale8(180); } } //180

/**
 * Removes old words and inserts new words in a typewriter style.
 */
void showTypewriter(CRGB on, CRGB off) {
  if (new_words_length > 0 || old_words_length > 0) {
    byte max_old_word = 0;
    byte max_new_word = 0;
  
    for (byte i = 0; i < old_words_length; i++)
      max_old_word = max(max_old_word, old_words[i][2]);
      
    for (byte i = 0; i < new_words_length; i++)
      max_new_word = max(max_new_word, new_words[i][2]);
  
    for (byte i = 0; i <= max_old_word; i++) {
      fillLeds(off);
      showAllWords(on, old_words, old_words_length, 0, 0, 200, i);
      showAllWords(on, const_words, const_words_length);
      FastLED.show();
      delay(180);
    }
    
    fillLeds(off);
    showAllWords(on, const_words, const_words_length);
    for (byte i = 0; i <= max_new_word; i++) {
      showAllWords(on, new_words, new_words_length, 0, 0, i, 0);
      FastLED.show();
      delay(180);
    }
  }

  showSimple(on, off);
}

/**
 * Rolls characters out and in to change display..
 */
void showParty(CRGB on, CRGB off) {
  static byte e = 0;
  e++;
  if (e < 3) return;
  e = 0;
  for(int i = 0; i < 110; i++) {
    leds[i] = CHSV(random8(), 255, 180);
  }
  
  showAllWords(on, new_words, new_words_length);
  showAllWords(on, const_words, const_words_length);
}

/**
 * Parses and executes the bluetooh commands.
 */
void handleBluetooth() {
  while (btSerial.available() >= 4) {
    byte type = btSerial.read();
    switch (type) {
      case 'F': { //foreground color
        byte red = btSerial.read();
        byte green = btSerial.read();
        byte blue = btSerial.read();
        foreground = CRGB(red, green, blue);
        break;
      }
      case 'B': { //background color
        byte red = btSerial.read();
        byte green = btSerial.read();
        byte blue = btSerial.read();
        background = CRGB(red, green, blue);
        break;
      }
      case 'E':
        effect = btSerial.read();
        showEsIst = (btSerial.read() == 1);
        btSerial.read();
        break;
      case 'T': { //time
        byte h = btSerial.read();
        byte m = btSerial.read();
        byte s = btSerial.read();
        setTime(h, m, s, day(), month(), year());
        RTC.set(now());
        break;
      }
      case 'D': { //date
        byte d = btSerial.read();
        byte m = btSerial.read();
        byte y = btSerial.read();
        setTime(hour(), minute(), second(), d, m, y);
        RTC.set(now());
        break;
      }
      case 'Z': { //timeZone
        timezone = btSerial.read();
        btSerial.read(); btSerial.read();
        break;
      }
      case 'S': {
        btSerial.read(); btSerial.read(); btSerial.read(); storeSettings();
        break;
      }
      case 'G': { //get
        switch (btSerial.read()) {
          case 'F':
            btSerial.write('F');
            btSerial.write(foreground.r);
            btSerial.write(foreground.g);
            btSerial.write(foreground.b);
            break;
          case 'B':
            btSerial.write('B');
            btSerial.write(background.r);
            btSerial.write(background.g);
            btSerial.write(background.b);
            break;
          case 'E':
            btSerial.write('E');
            btSerial.write(effect);
            btSerial.write(showEsIst);
            btSerial.write('E');
            break;
          case 'T':
            btSerial.write('T');
            btSerial.write(hour());
            btSerial.write(minute());
            btSerial.write(second());
            break;
          case 'D':
            btSerial.write('D');
            btSerial.write(day());
            btSerial.write(month());
            btSerial.write(year());
            break;
          case 'Z':
            btSerial.write('Z');
            btSerial.write(timezone);
            btSerial.write('Z'); btSerial.write('Z');
            break;
        }
        btSerial.read(); btSerial.read();
        break;
      }
      default: {
        Serial.print("Unknown command ");
        Serial.println(type);
        break;
      }
    }
  }
}

/**
 * Adds a word to the new words array. When it is was previously in the new words it is added to the constant words.
 */
void addWord(const byte part[]) {
  bool in_old_words = 0;
  for (byte i = 0; i < old_words_length; i++) {  
    if (part == old_words[i]) {
      in_old_words = 1;
      
      const_words[const_words_length] = part;
      const_words_length++;

      //remove in old_words
      old_words_length--;
      for (byte e = i; e < old_words_length; e++) {
        old_words[e] = old_words[e + 1];
      }
    }
  }

  if (!in_old_words) {
    new_words[new_words_length] = part;
    new_words_length++;
  }
}

/**
 * Put all new and constant words to the old words.
 */
void clearWords() {
  for (byte i = 0; i < new_words_length; i++) {
    old_words[i] = new_words[i];
  }
  old_words_length = new_words_length;

  for (byte i = 0; i < const_words_length; i++) {
    old_words[old_words_length] = const_words[i];
    old_words_length++;
  }
  
  new_words_length = 0;
  const_words_length = 0;
}

/**
 * Set all Leds to a single color.
 */
void fillLeds(CRGB off) {
  setLeds(0, 0, off, 11 * 10, false);
}

/**
 * Shows a array of words in a specific color.
 */
void showAllWords(CRGB color, const byte *wds[], byte wds_length) {
  showAllWords(color, wds, wds_length, 0, 0, 200, 0);
}

/**
 * Shows a array of words in a specific color.
 */
void showAllWords(CRGB color, const byte *wds[], byte wds_length, char xadd, char yadd) {
  showAllWords(color, wds, wds_length, xadd, yadd, 200, 0);
}

/**
 * Shows a array of words in a specific color.
 * @param maxlen determines the max length of every word.
 * @param cut cuts the x last characters of every word
 */
void showAllWords(CRGB color, const byte *wds[], byte wds_length, char xadd, char yadd, byte maxlen, byte cut) {
  for (byte i = 0; i < wds_length; i++) {
    setLeds(wds[i][0] + yadd, wds[i][1] + xadd, color, min(max(wds[i][2] - cut, 0), maxlen), false);
  }
}

/**
 * Sets a number of leds starting at x, y to the a specific color.
 * When add is true the color is added to the existing color at the positions.
 */
void setLeds(int y, int x, CRGB color, int len, bool add) {
  if (y < 0 || y > 9) return;
  int start_led = !(y % 2) ? y * 11 + x : y * 11 + (11 - x) - 1;
  int dir = !(y % 2) ? 1 : -1;
  if (hwVersion == 1) dir = !dir;

  if (add) {
    for (int i = 0; i < len; i++) {
      leds[start_led + i * dir] += color;
    }
  }
  else {
    for (int i = 0; i < len; i++) {
      leds[start_led + i * dir] = color;
    }
  }
}

/**
 * Stores settings to EEPROM.
 */
void storeSettings() {
  EEPROM.write(0, foreground.r);
  EEPROM.write(1, foreground.g);
  EEPROM.write(2, foreground.b);
  EEPROM.write(3, background.r);
  EEPROM.write(4, background.g);
  EEPROM.write(5, background.b);
  EEPROM.write(6, effect);
  EEPROM.write(7, showEsIst);
  EEPROM.write(8, hwVersion);
  EEPROM.write(9, timezone);
}

/** 
 * Loads settings from EEPROM.
 */
void loadSettings() {
  foreground.r = EEPROM.read(0);
  foreground.g = EEPROM.read(1);
  foreground.b = EEPROM.read(2);
  background.r = EEPROM.read(3);
  background.g = EEPROM.read(4);
  background.b = EEPROM.read(5);
  effect =       EEPROM.read(6);
  showEsIst =    EEPROM.read(7);
  hwVersion =    EEPROM.read(8);
  timezone =     EEPROM.read(9);
}

/**
 * Shows a 5x7 char at a specific position.

void showChar(byte x, byte y, unsigned char c, CRGB color) {
  for (int8_t i=0; i<6; i++ ) {
    uint8_t line;
    if (i == 5) 
      line = 0x0;
    else 
      line = pgm_read_byte(font+(c*5)+i);
    for (int8_t j = 0; j<8; j++) {
      if (line & 0x1) {
          setLeds(y+j, x+i, color, 1, false);
      }
      line >>= 1;
    }
  }
}
 */
