/**
   Raduino_v1.13 for BITX40 - Allard Munters PE1NWL (pe1nwl@gooddx.net)

   This source file is under General Public License version 3.

   Most source code are meant to be understood by the compilers and the computers.
   Code that has to be hackable needs to be well understood and properly documented.
   Donald Knuth coined the term Literate Programming to indicate code that is written be
   easily read and understood.

   The Raduino is a small board that includes the Arduin Nano, a 16x2 LCD display and
   an Si5351a frequency synthesizer. This board is manufactured by Paradigm Ecomm Pvt Ltd

   To learn more about Arduino you may visit www.arduino.cc.

   The Arduino works by firt executing the code in a function called setup() and then it
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

    NOTE 2: Although all V2 versions of the Si5351 library will technically work, it is recommended to
    use v2.0.1 although this is not the very latest version. It has been noticed that newer versions produce
    strong clicks during tuning. That issue is still under investigation. Please use v2.0.1 until this
    is resolved. Download v1.0.2 from https://github.com/etherkit/Si5351Arduino/releases/tag/v2.0.1
*/

#include <si5351.h> // https://github.com/etherkit/Si5351Arduino/releases/tag/v2.0.1
Si5351 si5351;
/**
   The Raduino board is the size of a standard 16x2 LCD panel. It has three connectors:

   First, is an 8 pin connector that provides +5v, GND and six analog input pins that can also be
   configured to be used as digital input or output pins. These are referred to as A0,A1,A2,
   A3,A6 and A7 pins. The A4 and A5 pins are missing from this connector as they are used to
   talk to the Si5351 over I2C protocol.

    A0     A1   A2   A3    GND    +5V   A6   A7
   BLACK BROWN RED ORANGE YELLOW GREEN BLUE VIOLET

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

char c[30], b[30], printBuff[32];

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

byte PTTsense_installed; //whether or not the PTT sense line is installed (detected automatically during startup)

/**
    The second set of 16 pins on the bottom connector P3 have the three clock outputs and the digital lines to control the rig.
    This assignment is as follows :
      Pin   1   2    3    4    5    6    7    8    9    10   11   12   13   14   15   16  (connector P3)
         +12V +12V CLK2  GND  GND CLK1  GND  GND  CLK0  GND  D2   D3   D4   D5   D6   D7
    These too are flexible with what you may do with them, for the Raduino, we use them to :

    output D5 - CW_TONE : Side tone output
    output D6 - CW_CARRIER line : turns on the carrier for CW
    output D7 - TX_RX line : Switches between Transmit and Receive in CW mode
*/

#define CW_TONE (5)
#define CW_CARRIER (6)
#define TX_RX (7)

/**
    The raduino has a number of timing parameters, all specified in milliseconds
    CW_TIMEOUT : how many milliseconds between consecutive keyup and keydowns before switching back to receive?
*/

unsigned int CW_TIMEOUT; // in milliseconds, this is the parameter that determines how long the tx will hold between cw key downs

/**
   The Raduino supports two VFOs : A and B and receiver incremental tuning (RIT).
   we define a variables to hold the frequency of the two VFOs, RITs
   the rit offset as well as status of the RIT

   To use this facility, wire up push button on A3 line of the control connector (Function Button)
*/
#define VFO_A 0
#define VFO_B 1
unsigned long vfoA, vfoB; // the frequencies the VFOs
byte ritOn; // whether or not the RIT is on
byte vfoActive; // which VFO A or B is active
byte mode_A, mode_B; // the mode (LSB or USB) of each VFO

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
int LSBdrive;
int USBdrive;

// scan parameters

int scan_start_freq; // lower scan limit (kHz)
int scan_stop_freq; // upper scan limit (kHz)
int scan_step_freq; // step size (Hz)
int scan_step_delay; // step delay (ms)

/**
   Raduino needs to keep track of current state of the transceiver. These are a few variables that do it
*/
byte isUSB; // mode of the currently active VFO
byte inTx = 0; // whether or not we are in transmit mode
byte keyDown = 0; // whether we have a key up or key down
unsigned long cwTimeout = 0;

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

unsigned long CW_OFFSET; // the amount of carrier offset (Hz) when transmitting CW, equal to sidetone frequency
unsigned long CWoffset = 0;

#define LOWEST_FREQ  (6995000L) // absolute minimum frequency (Hz)
#define HIGHEST_FREQ (7500000L) //  absolute maximum frequency (Hz)

unsigned long frequency; // the 'dial' frequency as shown on the display

/**
   The raduino has multiple modes:
*/
#define MODE_NORMAL (0)  // normal operation
#define MODE_CALIBRATE (1) // calibrate VFO frequency in LSB mode
#define MODE_DRIVELEVEL (2) // set VFO drive level
#define MODE_TUNERANGE (3) // set the range of the tuning pot
#define MODE_CWOFFSET (4) // set the CW offset (=sidetone pitch)
#define MODE_SCAN (5) // frequency scanning mode
#define MODE_SCAN_PARAMS (6) // set scan parameters

byte mode = MODE_NORMAL;

/**
   Display Routines
   These two display routines print a line of characters to the upper and lower lines of the 16x2 display
*/

void printLine1(char *c) {
  if (strcmp(c, printBuff)) {
    lcd.setCursor(0, 0);
    lcd.print(c);
    strcpy(printBuff, c);
  }
}

void printLine2(char *c) {
  lcd.setCursor(0, 1);
  lcd.print(c);
}

/**
   Building upon the previous  two functions,
   update Display paints the first line as per current state of the radio
*/

void updateDisplay() {
  // improved by Jack Purdum, W8TEE
  // replaced fsprint commmands by str commands for code size reduction

  memset(c, 0, sizeof(c));
  memset(b, 0, sizeof(b));

  ltoa(frequency, b, DEC);

  if (VFO_A == vfoActive)
    strcpy(c, "A ");
  else
    strcpy(c, "B ");

  c[2] = b[0];
  strcat(c, ".");
  strncat(c, &b[1], 3);
  strcat(c, ".");
  strncat(c, &b[4], 1);


  if (CWoffset == 0) {
    if (isUSB)
      strcat(c, " USB");
    else
      strcat(c, " LSB");
  }
  else {
    if (isUSB)
      strcat(c, " CWU");
    else
      strcat(c, " CWL");
  }

  if (inTx)
    strcat(c, " TX");
  else if (ritOn)
    strcat(c, " +R");
  else
    strcat(c, "   ");

  printLine1(c);
}

// function to generate a bleep sound (FB menu)
void bleep(int pitch, int duration, int repeat) {
  for (int i = 0; i < repeat; i++) {
    tone(CW_TONE, pitch);
    delay(duration);
    noTone(CW_TONE);
    delay(duration);
  }
}

bool calbutton_prev = HIGH;

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

  if (mode != MODE_CALIBRATE) {

    if (isUSB)
      current_setting = USB_OFFSET;
    else
      current_setting = cal;

    shift = current_setting - (analogRead(ANALOG_TUNING) * 10) + 5000;
  }

  // The tuning knob gives readings from 0 to 1000
  // Each step is taken as 10 Hz and the mid setting of the knob is taken as zero

  if (isUSB) {
    USB_OFFSET = constrain((analogRead(ANALOG_TUNING) * 10) - 5000 + shift, -5000, 5000);

    if (analogRead(ANALOG_TUNING) < 5 && USB_OFFSET > -5000)
      shift = shift - 10;
    if (analogRead(ANALOG_TUNING) > 1020 && USB_OFFSET < 5000)
      shift = shift + 10;
  }
  else {
    cal = constrain((analogRead(ANALOG_TUNING) * 10) - 5000 + shift, -5000, 5000);

    if (analogRead(ANALOG_TUNING) < 5 && cal > -5000)
      shift = shift - 10;
    if (analogRead(ANALOG_TUNING) > 1020 && cal < 5000)
      shift = shift + 10;
  }

  // if Fbutton is pressed again, we save the setting
  if (digitalRead(FBUTTON) == LOW) {
    mode = MODE_NORMAL;

    if (isUSB) {
      printLine2((char *)"USB Calibrated! ");
      //Write the 2 bytes of the USB offset into the eeprom memory.
      EEPROM.put(4, USB_OFFSET);
    }

    else {
      printLine2((char *)"LSB Calibrated! ");
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
    mode = MODE_CALIBRATE;

    if (isUSB) {
      si5351.set_freq((bfo_freq + frequency + cal / 5 * 19 - USB_OFFSET) * 100LL, SI5351_CLK2);
      itoa(USB_OFFSET, b, DEC);
    }

    else {
      si5351.set_freq((bfo_freq - frequency + cal) * 100LL, SI5351_CLK2);
      itoa(cal, b, DEC);
    }

    strcpy(c, "offset ");
    strcat(c, b);
    strcat(c, " Hz     ");
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

   Furthermore we add/substract the sidetone frequency only when we transmit CW, to assure zero beat
   between the transmitting and receiving station.
   The desired sidetone frequency can be set in the SETTINGS menu.
*/

void setFrequency(unsigned long f) {

  if (isUSB)
    si5351.set_freq((bfo_freq + f + cal / 5 * 19 - USB_OFFSET + CWoffset) * 100ULL, SI5351_CLK2);
  else
    si5351.set_freq((bfo_freq - f + cal + CWoffset) * 100ULL, SI5351_CLK2);
  frequency = f;
  delay(25);
}

/**
   The checkTX toggles the T/R line. If you would like to make use of RIT, etc,
   you must connect pin A0 (black wire) via a 10K resistor to the output of U3
   This is a voltage regulator LM7805 which goes on during TX. We use the +5V output
   as a PTT sense line (to tell the Raduino that we are in TX).
*/

void checkTX() {
  // We don't check for ptt when transmitting cw
  // as long as the cwTimeout is non-zero, we will continue to hold the
  // radio in transmit mode
  if (cwTimeout > 0)
    return;

  if (digitalRead(PTT_SENSE) == 1 && inTx == 0) {
    // go in transmit mode
    inTx = 1;
    updateDisplay();
    if (ritOn) { // when RIT is on, swap the VFOs
      swapVFOs();
    }
  }

  if (digitalRead(PTT_SENSE) == 0 && inTx == 1) {
    //go in receive mode
    inTx = 0;
    updateDisplay();
    if (ritOn) { // when RIT was on, swap the VFOs back to original state
      swapVFOs();
    }
  }
}

/* CW is generated by unbalancing the mixer when the key is down.
   During key down, the output CW_CARRIER is HIGH (+5V).
   This output is connected via a 47K resistor to the mixer input. The mixer will
   become unbalanced when CW_CARRIER is HIGH, so a carrier will be transmitted.
   During key up, the output CW_CARRIER is LOW (0V). The mixer will remain balanced
   and the carrrier will be suppressed.

   The radio will go into CW mode automatically as soon as the key goes down, and
   return to normal LSB/USB mode when the key has been up for some time.

   There are three variables that track the CW mode
   inTX     : true when the radio is in transmit mode
   keyDown  : true when the CW is keyed down, you maybe in transmit mode (inTX true)
              and yet between dots and dashes and hence keyDown could be true or false
   cwTimeout: Figures out how long to wait between dots and dashes before putting
              the radio back in receive mode

   When we transmit CW, we need to apply some offset (800Hz) to the TX frequency, in
   order to keep zero-beat between the transmitting and receiving station. The shift
   depends on whether upper or lower sideband CW is used:
   In CW-U (USB) mode we must shift the TX frequency 800Hz up
   In CW-L (LSB) mode we must shift the TX frequency 800Hz down

   The default offset (CW_OFFSET) is 800Hz, the default timeout (CW_TIMEOUT) is 350ms.
   The user can change these in the SETTINGS menu.

   Although the frequency will be shifted during TX, the frequency shown on the display
   (dial frequency) will not change (it will always show the RX frequency).
*/

void checkCW() {

  if (keyDown == 0 && digitalRead(KEY) == LOW) {
    //switch to transmit mode if we are not already in it
    if (inTx == 0) {
      if (ritOn) // when RIT is on, swap the VFOs
        swapVFOs();
      CWoffset = CW_OFFSET;
      setFrequency(frequency);
      digitalWrite(TX_RX, 1); // activate the PTT switch - go in transmit mode
      //give the relays a few ms to settle the T/R relays
      delay(10);
      inTx = 1;
    }
    digitalWrite(CW_CARRIER, 1); // generate a carrier
    keyDown = 1;
    tone(CW_TONE, CW_OFFSET); // generate a sidetone
    updateDisplay();
  }

  //reset the timer as long as the key is down
  if (keyDown == 1)
    cwTimeout = millis() + CW_TIMEOUT;

  //if we have a keyup
  if (keyDown == 1 && digitalRead(KEY) == HIGH) {
    digitalWrite(CW_CARRIER, 0); // stop generating the carrier
    keyDown = 0;
    noTone(CW_TONE); // stop generating the sidetone
    cwTimeout = millis() + CW_TIMEOUT;
  }

  //if we are in cw-mode and have a keyup for a "longish" time (=cwTimout value in ms)
  if (cwTimeout > 0 && inTx == 1 && cwTimeout < millis()) {
    //move the radio back to receive
    digitalWrite(TX_RX, 0); // release the PTT switch - go back to receive mode
    CWoffset = 0;
    setFrequency(frequency);
    inTx = 0;
    cwTimeout = 0; // reset the CW timeout counter
    if (ritOn) // when RIT was on, swap the VFOs back
      swapVFOs();
    updateDisplay();
  }
}

/**
   A trivial function to wrap around the function button
*/
int btnDown() {
  if (digitalRead(FBUTTON) == HIGH)
    return 0;
  else
    return 1;
}

byte param;

/**
   The Function Button is used for several functions
   NORMAL menu (normal operation):
   1 short press: swap VFO A/B
   2 short presses: toggle RIT on/off
   3 short presses: toggle LSB/USB
   4 short presses: start scan mode
   long press (>1 Sec): VFO A=B
   VERY long press (>3 sec): go to SETTINGS menu

   SETTINGS menu:
   1 short press: LSB calibration
   2 short presses: USB calibration
   3 short presses: VFO drive level in LSB mode
   4 short presses: VFO drive level in USB mode
   5 short presses: Tuning range upper&lower limit settings
   6 short presses: Set the CW parameters (sidetone pitch, CW timeout)
   7 short presses: Set the 4 scan parameters (lower limit, upper limit, step size, step delay)
   long press: exit SETTINGS menu - go back to NORMAL menu
*/
void checkButton() {

  static int clicks, action;
  static long t1, t2;
  static byte pressed = 0;

  if (!btnDown()) {
    t2 = millis() - t1; //time elapsed since last button press
    if (pressed == 1)
      if (clicks < 10 && t2 > 600 && t2 < 3000) { //detect long press to reset the VFO's
        bleep(600, 50, 1);
        resetVFOs();
        delay(700);
        printLine2((char *)"                ");
        clicks = 0;
      }

    if (t2 > 500) { // max time between button clicks (ms)
      action = clicks;
      if (clicks >= 10)
        clicks = 10;
      else
        clicks = 0;
    }
    pressed = 0;
  }
  else {
    delay(10);
    if (btnDown()) {
      // button was really pressed, not just some noise
      if (pressed == 0) {
        pressed = 1;
        t1 = millis();
        bleep(1200, 50, 1);
        action = 0;
        clicks++;
        if (clicks > 17)
          clicks = 11;
        if (clicks > 4 && clicks < 10)
          clicks = 1;
        switch (clicks) {
          //Normal menu options
          case 1:
            printLine2((char *)"Swap VFOs       ");
            break;
          case 2:
            printLine2((char *)"RIT ON/OFF      ");
            break;
          case 3:
            printLine2((char *)"Toggle LSB/USB  ");
            break;
          case 4:
            printLine2((char *)"Start SCAN      ");
            break;

          //SETTINGS menu options
          case 11:
            printLine2((char *)"LSB calibration ");
            break;
          case 12:
            printLine2((char *)"USB calibration ");
            break;
          case 13:
            printLine2((char *)"VFO drive - LSB ");
            break;
          case 14:
            printLine2((char *)"VFO drive - USB ");
            break;
          case 15:
            printLine2((char *)"Set tuning range");
            break;
          case 16:
            printLine2((char *)"Set CW params   ");
            break;
          case 17:
            printLine2((char *)"Set scan params ");
            break;
        }
      }
      else if ((millis() - t1) > 600 && (millis() - t1) < 800 && clicks < 10) // long press: reset the VFOs
        printLine2((char *)"Reset VFOs      ");

      if ((millis() - t1) > 3000 && clicks < 10) { // VERY long press: go to the SETTINGS menu
        bleep(1200, 150, 3);
        printLine2((char *)"--- SETTINGS ---");
        clicks = 10;
      }

      else if ((millis() - t1) > 1500 && clicks > 10) { // long press: return to the NORMAL menu
        bleep(1200, 150, 3);
        clicks = -1;
        pressed = 0;
        printLine2((char *)" --- NORMAL --- ");
        delay(700);
        printLine2((char *)"                ");
      }
    }
  }
  if (action != 0 && action != 10)
    bleep(600, 50, 1);
  switch (action) {
    // NORMAL menu

    case 1: // swap the VFOs
      swapVFOs();
      EEPROM.put(26, vfoActive);
      printLine2((char *)"VFOs swapped!   ");
      delay(700);
      printLine2((char *)"                ");
      break;

    case 2: // toggle the RIT on/off
      toggleRIT();
      delay(700);
      printLine2((char *)"                ");
      break;

    case 3: // toggle the mode LSB/USB
      toggleMode();
      delay(700);
      printLine2((char *)"                ");
      break;

    case 4: // start scan mode
      mode = MODE_SCAN;
      old_knob = knob_position();
      if (frequency < scan_start_freq * 1000L)
        frequency = scan_start_freq * 1000L;
      printLine2((char *)"scanning        ");
      break;

    // SETTINGS MENU

    case 11: // calibrate the dial frequency in LSB
      setLSB();
      calibrate();
      break;

    case 12: // calibrate the dial frequency in USB
      setUSB();
      calibrate();
      break;

    case 13: // set the VFO drive level in LSB
      setLSB();
      VFOdrive();
      break;

    case 14: // set the VFO drive level in USB
      setUSB();
      VFOdrive();
      break;

    case 15: // set the tuning pot range
      set_tune_range();
      break;

    case 16: // set CW parameters (sidetone pitch, CW timeout)
      param = 1;
      set_CWoffset();
      break;

    case 17: // set the 4 scan parameters
      param = 1;
      scan_params();
      break;
  }
}

void swapVFOs() {
  if (vfoActive == VFO_B) {
    vfoActive = VFO_A;
    vfoB = frequency;
    frequency = vfoA;
    if (mode_A)
      setUSB();
    else
      setLSB();
  }
  else {
    vfoActive = VFO_B;
    vfoA = frequency;
    frequency = vfoB;
    if (mode_B)
      setUSB();
    else
      setLSB();
  }

  shiftBase(); //align the current knob position with the current frequency
}

void toggleRIT() {
  if (PTTsense_installed == 0) {
    printLine2((char *)"Not available!  ");
    return;
  }
  if (ritOn) {
    ritOn = 0;
    printLine2((char *)"RIT disabled!   ");
  }
  else {
    ritOn = 1;
    printLine2((char *)"RIT enabled!    ");
  }
  EEPROM.put(27, ritOn);
  updateDisplay();
}

void toggleMode() {
  if (isUSB) {
    setLSB();
    printLine2((char *)"Mode is now LSB! ");
  }
  else {
    setUSB();
    printLine2((char *)"Mode is now USB! ");
  }
}

void setUSB() {
  isUSB = 1;
  set_drive_level(USBdrive);
  setFrequency(frequency);
  updateDisplay();
  if (vfoActive == VFO_A) {
    mode_A = 1;
    EEPROM.put(24, mode_A);
  }
  else {
    mode_B = 1;
    EEPROM.put(25, mode_B);
  }
}

void setLSB() {
  isUSB = 0;
  set_drive_level(LSBdrive);
  setFrequency(frequency);
  updateDisplay();
  if (vfoActive == VFO_A) {
    mode_A = 0;
    EEPROM.put(24, mode_A);
  }
  else {
    mode_B = 0;
    EEPROM.put(25, mode_B);
  }
}

// resetting the VFO's will set both VFO's to the current frequency and mode
void resetVFOs() {
  printLine2((char *)"VFO A=B !       ");
  vfoA = vfoB = frequency;
  mode_A = mode_B = isUSB;
  updateDisplay();
  bleep(600, 50, 1);
  EEPROM.put(24, mode_A);
  EEPROM.put(25, mode_B);
}

void VFOdrive() {
  static int drive;

  if (mode != MODE_DRIVELEVEL) {
    if (isUSB)
      current_setting = USBdrive / 2 - 1;
    else
      current_setting = LSBdrive / 2 - 1;

    shift = analogRead(ANALOG_TUNING);
  }

  //generate drive level values 2,4,6,8 from tuning pot
  drive = 2 * ((((analogRead(ANALOG_TUNING) - shift) / 50 + current_setting) & 3) + 1);

  // if Fbutton is pressed again, we save the setting

  if (digitalRead(FBUTTON) == LOW) {
    mode = MODE_NORMAL;
    printLine2((char *)"Drive level set!");

    if (isUSB) {
      USBdrive = drive;
      //Write the 2 bytes of the USBdrive level into the eeprom memory.
      EEPROM.put(8, drive);
    }
    else {
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
    mode = MODE_DRIVELEVEL;
    set_drive_level(drive);

    itoa(drive, b, DEC);
    strcpy(c, "drive level ");
    strcat(c, b);
    strcat(c, "mA ");
    printLine2(c);
  }
}

/* this function allows the user to set the tuning range depending on the type of potentiometer
  for a standard 1-turn pot, a range of 50 KHz is recommended
  for a 10-turn pot, a tuning range of 200 kHz is recommended
*/
void set_tune_range() {

  if (mode != MODE_TUNERANGE) {
    current_setting = TUNING_RANGE;
    shift = current_setting - 10 * analogRead(ANALOG_TUNING) / 20;
  }

  //generate values 10-500 from the tuning pot
  TUNING_RANGE = constrain(10 * analogRead(ANALOG_TUNING) / 20 + shift, 10, 500);
  if (analogRead(ANALOG_TUNING) < 5 && TUNING_RANGE > 10)
    shift = shift - 10;
  if (analogRead(ANALOG_TUNING) > 1020 && TUNING_RANGE < 500)
    shift = shift + 10;

  // if Fbutton is pressed again, we save the setting
  if (digitalRead(FBUTTON) == LOW) {
    //Write the 2 bytes of the tuning range into the eeprom memory.
    EEPROM.put(10, TUNING_RANGE);
    printLine2((char *)"Tune range set! ");
    mode = MODE_NORMAL;
    delay(700);
    bleep(600, 50, 2);
    printLine2((char *)"--- SETTINGS ---");
  }

  else {
    mode = MODE_TUNERANGE;
    itoa(TUNING_RANGE, b, DEC);
    strcpy(c, "range ");
    strcat(c, b);
    strcat(c, " kHz    ");
    printLine2(c);
  }

  shiftBase(); //align the current knob position with the current frequency
}

byte firstrun = true;

/* this function allows the user to set the sidetone pitch. This is also the offset when we transmit CW.
*/

void set_CWoffset() {

  if (firstrun) {
    if (param == 1) {
      current_setting = CW_OFFSET;
      shift = current_setting - map(analogRead(ANALOG_TUNING), 0, 1024, 400, 1200);
    }
    else {
      current_setting = CW_TIMEOUT;
      shift = current_setting - map(analogRead(ANALOG_TUNING), 0, 1024, 50, 1000);
    }
  }

  if (param == 1) {
    //generate values 400-1200 from the tuning pot
    CW_OFFSET = constrain(map(analogRead(ANALOG_TUNING), 0, 1024, 400, 1200) + shift, 400, 1200);
    if (analogRead(ANALOG_TUNING) < 5 && CW_OFFSET > 400)
      shift = shift - 10;
    if (analogRead(ANALOG_TUNING) > 1020 && CW_OFFSET < 1200)
      shift = shift + 10;
  }
  else {
    //generate values 50-1000 from the tuning pot
    CW_TIMEOUT = constrain(map(analogRead(ANALOG_TUNING), 0, 1024, 50, 1000) + shift, 50, 1000);
    if (analogRead(ANALOG_TUNING) < 5 && CW_TIMEOUT > 50)
      shift = shift - 10;
    if (analogRead(ANALOG_TUNING) > 1020 && CW_TIMEOUT < 1000)
      shift = shift + 10;
  }

  // if Fbutton is pressed again, we save the setting
  if (digitalRead(FBUTTON) == LOW) {
    if (param == 1) {
      //Write the 2 bytes of the CW offset into the eeprom memory.
      EEPROM.put(12, CW_OFFSET);
      bleep(600, 50, 1);
      delay(200);
    }
    else {
      //Write the 2 bytes of the CW Timout into the eeprom memory.
      EEPROM.put(36, CW_TIMEOUT);
      printLine2((char *)"CW params set!  ");
      mode = MODE_NORMAL;
      delay(700);
      bleep(600, 50, 2);
      printLine2((char *)"--- SETTINGS ---");
    }
    param ++;
    firstrun = true;
  }

  else {
    mode = MODE_CWOFFSET;
    firstrun = false;
    if (param == 1) {
      bleep(CW_OFFSET, 50, 3);
      bleep(CW_OFFSET, 150, 1);
      itoa(CW_OFFSET, b, DEC);
      strcpy(c, "sidetone ");
      strcat(c, b);
      strcat(c, " Hz  ");
      printLine2(c);
    }
    else {
      itoa(CW_TIMEOUT, b, DEC);
      strcpy(c, "timeout ");
      strcat(c, b);
      strcat(c, " ms  ");
      printLine2(c);
    }
  }

  shiftBase(); //align the current knob position with the current frequency
}

/* this function allows the user to set the 4 scan parameters: lower limit, upper limit, step size and step delay
*/

void scan_params() {

  if (firstrun) {
    switch (param) {

      case 1: // set the lower scan limit

        current_setting = scan_start_freq;
        shift = current_setting - map(analogRead(ANALOG_TUNING), 0, 1024, 7000, 7500);
        break;

      case 2: // set the upper scan limit

        current_setting = scan_stop_freq;
        shift = current_setting - map(analogRead(ANALOG_TUNING), 0, 1024, scan_start_freq, 7500);
        break;

      case 3: // set the scan step size

        current_setting = scan_step_freq;
        shift = current_setting - 50 * (map(analogRead(ANALOG_TUNING), 0, 1024, 0, 10000) / 50);
        break;

      case 4: // set the scan step delay

        current_setting = scan_step_delay;
        shift = current_setting - 50 * (map(analogRead(ANALOG_TUNING), 0, 1024, 0, 2500) / 50);
        break;
    }
  }

  switch (param) {

    case 1: // set the lower scan limit

      //generate values 7000-7500 from the tuning pot
      scan_start_freq = constrain(map(analogRead(ANALOG_TUNING), 0, 1024, 7000, 7500) + shift, 7000, 7500);
      if (analogRead(ANALOG_TUNING) < 5 && scan_start_freq > 7000)
        shift = shift - 1;
      if (analogRead(ANALOG_TUNING) > 1020 && scan_start_freq < 7500)
        shift = shift + 1;
      break;

    case 2: // set the upper scan limit

      //generate values 7000-7500 from the tuning pot
      scan_stop_freq = constrain(map(analogRead(ANALOG_TUNING), 0, 1024, scan_start_freq, 7500) + shift, scan_start_freq, 7500);
      if (analogRead(ANALOG_TUNING) < 5 && scan_stop_freq > scan_start_freq)
        shift = shift - 1;
      if (analogRead(ANALOG_TUNING) > 1020 && scan_stop_freq < 7500)
        shift = shift + 1;
      break;

    case 3: // set the scan step size

      //generate values 50-10000 from the tuning pot
      scan_step_freq = constrain(50 * (map(analogRead(ANALOG_TUNING), 0, 1024, 0, 10000) / 50) + shift, 50, 10000);
      if (analogRead(ANALOG_TUNING) < 5 && scan_step_freq > 50)
        shift = shift - 50;
      if (analogRead(ANALOG_TUNING) > 1020 && scan_step_freq < 10000)
        shift = shift + 50;
      break;

    case 4: // set the scan step delay

      //generate values 0-2500 from the tuning pot
      scan_step_delay = constrain(50 * (map(analogRead(ANALOG_TUNING), 0, 1024, 0, 2500) / 50) + shift, 0, 2500);
      if (analogRead(ANALOG_TUNING) < 5 && scan_step_delay > 50)
        shift = shift - 50;
      if (analogRead(ANALOG_TUNING) > 1020 && scan_step_delay < 2500)
        shift = shift + 50;
      break;
  }

  // if Fbutton is pressed, we save the setting

  if (digitalRead(FBUTTON) == LOW) {
    switch (param) {

      case 1: // save the lower scan limit

        //Write the 2 bytes of the start freq into the eeprom memory.
        EEPROM.put(28, scan_start_freq);
        delay(50);
        bleep(600, 50, 1);
        break;

      case 2: // save the upper scan limit

        //Write the 2 bytes of the stop freq into the eeprom memory.
        EEPROM.put(30, scan_stop_freq);
        delay(50);
        bleep(600, 50, 1);
        break;

      case 3: // save the scan step size

        //Write the 2 bytes of the step size into the eeprom memory.
        EEPROM.put(32, scan_step_freq);
        delay(50);
        bleep(600, 50, 1);
        break;

      case 4: // save the scan step delay

        //Write the 2 bytes of the step delay into the eeprom memory.
        EEPROM.put(34, scan_step_delay);
        printLine2((char *)"Scan params set!");
        mode = MODE_NORMAL;
        delay(700);
        bleep(600, 50, 2);
        printLine2((char *)"--- SETTINGS ---");
        break;
    }
    param ++;
    firstrun = true;
  }

  else {
    mode = MODE_SCAN_PARAMS;
    firstrun = false;
    switch (param) {

      case 1: // display the lower scan limit

        itoa(scan_start_freq, b, DEC);
        strcpy(c, "lower ");
        strcat(c, b);
        strcat(c, " kHz  ");
        break;

      case 2: // display the upper scan limit

        itoa(scan_stop_freq, b, DEC);
        strcpy(c, "upper ");
        strcat(c, b);
        strcat(c, " kHz  ");
        break;

      case 3: // display the scan step size

        itoa(scan_step_freq, b, DEC);
        strcpy(c, "step ");
        strcat(c, b);
        strcat(c, " Hz      ");
        break;

      case 4: // display the scan step delay

        itoa(scan_step_delay, b, DEC);
        strcpy(c, "delay ");
        strcat(c, b);
        strcat(c, " ms     ");
        break;
    }
    printLine2(c);
  }

  shiftBase(); //align the current knob position with the current frequency
}


// function to read the position of the tuning knob at high precision (Allard, PE1NWL)
long knob_position() {
  long knob = 0;
  // the knob value normally ranges from 0 through 1023 (10 bit ADC)
  // in order to increase the precision by a factor 10, we need 10^2 = 100x oversampling

  for (int i = 0; i < 100; i++) {
    knob = knob + analogRead(ANALOG_TUNING) - 10; // take 100 readings from the ADC
  }
  knob = (knob + 5L) / 10L; // take the average of the 100 readings and multiply the result by 10
  //now the knob value ranges from -100 through 10130 (10x more precision)
  return knob;
}

/* Many BITX40's suffer from a strong birdie at 7199 kHz (LSB).
  This birdie may be eliminated by using a different VFO drive level in LSB mode.
  In USB mode, a high drive level may be needed to compensate for the attenuation of
  higher VFO frequencies.
  The drive level for each mode can be set in the SETTINGS menu
*/

void set_drive_level(int level) {
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
  long knob = knob_position(); // get the current tuning knob position
  baseTune = frequency - (50L * knob * TUNING_RANGE / 500);
  updateDisplay();
}

/**
   The Tuning mechansim of the Raduino works in a very innovative way. It uses a tuning potentiometer.
   The tuning potentiometer that a voltage between 0 and 5 volts at ANALOG_TUNING pin of the control connector.
   This is read as a value between 0 and 1000. By 100x oversampling ths range is expanded by a factor 10.
   Hence, the tuning pot gives you 10,000 steps from one end to the other end of its rotation. Each step is 50 Hz,
   thus giving maximum 500 Khz of tuning range. The tuning range is scaled down depending on the TUNING_RANGE value.
   The standard tuning range (for the standard 1-turn pot) is 50 Khz. But it is also possible to use a 10-turn pot
   to tune accross the entire 40m band. The desired TUNING_RANGE can be set via the Function Button in the SETTINGS menu.
   When the potentiometer is moved to either end of the range, the frequency starts automatically moving
   up or down in 10 Khz increments, so it is still possible to tune beyond the range set by TUNING_RANGE.
*/

void doTuning() {

  long knob = knob_position(); // get the current tuning knob position

  // the knob is fully on the low end, move down by 10 Khz and wait for 200 msec
  if (knob < -80 && frequency > LOWEST_FREQ) {
    baseTune = baseTune - 10000L;
    frequency = baseTune + (50L * knob * TUNING_RANGE / 500);
    updateDisplay();
    setFrequency(frequency);
    delay(200);
  }

  // the knob is full on the high end, move up by 10 Khz and wait for 200 msec
  else if (knob > 10120L && frequency < HIGHEST_FREQ) {
    baseTune = baseTune + 10000L;
    frequency = baseTune + (50L * knob * TUNING_RANGE / 500);
    setFrequency(frequency);
    updateDisplay();
    delay(200);
  }

  // the tuning knob is at neither extremities, tune the signals as usual ("flutter fix" by Jerry, KE7ER)
  else if (knob != old_knob) {
    static byte dir_knob;
    if ( (knob > old_knob) && ((dir_knob == 1) || ((knob - old_knob) > 5)) ||
         (knob < old_knob) && ((dir_knob == 0) || ((old_knob - knob) > 5)) )   {
      if (knob > old_knob) {
        dir_knob = 1;
        frequency = baseTune + (50L * (knob - 5) * TUNING_RANGE / 500);
      }
      else {
        dir_knob = 0;
        frequency = baseTune + (50L * knob * TUNING_RANGE / 500);
      }
      old_knob = knob;
      setFrequency(frequency);
      updateDisplay();
    }
  }

  if (vfoActive == VFO_A)
    vfoA = frequency;
  else
    vfoB = frequency;
}

byte raduino_version; //version identifier

void factory_settings() {
  printLine1((char *)"loading standard");
  printLine2((char *)"settings...     ");
  EEPROM.put(0, raduino_version); //version identifier
  delay(10);
  EEPROM.put(2, 0L); //corr factor (0 Hz)
  delay(10);
  EEPROM.put(4, 1500); //USB offset (1500 Hz)
  delay(10);
  EEPROM.put(6, 4); //VFO drive level in LSB mode (4 mA)
  delay(10);
  EEPROM.put(8, 8); //VFO drive level in USB mode (8 mA)
  delay(10);
  EEPROM.put(10, 50); //tuning range (50 kHz)
  delay(10);
  EEPROM.put(12, 800UL); //CW sidetone pitch (800 Hz)
  delay(10);
  EEPROM.put(16, 7125000UL); // VFO A frequency (7125 kHz)
  delay(10);
  EEPROM.put(20, 7125000UL); // VFO B frequency (7125 kHz)
  delay(10);
  EEPROM.put(24, 0); // mode A (LSB)
  delay(10);
  EEPROM.put(25, 0); // mode B (LSB)
  delay(10);
  EEPROM.put(25, 0); // vfoActive (VFO A)
  delay(10);
  EEPROM.put(27, 0); // RIT off
  delay(10);
  EEPROM.put(28, 7100); // scan_start_freq (7100 kHz)
  delay(10);
  EEPROM.put(30, 7150); // scan_stop_freq (7150 kHz)
  delay(10);
  EEPROM.put(32, 1000); // scan_step_freq (1000 Hz)
  delay(10);
  EEPROM.put(34, 500); // scan_step_delay (500 ms)
  delay(10);
  EEPROM.put(36, 350); // CW timout (350 ms)
  delay(10);
}

// save the VFO frequencies when they haven't changed more than 500Hz in the past 30 seconds
// (EEPROM.put writes the data only if it is different from the previous content of the eeprom location)

void save_frequency() {
  static long t3, diffA, diffB;
  static unsigned long old_vfoA, old_vfoB;
  if ((abs(diffA) < 500UL) && (abs(diffB) < 500UL)) {
    if (millis() - t3 > 30000) {
      EEPROM.put(16, old_vfoA); // save VFO_A frequency
      delay(10);
      EEPROM.put(20, old_vfoB); // save VFO_B frequency
      delay(10);
      t3 = millis();
    }
  }
  else
    t3 = millis();
  diffA = vfoA - old_vfoA;
  if (abs(diffA) > 500UL)
    old_vfoA = vfoA;
  diffB = vfoB - old_vfoB;
  if (abs(diffB) > 500UL)
    old_vfoB = vfoB;
}

void scan() {

  long knob = knob_position();
  if (abs(knob - old_knob) > 2 || (digitalRead(PTT_SENSE) == 1  && PTTsense_installed) || digitalRead(FBUTTON) == 0 || digitalRead(KEY) == 0) {
    //stop scanning
    mode = MODE_NORMAL;
    shiftBase();
    printLine2((char *)"                ");
  }

  else {
    // change frequency
    frequency = frequency + scan_step_freq;

    // test for upper limit of scan
    if (frequency > scan_stop_freq * 1000L)
      frequency = scan_start_freq * 1000L;

    setFrequency(frequency);
    updateDisplay();

    delay(scan_step_delay);
  }

  old_knob = knob_position();
}


/**
   setup is called on boot up
   It setups up the modes for various pins as inputs or outputs
   initiliaizes the Si5351 and sets various variables to initial state

   Just in case the LCD display doesn't work well, the debug log is dumped on the serial monitor
   Choose Serial Monitor from Arduino IDE's Tools menu to see the Serial.print messages
*/
void setup() {
  raduino_version = 14;
  strcpy (c, "Raduino v1.13   ");

  lcd.begin(16, 2);
  printBuff[0] = 0;

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
  delay(10);
  if ((digitalRead(CAL_BUTTON) == LOW) || (digitalRead(FBUTTON) == LOW) || (old_version != raduino_version)) {
    factory_settings();
  }

  // check if PTT sense line is installed
  if (digitalRead(PTT_SENSE) == LOW)
    PTTsense_installed = 1; //yes it's installed
  else
    PTTsense_installed = 0; //no it's not installed

  printLine1(c);
  printLine2((char *)"             ");
  delay(1000);

  //retrieve user settings from EEPROM
  EEPROM.get(2, cal);
  //Serial.println(cal);
  delay(10);

  EEPROM.get(4, USB_OFFSET);
  //Serial.println(USB_OFFSET);
  delay(10);

  //display warning message when calibration data was erased
  if ((cal == 0) && (USB_OFFSET == 1500))
    printLine2((char *)"uncalibrated!");

  EEPROM.get(6, LSBdrive);
  //Serial.println(LSBdrive);
  delay(10);

  EEPROM.get(8, USBdrive);
  //Serial.println(USBdrive);
  delay(10);

  EEPROM.get(10, TUNING_RANGE);
  //Serial.println(TUNING_RANGE);
  delay(10);

  EEPROM.get(12, CW_OFFSET);
  //Serial.println(CW_OFFSET);
  delay(10);

  EEPROM.get(16, vfoA);
  //Serial.println(vfoA);
  delay(10);

  EEPROM.get(20, vfoB);
  //Serial.println(vfoB);
  delay(10);

  EEPROM.get(24, mode_A);
  //Serial.println(mode_A);
  delay(10);

  EEPROM.get(25, mode_B);
  //Serial.println(mode_B);
  delay(10);

  EEPROM.get(26, vfoActive);
  //Serial.println(vfoActive);
  delay(10);

  EEPROM.get(27, ritOn);
  //Serial.println(vfoActive);
  delay(10);

  EEPROM.get(28, scan_start_freq);
  //Serial.println(scan_start_freq);
  delay(10);

  EEPROM.get(30, scan_stop_freq);
  //Serial.println(scan_stop_freq);
  delay(10);

  EEPROM.get(32, scan_step_freq);
  //Serial.println(scan_step_freq);
  delay(10);

  EEPROM.get(34, scan_step_delay);
  //Serial.println(scan_step_delay);
  delay(10);

  EEPROM.get(36, CW_TIMEOUT);
  //Serial.println(CW_TIMEOUT);
  delay(10);

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

  if (vfoActive == VFO_A) {
    frequency = vfoA;
    isUSB = mode_A;
  }
  else {
    frequency = vfoB;
    isUSB = mode_B;
  }

  if (isUSB)
    setUSB();
  else
    setLSB();

  shiftBase(); //align the current knob position with the current frequency

  //If no FButton is installed, and you still want to use custom tuning range settings,
  //uncomment the following line and adapt the value as desired:

  //TUNING_RANGE = 50;    // tuning range (in kHz) of the tuning pot

  //recommended tuning range for a 1-turn pot: 50kHz, for a 10-turn pot: 200kHz

  bleep(CW_OFFSET, 60, 3);
  bleep(CW_OFFSET, 180, 1);
}

void loop() {
  switch (mode) {
    case 0: // for backward compatibility: execute calibration when CAL button is pressed
      if (digitalRead(CAL_BUTTON) == LOW) {
        mode = MODE_CALIBRATE;
        calbutton_prev = LOW;
        factory_settings();
        printLine1((char *)"Calibrating: Set");
        printLine2((char *)"to zerobeat.    ");
        delay(2000);
        return;
      }
      break;
    case 1: //LSB calibration
      calibrate();
      return;
    case 2: //set VFO drive level
      VFOdrive();
      return;
    case 3: // set tuning range
      set_tune_range();
      return;
    case 4: // set CW offset
      set_CWoffset();
      return;
    case 5: // scan mode
      scan();
      return;
    case 6: // set scan paramaters
      scan_params();
      return;
  }

  checkCW();
  if (PTTsense_installed == 1)
    checkTX();
  save_frequency();
  checkButton();
  if (!(digitalRead(PTT_SENSE) == 1 && PTTsense_installed == 1)) // tuning is disabled during TX (only when PTT sense line is installed)
    doTuning();
}
