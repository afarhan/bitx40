/**
   Raduino_v1.19 for BITX40 - Allard Munters PE1NWL (pe1nwl@gooddx.net)

   This source file is under General Public License version 3.

   Most source code are meant to be understood by the compilers and the computers.
   Code that has to be hackable needs to be well understood and properly documented.
   Donald Knuth coined the term Literate Programming to indicate code that is written be
   easily read and understood.

   The Raduino is a small board that includes the Arduino Nano, a 16x2 LCD display and
   an Si5351a frequency synthesizer. This board is manufactured by Paradigm Ecomm Pvt Ltd.

   To learn more about Arduino you may visit www.arduino.cc.

   The Arduino works by first executing the code in a function called setup() and then it
   repeatedly keeps calling loop() forever. All the initialization code is kept in setup()
   and code to continuously sense the tuning knob, the function button, transmit/receive,
   etc is all in the loop() function. If you wish to study the code top down, then scroll
   to the bottom of this file and read your way up.

   Below are the libraries to be included for building the Raduino

   The EEPROM library is used to store settings like the frequency memory, caliberation data, etc.
*/

#include <EEPROM.h>

/**
    The Wire.h library is used to talk to the Si5351 and we also declare an instance of
    Si5351 object to control the clocks.
*/
#include <Wire.h>

/**
    The main chip which generates upto three oscillators of various frequencies in the
    Raduino is the Si5351a. To learn more about Si5351a you can download the datasheet
    from www.silabs.com although, strictly speaking it is not a requirment to understand this code.
    Instead, you can look up the Si5351 library written by Jason Mildrum, NT7S. You can download and
    install it from https://github.com/etherkit/Si5351Arduino to complile this file.

    NOTE 1: This sketch is based on version V2 of the Si5351 library. It will not compile with V1!

    NOTE 2: SI5351 library version 2.0.6 has been confirmed OK. Earlier versions compile OK but may produce
    strong "tuning clicks" (clicks each time the frequency is updated).
*/

#include <si5351.h>
Si5351 si5351;
/**
   The Raduino board is the size of a standard 16x2 LCD panel. It has three connectors:

   First, is an 8 pin connector that provides +5v, GND and six analog input pins that can also be
   configured to be used as digital input or output pins. These are referred to as A0,A1,A2,
   A3,A6 and A7 pins. The A4 and A5 pins are missing from this connector as they are used to
   talk to the Si5351 over I2C protocol.

    A0     A1   A2   A3    GND    +5V   A6   A7
   BLACK BROWN RED ORANGE YELLOW GREEN BLUE VIOLET  (same color coding as used for resistors)

   Second is a 16 pin LCD connector. This connector is meant specifically for the standard 16x2
   LCD display in 4 bit mode. The 4 bit mode requires 4 data lines and two control lines to work:
   Lines used are : RESET, ENABLE, D4, D5, D6, D7
   We include the library and declare the configuration of the LCD panel too
*/

#include <LiquidCrystal.h>
LiquidCrystal lcd(8, 9, 10, 11, 12, 13);

/**
   The Arduino, unlike C/C++ on a regular computer with gigabytes of RAM, has very little memory.
   We have to be very careful with variables that are declared inside the functions as they are
   created in a memory region called the stack. The stack has just a few bytes of space on the Arduino
   if you declare large strings inside functions, they can easily exceed the capacity of the stack
   and mess up your programs.
   We circumvent this by declaring a few global buffers as  kitchen counters where we can
   slice and dice our strings. These strings are mostly used to control the display or handle
   the input and output from the USB port. We must keep a count of the bytes used while reading
   the serial port as we can easily run out of buffer space. This is done in the serial_in_count variable.
*/

char c[17], b[10], printBuff1[17], printBuff2[17];

/**
   We need to carefully pick assignment of pin for various purposes.
   There are two sets of completely programmable pins on the Raduino.
   First, on the top of the board, in line with the LCD connector is an 8-pin connector
   that is largely meant for analog inputs and front-panel control. It has a regulated 5v output,
   ground and six pins. Each of these six pins can be individually programmed
   either as an analog input, a digital input or a digital output.
   The pins are assigned as follows:
          A0,   A1,  A2,  A3,   GND,   +5V,  A6,  A7
       pin 8     7    6    5     4       3    2    1 (connector P1)
        BLACK BROWN RED ORANGE YELLW  GREEN  BLUE VIOLET
        (while holding the board up so that back of the board faces you)

   Though, this can be assigned anyway, for this application of the Arduino, we will make the following
   assignment:

   A0 (digital input) for sensing the PTT. Connect to the output of U3 (LM7805) of the BITX40.
      This way the A0 input will see 0V (LOW) when PTT is not pressed, +5V (HIGH) when PTT is pressed.
   A1 (digital input) is to connect to a straight key. Open (HIGH) during key up, switch to ground (LOW) during key down.
   A2 (digital input) can be used for calibration by grounding this line (not required when you have the Function Button at A3).
   A3 (digital input) is connected to a push button that can momentarily ground this line. This Function Button will be used to switch between different modes, etc.
   A4 (already in use for talking to the SI5351)
   A5 (already in use for talking to the SI5351)
   A6 (analog input) is not currently used
   A7 (analog input) is connected to a center pin of good quality 100K or 10K linear potentiometer with the two other ends connected to
       ground and +5v lines available on the connector. This implments the tuning mechanism.
*/

#define PTT_SENSE (A0)
#define KEY (A1)
#define CAL_BUTTON (A2)
#define FBUTTON (A3)
#define ANALOG_TUNING (A7)

bool PTTsense_installed; //whether or not the PTT sense line is installed (detected automatically during startup)

/**
    The second set of 16 pins on the bottom connector P3 have the three clock outputs and the digital lines to control the rig.
    This assignment is as follows :
      Pin   1   2    3    4    5    6    7    8    9    10   11   12   13   14   15   16  (connector P3)
         +12V +12V CLK2  GND  GND CLK1  GND  GND  CLK0  GND  D2   D3   D4   D5   D6   D7
    These too are flexible with what you may do with them, for the Raduino, we use them to :

    input D4  - SPOT : is connected to a push button that can momentarily ground this line. When the SPOT button is pressed a sidetone will be generated for zero beat tuning.
    output D5 - CW_TONE : Side tone output
    output D6 - CW_CARRIER line : turns on the carrier for CW
    output D7 - TX_RX line : Switches between Transmit and Receive in CW mode
*/

#define SPOT (4)
#define CW_TONE (5)
#define CW_CARRIER (6)
#define TX_RX (7)

/**
    The raduino has a number of timing parameters, all specified in milliseconds
    QSK_DELAY : how many milliseconds between consecutive keyup and keydowns before switching back to receive?
*/

int QSK_DELAY; // in milliseconds, this is the parameter that determines how long the tx will hold between cw key downs

/**
   The Raduino supports two VFOs : A and B and receiver incremental tuning (RIT).
   we define a variables to hold the frequency of the two VFOs, RIT, SPLIT
   the rit offset as well as status of the RIT

   To use this facility, wire up push button on A3 line of the control connector (Function Button)
*/

unsigned long vfoA, vfoB; // the frequencies the VFOs
bool ritOn = false; // whether or not the RIT is on
int RIToffset = 0;  // offset (Hz)
int RIT = 0; // actual RIT offset that is applied during RX when RIT is on
int RIT_old;
bool splitOn; // whether or not SPLIT is on
bool vfoActive; // which VFO (false=A or true=B) is active
byte mode_A, mode_B; // the mode of each VFO

bool firstrun = true;

/**
   We need to apply some frequency offset to calibrate the dial frequency. Calibration is done in LSB mode.
*/
int cal; // frequency offset in Hz

/**
   In USB mode we need to apply some additional frequency offset, so that zerobeat in USB is same as in LSB
*/
int USB_OFFSET; // USB offset in Hz

/**
   We can set the VFO drive to a certain level (2,4,6,8 mA)
*/
byte LSBdrive;
byte USBdrive;

// scan parameters

int scan_start_freq; // lower scan limit (kHz)
int scan_stop_freq; // upper scan limit (kHz)
int scan_step_freq; // step size (Hz)
int scan_step_delay; // step delay (ms)


/**
  Raduino has 4 modes of operation:
*/

#define LSB (0)
#define USB (1)
#define CWL (2)
#define CWU (3)

/**
   Raduino needs to keep track of current state of the transceiver. These are a few variables that do it
*/
byte mode = LSB; // mode of the currently active VFO
bool inTx = false; // whether or not we are in transmit mode
bool keyDown = false; // whether we have a key up or key down
unsigned long TimeOut = 0;
bool semiQSK; //whether we use semi QSK or manual PTT

/** Tuning Mechanism of the Raduino
    We use a linear pot that has two ends connected to +5 and the ground. the middle wiper
    is connected to ANALOG_TUNNING pin. Depending upon the position of the wiper, the
    reading can be anywhere from 0 to 1023.
    If we want to use a multi-turn potentiometer with a tuning range of 500 kHz and a step
    size of 50 Hz we need 10,000 steps which is about 10x more than the steps that the ADC
    provides. Arduino's ADC has 10 bits which results in 1024 steps only.
    We can artificially expand the number of steps by a factor 10 by oversampling 100 times.
    As a result we get 10240 steps.
    The tuning control works in steps of 50Hz each for every increment between 10 and 10230.
    Hence the turning the pot fully from one end to the other will cover 50 x 10220 = 511 KHz.
    But if we use the standard 1-turn pot, then a tuning range of 500 kHz would be too much.
    (tuning would become very touchy). In the SETTINGS menu we can limit the tuning range
    depending on the potentiometer used and the band section of interest. Tuning beyond the
    limits is still possible by the 'scan-up' and 'scan-down' mode at the end of the pot.
    At the two ends, that is, the tuning starts stepping up or down in 10 KHz steps.
    To stop the scanning the pot is moved back from the edge.F
*/

int TUNING_RANGE; // tuning range (in kHz) of the tuning pot
unsigned long baseTune = 7100000UL; // frequency (Hz) when tuning pot is at minimum position

#define bfo_freq (11998000UL)

int old_knob = 0;

int CW_OFFSET; // the amount of offset (Hz) during RX, equal to sidetone frequency
int RXshift = 0; // the actual frequency shift that is applied during RX depending on the operation mode

#define LOWEST_FREQ  (6990000UL) // absolute minimum frequency (Hz)
#define HIGHEST_FREQ (7310000UL) //  absolute maximum frequency (Hz)

unsigned long frequency; // the 'dial' frequency as shown on the display
int fine = 0; // fine tune offset (Hz)

/**
   The raduino has multiple RUN-modes:
*/
#define RUN_NORMAL (0)  // normal operation
#define RUN_CALIBRATE (1) // calibrate VFO frequency in LSB mode
#define RUN_DRIVELEVEL (2) // set VFO drive level
#define RUN_TUNERANGE (3) // set the range of the tuning pot
#define RUN_CWOFFSET (4) // set the CW offset (=sidetone pitch)
#define RUN_SCAN (5) // frequency scanning mode
#define RUN_SCAN_PARAMS (6) // set scan parameters
#define RUN_MONITOR (7) // frequency scanning mode
#define RUN_FINETUNING (8) // fine tuning mode

byte RUNmode = RUN_NORMAL;

/**
   Display Routines
   These two display routines print a line of characters to the upper and lower lines of the 16x2 display
*/

void printLine1(char *c) {
  if (strcmp(c, printBuff1)) { // only refresh the display when there was a change
    lcd.setCursor(0, 0); // place the cursor at the beginning of the top line
    lcd.print(c); // write text
    strcpy(printBuff1, c);

    for (byte i = strlen(c); i < 16; i++) { // add white spaces until the end of the 16 characters line is reached
      lcd.print(' ');
    }
  }
}

void printLine2(char *c) {
  if (strcmp(c, printBuff2)) { // only refresh the display when there was a change
    lcd.setCursor(0, 1); // place the cursor at the beginning of the bottom line
    lcd.print(c);
    strcpy(printBuff2, c);

    for (byte i = strlen(c); i < 16; i++) { // add white spaces until the end of the 16 characters line is reached
      lcd.print(' ');
    }
  }
}

/**
   Building upon the previous  two functions,
   update Display paints the first line as per current state of the radio
*/

void updateDisplay() {
  // tks Jack Purdum W8TEE
  // replaced fsprint commmands by str commands for code size reduction

  memset(c, 0, sizeof(c));
  memset(b, 0, sizeof(b));

  ultoa((frequency + 50), b, DEC);

  if (!vfoActive) // VFO A is active
    strcpy(c, "A ");
  else
    strcpy(c, "B ");

  c[2] = b[0];
  strcat(c, ".");
  strncat(c, &b[1], 3);
  strcat(c, ".");
  strncat(c, &b[4], 1);

  switch (mode) {
    case LSB:
      strcat(c, " LSB");
      break;
    case USB:
      strcat(c, " USB");
      break;
    case CWL:
      strcat(c, " CWL");
      break;
    case CWU:
      strcat(c, " CWU");
      break;
  }

  if (inTx)
    strcat(c, " TX");
  else if (splitOn)
    strcat(c, " SP");

  printLine1(c);
}

// function to generate a bleep sound (FB menu)
void bleep(int pitch, int duration, byte repeat) {
  for (byte i = 0; i < repeat; i++) {
    tone(CW_TONE, pitch);
    delay(duration);
    noTone(CW_TONE);
    delay(duration);
  }
}

bool calbutton = false;

/**
   To use calibration sets the accurate readout of the tuned frequency
   To calibrate, follow these steps:
   1. Tune in a LSB signal that is at a known frequency.
   2. Now, set the display to show the correct frequency,
      the signal will no longer be tuned up properly
   3. Use the "LSB calibrate" option in the "Settings" menu (or Press the CAL_BUTTON line to the ground (pin A2 - red wire))
   4. tune in the signal until it sounds proper.
   5. Press the FButton (or Release CAL_BUTTON)
   In step 4, when we say 'sounds proper' then, for a CW signal/carrier it means zero-beat
   and for LSB it is the most natural sounding setting.

   Calibration is an offset value that is added to the VFO frequency.
   We store it in the EEPROM and read it in setup() when the Radiuno is powered up.

   Then select the "USB calibrate" option in the "Settings" menu and repeat the same steps for USB mode.
*/

int shift, current_setting;
void calibrate() {
  if (RUNmode != RUN_CALIBRATE) {

    if (mode == USB)
      current_setting = USB_OFFSET;
    else
      current_setting = cal;

    shift = current_setting - analogRead(ANALOG_TUNING) + 500;
  }

  // The tuning knob gives readings from 0 to 1000
  // Each step is taken as 1 Hz and the mid setting of the knob is taken as zero

  if (mode == USB) {
    USB_OFFSET = constrain(analogRead(ANALOG_TUNING) - 500 + shift, -10000, 10000);

    if (analogRead(ANALOG_TUNING) < 5 && USB_OFFSET > -10000)
      shift = shift - 10;
    else if (analogRead(ANALOG_TUNING) > 1020 && USB_OFFSET < 10000)
      shift = shift + 10;
  }
  else {
    cal = constrain(analogRead(ANALOG_TUNING) - 500 + shift, -10000, 10000);

    if (analogRead(ANALOG_TUNING) < 5 && cal > -10000)
      shift = shift - 10;
    else if (analogRead(ANALOG_TUNING) > 1020 && cal < 10000)
      shift = shift + 10;
  }

  // if Fbutton is pressed again (or when the CAL button is released), we save the setting
  if (!digitalRead(FBUTTON) || (calbutton && digitalRead(CAL_BUTTON))) {
    RUNmode = RUN_NORMAL;
    calbutton = false;

    if (mode == USB) {
      printLine2((char *)"USB Calibrated!");
      //Write the 2 bytes of the USB offset into the eeprom memory.
      EEPROM.put(4, USB_OFFSET);
    }

    else {
      printLine2((char *)"LSB Calibrated!");
      //Write the 2 bytes of the LSB offset into the eeprom memory.
      EEPROM.put(2, cal);
    }

    delay(700);
    bleep(600, 50, 2);
    printLine2((char *)"--- SETTINGS ---");
    shiftBase(); //align the current knob position with the current frequency
  }

  else {
    // while offset adjustment is in progress, keep tweaking the
    // frequency as read out by the knob, display the change in the second line
    RUNmode = RUN_CALIBRATE;

    if (mode == USB) {
      si5351.set_freq((bfo_freq + frequency + cal / 5 * 19 - USB_OFFSET) * 100LL, SI5351_CLK2);
      itoa(USB_OFFSET, b, DEC);
    }

    else {
      si5351.set_freq((bfo_freq - frequency + cal) * 100LL, SI5351_CLK2);
      itoa(cal, b, DEC);
    }

    strcpy(c, "offset ");
    strcat(c, b);
    strcat(c, " Hz");
    printLine2(c);
  }
}

/**
   The setFrequency is a little tricky routine, it works differently for USB and LSB
   The BITX BFO is permanently set to lower sideband, (that is, the crystal frequency
   is on the higher side slope of the crystal filter).

   LSB: The VFO frequency is subtracted from the BFO. Suppose the BFO is set to exactly 12 MHz
   and the VFO is at 5 MHz. The output will be at 12.000 - 5.000  = 7.000 MHz
   USB: The BFO is subtracted from the VFO. Makes the LSB signal of the BITX come out as USB!!
   Here is how it will work:
   Consider that you want to transmit on 14.000 MHz and you have the BFO at 12.000 MHz. We set
   the VFO to 26.000 MHz. Hence, 26.000 - 12.000 = 14.000 MHz. Now, consider you are whistling a tone
   of 1 KHz. As the BITX BFO is set to produce LSB, the output from the crystal filter will be 11.999 MHz.
   With the VFO still at 26.000, the 14 Mhz output will now be 26.000 - 11.999 = 14.001, hence, as the
   frequencies of your voice go down at the IF, the RF frequencies will go up!

   Thus, setting the VFO on either side of the BFO will flip between the USB and LSB signals.

   In addition we add some offset to USB mode so that the dial frequency is correct in both LSB and USB mode.
   The amount of offset can be set in the SETTING menu as part of the calibration procedure.

   Furthermore we add/substract the sidetone frequency only when we receive CW, to assure zero beat
   between the transmitting and receiving station (RXshift)
   The desired sidetone frequency can be set in the SETTINGS menu.
*/

void setFrequency(unsigned long f) {

  if (mode & 1) // if we are in UPPER side band mode
    si5351.set_freq((bfo_freq + f + cal * 19 / 5 - USB_OFFSET - RXshift + RIT + fine) * 100ULL, SI5351_CLK2);
  else // if we are in LOWER side band mode
    si5351.set_freq((bfo_freq - f + cal - RXshift - RIT - fine) * 100ULL, SI5351_CLK2);
  updateDisplay();
}

/**
   The checkTX toggles the T/R line. If you would like to make use of RIT, etc,
   you must connect pin A0 (black wire) via a 10K resistor to the output of U3
   This is a voltage regulator LM7805 which goes on during TX. We use the +5V output
   as a PTT sense line (to tell the Raduino that we are in TX).
*/

void checkTX() {
  // We don't check for ptt when transmitting cw in semi-QSK mode
  // as long as the TimeOut is non-zero, we will continue to hold the
  // radio in transmit mode
  if (TimeOut > 0 && semiQSK)
    return;

  if (digitalRead(PTT_SENSE) && !inTx) {
    // go in transmit mode
    inTx = true;
    RXshift = RIT = RIT_old = 0; // no frequency offset during TX

    if (semiQSK) {
      mode = mode & B11111101; // leave CW mode, return to SSB mode

      if (!vfoActive) { // if VFO A is active
        mode_A = mode;
        EEPROM.put(24, mode_A);
      }
      else { // if VFO B is active
        mode_B = mode;
        EEPROM.put(25, mode_B);
      }
    }

    setFrequency(frequency);
    shiftBase();
    updateDisplay();

    if (splitOn) { // when SPLIT is on, swap the VFOs
      swapVFOs();
    }
  }

  if (!digitalRead(PTT_SENSE) && inTx) {
    //go in receive mode
    inTx = false;
    updateDisplay();
    if (splitOn) { // when SPLIT was on, swap the VFOs back to original state
      swapVFOs();
    }
    if (mode & 2) { // if we are in CW mode
      RXshift = CW_OFFSET; // apply the frequency offset in RX
      setFrequency(frequency);
      shiftBase();
    }
  }
}

/* CW is generated by unbalancing the mixer when the key is down.
   During key down, the output CW_CARRIER is HIGH (+5V).
   This output is connected via a 10K resistor to the mixer input. The mixer will
   become unbalanced when CW_CARRIER is HIGH, so a carrier will be transmitted.
   During key up, the output CW_CARRIER is LOW (0V). The mixer will remain balanced
   and the carrrier will be suppressed.

   The radio will go into CW mode automatically as soon as the key goes down, and
   return to normal LSB/USB mode when the key has been up for some time.

   There are three variables that track the CW mode
   inTX     : true when the radio is in transmit mode
   keyDown  : true when the CW is keyed down, you maybe in transmit mode (inTX true)
              and yet between dots and dashes and hence keyDown could be true or false
   TimeOut: Figures out how long to wait between dots and dashes before putting
              the radio back in receive mode

   When we transmit CW, we need to apply some offset (800Hz) to the TX frequency, in
   order to keep zero-beat between the transmitting and receiving station. The shift
   depends on whether upper or lower sideband CW is used:
   In CW-U (USB) mode we must shift the TX frequency 800Hz up
   In CW-L (LSB) mode we must shift the TX frequency 800Hz down

   The default offset (CW_OFFSET) is 800Hz, the default timeout (QSK_DELAY) is 350ms.
   The user can change these in the SETTINGS menu.

*/

void checkCW() {

  if (!keyDown && !digitalRead(KEY)) {
    keyDown = true;

    if (!inTx && semiQSK) {     //switch to transmit mode if we are not already in it

      digitalWrite(TX_RX, 1); // activate the PTT switch - go in transmit mode
      delay(5);  //give the relays a few ms to settle the T/R relays
      inTx = true;

      if (splitOn) // when SPLIT is on, swap the VFOs first
        swapVFOs();

      mode = mode | 2; // go into to CW mode

      if (!vfoActive) { // if VFO A is active
        mode_A = mode;
        EEPROM.put(24, mode_A);
      }
      else { // if VFO B is active
        mode_B = mode;
        EEPROM.put(25, mode_B);
      }

      RXshift = RIT = RIT_old = 0; // no frequency offset during TX
      setFrequency(frequency);
      shiftBase();
    }
  }

  //keep resetting the timer as long as the key is down
  if (keyDown)
    TimeOut = millis() + QSK_DELAY;

  //if the key goes up again after it's been down
  if (keyDown && digitalRead(KEY)) {
    keyDown = false;
    TimeOut = millis() + QSK_DELAY;
  }

  // if we are in semi-QSK mode and have a keyup for a "longish" time (QSK_DELAY value in ms)
  // then go back to RX

  if (TimeOut > 0 && inTx && TimeOut < millis() && semiQSK) {

    inTx = false;
    TimeOut = 0; // reset the CW timeout counter
    RXshift = CW_OFFSET; // apply the frequency offset in RX
    setFrequency(frequency);
    shiftBase();

    if (splitOn) // then swap the VFOs back when SPLIT was on
      swapVFOs();

    digitalWrite(TX_RX, 0); // release the PTT switch - move the radio back to receive
    delay(10);  //give the relays a few ms to settle the T/R relays
  }

  if (keyDown && mode & B00000010) {
    digitalWrite(CW_CARRIER, 1); // generate carrier
    tone(CW_TONE, CW_OFFSET); // generate sidetone
  }
  else if (digitalRead(SPOT) == HIGH) {
    digitalWrite(CW_CARRIER, 0); // stop generating the carrier
    noTone(CW_TONE); // stop generating the sidetone
  }
}

byte param;

/**
   The Function Button is used for several functions
   NORMAL menu (normal operation):
   1 short press: swap VFO A/B
   2 short presses: toggle RIT on/off
   3 short presses: toggle SPLIT on/off
   4 short presses: toggle LSB/USB
   5 short presses: start freq scan mode
   5 short presses: start A/B monitor mode
   long press (>1 Sec): VFO A=B
   VERY long press (>3 sec): go to SETTINGS menu

   SETTINGS menu:
   1 short press: LSB calibration
   2 short presses: USB calibration
   3 short presses: Set VFO drive level in LSB mode
   4 short presses: Set VFO drive level in USB mode
   5 short presses: Set tuning range
   6 short presses: Set the 3 CW parameters (sidetone pitch, semi-QSK on/off, CW timeout)
   7 short presses: Set the 4 scan parameters (lower limit, upper limit, step size, step delay)
   long press: exit SETTINGS menu - go back to NORMAL menu
*/
char clicks;

void checkButton() {

  static byte action;
  static long t1, t2;
  static bool pressed = false;

  if (digitalRead(FBUTTON)) {
    t2 = millis() - t1; //time elapsed since last button press
    if (pressed)
      if (clicks < 10 && t2 > 600 && t2 < 3000) { //detect long press to reset the VFO's
        bleep(600, 50, 1);
        resetVFOs();
        delay(700);
        clicks = 0;
      }

    if (t2 > 500) { // max time between button clicks (ms)
      action = clicks;
      if (clicks >= 10)
        clicks = 10;
      else
        clicks = 0;
    }
    pressed = false;
  }
  else {
    delay(10);
    if (!digitalRead(FBUTTON)) {
      // button was really pressed, not just some noise
      if (ritOn) {
        toggleRIT(); // disable the RIT when it was on and the FB is pressed again
        old_knob = knob_position();
        bleep(600, 50, 1);
        delay(100);
        return;
      }
      if (!pressed) {
        pressed = true;
        t1 = millis();
        bleep(1200, 50, 1);
        action = 0;
        clicks++;
        if (clicks > 17)
          clicks = 11;
        if (clicks > 6 && clicks < 10)
          clicks = 1;
        switch (clicks) {
          //Normal menu options
          case 1:
            printLine2((char *)"Swap VFOs");
            break;
          case 2:
            printLine2((char *)"RIT ON");
            break;
          case 3:
            printLine2((char *)"SPLIT ON/OFF");
            break;
          case 4:
            printLine2((char *)"Switch mode");
            break;
          case 5:
            printLine2((char *)"Start freq scan");
            break;
          case 6:
            printLine2((char *)"Monitor VFO A/B");
            break;

          //SETTINGS menu options
          case 11:
            printLine2((char *)"LSB calibration");
            break;
          case 12:
            printLine2((char *)"USB calibration");
            break;
          case 13:
            printLine2((char *)"VFO drive - LSB");
            break;
          case 14:
            printLine2((char *)"VFO drive - USB");
            break;
          case 15:
            printLine2((char *)"Set tuning range");
            break;
          case 16:
            printLine2((char *)"Set CW params");
            break;
          case 17:
            printLine2((char *)"Set scan params");
            break;
        }
      }
      else if ((millis() - t1) > 600 && (millis() - t1) < 800 && clicks < 10) // long press: reset the VFOs
        printLine2((char *)"Reset VFOs");

      if ((millis() - t1) > 3000 && clicks < 10) { // VERY long press: go to the SETTINGS menu
        bleep(1200, 150, 3);
        printLine2((char *)"--- SETTINGS ---");
        clicks = 10;
      }

      else if ((millis() - t1) > 1500 && clicks > 10) { // long press: return to the NORMAL menu
        bleep(1200, 150, 3);
        clicks = -1;
        pressed = false;
        printLine2((char *)" --- NORMAL ---");
        delay(700);
      }
    }
  }
  if (action != 0 && action != 10) {
    bleep(600, 50, 1);
  }
  switch (action) {
    // NORMAL menu

    case 1: // swap the VFOs
      swapVFOs();
      EEPROM.put(26, vfoActive);
      delay(700);
      break;

    case 2: // toggle the RIT on/off
      toggleRIT();
      delay(700);
      break;

    case 3: // toggle SPLIT on/off
      toggleSPLIT();
      delay(700);
      break;

    case 4: // toggle the mode LSB/USB
      toggleMode();
      delay(700);
      break;

    case 5: // start scan mode
      RUNmode = RUN_SCAN;
      TimeOut = millis() + scan_step_delay;
      frequency = scan_start_freq * 1000L;
      printLine2((char *)"freq scanning");
      break;

    case 6: // Monitor mode
      RUNmode = RUN_MONITOR;
      TimeOut = millis() + scan_step_delay;
      printLine2((char *)"A/B monitoring");
      break;

    // SETTINGS MENU

    case 11: // calibrate the dial frequency in LSB
      RXshift = 0;
      mode = LSB;
      setFrequency(frequency);
      SetSideBand(LSBdrive);
      calibrate();
      break;

    case 12: // calibrate the dial frequency in USB
      RXshift = 0;
      mode = USB;
      setFrequency(frequency);
      SetSideBand(USBdrive);
      calibrate();
      break;

    case 13: // set the VFO drive level in LSB
      mode = LSB;
      SetSideBand(LSBdrive);
      VFOdrive();
      break;

    case 14: // set the VFO drive level in USB
      mode = USB;
      SetSideBand(USBdrive);
      VFOdrive();
      break;

    case 15: // set the tuning pot range
      set_tune_range();
      break;

    case 16: // set CW parameters (sidetone pitch, CW timeout)
      param = 1;
      set_CWparams();
      break;

    case 17: // set the 4 scan parameters
      param = 1;
      scan_params();
      break;
  }
}

void swapVFOs() {
  if (vfoActive) { // if VFO B is active
    vfoActive = false; // switch to VFO A
    vfoB = frequency;
    frequency = vfoA;
    mode = mode_A;
  }

  else { //if VFO A is active
    vfoActive = true; // switch to VFO B
    vfoA = frequency;
    frequency = vfoB;
    mode = mode_B;
  }

  if (mode & 1) // if we are in UPPER side band mode
    SetSideBand(USBdrive);
  else // if we are in LOWER side band mode
    SetSideBand(LSBdrive);

  if (!inTx && mode > 1)
    RXshift = CW_OFFSET; // add RX shift when we are receiving in CW mode
  else
    RXshift = 0; // no RX shift when we are receiving in SSB mode

  shiftBase(); //align the current knob position with the current frequency
}

void toggleRIT() {
  if (!PTTsense_installed) {
    printLine2((char *)"Not available!");
    return;
  }
  ritOn = !ritOn; // toggle RIT
  if (!ritOn)
    RIT = RIT_old = 0;
  shiftBase(); //align the current knob position with the current frequency
  firstrun = true;
  if (splitOn) {
    splitOn = false;
    EEPROM.put(27, 0);
  }
  updateDisplay();
}

void toggleSPLIT() {
  if (!PTTsense_installed) {
    printLine2((char *)"Not available!");
    return;
  }
  splitOn = !splitOn; // toggle SPLIT
  EEPROM.put(27, splitOn);
  if (ritOn) {
    ritOn = false;
    RIT = RIT_old = 0;
    shiftBase();
  }
  updateDisplay();
}

void toggleMode() {
  if (PTTsense_installed && !semiQSK)
    mode = (mode + 1) & 3; // rotate through LSB-USB-CWL-CWU
  else
    mode = (mode + 1) & 1; // switch between LSB and USB only (no CW)

  if (mode & 2) // if we are in CW mode
    RXshift = CW_OFFSET;
  else // if we are in SSB mode
    RXshift = 0;

  if (mode & 1) // if we are in UPPER side band mode
    SetSideBand(USBdrive);
  else // if we are in LOWER side band mode
    SetSideBand(LSBdrive);
}

void SetSideBand(byte drivelevel) {

  set_drive_level(drivelevel);
  setFrequency(frequency);
  if (!vfoActive) { // if VFO A is active
    mode_A = mode;
    EEPROM.put(24, mode_A);
  }
  else { // if VFO B is active
    mode_B = mode;
    EEPROM.put(25, mode_B);
  }
}

// resetting the VFO's will set both VFO's to the current frequency and mode
void resetVFOs() {
  printLine2((char *)"VFO A=B !");
  vfoA = vfoB = frequency;
  mode_A = mode_B = mode;
  updateDisplay();
  bleep(600, 50, 1);
  EEPROM.put(24, mode_A);
  EEPROM.put(25, mode_B);
}

void VFOdrive() {
  static byte drive;

  if (RUNmode != RUN_DRIVELEVEL) {

    if (mode & 1) // if UPPER side band mode
      current_setting = USBdrive / 2 - 1;
    else // if LOWER side band mode
      current_setting = LSBdrive / 2 - 1;

    shift = analogRead(ANALOG_TUNING);
  }

  //generate drive level values 2,4,6,8 from tuning pot
  drive = 2 * ((((analogRead(ANALOG_TUNING) - shift) / 50 + current_setting) & 3) + 1);

  // if Fbutton is pressed again, we save the setting

  if (!digitalRead(FBUTTON)) {
    RUNmode = RUN_NORMAL;
    printLine2((char *)"Drive level set!");

    if (mode & 1) { // if UPPER side band mode
      USBdrive = drive;
      //Write the 2 bytes of the USBdrive level into the eeprom memory.
      EEPROM.put(7, drive);
    }
    else { // if LOWER side band mode
      LSBdrive = drive;
      //Write the 2 bytes of the LSBdrive level into the eeprom memory.
      EEPROM.put(6, drive);
    }
    delay(700);
    bleep(600, 50, 2);
    printLine2((char *)"--- SETTINGS ---");
    shiftBase(); //align the current knob position with the current frequency
  }
  else {
    // while the drive level adjustment is in progress, keep tweaking the
    // drive level as read out by the knob and display it in the second line
    RUNmode = RUN_DRIVELEVEL;
    set_drive_level(drive);

    itoa(drive, b, DEC);
    strcpy(c, "drive level ");
    strcat(c, b);
    strcat(c, "mA");
    printLine2(c);
  }
}

/* this function allows the user to set the tuning range depending on the type of potentiometer
  for a standard 1-turn pot, a range of 50 KHz is recommended
  for a 10-turn pot, a tuning range of 200 kHz is recommended
*/
void set_tune_range() {

  if (RUNmode != RUN_TUNERANGE) {
    current_setting = TUNING_RANGE;
    shift = current_setting - 10 * analogRead(ANALOG_TUNING) / 20;
  }

  //generate values 10-500 from the tuning pot
  TUNING_RANGE = constrain(10 * analogRead(ANALOG_TUNING) / 20 + shift, 10, 500);
  if (analogRead(ANALOG_TUNING) < 5 && TUNING_RANGE > 10)
    shift = shift - 10;
  else if (analogRead(ANALOG_TUNING) > 1020 && TUNING_RANGE < 500)
    shift = shift + 10;

  // if Fbutton is pressed again, we save the setting
  if (!digitalRead(FBUTTON)) {
    //Write the 2 bytes of the tuning range into the eeprom memory.
    EEPROM.put(10, TUNING_RANGE);
    printLine2((char *)"Tune range set!");
    RUNmode = RUN_NORMAL;
    delay(700);
    bleep(600, 50, 2);
    printLine2((char *)"--- SETTINGS ---");
    shiftBase(); //align the current knob position with the current frequency
  }

  else {
    RUNmode = RUN_TUNERANGE;
    itoa(TUNING_RANGE, b, DEC);
    strcpy(c, "range ");
    strcat(c, b);
    strcat(c, " kHz");
    printLine2(c);
  }
}

/* this function allows the user to set the two CW parameters: CW-OFFSET (sidetone pitch) and CW-TIMEOUT.
*/

void set_CWparams() {

  if (firstrun) {
    switch (param) {
      case 1:
        mode = mode | 2; //switch to CW mode
        updateDisplay();
        RXshift = CW_OFFSET;
        QSK_DELAY = 10; // during CW offset adjustment, temporarily set QSK_DELAY to minimum
        current_setting = CW_OFFSET;
        shift = current_setting - analogRead(ANALOG_TUNING) - 200;
        break;
      case 2:
        current_setting = semiQSK;
        shift = analogRead(ANALOG_TUNING);
        break;
      case 3:
        current_setting = QSK_DELAY;
        shift = current_setting - 10 * (analogRead(ANALOG_TUNING) / 10);
        break;
    }
  }

  switch (param) {
    case 1:
      //generate values 500-1000 from the tuning pot
      CW_OFFSET = constrain(analogRead(ANALOG_TUNING) + 200 + shift, 200, 1200);
      if (analogRead(ANALOG_TUNING) < 5 && CW_OFFSET > 200)
        shift = shift - 10;
      else if (analogRead(ANALOG_TUNING) > 1020 && CW_OFFSET < 1200)
        shift = shift + 10;
      break;
    case 2:
      //generate values 0-1-0-1 from the tuning pot
      semiQSK = ((((analogRead(ANALOG_TUNING) - shift) & 64) / 64) + current_setting) & 1;
      break;
    case 3:
      //generate values 10-1000 from the tuning pot
      QSK_DELAY = constrain(10 * ( analogRead(ANALOG_TUNING) / 10) + shift, 10, 1000);
      if (analogRead(ANALOG_TUNING) < 5 && QSK_DELAY >= 20)
        shift = shift - 10;
      else if (analogRead(ANALOG_TUNING) > 1020 && QSK_DELAY < 1000)
        shift = shift + 10;
      break;
  }

  // if Fbutton is pressed again, we save the setting
  if (!digitalRead(FBUTTON)) {
    switch (param) {
      case 1:
        EEPROM.get(36, QSK_DELAY); // restore QSK_DELAY to original value
        //Write the 2 bytes of the CW offset into the eeprom memory.
        EEPROM.put(12, CW_OFFSET);
        bleep(600, 50, 1);
        delay(200);
        break;
      case 2:
        EEPROM.put(8, semiQSK);
        if (semiQSK) {
          bleep(600, 50, 1);
          delay(200);
        }
        else {
          QSK_DELAY = 10; // set CW timeout to minimum when manual PTT is selected
          EEPROM.put(36, QSK_DELAY);
          printLine2((char *)"CW params set!");
          RUNmode = RUN_NORMAL;
          delay(700);
          bleep(600, 50, 2);
          printLine2((char *)"--- SETTINGS ---");
          shiftBase(); //align the current knob position with the current frequency
        }
        break;
      case 3:
        //Write the 2 bytes of the CW Timout into the eeprom memory.
        EEPROM.put(36, QSK_DELAY);
        printLine2((char *)"CW params set!");
        RUNmode = RUN_NORMAL;
        delay(700);
        bleep(600, 50, 2);
        printLine2((char *)"--- SETTINGS ---");
        shiftBase(); //align the current knob position with the current frequency
        break;
    }
    param ++;
    firstrun = true;
  }

  else {
    RUNmode = RUN_CWOFFSET;
    firstrun = false;
    switch (param) {
      case 1:
        itoa(CW_OFFSET, b, DEC);
        strcpy(c, "sidetone ");
        strcat(c, b);
        strcat(c, " Hz");
        break;
      case 2:
        strcpy(c, "Semi-QSK: ");
        if (semiQSK)
          strcat(c, "ON");
        else
          strcat(c, "OFF");
        break;
      case 3:
        itoa(QSK_DELAY, b, DEC);
        strcpy(c, "QSK delay ");
        strcat(c, b);
        strcat(c, "ms");
        break;
    }
    printLine2(c);
  }
}

/* this function allows the user to set the 4 scan parameters: lower limit, upper limit, step size and step delay
*/

void scan_params() {

  if (firstrun) {
    switch (param) {

      case 1: // set the lower scan limit

        current_setting = scan_start_freq;
        shift = current_setting - analogRead(ANALOG_TUNING) / 2 - 7000;
        break;

      case 2: // set the upper scan limit

        current_setting = scan_stop_freq;
        shift = current_setting - map(analogRead(ANALOG_TUNING), 0, 1024, scan_start_freq, 7500);
        break;

      case 3: // set the scan step size

        current_setting = scan_step_freq;
        shift = current_setting - 50 * (analogRead(ANALOG_TUNING) / 5);
        break;

      case 4: // set the scan step delay

        current_setting = scan_step_delay;
        shift = current_setting - 50 * (analogRead(ANALOG_TUNING) / 25);
        break;
    }
  }

  switch (param) {

    case 1: // set the lower scan limit

      //generate values 7000-7500 from the tuning pot
      scan_start_freq = constrain(analogRead(ANALOG_TUNING) / 2 + 7000 + shift, 7000, 7500);
      if (analogRead(ANALOG_TUNING) < 5 && scan_start_freq > 7000)
        shift = shift - 1;
      else if (analogRead(ANALOG_TUNING) > 1020 && scan_start_freq < 7500)
        shift = shift + 1;
      break;

    case 2: // set the upper scan limit

      //generate values 7000-7500 from the tuning pot
      scan_stop_freq = constrain(map(analogRead(ANALOG_TUNING), 0, 1024, scan_start_freq, 7500) + shift, scan_start_freq, 7500);
      if (analogRead(ANALOG_TUNING) < 5 && scan_stop_freq > scan_start_freq)
        shift = shift - 1;
      else if (analogRead(ANALOG_TUNING) > 1020 && scan_stop_freq < 7500)
        shift = shift + 1;
      break;

    case 3: // set the scan step size

      //generate values 50-10000 from the tuning pot
      scan_step_freq = constrain(50 * (analogRead(ANALOG_TUNING) / 5) + shift, 50, 10000);
      if (analogRead(ANALOG_TUNING) < 5 && scan_step_freq > 50)
        shift = shift - 50;
      else if (analogRead(ANALOG_TUNING) > 1020 && scan_step_freq < 10000)
        shift = shift + 50;
      break;

    case 4: // set the scan step delay

      //generate values 0-2500 from the tuning pot
      scan_step_delay = constrain(50 * (analogRead(ANALOG_TUNING) / 25) + shift, 0, 2000);
      if (analogRead(ANALOG_TUNING) < 5 && scan_step_delay > 0)
        shift = shift - 50;
      else if (analogRead(ANALOG_TUNING) > 1020 && scan_step_delay < 2000)
        shift = shift + 50;
      break;
  }

  // if Fbutton is pressed, we save the setting

  if (!digitalRead(FBUTTON)) {
    switch (param) {

      case 1: // save the lower scan limit

        //Write the 2 bytes of the start freq into the eeprom memory.
        EEPROM.put(28, scan_start_freq);
        bleep(600, 50, 1);
        break;

      case 2: // save the upper scan limit

        //Write the 2 bytes of the stop freq into the eeprom memory.
        EEPROM.put(30, scan_stop_freq);
        bleep(600, 50, 1);
        break;

      case 3: // save the scan step size

        //Write the 2 bytes of the step size into the eeprom memory.
        EEPROM.put(32, scan_step_freq);
        bleep(600, 50, 1);
        break;

      case 4: // save the scan step delay

        //Write the 2 bytes of the step delay into the eeprom memory.
        EEPROM.put(34, scan_step_delay);
        printLine2((char *)"Scan params set!");
        RUNmode = RUN_NORMAL;
        delay(700);
        bleep(600, 50, 2);
        printLine2((char *)"--- SETTINGS ---");
        shiftBase(); //align the current knob position with the current frequency
        break;
    }
    param ++;
    firstrun = true;
  }

  else {
    RUNmode = RUN_SCAN_PARAMS;
    firstrun = false;
    switch (param) {

      case 1: // display the lower scan limit

        itoa(scan_start_freq, b, DEC);
        strcpy(c, "lower ");
        strcat(c, b);
        strcat(c, " kHz");
        break;

      case 2: // display the upper scan limit

        itoa(scan_stop_freq, b, DEC);
        strcpy(c, "upper ");
        strcat(c, b);
        strcat(c, " kHz");
        break;

      case 3: // display the scan step size

        itoa(scan_step_freq, b, DEC);
        strcpy(c, "step ");
        strcat(c, b);
        strcat(c, " Hz");
        break;

      case 4: // display the scan step delay

        itoa(scan_step_delay, b, DEC);
        strcpy(c, "delay ");
        strcat(c, b);
        strcat(c, " ms");
        break;
    }
    printLine2(c);
  }
}

// function to read the position of the tuning knob at high precision (Allard, PE1NWL)
int knob_position() {
  unsigned long knob = 0;
  // the knob value normally ranges from 0 through 1023 (10 bit ADC)
  // in order to increase the precision by a factor 10, we need 10^2 = 100x oversampling
  for (byte i = 0; i < 100; i++) {
    knob = knob + analogRead(ANALOG_TUNING); // take 100 readings from the ADC
  }
  knob = (knob + 5L) / 10L; // take the average of the 100 readings and multiply the result by 10
  //now the knob value ranges from 0 through 10230 (10x more precision)
  return (int)knob;
}

/* Many BITX40's suffer from a strong birdie at 7199 kHz (LSB).
  This birdie may be eliminated by using a different VFO drive level in LSB mode.
  In USB mode, a high drive level may be needed to compensate for the attenuation of
  higher VFO frequencies.
  The drive level for each mode can be set in the SETTINGS menu
*/

void set_drive_level(byte level) {
  switch (level) {
    case 2:
      si5351.drive_strength(SI5351_CLK2, SI5351_DRIVE_2MA);
      break;
    case 4:
      si5351.drive_strength(SI5351_CLK2, SI5351_DRIVE_4MA);
      break;
    case 6:
      si5351.drive_strength(SI5351_CLK2, SI5351_DRIVE_6MA);
      break;
    case 8:
      si5351.drive_strength(SI5351_CLK2, SI5351_DRIVE_8MA);
      break;
  }
}

void doRIT() {

  int knob = knob_position(); // get the current tuning knob position

  if (firstrun) {
    current_setting = RIToffset;
    shift = current_setting - ((knob - 5000) / 2);
    firstrun = false;
  }

  //generate values -2500 ~ +2500 from the tuning pot
  RIToffset = (knob - 5000) / 2 + shift;
  if (knob < 5 && RIToffset > -2500)
    shift = shift - 50;
  else if (knob > 10220 && RIToffset < 2500)
    shift = shift + 50;

  RIT = RIToffset;
  if (RIT != RIT_old) {
    setFrequency(frequency);
    itoa(RIToffset, b, DEC);
    strcpy(c, "RIT ");
    strcat(c, b);
    strcat(c, " Hz");
    printLine2(c);

    RIT_old = RIT;
    old_knob = knob_position();
    delay(30);
  }
}

/**
   Function to align the current knob position with the current frequency
   If we switch between VFO's A and B, the frequency will change but the tuning knob
   is still in the same position. We need to apply some offset so that the new frequency
   corresponds with the current knob position.
   This function reads the current knob position, then it shifts the baseTune value up or down
   so that the new frequency matches again with the current knob position.
*/

void shiftBase() {
  setFrequency(frequency);
  unsigned long knob = knob_position(); // get the current tuning knob position
  baseTune = frequency - (knob * (unsigned long)TUNING_RANGE / 10UL);
}

/**
   The Tuning mechansim of the Raduino works in a very innovative way. It uses a tuning potentiometer.
   The tuning potentiometer that a voltage between 0 and 5 volts at ANALOG_TUNING pin of the control connector.
   This is read as a value between 0 and 1000. By 100x oversampling this range is expanded by a factor 10.
   Hence, the tuning pot gives you 10,000 steps from one end to the other end of its rotation. Each step is 50 Hz,
   thus giving maximum 500 Khz of tuning range. The tuning range is scaled down depending on the TUNING_RANGE value.
   The standard tuning range (for the standard 1-turn pot) is 50 Khz. But it is also possible to use a 10-turn pot
   to tune accross the entire 40m band. The desired TUNING_RANGE can be set via the Function Button in the SETTINGS menu.
   When the potentiometer is moved to either end of the range, the frequency starts automatically moving
   up or down in 10 Khz increments, so it is still possible to tune beyond the range set by TUNING_RANGE.
*/

void doTuning() {
  int knob = analogRead(ANALOG_TUNING) * 10; // get the current tuning knob position

  // tuning is disabled during TX (only when PTT sense line is installed)
  if (inTx && (clicks < 10) && (abs(knob - old_knob) > 20)) {
    printLine2((char *)"dial is locked");
    shiftBase();
    firstrun = true;
    return;
  }
  else if (inTx)
    return;

  knob = knob_position(); // get the precise tuning knob position
  // the knob is fully on the low end, do fast tune: move down by max 10 Khz and wait for 300 msec
  // if the selected TUNING RANGE is very small (less than 25 kHz) then use max 1 kHz steps instead
  // step size is variable: the closer the knob is to the end, the larger the step size
  if (knob < 100 && frequency > LOWEST_FREQ) {
    if (TUNING_RANGE < 25)
      baseTune = baseTune - (100 - knob) * 10UL; // fast tune down in max 1 kHz steps
    else
      baseTune = baseTune - (100 - knob) * 100UL; // fast tune down in max 10 kHz steps
    frequency = baseTune + (unsigned long)knob * (unsigned long)TUNING_RANGE / 10UL;
    setFrequency(frequency);
    if (clicks < 10)
      printLine2((char *)"<<<<<<<"); // tks Paul KC8WBK
    delay(300);
  }

  // the knob is full on the high end, do fast tune: move up by max 10 Khz and wait for 300 msec
  // if the selected TUNING RANGE is very small (less than 25 kHz) then use max 1 kHz steps instead
  // step size is variable: the closer the knob is to the end, the larger the step size
  else if (knob > 10130 && frequency < HIGHEST_FREQ) {
    if (TUNING_RANGE < 25)
      baseTune = baseTune + (knob - 10130) * 10UL; // fast tune up in max 1 kHz steps
    else
      baseTune = baseTune + (knob - 10130) * 100UL; // fast tune up in max 10 kHz steps
    frequency = baseTune + (unsigned long)knob * (unsigned long)TUNING_RANGE / 10UL;
    setFrequency(frequency);
    if (clicks < 10)
      printLine2((char *)"         >>>>>>>"); // tks Paul KC8WBK
    delay(300);
  }

  // the tuning knob is at neither extremities, tune the signals as usual
  else {
    if (abs(knob - old_knob) > 2) { // improved "flutter fix": only change frequency when the current knob position is more than 2 steps away from the previous position
      knob = (knob + old_knob) / 2; // tune to the midpoint between current and previous knob reading
      old_knob = knob;
      frequency = baseTune + (unsigned long)knob * (unsigned long)TUNING_RANGE / 10UL;
      setFrequency(frequency);
      delay(10);
    }
  }

  if (!vfoActive) // VFO A is active
    vfoA = frequency;
  else
    vfoB = frequency;
}

/**
   "CW SPOT" function: When operating CW it is important that both stations transmit their carriers on the same frequency.
   When the SPOT button is pressed while the radio is in CW mode, the RIT will be turned off and the sidetone will be generated (but no carrier will be transmitted).
   The FINETUNE mode is then also enabled (fine tuning +/- 1000Hz). Fine tune the VFO so that the pitch of the received CW signal is equal to the pitch of the CW Spot tone.
   By aligning the CW Spot tone to match the pitch of an incoming station's signal, you will cause your signal and the other station's signal to be exactly on the same frequency.

   When the SPOT button is pressed while the radio is in SSB mode, the radio will only be put in FINETUNE mode (no sidetone will be generated).
*/

void checkSPOT() {
  if (digitalRead(SPOT) == LOW && !inTx) {
    RUNmode = RUN_FINETUNING;
    TimeOut = millis() - 1000UL;
    if (ritOn) {
      toggleRIT(); // disable the RIT if it was on
      old_knob = knob_position();
    }
  }
}

void finetune() {

  // for SPOT tuning: in CW mode only, generate short side tone pulses every one second
  if ((mode & 2) && millis() - TimeOut > 1000UL) {
    tone(CW_TONE, CW_OFFSET);
    if (millis() - TimeOut > 1150UL) {
      noTone(CW_TONE);
      TimeOut = millis();
    }
  }

  int knob = knob_position(); // get the current tuning knob position
  static int fine_old;

  if (digitalRead(SPOT) == LOW && !inTx) {
    if (firstrun) {
      firstrun = false;
      fine = 0;
      fine_old = 9999;
      shift = (5000 - knob) / 5;
    }

    //generate values -1000 ~ +1000 from the tuning pot
    fine = (knob - 5000) / 5 + shift;
    if (knob < 10 && fine > -1000 && frequency + fine > LOWEST_FREQ)
      shift = shift - 10;
    else if (knob > 10220 && fine < 1000 && frequency + fine < HIGHEST_FREQ)
      shift = shift + 10;

    if (fine != fine_old) {
      setFrequency(frequency); // apply the finetuning offset

      memset(c, 0, sizeof(c));
      memset(b, 0, sizeof(b));

      ultoa((frequency + fine), b, DEC);

      if (!vfoActive) // VFO A is active
        strcpy(c, "A ");
      else
        strcpy(c, "B ");

      c[2] = b[0];
      strcat(c, ".");
      strncat(c, &b[1], 3);
      strcat(c, ".");
      strncat(c, &b[4], 3); // show two more digits

      switch (mode) {
        case LSB:
          strcat(c, " LSB");
          break;
        case USB:
          strcat(c, " USB");
          break;
        case CWL:
          strcat(c, " CWL");
          break;
        case CWU:
          strcat(c, " CWU");
          break;
      }
    }

    printLine1(c);
    if (mode & 2)
      printLine2((char *)"SPOT + FINE TUNE");
    else
      printLine2((char *)"FINE TUNE");

    fine_old = fine;
  }

  else { // return to normal mode when SPOT button is released
    firstrun = true;
    TimeOut = 0;
    noTone(CW_TONE);
    RUNmode = RUN_NORMAL;
    frequency = frequency + fine; // apply the finetuning offset
    fine = 0;
    setFrequency(frequency);
    shiftBase();
    old_knob = knob_position();
  }
}

byte raduino_version; //version identifier

void factory_settings() {
  printLine1((char *)"loading standard");
  printLine2((char *)"settings...");
  EEPROM.put(0, raduino_version); //version identifier
  EEPROM.put(2, 0); //cal offset value (0 Hz)
  EEPROM.put(4, 1500); //USB offset (1500 Hz)
  EEPROM.put(6, 4); //VFO drive level in LSB/CWL mode (4 mA)
  EEPROM.put(7, 8); //VFO drive level in USB/CWU mode (8 mA)
  EEPROM.put(8, true); // T/R is semi QSK
  EEPROM.put(10, 50); //tuning range (50 kHz)
  EEPROM.put(12, 800); //CW offset / sidetone pitch (800 Hz)
  EEPROM.put(16, 7125000UL); // VFO A frequency (7125 kHz)
  EEPROM.put(20, 7125000UL); // VFO B frequency (7125 kHz)
  EEPROM.put(24, 0); // mode VFO A (LSB)
  EEPROM.put(25, 0); // mode VFO B (LSB)
  EEPROM.put(26, false); // vfoActive (VFO A)
  EEPROM.put(27, false); // SPLIT off
  EEPROM.put(28, 7100); // scan_start_freq (7100 kHz)
  EEPROM.put(30, 7150); // scan_stop_freq (7150 kHz)
  EEPROM.put(32, 1000); // scan_step_freq (1000 Hz)
  EEPROM.put(34, 500); // scan_step_delay (500 ms)
  EEPROM.put(36, 350); // CW timout (350 ms)
  delay(1000);
}

// save the VFO frequencies when they haven't changed more than 500Hz in the past 30 seconds
// (EEPROM.put writes the data only if it is different from the previous content of the eeprom location)

void save_frequency() {
  static long t3;
  static unsigned long old_vfoA, old_vfoB;
  unsigned long aDif, bDif; // tks Richard Blessing
  if (vfoA >= old_vfoA)
  {
    aDif = vfoA - old_vfoA;
  }
  else
  {
    aDif = old_vfoA - vfoA;
  }
  if (vfoB >= old_vfoB)
  {
    bDif = vfoB - old_vfoB;
  }
  else
  {
    bDif = old_vfoB - vfoB;
  }

  if ((aDif < 500UL) && (bDif < 500UL)) {
    if (millis() - t3 > 30000UL) {
      EEPROM.put(16, old_vfoA); // save VFO_A frequency
      EEPROM.put(20, old_vfoB); // save VFO_B frequency
      t3 = millis();
    }
  }
  else
    t3 = millis();
  if (aDif > 500UL)
    old_vfoA = vfoA;
  if (bDif > 500UL)
    old_vfoB = vfoB;
}

void scan() {

  int knob = knob_position();

  if (abs(knob - old_knob) > 8 || (digitalRead(PTT_SENSE) && PTTsense_installed) || !digitalRead(FBUTTON) || !digitalRead(KEY)) {
    //stop scanning
    TimeOut = 0; // reset the timeout counter
    RUNmode = RUN_NORMAL;
    shiftBase();
    delay(400);
  }

  else if (TimeOut < millis()) {
    if (RUNmode == RUN_SCAN) {
      frequency = frequency + scan_step_freq; // change frequency
      // test for upper limit of scan
      if (frequency > scan_stop_freq * 1000UL)
        frequency = scan_start_freq * 1000UL;
      setFrequency(frequency);
    }
    else // monitor mode
      swapVFOs();

    TimeOut = millis() + scan_step_delay;
  }
}

/**
   setup is called on boot up
   It setups up the modes for various pins as inputs or outputs
   initiliaizes the Si5351 and sets various variables to initial state

   Just in case the LCD display doesn't work well, the debug log is dumped on the serial monitor
   Choose Serial Monitor from Arduino IDE's Tools menu to see the Serial.print messages
*/

void setup() {
  raduino_version = 19;
  strcpy (c, "Raduino v1.19");

  lcd.begin(16, 2);
  printBuff1[0] = 0;
  printBuff2[0] = 0;

  // Start serial and initialize the Si5351
  Serial.begin(9600);
  analogReference(DEFAULT);
  //Serial.println("*Raduino booting up");

  //configure the morse key input to use the internal pull-up
  pinMode(KEY, INPUT_PULLUP);
  //configure the function button to use the internal pull-up
  pinMode(FBUTTON, INPUT_PULLUP);
  //configure the PTT SENSE to use the internal pull-up
  pinMode(PTT_SENSE, INPUT_PULLUP);
  //configure the CAL button to use the internal pull-up
  pinMode(CAL_BUTTON, INPUT_PULLUP);
  //configure the SPOT button to use the internal pull-up
  pinMode(SPOT, INPUT_PULLUP);

  pinMode(TX_RX, OUTPUT);
  digitalWrite(TX_RX, 0);
  pinMode(CW_CARRIER, OUTPUT);
  digitalWrite(CW_CARRIER, 0);
  pinMode(CW_TONE, OUTPUT);
  digitalWrite(CW_TONE, 0);

  // when Fbutton or CALbutton is pressed during power up,
  // or after a version update,
  // then all settings will be restored to the standard "factory" values
  byte old_version;
  EEPROM.get(0, old_version); // previous sketch version
  if (!digitalRead(CAL_BUTTON) || !digitalRead(FBUTTON) || (old_version != raduino_version)) {
    factory_settings();
  }

  // check if PTT sense line is installed
  if (!digitalRead(PTT_SENSE))
    PTTsense_installed = true; //yes it's installed
  else
    PTTsense_installed = false; //no it's not installed

  printLine1(c);
  delay(1000);

  //retrieve user settings from EEPROM
  EEPROM.get(2, cal);
  EEPROM.get(4, USB_OFFSET);

  //display warning message when calibration data was erased
  if ((cal == 0) && (USB_OFFSET == 1500))
    printLine2((char *)"uncalibrated!");

  EEPROM.get(6, LSBdrive);
  EEPROM.get(7, USBdrive);
  EEPROM.get(8, semiQSK);
  EEPROM.get(10, TUNING_RANGE);
  EEPROM.get(12, CW_OFFSET);
  EEPROM.get(16, vfoA);
  EEPROM.get(20, vfoB);
  EEPROM.get(24, mode_A);
  EEPROM.get(25, mode_B);
  EEPROM.get(26, vfoActive);
  EEPROM.get(27, splitOn);
  EEPROM.get(28, scan_start_freq);
  EEPROM.get(30, scan_stop_freq);
  EEPROM.get(32, scan_step_freq);
  EEPROM.get(34, scan_step_delay);
  EEPROM.get(36, QSK_DELAY);

  //initialize the SI5351
  si5351.init(SI5351_CRYSTAL_LOAD_8PF, 25000000L, 0);
  //Serial.println("*Initiliazed Si5351\n");
  si5351.set_pll(SI5351_PLL_FIXED, SI5351_PLLA);
  si5351.set_pll(SI5351_PLL_FIXED, SI5351_PLLB);
  //Serial.println("*Fixed PLL\n");
  si5351.output_enable(SI5351_CLK0, 0);
  si5351.output_enable(SI5351_CLK1, 0);
  si5351.output_enable(SI5351_CLK2, 1);
  //Serial.println("*Output enabled PLL\n");
  si5351.set_freq(500000000L , SI5351_CLK2);
  //Serial.println("*Si5350 ON\n");

  if (!vfoActive) { // VFO A is active
    frequency = vfoA;
    mode = mode_A;
  }
  else {
    frequency = vfoB;
    mode = mode_B;
  }

  if (mode & 1) // if UPPER side band
    SetSideBand(USBdrive);
  else // if LOWER side band
    SetSideBand(LSBdrive);

  if (mode > 1) // if in CW mode
    RXshift = CW_OFFSET;

  shiftBase(); //align the current knob position with the current frequency

  //If no FButton is installed, and you still want to use custom tuning range settings,
  //uncomment the following line and adapt the value as desired:

  //TUNING_RANGE = 50;    // tuning range (in kHz) of the tuning pot

  //recommended tuning range for a 1-turn pot: 50kHz, for a 10-turn pot: 100-200kHz
  //recommended tuning range when radio is used mainly for CW: 10-25 kHz

  bleep(CW_OFFSET, 60, 3);
  bleep(CW_OFFSET, 180, 1);
}

void loop() {
  switch (RUNmode) {
    case 0: // for backward compatibility: execute calibration when CAL button is pressed
      if (!digitalRead(CAL_BUTTON)) {
        RUNmode = RUN_CALIBRATE;
        calbutton = true;
        factory_settings();
        printLine1((char *)"Calibrating: Set");
        printLine2((char *)"to zerobeat");
        delay(2000);
      }
      else {
        if (!inTx) {
          checkButton();
          checkSPOT();
          save_frequency();
          if (clicks == 0 && !ritOn)
            printLine2((char *)" ");
        }
        if (PTTsense_installed) {
          checkCW();
          checkTX();
        }
        if (ritOn && !inTx)
          doRIT();
        else
          doTuning();
      }
      return;
    case 1: //calibration
      calibrate();
      break;
    case 2: //set VFO drive level
      VFOdrive();
      break;
    case 3: // set tuning range
      set_tune_range();
      break;
    case 4: // set CW parameters
      set_CWparams();
      checkCW();
      break;
    case 5: // scan mode
      scan();
      break;
    case 6: // set scan paramaters
      scan_params();
      break;
    case 7: // A/B monitor mode
      scan();
      break;
    case 8: // Fine tuning mode
      finetune();
      break;
  }
  delay(100);
}
