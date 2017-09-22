// "Player" example sketch for Lilypad MP3 Player
// Mike Grusin, SparkFun Electronics
// http://www.sparkfun.com

// This sketch turns the Lilypad MP3 Player into a basic MP3
// player that you can control with the optional rotary encoder.

// HARDWARE

// To use this sketch, an optional RGB rotary encoder must be soldered
// to the MP3 Player board. The board is designed to hold SparkFun
// part number COM-10892. https://www.sparkfun.com/products/10982

// SOFTWARE

// This sketch requires the following libraries. These are included
// in the Lilypad MP3 Player firmware zip file and must be copied to
// a "libraries" folder in your Arduino sketch directory:

// Uses the SdFat library by William Greiman, which is supplied
// with this archive, or download from http://code.google.com/p/sdfatlib/

// Uses the SFEMP3Shield library by Porter and Flaga, which is supplied
// with this archive, or download from http://www.billporter.info/

// Uses the PinChangeInt library by Lex Talionis, which is supplied
// with this archive, or download from http://code.google.com/p/arduino-pinchangeint/

// BASIC OPERATION:

// Place your audio files in the root directory of the SD card.
// Your files MUST have one of the following extensions: MP3, WAV,
// MID (MIDI), MP4, WMA, AAC, FLA, OGG. Note that this is solely to
// prevent the VS1053 from locking up from being fed non-audio data
// (files without one of the above extensions are quietly skipped).
// You can rename any playable file to any of the above extensions,
// or add additional extensions to the isPlayable() function below.
// See the VS1053 datasheet for the audio file types it can play.

// The player has two modes, TRACK and VOLUME. In TRACK mode, turning
// the knob will move to the next or previous track. In VOLUME mode,
// turning the knob will increase or decrease the volume.

// You can tell what mode you're in by the color of the RGB LED in the
// knob of the rotary encoder. TRACK mode is red, VOLUME mode is green.

// To switch between modes, hold down the button on the rotary encoder
// until the color changes (more than one second).

// To start and stop playback, press the button on the rotary encoder
// *quickly* (less than one second). When the player is playing, it
// will stubbornly keep playing; starting new tracks when the previous
// one ends, and switching to new tracks if you turn the knob in TRACK
// mode. When the player is stopped, it will not start playing until
// you press the button *quickly*, but it will silently change tracks
// or adjust the volume if you turn the knob.

// SERIAL DEBUGGING

// This sketch can output serial debugging information if desired
// by changing the global variable "debugging" to true. Note that
// this will take away trigger inputs 4 and 5, which are shared
// with the TX and RX lines. You can keep these lines connected to
// trigger switches and use the serial port as long as the triggers
// are normally open (not grounded) and remain ungrounded while the
// serial port is in use.

// License:
// We use the "beerware" license for our firmware. You can do
// ANYTHING you want with this code. If you like it, and we meet
// someday, you can, but are under no obligation to, buy me a
// (root) beer in return.

// Have fun!
// -your friends at SparkFun

// Revision history:
// 1.0 initial release MDG 2013/1/31

// Required libraries:

#include <SPI.h>
#include <SdFat.h>
#include <FreeStack.h>
#include <SFEMP3Shield.h>
#include <PinChangeInt.h>



// Set debugging to true to get serial messages:

boolean debugging = true;

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





// Possible modes (first and last are there to make
// rotating through them easier):

#define FIRST_MODE 0
#define TRACK 0
#define VOLUME 1
#define LAST_MODE 1

// Initial mode for the rotary encoder. TRACK lets you
// select audio tracks, VOLUME lets you change the volume.
// In any mode, a quick press will start and stop playback.
// A longer press will switch to the next mode.

unsigned char rotary_mode = TRACK;

// Initial volume for the MP3 chip. 0 is the loudest, 255
// is the lowest.

unsigned char volume = 40;
unsigned int vol_pin_value = 0;
unsigned int vol_pin_value_old = 0;


// Start up *not* playing:

boolean playing = true;

// Set loop_all to true if you would like to automatically
// start playing the next file after the current one ends:

boolean loop_all = true;



#define TRACK_MODE 0
#define SKIP_MODE 1
unsigned char mode = TRACK_MODE;

int32_t SKIP_STEP = 1000;


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



#define SHDN_GPIO1 A2
#define MP3_DREQ 2
#define MP3_CS 6
#define MP3_DCS 7
#define MP3_RST 8
#define SD_CS 9
#define MOSI 11
#define MISO 12
#define SCK 13





/*  Not used pins or former definition

#define ROT_LEDR 10
#define ROT_LEDG A1
#define ROT_LEDB 5

#define ROT_A 3
#define ROT_B A3
#define ROT_SW 4

#define RIGHT A6
#define LEFT A7


// RGB LED colors (for common anode LED, 0 is on, 1 is off)

#define OFF B111
#define RED B110
#define GREEN B101
#define YELLOW B100
#define BLUE B011
#define PURPLE B010
#define CYAN B001
#define WHITE B000


*/





// Global variables and flags for interrupt request functions:

// volatile int rotary_counter = 0; // Current "position" of rotary encoder (increments CW)
// volatile boolean rotary_change = false; // Will turn true if rotary_counter has changed
// volatile boolean rotary_direction; // Direction rotary encoder was turned (true = CW)
// volatile boolean button_pressed = false; // Will turn true if the button has been pushed
// volatile boolean button_released = false; // Will turn true if the button has been released (sets button_downtime)
// volatile unsigned long button_downtime = 0L; // ms the button was pushed before release


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


void setup()
{
  byte result;

  #ifdef DEBUG

  Serial.begin(9600);
  DEBUG_PRINTLN(F("Lilypad MP3 Player"));

  // ('F' places constant strings in program flash to save RAM)
  DEBUG_PRINT(F("Free RAM = "));
  DEBUG_PRINTLNDEC(FreeStack());

  #endif

  if (debugging)
  {

  }

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
  // setLEDcolor(OFF);



/* Not used anymore

  pinMode(ROT_B, INPUT);
  digitalWrite(ROT_B, HIGH); // turn on weak pullup
  pinMode(ROT_A, INPUT);
  digitalWrite(ROT_A, HIGH); // turn on weak pullup
  pinMode(ROT_SW, INPUT);
  // switch is common anode with external pulldown, do not turn on pullup

  pinMode(ROT_LEDB, OUTPUT);
  pinMode(ROT_LEDG, OUTPUT);
  pinMode(ROT_LEDR, OUTPUT);


*/



  // Initialize the SD card:

  if (debugging) Serial.println(F("Initializing SD card... "));

  result = sd.begin(SD_SEL, SPI_HALF_SPEED);

  if (result != 1)
  {
    if (debugging) Serial.println(F("error, halting"));
    // errorBlink(1,RED);
  }
  else
    if (debugging) Serial.println(F("OK"));

  //Initialize the MP3 chip:

  if (debugging) Serial.println(F("Initializing MP3 chip... "));

  result = MP3player.begin();

  // Check result, 0 and 6 are OK:

  if((result != 0) && (result != 6))
  {
    if (debugging)
    {
      Serial.print(F("error "));
      Serial.println(result);
    }
    // errorBlink(result,BLUE);
  }
  else
    if (debugging) Serial.println(F("OK"));

  // Set up interrupts. We'll use the standard external interrupt
  // pin for the rotary, but we'll use the pin change interrupt
  // library for the button:

  // attachInterrupt(1,rotaryIRQ,CHANGE);
  // PCintPort::attachInterrupt(ROT_SW, &buttonIRQ, CHANGE);
  PCintPort::attachInterrupt(PREV, &prevButtonIRQ, CHANGE);
  PCintPort::attachInterrupt(NEXT, &nextButtonIRQ, CHANGE);


  Serial.println(sizeof(track));

  // Get initial track:

  sd.chdir("/",true); // Index beginning of root directory
  getNextTrack();
  if (debugging)
  {
    Serial.print(F("current track: "));
    Serial.println(track);
  }

  // Set initial volume (same for both left and right channels)

  MP3player.setVolume(volume, volume);

  // Initial mode for the rotary encoder

  LEDmode(rotary_mode);

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

  // read analog value from volume pin (0-1023)
  // since 0 is the loudest on LilypadMP3 and we want to use a positive logarithmic potentionmeter
  // for volume control, we switch the direction here by subtracting the measured value from the max value
  vol_pin_value = 1023 - analogRead(VOL);

  // we don't want to react on tiny shaky changes which are normal for analog input
  // so we check if the value at volume pin changed substantially (i.e. changed by more than 2)
  if ( abs(vol_pin_value - vol_pin_value_old) > 2 ) {
    // Serial.print("New volume pin value: ");
    // Serial.println(vol_pin_value);
    vol_pin_value_old = vol_pin_value;  // remember the pin value for comparison next time

    // divide pin value by 4, so we get a value between 0 and 255,
    // which can be used directly for setVolume method
    unsigned char new_volume = vol_pin_value / 4;

    // make sure that we call the setVolume method only if we really have a new volume value
    if (new_volume != volume) {
      Serial.print("Setting volume to: ");
      Serial.println(new_volume);
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

  switch (mode)
  {
    case TRACK:
      // setLEDcolor(RED);
      break;
    case VOLUME:
      // setLEDcolor(GREEN);
      break;
    default:
      // setLEDcolor(OFF);
      break;
  }
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

