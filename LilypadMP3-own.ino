

// Required libraries:

#include <SPI.h>
#include <SdFat.h>
#include <FreeStack.h>
#include <SFEMP3Shield.h>
#include <PinChangeInt.h>



// Set debugging to true to get serial messages:

boolean debugging = true;   // TODO: remove this, after DEBUG outputs have been refactored

#define DEBUG    // remove or comment to disable debugging

#ifdef DEBUG
#define DEBUG_PRINT(x) Serial.print(x)
#define DEBUG_PRINTLN(x) Serial.println(x)
#define DEBUG_PRINTLNDEC(x) Serial.println(x, DEC)
#else
#define DEBUG_PRINT(x)
#define DEBUG_PRINTLN(x)
#define DEBUG_PRINTLNDEC(x) Serial.println(x, DEC)
#endif




// LilyPad MP3 pin definitions:

// Constants for the trigger input pins, which we'll place
// in an array for convenience:

#define TRIG1 A0
#define TRIG2 A4
#define TRIG3 A5
#define TRIG4 1
#define TRIG5 0
int trigger[5] = {TRIG1,TRIG2,TRIG3,TRIG4,TRIG5};


// #define PREV 5    // black cable, former blue LED
#define PREV A3
// #define NEXT 10   // red cable, former red LED
#define NEXT 3
#define VOL A1    // white cable, former green LED


// MP3 library pins (just reused from original examples)

#define SHDN_GPIO1 A2
#define MP3_DREQ 2
#define MP3_CS 6
#define MP3_DCS 7
#define MP3_RST 8
#define SD_CS 9
#define MOSI 11
#define MISO 12
#define SCK 13




// Player modes
const unsigned char TRACK_MODE = 0;
const unsigned char SKIP_MODE = 1;
unsigned char mode = TRACK_MODE;

const int32_t SKIP_STEP = 1000;


// Volume settings and flags (0 is the loudest, 255 is the lowest)
unsigned char volume = 40;              // Initial volume
const unsigned char MIN_VOLUME = 128;

// (analog read) values at volume pin
unsigned int vol_pin_value = 0;
unsigned int vol_pin_value_old = 0;


// Start up playing or not playing
boolean playing = true;


// Set loop_all to true if you would like to automatically
// start playing the next file after the current one ends:
boolean loop_all = true;


// Flags for button presses

volatile boolean prev_button_pressed = false;
volatile boolean prev_button_released = false;
volatile unsigned long prev_button_was_down_for = 0L;

volatile boolean next_button_pressed = false;
volatile boolean next_button_released = false;
volatile unsigned long next_button_was_down_for = 0L;


char track[13];   // TODO: Make it work with longer filenames


// Library objects:
SdFat sd;
SdFile file;
SFEMP3Shield MP3player;


void setup() {

  byte result;


  #ifdef DEBUG

  Serial.begin(9600);
  DEBUG_PRINTLN(F("Lilypad MP3 Player"));

  // ('F' places constant strings in program flash to save RAM)
  DEBUG_PRINT(F("Free RAM = "));
  DEBUG_PRINTLNDEC(FreeStack());

  #endif


  // Set up I/O pins:

  pinMode(TRIG1, INPUT);
  digitalWrite(TRIG1, HIGH);  // turn on weak pullup
  pinMode(TRIG2, INPUT);
  digitalWrite(TRIG2, HIGH);  // turn on weak pullup
  pinMode(TRIG3, INPUT);
  digitalWrite(TRIG3, HIGH);  // turn on weak pullup
  pinMode(TRIG4, INPUT);
  digitalWrite(TRIG4, HIGH);  // turn on weak pullup
  pinMode(TRIG5, INPUT);
  digitalWrite(TRIG5, HIGH);  // turn on weak pullup

  pinMode(PREV, INPUT);
  digitalWrite(PREV, HIGH);  // turn on weak pullup
  pinMode(NEXT, INPUT);
  digitalWrite(NEXT, HIGH);  // turn on weak pullup

  pinMode(VOL, INPUT);  // volume pin will be used as analog pin, so no pullup (?)


  // MP3 library pins (just reused from original examples)

  pinMode(SHDN_GPIO1, OUTPUT);
  pinMode(MP3_CS, OUTPUT);
  pinMode(MP3_DREQ, INPUT);
  pinMode(MP3_DCS, OUTPUT);
  pinMode(MP3_RST, OUTPUT);
  pinMode(SD_CS, OUTPUT);
  pinMode(MOSI, OUTPUT);
  pinMode(MISO, INPUT);
  pinMode(SCK, OUTPUT);


  // Turn off amplifier chip / turn on MP3 mode:
  digitalWrite(SHDN_GPIO1, LOW);




  // Initialize the SD card:

  DEBUG_PRINTLN(F("Initializing SD card... "));

  result = sd.begin(SD_SEL, SPI_HALF_SPEED);

  if (result != 1) {
    DEBUG_PRINTLN(F("error, halting"));
    // errorBlink(1,RED);
  }
  else {
    DEBUG_PRINTLN(F("OK"));
  }


  //Initialize the MP3 chip:
  DEBUG_PRINTLN(F("Initializing MP3 chip... "));

  result = MP3player.begin();

  // Check result, 0 and 6 are OK:
  if((result != 0) && (result != 6)) {
    DEBUG_PRINT(F("error "));
    DEBUG_PRINTLN(result);
    // errorBlink(result,BLUE);
  }
  else {
    DEBUG_PRINTLN(F("OK"));
  }


  // Set up interrupts.
  // We'll use the pin change interrupt library
  PCintPort::attachInterrupt(PREV, &prevButtonIRQ, CHANGE);
  PCintPort::attachInterrupt(NEXT, &nextButtonIRQ, CHANGE);


  DEBUG_PRINTLN(sizeof(track));


  // Get initial track:
  sd.chdir("/",true); // Index beginning of root directory
  getNextTrack();
  DEBUG_PRINT(F("current track: "));
  DEBUG_PRINTLN(track);

  // Set initial volume (same for both left and right channels)
  MP3player.setVolume(volume, volume);

  // Uncomment to get a directory listing of the SD card:
  sd.ls(LS_R | LS_DATE | LS_SIZE);

  // Turn on amplifier chip:
  digitalWrite(SHDN_GPIO1, HIGH);
  delay(2);

}


// TODO: prevent prev and next button from overlapping (i.e. when pressed simultaneously)

void prevButtonIRQ() {

  // Raw information from PinChangeInt library:

  // Serial.print("pin: ");
  // Serial.print(PCintPort::arduinoPin);
  // Serial.print(" state: ");
  // Serial.println(PCintPort::pinState);


  static boolean prev_button_state = false;
  static unsigned long prev_start, prev_end;


  // if button is currently being pressed down, but was up before
  if ( (PCintPort::pinState == LOW) && (prev_button_state == false) ) {

    prev_start = millis();  // discard button presses too close together (debounce)
    if (prev_start > (prev_end + 10) ) {  // 10ms debounce timer
      prev_button_state = true;
      prev_button_pressed = true;  // this is the flag the main loop can react on
    }

  }
  // button has been released but was down before
  else if ( (PCintPort::pinState == HIGH) && (prev_button_state == true) ) {

    prev_end = millis();  // discard button releases too close together (debounce)
    if (prev_end > (prev_start + 10) ) {  // 10ms debounce timer
      prev_button_state = false;
      prev_button_released = true;  // this is the flag the main loop can react on
      prev_button_was_down_for = prev_end - prev_start;
    }

  }

}


// TODO: prevent prev and next button from overlapping (i.e. when pressed simultaneously)

void nextButtonIRQ() {

  // Raw information from PinChangeInt library:

  // Serial.print("pin: ");
  // Serial.print(PCintPort::arduinoPin);
  // Serial.print(" state: ");
  // Serial.println(PCintPort::pinState);

  static boolean next_button_state = false;
  static unsigned long next_start, next_end;


  // if button is currently being pressed down, but was up before
  if ( (PCintPort::pinState == LOW) && (next_button_state == false) ) {

    next_start = millis();  // discard button presses too close together (debounce)
    if (next_start > (next_end + 10) ) {  // 10ms debounce timer
      next_button_state = true;
      next_button_pressed = true;  // this is the flag the main loop can react on
    }

  }
  // button has been released but was down before
  else if ( (PCintPort::pinState == HIGH) && (next_button_state = true) ) {

    next_end = millis();  // discard button releases too close together (debounce)
    if (next_end > (next_start + 10) ) {  // 10ms debounce timer
      next_button_state = false;
      next_button_released = true;  // this is the flag the main loop can react on
      next_button_was_down_for = next_end - next_start;
    }

  }

}




void loop() {

  // "Static" variables are initalized once the first time
  // the loop runs, but they keep their values through
  // successive loops.

  static boolean prev_button_down = false;
  static unsigned long int prev_button_down_start, prev_button_downtime;

  static boolean next_button_down = false;
  static unsigned long int next_button_down_start, next_button_downtime;


  // DELETE ME: static boolean reported = false;



  // Volume Management

  // TODO: prevent complete muting, so that it does not continue playing without being noticed


  // Volume on LilypadMP3 is a value between 0 and 255 with 0 being the loudest

  // We don't want the player to run quietly without being noticed.
  // Also, when a speaker is connected to the Lilypad, then the lower half of the
  // volume range (255 to about 128) is very very quiet anyway (most likely too quiet).
  // That's why we consider the minimum volume defined at the top.
  // The potentiometer is then just adjusting withing the remaining volume
  // range (i.e. between MIN_VOLUME and 0)


  // Read analog value from volume pin (0-1023).
  // At the pin, 0 means potentiomater is "closed", 1023 means full open.
  // Since 0 is the loudest on LilypadMP3, we switch the direction here by subtracting
  // the measured value from the max value (i.e. 1023)
  vol_pin_value = 1023 - analogRead(VOL);

  // we don't want to react on tiny shaky changes which are normal for analog input
  // so we check if the value at volume pin changed substantially (i.e. changed by more than 2)
  if ( abs(vol_pin_value - vol_pin_value_old) > 2 ) {

    vol_pin_value_old = vol_pin_value;  // remember the pin value for comparison next time

    // Divide the value at the volume pin in such a way, that the vol_pin_range (0-1023)
    // evenly covers the volume range between 0 (loudest) and defined MIN_VOLUME
    unsigned char new_volume = vol_pin_value / (1024 / MIN_VOLUME);

    // make sure that we call the setVolume method only if we really have a new volume value
    if (new_volume != volume) {
      // DEBUG_PRINT("Setting volume to: ");
      // DEBUG_PRINTLN(new_volume);
      volume = new_volume;
      MP3player.setVolume(volume, volume);  // set volume, same value for left and right speaker
    }
  }



  // The prev button IRQ sets several flags to true, one for
  // button_pressed, one for button_released. We'll clear these
  // when we're done handling them:


  // prev button has been pressed down "recently"
  if (prev_button_pressed) {

    if (debugging) Serial.println(F("prev button pressed"));

    prev_button_pressed = false;  // Clear flag set by interrupt handler

    // We'll set another flag saying the button is now down.
    // This way we can keep track of how long the button is being held down.
    //
    // We can't do this in interrupts, because the button state doesn't change
    // while it's being held down.

    prev_button_down = true;
    prev_button_down_start = millis();
  }


  // prev button has been released "recently"
  if (prev_button_released) {

    if (debugging)
    {
      Serial.print(F("prev button released, was down for: "));
      Serial.println(prev_button_was_down_for,DEC);
    }

    // For short button presses, we jump to the previous track
    if (prev_button_was_down_for < 1000) {

      if (playing) {
        // TODO: stay in same playlist when jumping back
        mode = TRACK_MODE;
        Serial.println(F("Previous track!"));
        stopPlaying();
        getPrevTrack();
        startPlaying();
      }
    }

    prev_button_released = false;  // Clear flag set by interrupt handler
    prev_button_down = false;  // now button is not down anymore
  }



  // as long as the prev button is down, we'll enter this segment
  if (prev_button_down) {

    prev_button_downtime = millis() - prev_button_down_start;

    // button is now down already for more than 1 second -> rewind
    if (prev_button_downtime > 1000) {

      mode = SKIP_MODE;

      DEBUG_PRINTLN(F("Rewinding ..."));
      uint8_t result;
      result = MP3player.skip(SKIP_STEP * -1);
      if(result != 0) {
        DEBUG_PRINT(F("Error code: "));
        DEBUG_PRINT(result);
        DEBUG_PRINT(F(" when trying to rewind track. Jumping to beginning of track: "));
        result = MP3player.skipTo(0);
        DEBUG_PRINTLN(result);
        prev_button_down = false;
      }


      if (debugging) {
      }
    }
  }




  // next button has been pressed down "recently"
  if (next_button_pressed) {

    if (debugging) Serial.println(F("next button pressed"));

    next_button_pressed = false;  // Clear flag set by interrupt handler

    // We'll set another flag saying the button is now down.
    // This way we can keep track of how long the button is being held down.
    //
    // We can't do this in interrupts, because the button state doesn't change
    // while it's being held down.

    next_button_down = true;
    next_button_down_start = millis();
  }


  // next button has been released "recently"
  if (next_button_released) {

    if (debugging) {
      Serial.print(F("next button released, was down for: "));
      Serial.println(next_button_was_down_for, DEC);
    }

    // For short buttton presses, we jump to the next track
    if (next_button_was_down_for < 1000) {

      if (playing) {
        // TODO: stay in same playlist when jumping forward
        mode = TRACK_MODE;
        Serial.println(F("Next track!"));
        stopPlaying();
        getNextTrack();
        startPlaying();
      }
    }

    next_button_released = false;   // Clear flag set by interrupt handler
    next_button_down = false;   // now the button is not down anymore
  }




  // as long as the next button is down, we'll enter this segment
  if (next_button_down) {

    next_button_downtime = millis() - next_button_down_start;

    // button is now down already for more than 1 second -> fast forward
    if (next_button_downtime > 1000) {

      mode = SKIP_MODE;

      //log(F("Fast forwarding ..."), true);

      if (debugging) {
        Serial.println(F("Fast forwarding ..."));
        uint8_t result;
        result = MP3player.skip(SKIP_STEP);
        if(result != 0) {
          Serial.print(F("Error code: "));
          Serial.print(result);
          Serial.println(F(" when trying to fast forward track. Jumping to beginning of next track."));
          stopPlaying();
          getNextTrack();
          startPlaying();
        }
      }
    }
  }





  // Handle "last track ended" situations
  // (should we play the next track?)

  // Are we in "playing" mode, and has the
  // current file ended?

  if (playing && !MP3player.isPlaying())
  {
    getNextTrack(); // Set up for next track

    // If loop_all is true, start the next track

    if (loop_all)
    {
      startPlaying();
    }
    else
      playing = false;
  }
}


void changeVolume(boolean direction)
{
  // Increment or decrement the volume.
  // This is handled internally in the VS1053 MP3 chip.
  // Lower numbers are louder (0 is the loudest).

  if (volume < 255 && direction == false)
    volume += 2;

  if (volume > 0 && direction == true)
    volume -= 2;

  MP3player.setVolume(volume, volume);

  if (debugging)
  {
    Serial.print(F("volume "));
    Serial.println(volume);
  }
}


void getNextTrack()
{
  // Get the next playable track (check extension to be
  // sure it's an audio file)

  do
    getNextFile();
  while(isPlayable() != true);
}


void getPrevTrack()
{
  // Get the previous playable track (check extension to be
  // sure it's an audio file)

  do
    getPrevFile();
  while(isPlayable() != true);
}


void getNextFile()
{
  // Get the next file (which may be playable or not)

  int result = (file.openNext(sd.vwd(), O_READ));

  // If we're at the end of the directory,
  // loop around to the beginning:

  if (!result)
  {
    Serial.println("looping around to beginning of directory");
    sd.chdir("/",true);
    getNextTrack();
    return;
  }
  file.getName(track, 13);
  file.close();
  Serial.print("Next file: ");
  Serial.println(track);
}


void getPrevFile()
{
  // Get the previous file (which may be playable or not)

  char test[13], prev[13];

  // Getting the previous file is tricky, since you can
  // only go forward when reading directories.

  // To handle this, we'll save the name of the current
  // file, then keep reading all the files until we loop
  // back around to where we are. While doing this we're
  // saving the last file we looked at, so when we get
  // back to the current file, we'll return the previous
  // one.

  // Yeah, it's a pain.

  strcpy(test,track);

  do
  {
    strcpy(prev,track);
    getNextTrack();
  }
  while(strcasecmp(track,test) != 0);

  strcpy(track,prev);
}


void startPlaying()
{
  int result;

  if (debugging)
  {
    Serial.print(F("playing "));
    Serial.print(track);
    Serial.print(F("..."));
  }

  result = MP3player.playMP3(track);

  if (debugging)
  {
    Serial.print(F(" result "));
    Serial.println(result);
  }
}


void stopPlaying()
{
  if (debugging) Serial.println(F("stopping playback"));
  MP3player.stopTrack();
}


boolean isPlayable()
{
  // Check to see if a filename has a "playable" extension.
  // This is to keep the VS1053 from locking up if it is sent
  // unplayable data.

  char *extension;

  extension = strrchr(track,'.');
  extension++;
  if (
    (strcasecmp(extension,"MP3") == 0) ||
    (strcasecmp(extension,"WAV") == 0) ||
    (strcasecmp(extension,"MID") == 0) ||
    (strcasecmp(extension,"MP4") == 0) ||
    (strcasecmp(extension,"WMA") == 0) ||
    (strcasecmp(extension,"FLA") == 0) ||
    (strcasecmp(extension,"OGG") == 0) ||
    (strcasecmp(extension,"AAC") == 0)
  )
    return true;
  else
    return false;
}


void LEDmode(unsigned char mode)
{
  // Change the RGB LED to a specific color for each mode
  // (See #defines at start of sketch for colors.)


}


void setLEDcolor(unsigned char color)
{
  // Set the RGB LED in the (optional) rotary encoder
  // to a specific color. See the color #defines at the
  // start of this sketch.

  // digitalWrite(ROT_LEDR,color & B001);
  // digitalWrite(ROT_LEDG,color & B010);
  // digitalWrite(ROT_LEDB,color & B100);
}


void errorBlink(int blinks, byte color)
{
  // This function will blink the RGB LED in the rotary encoder
  // (optional) a given number of times and repeat forever.
  // This is so you can see error codes without having to use
  // the serial monitor window.

  int x;

  while(true) // Loop forever
  {
    for (x=0; x < blinks; x++) // Blink a given number of times
    {
      setLEDcolor(color);
      delay(250);
      // setLEDcolor(OFF);
      delay(250);
    }
    delay(1500); // Longer pause between blink-groups
  }
}


void log(char *message, boolean linebreak) {

  if (debugging) {
    if (linebreak) {
      Serial.println(message);
    }
    else {
      Serial.print(message);
    }
  }

}

