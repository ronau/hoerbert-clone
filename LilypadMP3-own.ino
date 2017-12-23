
// Required libraries:

#include <SPI.h>
#include <SdFat.h>
#include <FreeStack.h>
#include <SFEMP3Shield.h>
#include <PinChangeInt.h>
#include <EEPROM.h>
#include <Wire.h>
#include <SFE_TPA2016D2.h>

// forward declaration of changePlaylist method
void changePlaylist(byte pl, boolean reset_track_resume_value = true);



#define DEBUG    // remove or comment to disable debugging

#ifdef DEBUG
#define DPRINT(x) Serial.print(x)
#define DPRINTF(x) Serial.print(F(x))
#define DPRINTLN(x) Serial.println(x)
#define DPRINTLNF(x) Serial.println(F(x))
#define DPRINTLNDEC(x) Serial.println(x, DEC)
#define DPRINTLNBIN(x) Serial.println(x, BIN)
#else
#define DPRINT(x)
#define DPRINTF(x)
#define DPRINTLN(x)
#define DPRINTLNF(x)
#define DPRINTLNDEC(x)
#define DPRINTLNBIN(x)
#endif




// LilyPad MP3 pin definitions:

// Constants for the trigger input pins, which we'll place
// in an array for convenience:

const byte TRIG1_PIN = A0;
const byte TRIG2_PIN = A4;
const byte TRIG3_PIN = A5;
const byte TRIG4_PIN = 1;
const byte TRIG5_PIN = 0;
byte trigger[5] = {TRIG1_PIN,TRIG2_PIN,TRIG3_PIN,TRIG4_PIN,TRIG5_PIN};


const byte PREV_PIN = A3;
const byte NEXT_PIN = 3;
const byte VOL_PIN = A1;


// MP3 library pins (just reused from original examples)

const byte SHDN_GPIO1_PIN = A2;
const byte MP3_DREQ_PIN = 2;
const byte MP3_CS_PIN = 6;
const byte MP3_DCS_PIN = 7;
const byte MP3_RST_PIN = 8;
const byte SD_CS_PIN = 9;
const byte MOSI_PIN = 11;
const byte MISO_PIN = 12;
const byte SCK_PIN = 13;




// Player properties

const int SKIP_STEP = 3000;     // milliseconds to skip with each skip step

// Start up playing or not playing
boolean playing = true;

// Set loop_all to true if you would like to automatically
// start playing the next file after the current one ends:
boolean loop_all = true;


// Volume settings and flags (0 is the loudest, 255 is the lowest)
byte volume = 40;              // Default volume
const byte MIN_VOLUME = 104;

// (analog read) values at volume pin
unsigned int vol_pin_value = 0;
unsigned int vol_pin_value_old = 0;

// EEPROM addresses for resume function on startup
const int PL_ADDRESS = 0;
const int TR_ADDRESS = 1;



// Flags for button presses

volatile boolean prev_button_pressed = false;
volatile boolean prev_button_released = false;

volatile boolean next_button_pressed = false;
volatile boolean next_button_released = false;

volatile boolean trigger1_pressed = false;
volatile boolean trigger1_released = false;
volatile boolean trigger2_pressed = false;
volatile boolean trigger2_released = false;
volatile boolean trigger3_pressed = false;
volatile boolean trigger3_released = false;
volatile boolean trigger4_pressed = false;
volatile boolean trigger4_released = false;
volatile boolean trigger5_pressed = false;
volatile boolean trigger5_released = false;

// Timer variables (commented out)
// Uncomment here and in IRQ methods below
// if you need information about how long a button was presed
//
// volatile unsigned long prev_button_was_down_for = 0L;
// volatile unsigned long next_button_was_down_for = 0L;
// volatile unsigned long trigger1_was_down_for = 0L;
// volatile unsigned long trigger2_was_down_for = 0L;
// volatile unsigned long trigger3_was_down_for = 0L;
// volatile unsigned long trigger4_was_down_for = 0L;
// volatile unsigned long trigger5_was_down_for = 0L;



// File and playlist handling

const byte MAX_PLAYLIST = 5;    // Number of playlists
const byte MAX_TRACKS = 30;     // Maximum number of tracks per playlist
                                // Attention: increasing this number could cause memory issues

// The folder names for the playlists
const char* playlist_folder[] = {"/01", "/02", "/03", "/04", "/05"};


char track[13] = "";            // Filename of current track (8.3 filename format supported only)
byte track_index;               // Index of current track within current playlist (directory), starting with 0
byte playlist_index;            // Index of current playlist, starting with 0

char filename[MAX_TRACKS][13];  // All track names in current directory
                                // - will be read and kept in RAM for sorting alphabetically
byte num_tracks = 0;            // Number of playable tracks in current directory


// Library objects:
SdFat sd;
SdFile file;
SFEMP3Shield MP3player;
SFE_TPA2016D2 amp;




void setup() {

  byte result;


  #ifdef DEBUG
  Serial.begin(9600);
  #endif

  DPRINTLNF("Lilypad MP3 Player");
  DPRINTF("Free RAM = ");
  DPRINTLNDEC(FreeStack());




  // Set up I/O pins:

  pinMode(TRIG1_PIN, INPUT);
  digitalWrite(TRIG1_PIN, HIGH);  // turn on weak pullup
  pinMode(TRIG2_PIN, INPUT);
  digitalWrite(TRIG2_PIN, HIGH);  // turn on weak pullup
  pinMode(TRIG3_PIN, INPUT);
  digitalWrite(TRIG3_PIN, HIGH);  // turn on weak pullup
  pinMode(TRIG4_PIN, INPUT);
  digitalWrite(TRIG4_PIN, HIGH);  // turn on weak pullup
  pinMode(TRIG5_PIN, INPUT);
  digitalWrite(TRIG5_PIN, HIGH);  // turn on weak pullup

  pinMode(PREV_PIN, INPUT);
  digitalWrite(PREV_PIN, HIGH);  // turn on weak pullup
  pinMode(NEXT_PIN, INPUT);
  digitalWrite(NEXT_PIN, HIGH);  // turn on weak pullup

  pinMode(VOL_PIN, INPUT);  // volume pin will be used as analog pin, so no pullup (?)


  // MP3 library pins (just reused from original examples)
  pinMode(SHDN_GPIO1_PIN, OUTPUT);
  pinMode(MP3_CS_PIN, OUTPUT);
  pinMode(MP3_DREQ_PIN, INPUT);
  pinMode(MP3_DCS_PIN, OUTPUT);
  pinMode(MP3_RST_PIN, OUTPUT);
  pinMode(SD_CS_PIN, OUTPUT);
  pinMode(MOSI_PIN, OUTPUT);
  pinMode(MISO_PIN, INPUT);
  pinMode(SCK_PIN, OUTPUT);

  // Turn off amplifier chip / turn on MP3 mode:
  digitalWrite(SHDN_GPIO1_PIN, LOW);




  // Initialize the SD card:
  DPRINTLNF("Initializing SD card... ");
  result = sd.begin(SD_SEL, SPI_HALF_SPEED);

  if (result != 1) {
    DPRINTLNF("error, halting");
  }
  else {
    DPRINTLNF("OK");
  }


  //Initialize the MP3 chip:
  DPRINTLNF("Initializing MP3 chip... ");
  result = MP3player.begin();

  // Check result, 0 and 6 are OK:
  if((result != 0) && (result != 6)) {
    DPRINTF("error ");
    DPRINTLN(result);
  }
  else {
    DPRINTLNF("OK");
  }

  MP3player.setMonoMode(1);   // Enable mono mode


  // Turn on amplifier chip and reconfigure using I2C
  digitalWrite(SHDN_GPIO1_PIN, HIGH);
  delay(2);

  // The amp chip has a feature called 'automatic gain control'.
  // From the datasheet:
  //
  // The Automatic Gain Control (AGC) feature provides continuous automatic
  // gain adjustment to the amplifier through an internal PGA. This feature
  // enhances the perceived audio loudness and at the same time prevents
  // speaker damage from occurring (Limiter function).
  //
  //
  // As a side effect the amp will slowly increase volume on startup of the MP3 player
  // and if volume has been increased rapidly.
  // This might confuse the user of the player (especially children).
  // That's why we will switch off this behaviour.
  //
  // We will do so by rewriting the amp chip's configuration register 7 via I2C (Wire).
  // By default, register 7 has the following value: 0b11000001
  // By changing the last two bits to 00 we switch off the compression of the amp,
  // thus disabling the whole 'automatic gain control' feature.
  // For ease of use, we use the SFE_TPA2016D2 library provided by SparkFun.

  byte compressionratio;

  if ( amp.writeCompressionRatio(0) ) {
    DPRINTLNF("Wire/I2C: Automatic Gain Control disabled.");
  }
  else {
    DPRINTLNF("Wire/I2C: Disabling Automatic Gain Control failed.");
  }

  if ( amp.readCompressionRatio(compressionratio) ) {
    DPRINTF("Wire/I2C: Compression ratio: ");
    DPRINTLNBIN(compressionratio);
  }
  else {
    DPRINTLNF("Wire/I2C: Reading compression ratio failed.");
  }


  // Set up interrupts.
  // We'll use the pin change interrupt library
  PCintPort::attachInterrupt(PREV_PIN, &prevButtonIRQ, CHANGE);
  PCintPort::attachInterrupt(NEXT_PIN, &nextButtonIRQ, CHANGE);

  PCintPort::attachInterrupt(TRIG1_PIN, &trigger1IRQ, CHANGE);
  PCintPort::attachInterrupt(TRIG2_PIN, &trigger2IRQ, CHANGE);
  PCintPort::attachInterrupt(TRIG3_PIN, &trigger3IRQ, CHANGE);
  #ifndef DEBUG
  PCintPort::attachInterrupt(TRIG4_PIN, &trigger4IRQ, CHANGE);
  PCintPort::attachInterrupt(TRIG5_PIN, &trigger5IRQ, CHANGE);
  #endif


  // RESUME PLAYING (i.e. start with last played track, but from beginning)
  byte resume_pl = EEPROM.read(PL_ADDRESS);
  byte resume_tr = EEPROM.read(TR_ADDRESS);

  DPRINTF("Resume values: playlist ");
  DPRINT(resume_pl);
  DPRINTF(", track ");
  DPRINTLN(resume_tr);

  // make sure that playlist resume value from EEPROM is within allowed range
  if (resume_pl > -1 && resume_pl < MAX_PLAYLIST) {

    changePlaylist(resume_pl, false);

    // change track only if track resume value is within size of current playlist
    if (resume_tr > -1 && resume_tr < num_tracks) {
      track_index = resume_tr;                // take over resume value as current track index
      strcpy(track, filename[track_index]);   // Get name of current track from filename array
    }
    else {
      DPRINTF("Track resume value ");
      DPRINT(resume_tr);
      DPRINTLNF(" out of range. Track has not been switched.");
    }

  }
  else {                                              // fallback: play first playlist
    DPRINTF("Playlist resume value ");
    DPRINT(resume_pl);
    DPRINTLNF(" out of range. Switching to first playlist.");

    changePlaylist(0, false);
  }


  // Uncomment to get a directory listing of the SD card:
  // sd.ls(LS_R | LS_DATE | LS_SIZE);

  updateVolume();   // Set initial volume

  startPlaying();     // Start playing the current track

}




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
      // prev_button_was_down_for = prev_end - prev_start;
    }

  }

}


void nextButtonIRQ() {

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
      // next_button_was_down_for = next_end - next_start;
    }

  }

}


void trigger1IRQ() {

  static boolean trigger1_state = false;
  static unsigned long t1_start, t1_end;

  // if button is currently being pressed down, but was up before
  if ( (PCintPort::pinState == LOW) && (trigger1_state == false) ) {

    t1_start = millis();    // discard button presses too close together (debounce)
    if (t1_start > (t1_end + 10) ) {  // 10 ms debounce timer
      trigger1_state = true;
      trigger1_pressed = true;  // this is the flag the main loop can react on
    }
  }

  // button has been released but was down before
  else if ( (PCintPort::pinState == HIGH) && (trigger1_state == true) ) {

    t1_end = millis();  // discard button releases too close together (debounce)
    if (t1_end > (t1_start + 10) ) {  // 10 ms debounce timer
      trigger1_state = false;
      trigger1_released = true;   // this is the flag the main loop can react on
      // trigger1_was_down_for = t1_end - t1_start;
    }
  }

}


void trigger2IRQ() {

  static boolean trigger2_state = false;
  static unsigned long t2_start, t2_end;

  // if button is currently being pressed down, but was up before
  if ( (PCintPort::pinState == LOW) && (trigger2_state == false) ) {

    t2_start = millis();    // discard button presses too close together (debounce)
    if (t2_start > (t2_end + 10) ) {  // 10 ms debounce timer
      trigger2_state = true;
      trigger2_pressed = true;  // this is the flag the main loop can react on
    }
  }

  // button has been released but was down before
  else if ( (PCintPort::pinState == HIGH) && (trigger2_state == true) ) {

    t2_end = millis();  // discard button releases too close together (debounce)
    if (t2_end > (t2_start + 10) ) {  // 10 ms debounce timer
      trigger2_state = false;
      trigger2_released = true;   // this is the flag the main loop can react on
      // trigger2_was_down_for = t2_end - t2_start;
    }
  }

}


void trigger3IRQ() {

  static boolean trigger3_state = false;
  static unsigned long t3_start, t3_end;

  // if button is currently being pressed down, but was up before
  if ( (PCintPort::pinState == LOW) && (trigger3_state == false) ) {

    t3_start = millis();    // discard button presses too close together (debounce)
    if (t3_start > (t3_end + 10) ) {  // 10 ms debounce timer
      trigger3_state = true;
      trigger3_pressed = true;  // this is the flag the main loop can react on
    }
  }

  // button has been released but was down before
  else if ( (PCintPort::pinState == HIGH) && (trigger3_state == true) ) {

    t3_end = millis();  // discard button releases too close together (debounce)
    if (t3_end > (t3_start + 10) ) {  // 10 ms debounce timer
      trigger3_state = false;
      trigger3_released = true;   // this is the flag the main loop can react on
      // trigger3_was_down_for = t3_end - t3_start;
    }
  }

}


void trigger4IRQ() {

  static boolean trigger4_state = false;
  static unsigned long t4_start, t4_end;

  // if button is currently being pressed down, but was up before
  if ( (PCintPort::pinState == LOW) && (trigger4_state == false) ) {

    t4_start = millis();    // discard button presses too close together (debounce)
    if (t4_start > (t4_end + 10) ) {  // 10 ms debounce timer
      trigger4_state = true;
      trigger4_pressed = true;  // this is the flag the main loop can react on
    }
  }

  // button has been released but was down before
  else if ( (PCintPort::pinState == HIGH) && (trigger4_state == true) ) {

    t4_end = millis();  // discard button releases too close together (debounce)
    if (t4_end > (t4_start + 10) ) {  // 10 ms debounce timer
      trigger4_state = false;
      trigger4_released = true;   // this is the flag the main loop can react on
      // trigger4_was_down_for = t4_end - t4_start;
    }
  }

}


void trigger5IRQ() {

  static boolean trigger5_state = false;
  static unsigned long t5_start, t5_end;

  // if button is currently being pressed down, but was up before
  if ( (PCintPort::pinState == LOW) && (trigger5_state == false) ) {

    t5_start = millis();    // discard button presses too close together (debounce)
    if (t5_start > (t5_end + 10) ) {  // 10 ms debounce timer
      trigger5_state = true;
      trigger5_pressed = true;  // this is the flag the main loop can react on
    }
  }

  // button has been released but was down before
  else if ( (PCintPort::pinState == HIGH) && (trigger5_state == true) ) {

    t5_end = millis();  // discard button releases too close together (debounce)
    if (t5_end > (t5_start + 10) ) {  // 10 ms debounce timer
      trigger5_state = false;
      trigger5_released = true;   // this is the flag the main loop can react on
      // trigger5_was_down_for = t5_end - t5_start;
    }
  }

}






void loop() {

  // "Static" variables are initalized the first time the loop runs,
  // but they keep their values through successive loops.

  static boolean fast_forwarding = false;


  // volume management
  if (! fast_forwarding) {    // during fast-forward, volume is fixed at a low level
    updateVolume();
  }



  // Processing of prev/next button flags (were set by IRQ handling methods above):
  //    boolean next_button_pressed, boolean prev_button_pressed
  //    boolean next_button_released, boolean prev_button_released

  // NEXT BUTTON - Fast forwarding
  if (next_button_pressed) {
    DPRINTLNF("Next button pressed.");
    next_button_pressed = false;   // clear flag set by interrupt handler
    fast_forwarding = true;        // switch into fast-forwarding mode
    if (volume < 100) {
      MP3player.setVolume(100, 100);   // set volume to a low level (because of annoying skipping sounds)

      // set global volume values off, so that volume will be updated when fast-forwarding ends
      vol_pin_value_old = -42;
      volume = 255;
    }
  }
  if (next_button_released) {
    DPRINTLNF("Next button released.");
    next_button_released = false;  // clear flag set by interrupt handler
    fast_forwarding = false;       // switch off fast-forwarding mode
    updateVolume();                // reset correct volume level again
  }

  if (fast_forwarding) {
    DPRINTLNF("Fast forwarding ...");
    uint8_t result;
    result = MP3player.skip(SKIP_STEP);
    if(result != 0) {
      DPRINTF("Error code: ");
      DPRINT(result);
      DPRINTLNF(" when trying to fast forward track. Jumping to beginning of next track.");
      playNextTrack();
    }
  }


  // PREV BUTTON - Jumping to previous track
  if (prev_button_pressed) {
    DPRINTLNF("Prev button pressed.");
    prev_button_pressed = false;
  }
  if (prev_button_released) {
    DPRINTLNF("Prev button released.");
    prev_button_released = false;

    if (playing && ! fast_forwarding) {   // only if we're not fast-forwarding currently
      DPRINTLNF("Previous track!");
      playPreviousTrack();
    }
  }



  // Processing of trigger/playlist button flags (were set by IRQ handling methods above):
  //    boolean triggerX_pressed
  //    boolean triggerX_released
  //
  // Currently we don't need to react on button presses. We only clear the flag set by the IRQ handler.
  // Instead we react when a button is released and switch to the corresponding playlist.

  // TRIGGER 1
  if (trigger1_pressed) {
    DPRINTLNF("Trigger 1 pressed.");
    trigger1_pressed = false;
  }
  if (trigger1_released) {
    DPRINTLNF("Trigger 1 released.");
    trigger1_released = false;

    if (! fast_forwarding) {    // only if we're not fast-forwarding currently
      if (playlist_index == 0)  // if we are in this playlist already, play next track
        playNextTrack();
      else                      // otherwise switch to this playlist
        playPlaylist(0);
    }
  }

  // TRIGGER 2
  if (trigger2_pressed) {
    DPRINTLNF("Trigger 2 pressed.");
    trigger2_pressed = false;
  }
  if (trigger2_released) {
    DPRINTLNF("Trigger 2 released.");
    trigger2_released = false;

    if (! fast_forwarding) {
      if (playlist_index == 1)
        playNextTrack();
      else
        playPlaylist(1);
    }
  }

  // TRIGGER 3
  if (trigger3_pressed) {
    DPRINTLNF("Trigger 3 pressed.");
    trigger3_pressed = false;
  }
  if (trigger3_released) {
    DPRINTLNF("Trigger 3 released.");
    trigger3_released = false;

    if (! fast_forwarding) {
      if (playlist_index == 2)
        playNextTrack();
      else
        playPlaylist(2);
    }
  }

  // Triggers 4 and 5 are also used by serial debugging.
  // Thus, interrupts for T4 and T5 are attached only if debugging is disabled (see at the top).
  // In this case we don't need to watch for flags for T4 and T5, too.
  #ifndef DEBUG

    if (trigger4_pressed) {
      DPRINTLNF("Trigger 4 pressed.");
      trigger4_pressed = false;
    }
    if (trigger4_released) {
      DPRINTLNF("Trigger 4 released.");
      trigger4_released = false;

      if (! fast_forwarding) {
        if (playlist_index == 3)
          playNextTrack();
        else
          playPlaylist(3);
      }
    }

    if (trigger5_pressed) {
      DPRINTLNF("Trigger 5 pressed.");
      trigger5_pressed = false;
    }
    if (trigger5_released) {
      DPRINTLNF("Trigger 5 released.");
      trigger5_released = false;

      if (! fast_forwarding) {
        if (playlist_index == 4)
          playNextTrack();
        else
          playPlaylist(4);
      }
    }

  #endif




  // Handle "last track ended" situations (should we play the next track?)
  // Are we in "playing" mode, and has the current file ended?
  if (playing && !MP3player.isPlaying()) {
    getNextTrack(); // Set up for next track

    // If loop_all is true, start the next track
    if (loop_all)
      startPlaying();
    else
      playing = false;
  }



}  // void loop()




void updateVolume() {

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
  vol_pin_value = 1023 - analogRead(VOL_PIN);

  // we don't want to react on tiny shaky changes which are normal for analog input
  // so we check if the value at volume pin changed substantially (i.e. changed by more than 2)
  if ( abs(vol_pin_value - vol_pin_value_old) > 2 ) {

    vol_pin_value_old = vol_pin_value;  // remember the pin value for comparison next time

    // Divide the value at the volume pin in such a way, that the vol_pin_range (0-1023)
    // evenly covers the volume range between 0 (loudest) and defined MIN_VOLUME
    unsigned char new_volume = vol_pin_value / (1024 / MIN_VOLUME);

    // make sure that we call the setVolume method only if we really have a new volume value
    if (new_volume != volume) {
      // DPRINT("Setting volume to: ");
      // DPRINTLN(new_volume);
      volume = new_volume;
      MP3player.setVolume(volume, volume);  // set volume, same value for left and right speaker
    }
  }

}




void changePlaylist(byte pl, boolean reset_track_resume_value) {

  if (pl > -1 && pl < MAX_PLAYLIST) {   // check that provided playlist number is within allowed range

    if ( sd.chdir(playlist_folder[pl], true) ) {  // if change into folder was successful

      playlist_index = pl;
      EEPROM.update(PL_ADDRESS, pl);  // update playlist number in EEPROM for resume on startup
      // sd.ls(LS_R | LS_DATE | LS_SIZE);
      DPRINTF("Switched to directory ");
      DPRINTLN(playlist_folder[pl]);
      scanCurrentDirectory();
      playing = true;   // if player was stopped, now we want it to play again

      if (reset_track_resume_value) {
        EEPROM.update(TR_ADDRESS, track_index); // update track number in EEPROM for resume on startup
      }

    }
    else {  // folder change was not successful
      DPRINTF("Switching to directory ");
      DPRINT(playlist_folder[pl]);
      DPRINTLNF(" failed.");
    }

  }
}


void scanCurrentDirectory() {

  // Clear current list of tracks
  num_tracks = 0;
  for (byte i = 0; i < MAX_TRACKS; i++) {
    strcpy(filename[i], "");
  }
  strcpy(track, "");  // clear current track, too


  // go through the directory until we reach our file number limit
  while (num_tracks < MAX_TRACKS) {

    // grab next playable file from directory
    do {
      if (! getNextFile() ) {  // if no next file can be determined,
        DPRINTLNF("No next file found. Canceling directory scan.");
        return;                // leave this method completely
      }
    }
    while (isPlayable() != true);

    // if first filename is reached again, cancel the loop
    if (strcasecmp(track, filename[0]) == 0)
      break;

    strcpy(filename[num_tracks], track);   // copy name of current track to our array
    num_tracks += 1;

  }


  // sort the array alphabetically
  qsort(filename, MAX_TRACKS, sizeof(filename[0]), filenameCompare);

  DPRINTF("Found ");
  DPRINT(num_tracks);
  DPRINTF(" tracks in directory ");
  DPRINTLN(playlist_folder[playlist_index]);


  strcpy(track, filename[0]);   // Set first filename in array as current track
  track_index = 0;

}


void playNextTrack() {
  stopPlaying();
  getNextTrack();
  startPlaying();
}


void playPreviousTrack() {
  stopPlaying();
  getPrevTrack();
  startPlaying();
}


void playPlaylist(byte pl) {
  stopPlaying();
  changePlaylist(pl);
  startPlaying();
}


void getNextTrack() {

  if (num_tracks == 0) {    // if there are no tracks to play, set playing flag to false
    playing = false;
  }

  else {                    // otherwise set next track as current track

    track_index += 1;

    // If track index is out of range (i.e. beyond the actual list of tracks), then reset to zero (i.e. first file)
    if (track_index >= num_tracks) {
      track_index = 0;
    }

    strcpy(track, filename[track_index]);   // Get name of current track from filename array
    EEPROM.update(TR_ADDRESS, track_index); // update track number in EEPROM for resume on startup
  }


}


void getPrevTrack() {

  if (num_tracks == 0) {    // if there are no tracks to play, set playing flag to false
    playing = false;
  }

  else {                    // otherwise set previous track as current track

    // If current track is already the first one, we set track_index to the last possible index (i.e. list size - 1)
    if (track_index == 0) {
      track_index = num_tracks - 1;
    }
    else {
      track_index -= 1;
    }

    strcpy(track, filename[track_index]);   // Get name of current track from filename array
    EEPROM.update(TR_ADDRESS, track_index); // update track number in EEPROM for resume on startup
  }

}


boolean getNextFile() {

  static byte loops = 0;

  // Get the next file (which may be playable or not)
  int result = (file.openNext(sd.vwd(), O_READ));

  // If we're at the end of the directory, loop around to the beginning.
  // Or cancel, if we are looping already for the 2nd time.
  if (!result) {

    if (loops > 0) {
      DPRINTLNF("Looping around for 2nd time already. Canceling.");
      loops = 0;
      return false;
    }

    else {
      DPRINTLNF("Looping around to beginning of directory");
      loops += 1;
      sd.chdir(playlist_folder[playlist_index], true);
      return getNextFile();
    }

  }

  file.getName(track, 13);
  file.close();
  DPRINTF("Next file: ");
  DPRINTLN(track);

  loops = 0;
  return true;
}


void getPrevFile() {

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

  do {
    strcpy(prev,track);
    getNextTrack();
  }
  while(strcasecmp(track,test) != 0);

  strcpy(track,prev);
}


void startPlaying() {

  int result;

  DPRINTF("playing ");
  DPRINT(track);
  DPRINTF("...");

  result = MP3player.playMP3(track);

  DPRINTF(" result ");
  DPRINTLN(result);
}


void stopPlaying() {

  DPRINTLNF("stopping playback");
  MP3player.stopTrack();
}


boolean isPlayable() {

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


int filenameCompare (const void* p1, const void* p2) {

  char* string1 = p1;
  char* string2 = p2;

  // DPRINT("Compare --- ");
  // DPRINT(string1);
  // DPRINT(" --- ");
  // DPRINTLN(string2);


  // If one of the strings is empty, the other one will be considered as being the smaller string.
  // If both strings are empty (we don't check that explicitly), the first one will be considered smaller.
  // Thus, empty string get sorted to the end of the array.

  if (strcmp(string2, "") == 0) {
    return -1;
  }
  else if (strcmp(string1, "") == 0) {
    return 1;
  }
  else {
    return strcmp(p1, p2);
  }

}




