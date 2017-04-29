/**
   Raduino_v1.09 for BITX40 - Allard Munters PE1NWL (pe1nwl@gooddx.net)

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
    The main chip which generates upto three oscillators of various frequencies in the
    Raduino is the Si5351a. To learn more about Si5351a you can download the datasheet
    from www.silabs.com although, strictly speaking it is not a requirment to understand this code.
    Instead, you can look up the Si5351 library written by Jason Mildrum, NT7S. You can download and
    install it from https://github.com/etherkit/Si5351Arduino to complile this file.
    NOTE: This sketch is based on version V2 of the Si5351 library. It will not compile with V1!

    The Wire.h library is used to talk to the Si5351 and we also declare an instance of
    Si5351 object to control the clocks.
*/
#include <Wire.h>
#include <si5351.h> // https://github.com/etherkit/Si5351Arduino/releases/tag/v2.0.1
Si5351 si5351;
/**
   The Raduino board is the size of a standard 16x2 LCD panel. It has three connectors:

   First, is an 8 pin connector that provides +5v, GND and six analog input pins that can also be
   configured to be used as digital input or output pins. These are referred to as A0,A1,A2,
   A3,A6 and A7 pins. The A4 and A5 pins are missing from this connector as they are used to
   talk to the Si5351 over I2C protocol.

    A0     A1   A2   A3    +5v    GND   A6   A7
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
int count = 0;

/**
   We need to carefully pick assignment of pin for various purposes.
   There are two sets of completely programmable pins on the Raduino.
   First, on the top of the board, in line with the LCD connector is an 8-pin connector
   that is largely meant for analog inputs and front-panel control. It has a regulated 5v output,
   ground and six pins. Each of these six pins can be individually programmed
   either as an analog input, a digital input or a digital output.
   The pins are assigned as follows:
          A0,   A1,  A2,  A3,   +5v,   GND,  A6,  A7
        BLACK BROWN RED ORANGE YELLW  GREEN  BLUEVIOLET
        (while holding the board up so that back of the board faces you)

   Though, this can be assigned anyway, for this application of the Arduino, we will make the following
   assignment
   A0 (digital input) for sensing the PTT. Connect to the output of U3 (LM7805) of the BITX40.
      This way the A0 input will see 0V (LOW) when PTT is not pressed, +5V (HIGH) when PTT is pressed.
   A1 (digital input) is to implement a keyer, it is reserved and not yet implemented
   A2 (digital input) can be used for calibration by grounding this line (not required when you have the Function Button at A3)
   A3 (digital input) is connected to a push button that can momentarily ground this line. This Function Button will be used to switch between different modes, etc.
   A4 (already in use for talking to the SI5351)
   A5 (already in use for talking to the SI5351)
   A6 (analog input) is not currently used
   A7 (analog input) is connected to a center pin of good quality 100K or 10K linear potentiometer with the two other ends connected to
       ground and +5v lines available on the connector. This implments the tuning mechanism.
*/

#define ANALOG_KEYER (A1)
#define CAL_BUTTON (A2)
#define FBUTTON (A3)
#define PTT (A0)
#define ANALOG_TUNING (A7)

/**
    The second set of 16 pins on the bottom connector are have the three clock outputs and the digital lines to control the rig.
    This assignment is as follows :
      Pin   1   2    3    4    5    6    7    8    9    10   11   12   13   14   15   16
           +5V +5V CLK0  GND  GND  CLK1 GND  GND  CLK2  GND  D2   D3   D4   D5   D6   D7
    These too are flexible with what you may do with them, for the Raduino, we use them to :
    - TX_RX line : Switches between Transmit and Receive after sensing the PTT or the morse keyer
    - CW_KEY line : turns on the carrier for CW
    These are not used at the moment.
*/

#define TX_RX (7)
#define CW_TONE (6)
#define CW_KEY (5)
#define TX_LPF_SEL (4)

/**
    The raduino has a number of timing parameters, all specified in milliseconds
    CW_TIMEOUT : how many milliseconds between consecutive keyup and keydowns before switching back to receive?
*/

#define CW_TIMEOUT (600L) // in milliseconds, this is the parameter that determines how long the tx will hold between cw key downs

/**
   The Raduino supports two VFOs : A and B and receiver incremental tuning (RIT).
   we define a variables to hold the frequency of the two VFOs, RITs
   the rit offset as well as status of the RIT

   To use this facility, wire up push button on A3 line of the control connector
*/
#define VFO_A 0
#define VFO_B 1
unsigned long vfoA, vfoB, sideTone = 800;

/**
   In USB mode we need to apply some frequency offset, so that zerobeat in USB is same as in LSB
*/
int USB_OFFSET; // USB offset in Hz

/**
   We can set the VFO drive to a certain level (2,4,6,8 mA)
*/
int LSBdrive;
int USBdrive;

/**
   Raduino needs to keep track of current state of the transceiver. These are a few variables that do it
*/
byte ritOn, vfoActive, isUSB, mode_A, mode_B;
byte inTx = 0;
byte keyDown = 0;
unsigned long cwTimeout = 0;
byte txFilter = 0;

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

unsigned long bfo_freq = 11998000UL;
int old_knob = 0;

#define CW_OFFSET (800L)

#define LOWEST_FREQ  (6995000L) // absolute minimum frequency (Hz)
#define HIGHEST_FREQ (7500000L) //  absolute maximum frequency (Hz)

unsigned long frequency;

/**
   The raduino has multiple modes:
*/
#define MODE_NORMAL (0)  // normal operation
#define MODE_CALIBRATE (1) // calibrate VFO frequency in LSB mode
#define MODE_USBOFFSET (2) // adjust frequency offset in USB mode
#define MODE_DRIVELEVEL (3) // set VFO drive level
#define MODE_TUNERANGE (4) // set upper and lower limits of tuning pot
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
    count++;
  }
}

void printLine2(char *c) {
  lcd.setCursor(0, 1);
  lcd.print(c);
}

/**
   Building upon the previous  two functions,
   update Display paints the first line as per current state of the radio

   At present, we are not using the second line. YOu could add a CW decoder or SWR/Signal strength
   indicator
*/

void updateDisplay() {
  // improved by Jack Purdum, W8TEE
  // replaced fsprint commmands by str commands for code size reduction

  memset(c, 0, sizeof(c));
  memset(b, 0, sizeof(b));

  ltoa(frequency, b, DEC);

  if (VFO_A == vfoActive)
    strcpy(c, "A:");
  else
    strcpy(c, "B:");

  if (frequency < 10000000L) {
    c[2] = '0';
    c[3] = b[0];
    strcat(c, ".");
    strncat(c, &b[1], 4);
  } else {
    strncat(c, b, 2);
    strcat(c, ".");
    strncat(c, &b[2], 4);
  }

  if (isUSB)
    strcat(c, " USB");
  else
    strcat(c, " LSB");

  if (inTx)
    strcat(c, " TX");
  else if (ritOn)
    strcat(c, " +R");
  else
    strcat(c, "   ");

  printLine1(c);
}


bool calbutton_prev = HIGH;
long cal;

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

   Calibration is a multiplying factor that is applied to the VFO frequency by the Si5351 library.
   We store it in the EEPROM's first four bytes and read it in setup() when the Radiuno is powered up
*/

void calibrate() {

  long cal_old, vfo;
  static long shift;

  if (mode != MODE_CALIBRATE) {
    setLSB();

    //fetch the current correction factor from EEPROM
    EEPROM.get(0, cal_old);
    vfo = bfo_freq - frequency;
    vfo = (vfo + 5000L) / 10000L;

    if (cal_old > 0)
      cal_old = ((cal_old * vfo + 500000L) / 1000000L);
    else
      cal_old = ((cal_old * vfo - 500000L) / 1000000L);
    cal_old = -cal_old * 10L;

    shift = cal_old - (analogRead(ANALOG_TUNING) * 10) + 5000;
  }

  // if Fbutton is pressed again, or if the CALbutton is released, we save the setting

  if ((digitalRead(FBUTTON) == LOW) || ((digitalRead(CAL_BUTTON) == HIGH) && (calbutton_prev == LOW))) {

    mode = MODE_NORMAL;
    printLine2((char *)"LSB Calibrated!  ");

    //Write the 4 bytes of the correction factor into the eeprom memory.
    EEPROM.put(0, cal);
    delay(700);

    printLine2((char *)"--- SETTINGS ---");

    // return to the original frequency that the VFO was set to before Calibration was executed
    long knob = knob_position(); // get the current tuning knob position
    baseTune = frequency - (50L * knob * TUNING_RANGE / 500);
    updateDisplay();
    calbutton_prev = HIGH;
  }
  else {
    // while the calibration is in progress (CAL_BUTTON is held down), keep tweaking the
    // frequency as read out by the knob, display the change in the second line
    mode = MODE_CALIBRATE;

    // The tuning knob gives readings from 0 to 1000
    // Each step is taken as 10 Hz and the mid setting of the knob is taken as zero

    cal = (analogRead(ANALOG_TUNING) * 10) - 5000 + shift;

    si5351.set_freq((bfo_freq - frequency) * 100LL, SI5351_CLK2);

    ltoa(cal, b, DEC);
    strcpy(c, "offset:");
    strcat(c, b);
    strcat(c, " Hz    ");
    printLine2(c);

    //calculate the correction factor in ppb and apply it
    cal = (cal * -1000000000LL) / (bfo_freq - frequency) ;
    si5351.set_correction(cal);
  }
}


/**
   Even when LSB is calibrated, USB may still not be zero beat.
   USB calibration sets the accurate readout of the tuned frequency by applying an extra offset in USB mode only
   To calibrate, follow these steps:
   1. First make sure the LSB is calibrated correctly
   2. Tune in a USB signal that is at a known frequency.
   3. Now, set the display to show the correct frequency,
      the signal will no longer be tuned up properly
   4. Use the "USB calibrate" option in the "Settings" menu
   5. tune in the signal until it sounds proper.
   6. Press the FButton again.
   In step 5, when we say 'sounds proper' then, for a CW signal/carrier it means zero-beat
   and for USB it is the most natural sounding setting.

   We store the USBoffset in the EEPROM and read it in setup() when the Radiuno is powered up
*/

void USBoffset() {

  static int USB_OFFSET_old, shift;

  if (mode != MODE_USBOFFSET) {
    setUSB();
    //fetch the current USB offset value from EEPROM
    EEPROM.get(4, USB_OFFSET_old);
    shift = USB_OFFSET_old - (analogRead(ANALOG_TUNING) * 10) + 5000;
  }

  // The tuning knob gives readings from 0 to 1000
  // Each step is taken as 10 Hz and the mid setting of the knob is taken as zero
  USB_OFFSET = (analogRead(ANALOG_TUNING) * 10) - 5000 + shift;

  // if Fbutton is pressed again, we save the setting
  if (digitalRead(FBUTTON) == LOW) {
    mode = MODE_NORMAL;
    printLine2((char *)"USB Calibrated! ");

    //Write the 2 bytes of the USB offset into the eeprom memory.
    EEPROM.put(4, USB_OFFSET);
    delay(700);
    printLine2((char *)"--- SETTINGS ---");

    // return to the original frequency that the VFO was set to before USBoffset adjustment
    long knob = knob_position(); // get the current tuning knob position
    baseTune = frequency - (50L * knob * TUNING_RANGE / 500);
    setLSB();
  }
  else {
    // while USB offset adjustment is in progress, keep tweaking the
    // frequency as read out by the knob, display the change in the second line
    mode = MODE_USBOFFSET;
    si5351.set_freq((bfo_freq + frequency - USB_OFFSET) * 100LL, SI5351_CLK2);
    itoa(USB_OFFSET, b, DEC);
    strcpy(c, "offset:");
    strcat(c, b);
    strcat(c, " Hz    ");
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
*/

void setFrequency(unsigned long f) {

  if (isUSB) {
    si5351.set_freq((bfo_freq + f - USB_OFFSET) * 100ULL, SI5351_CLK2);
  }
  else {
    si5351.set_freq((bfo_freq - f) * 100ULL, SI5351_CLK2);
  }
  frequency = f;
}

/**
   The Checkt TX toggles the T/R line. The current BITX wiring up doesn't use this
   but if you would like to make use of RIT, etc, You must wireup an NPN transistor to to the PTT line
   as follows :
   emitter to ground,
   base to TX_RX line through a 4.7K resistr(as defined at the top of this source file)
   collecter to the PTT line
   Now, connect the PTT to the control connector's PTT line (see the definitions on the top)
   Yeah, surprise! we have CW supported on the Raduino
*/

void checkTX() {
  // We don't check for ptt when transmitting cw
  // as long as the cwTimeout is non-zero, we will continue to hold the
  // radio in transmit mode
  if (cwTimeout > 0)
    return;

  if (digitalRead(PTT) == 1 && inTx == 0) {
    // go in transmit mode
    inTx = 1;
    digitalWrite(TX_RX, 1);
    updateDisplay();
    if (ritOn) { // when RIT is on, swap the VFO's
      swapVFOs();
    }
    printLine2((char *)"                ");
  }

  if (digitalRead(PTT) == 0 && inTx == 1) {
    //go in receive mode
    inTx = 0;
    digitalWrite(TX_RX, 0);
    updateDisplay();
    if (ritOn) { // when RIT is on, swap the VFO's back to original state
      swapVFOs();
    }
    printLine2((char *)"                ");
  }
}

/* CW is generated by injecting the side-tone into the mic line.
   Watch http://bitxhacks.blogspot.com for the CW hack
   nonzero cwTimeout denotes that we are in cw transmit mode.

   This function is called repeatedly from the main loop() hence, it branches
   each time to do a different thing

   There are three variables that track the CW mode
   inTX     : true when the radio is in transmit mode
   keyDown  : true when the CW is keyed down, you maybe in transmit mode (inTX true)
              and yet between dots and dashes and hence keyDown could be true or false
   cwTimeout: Figures out how long to wait between dots and dashes before putting
              the radio back in receive mode
*/

void checkCW() {

  if (keyDown == 0 && analogRead(ANALOG_KEYER) < 50) {
    //switch to transmit mode if we are not already in it
    if (inTx == 0) {
      digitalWrite(TX_RX, 1);
      //give the relays a few ms to settle the T/R relays
      delay(50);
    }
    inTx = 1;
    keyDown = 1;
    tone(CW_TONE, sideTone);
    updateDisplay();
  }

  //reset the timer as long as the key is down
  if (keyDown == 1) {
    cwTimeout = CW_TIMEOUT + millis();
  }

  //if we have a keyup
  if (keyDown == 1 && analogRead(ANALOG_KEYER) > 150) {
    keyDown = 0;
    noTone(CW_TONE);
    cwTimeout = millis() + CW_TIMEOUT;
  }

  //if we are in cw-mode and have a keyuup for a longish time
  if (cwTimeout > 0 && inTx == 1 && cwTimeout < millis()) {
    //move the radio back to receive
    digitalWrite(TX_RX, 0);
    inTx = 0;
    cwTimeout = 0;
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

/**
   The Function Button is used for several functions
   NORMAL menu (normal operation):
   1 short press (<0.5 sec): swap VFO A/B
   2 short presses: toggle RIT on/off
   3 short presses: toggle LSB/USB
   long press (>2 Sec): VFO A=B
   VERY long press (>5 sec): go to SETTINGS menu
   SETTINGS menu:
   1 short press: LSB calibration
   2 short presses: USB calibration
   3 short presses: VFO drive level in LSB mode
   4 short presses: VFO drive level in USB mode
   5 short presses: Tuning range upper&lower limit settings
   long press: exit SETTINGS menu - go back to NORMAL menu
*/
void checkButton() {

  static int clicks, action;
  static long t1, t2;
  static byte pressed = 0;
  static byte SETTINGSmenu = 0; // whether or not we are in the SETTINGS menu

  if (!btnDown()) {
    t2 = millis() - t1; //time elapsed since last button press
    if (pressed == 1) {
      if (!SETTINGSmenu && t2 > 5000) { // detect VERY long press to go to SETTINGS menu
        SETTINGSmenu = 1;
        printLine2((char *)"--- SETTINGS ---");
        clicks = 10;
      }
      else if (!SETTINGSmenu && t2 > 1200) { //detect long press to reset the VFO's
        resetVFOs();
        delay(700);
        printLine2((char *)"                ");
        clicks = 0;
      }
      else if (SETTINGSmenu && t2 > 1200) { //detect long press to exit the SETTINGS menu
        SETTINGSmenu = 0;
        clicks = 0;
        printLine2((char *)" --- NORMAL --- ");
        delay(700);
        printLine2((char *)"                ");
      }
    }
    if (t2 > 500) { // max time between button clicks
      action = clicks;
      if (SETTINGSmenu) {
        clicks = 10;
      }
      else {
        clicks = 0;
      }
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
        action = 0;
        clicks++;
        if (clicks > 15) {
          clicks = 11;
        }
        if (clicks > 3 && clicks < 10) {
          clicks = 1;
        }
        switch (clicks) {
          //Normal menu options
          case 1:
            printLine2((char *)"swap VFO's      ");
            break;
          case 2:
            printLine2((char *)"toggle RIT      ");
            break;
          case 3:
            printLine2((char *)"toggle LSB/USB  ");
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
            printLine2((char *)"set tuning range");
            break;
        }
      }
      else if ((millis() - t1) > 600)
        printLine2((char *)"                ");
    }
  }
  switch (action) {
    // Normal menu options
    case 1:
      swapVFOs();
      EEPROM.put(22, vfoActive);
      delay(700);
      printLine2((char *)"                ");
      break;
    case 2:
      toggleRIT();
      delay(700);
      printLine2((char *)"                ");
      break;
    case 3:
      toggleMode();
      delay(700);
      printLine2((char *)"                ");
      break;

    // SETTINGS MENU options
    case 11:
      calibrate();
      break;
    case 12:
      USBoffset();
      break;
    case 13:
      setLSB();
      VFOdrive();
      break;
    case 14:
      setUSB();
      VFOdrive();
      break;
    case 15:
      set_tune_range();
      break;
  }
}

void swapVFOs() {
  if (vfoActive == VFO_B) {
    vfoActive = VFO_A;
    vfoB = frequency;
    frequency = vfoA;
    if (mode_A) {
      setUSB();
    }
    else {
      setLSB();
    }
  }
  else {
    vfoActive = VFO_B;
    vfoA = frequency;
    frequency = vfoB;
    if (mode_B) {
      setUSB();
    }
    else {
      setLSB();
    }
  }
  setFrequency(frequency);
  long knob = knob_position(); // get the current tuning knob position
  baseTune = frequency - (50L * knob * TUNING_RANGE / 500);
  updateDisplay();
  printLine2((char *)"VFO's swapped!  ");
}

void toggleRIT() {
  if (ritOn) {
    ritOn = 0;
    printLine2((char *)"RIT is now OFF! ");
  }
  else {
    ritOn = 1;
    printLine2((char *)"RIT is now ON!  ");
  }
  EEPROM.put(23, ritOn);
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
    EEPROM.put(20, mode_A);
  }
  else {
    mode_B = 1;
    EEPROM.put(21, mode_B);
  }
}

void setLSB() {
  isUSB = 0;
  set_drive_level(LSBdrive);
  setFrequency(frequency);
  updateDisplay();
  if (vfoActive == VFO_A) {
    mode_A = 0;
    EEPROM.put(20, mode_A);
  }
  else {
    mode_B = 0;
    EEPROM.put(21, mode_B);
  }
}

// resetting the VFO's will set both VFO's to the current frequency and mode
void resetVFOs() {
  printLine2((char *)"VFO A=B !       ");
  vfoA = vfoB = frequency;
  mode_A = mode_B = isUSB;
  EEPROM.put(20, mode_A);
  EEPROM.put(21, mode_B);
  updateDisplay();
  return;
}

void VFOdrive() {
  static int drive;
  static int drive_old;
  static int shift;

  if (mode != MODE_DRIVELEVEL) {
    if (isUSB)
      drive_old = USBdrive / 2 - 1;
    else
      drive_old = LSBdrive / 2 - 1;

    shift = analogRead(ANALOG_TUNING);
  }

  //generate drive level values 2,4,6,8 from tuning pot
  drive = 2 * (((int((analogRead(ANALOG_TUNING) - shift) / 50) + drive_old) & 3) + 1);

  // if Fbutton is pressed again, we save the setting

  if (digitalRead(FBUTTON) == LOW) {
    mode = MODE_NORMAL;
    printLine2((char *)"drive level set!");

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

    printLine2((char *)"--- SETTINGS ---");

    long knob = knob_position(); // get the current tuning knob position
    baseTune = frequency - (50L * knob * TUNING_RANGE / 500);
  }
  else {
    // while the drive level adjustment is in progress, keep tweaking the
    // drive level as read out by the knob and display it in the second line
    mode = MODE_DRIVELEVEL;
    set_drive_level(drive);

    itoa(drive, b, DEC);
    strcpy(c, "drive level: ");
    strcat(c, b);
    strcat(c, "mA");
    printLine2(c);
  }
}

// this function allows the user to set the tuning range dependinn on the type of potentiometer
// for a standard 1-turn pot, a range of 50 KHz is recommended
// for a 10-turn pot, a tuning range of 200 kHz is recommended
void set_tune_range() {

  //generate values 10-520 from the tuning pot
  TUNING_RANGE = 10 + 10 * int(analogRead(ANALOG_TUNING) / 20);

  // if Fbutton is pressed again, we save the setting
  if (digitalRead(FBUTTON) == LOW) {
    //Write the 2 bytes of the tuning range into the eeprom memory.
    EEPROM.put(10, TUNING_RANGE);
    printLine2((char *)"tune range set! ");
    mode = MODE_NORMAL;
    delay(700);
    printLine2((char *)"--- SETTINGS ---");
  }
  
  else {
    mode = MODE_TUNERANGE;
    itoa(TUNING_RANGE, b, DEC);
    strcpy(c, "range: ");
    strcat(c, b);
    strcat(c, " kHz  ");
    printLine2(c);
  }
}

// function to read the position of the tuning knob
long knob_position() {
  long knob = 0;
  // the knob value normally ranges from 0 through 1023 (10 bit ADC)
  // in order to expand the range by a factor 10, we need 10^2 = 100x oversampling

  for (int i = 0; i < 100; i++) {
    knob = knob + analogRead(ANALOG_TUNING) - 10; // take 100 readings from the ADC
  }
  knob = (knob + 5L) / 10L; // take the average of the 100 readings and multiply the result by 10
  //now the knob value ranges from -100 through 10130
  return knob;
}

// Many BITX40's have a strong birdie at 7199 kHz (LSB).
// This birdie may be eliminated by using a different VFO drive level in LSB mode.
// In USB mode, a high drive level may be needed to compensate for the attenuation of
// higher VFO frequencies
// The drive level for each mode can be set in the SETTINGS menu

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
   The Tuning mechansim of the Raduino works in a very innovative way. It uses a tuning potentiometer.
   The tuning potentiometer that a voltage between 0 and 5 volts at ANALOG_TUNING pin of the control connector.
   This is read as a value between 0 and 1000. By 100x oversampling ths range is expanded by a factor 10.
   Hence, the tuning pot gives you 10,000 steps from one end to the other end of its rotation. Each step is 50 Hz,
   thus giving maximum 500 Khz of tuning range. The tuning range is scaled down depending on the limit settings.
   The standard tuning range (for the standard 1-turn pot) is 50 Khz. But it is also possible to use a 10-turn pot
   to tune accross the entire 40m band. In that case you need to change the values for TUNING_RANGE and baseTune.
   When the potentiometer is moved to either end of the range, the frequency starts automatically moving
   up or down in 10 Khz increments
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
      } else {
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

  EEPROM.put(0, 0L); //corr factor (0 Hz)
  delay(10);
  EEPROM.put(4, 1500); //USB offset (1500 Hz)
  delay(10);
  EEPROM.put(6, 4); //VFO drive level in LSB mode (4 mA)
  delay(10);
  EEPROM.put(8, 8); //VFO drive level in USB mode (8 mA)
  delay(10);
  EEPROM.put(10, 50); //tuning range (50 kHz)
  delay(10);
  EEPROM.put(12, 7125000UL); // VFO A frequency (7125 kHz)
  delay(10);
  EEPROM.put(16, 7125000UL); // VFO B frequency (7125 kHz)
  delay(10);
  EEPROM.put(20, 0); // mode A (LSB)
  delay(10);
  EEPROM.put(21, 0); // mode B (LSB)
  delay(10);
  EEPROM.put(22, 0); // vfoActive (VFO A)
  delay(10);
  EEPROM.put(23, 0); // RIT off
  delay(10);
  EEPROM.put(24, raduino_version); //version identifier
  delay(2000);
}

// save the VFO frequencies when they didn't change for more than 30 seconds
// (EEPROM.put writes the data only if it is different from the previous content of the eeprom location)

void save_frequency() {
  static long t3;
  static unsigned long old_vfoA, old_vfoB;
  if ((vfoA == old_vfoA) && (vfoB == old_vfoB)) {
    if (millis() - t3 > 30000) {
      EEPROM.put(12, vfoA); // save VFO_A frequency
      delay(10);
      EEPROM.put(16, vfoB); // save VFO_B frequency
      delay(10);
      t3 = millis();
    }
  }
  else {
    t3 = millis();
  }
  old_vfoA = vfoA;
  old_vfoB = vfoB;
}

/**
   setup is called on boot up
   It setups up the modes for various pins as inputs or outputs
   initiliaizes the Si5351 and sets various variables to initial state

   Just in case the LCD display doesn't work well, the debug log is dumped on the serial monitor
   Choose Serial Monitor from Arduino IDE's Tools menu to see the Serial.print messages
*/
void setup() {
  raduino_version = 10;
  strcpy (c, "Raduino v1.09   ");

  lcd.begin(16, 2);
  printBuff[0] = 0;

  // Start serial and initialize the Si5351
  Serial.begin(9600);
  analogReference(DEFAULT);
  Serial.println("*Raduino booting up");

  //configure the function button to use the internal pull-up
  pinMode(FBUTTON, INPUT_PULLUP);
  pinMode(PTT, INPUT);
  pinMode(CAL_BUTTON, INPUT_PULLUP);
  pinMode(CW_KEY, OUTPUT);
  pinMode(CW_TONE, OUTPUT);
  digitalWrite(CW_KEY, 0);
  digitalWrite(CW_TONE, 0);
  digitalWrite(TX_RX, 0);

  // when Fbutton or CALbutton is pressed during power up,
  // or after a version update,
  // then all settings will be restored to the standard "factory" values
  byte old_version;
  EEPROM.get(24, old_version); // previous sketch version
  delay(10);
  if ((digitalRead(CAL_BUTTON) == LOW) || (digitalRead(FBUTTON) == LOW) || (old_version != raduino_version)) {
    factory_settings();
  }

  printLine1(c);
  printLine2((char *)"             ");
  delay(1000);

  //retrieve user settings from EEPROM
  EEPROM.get(0, cal);
  //Serial.println(cal);
  delay(10);

  EEPROM.get(4, USB_OFFSET);
  //Serial.println(USB_OFFSET);
  delay(10);

  //display warning message when calibration data was erased
  if ((cal == 0) && (USB_OFFSET == 1500)) {
    printLine2((char *)"uncalibrated!");
  }

  EEPROM.get(6, LSBdrive);
  //Serial.println(LSBdrive);
  delay(10);

  EEPROM.get(8, USBdrive);
  //Serial.println(USBdrive);
  delay(10);

  EEPROM.get(10, TUNING_RANGE);
  //Serial.println(TUNING_RANGE);
  delay(10);

  EEPROM.get(12, vfoA);
  //Serial.println(vfoA);
  delay(10);

  EEPROM.get(16, vfoB);
  //Serial.println(vfoB);
  delay(10);

  EEPROM.get(20, mode_A);
  //Serial.println(mode_A);
  delay(10);

  EEPROM.get(21, mode_B);
  //Serial.println(mode_B);
  delay(10);

  EEPROM.get(22, vfoActive);
  //Serial.println(vfoActive);
  delay(10);

  EEPROM.get(23, ritOn);
  //Serial.println(vfoActive);
  delay(10);

  EEPROM.get(24, raduino_version);
  //Serial.println(raduino_version);
  delay(10);

  //initialize the SI5351 and apply the correction factor
  si5351.init(SI5351_CRYSTAL_LOAD_8PF, 25000000L, cal);
  Serial.println("*Initiliazed Si5351\n");
  si5351.set_pll(SI5351_PLL_FIXED, SI5351_PLLA);
  si5351.set_pll(SI5351_PLL_FIXED, SI5351_PLLB);
  Serial.println("*Fixed PLL\n");
  si5351.output_enable(SI5351_CLK0, 0);
  si5351.output_enable(SI5351_CLK1, 0);
  si5351.output_enable(SI5351_CLK2, 1);
  Serial.println("*Output enabled PLL\n");
  si5351.set_freq(500000000L , SI5351_CLK2);
  Serial.println("*Si5350 ON\n");
  mode = MODE_NORMAL;

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

  setFrequency(frequency);
  long knob = knob_position(); // get the current tuning knob position
  baseTune = frequency - (50L * knob * TUNING_RANGE / 500);
  updateDisplay();

  //If no FButton is installed, and you still want to use custom tuning range settings,
  //uncomment the following 2 lines and adapt the values to your liking:
  //TUNING_RANGE = 50;    // tuning range (in kHz) of the tuning pot
  //baseTune = 7100000UL; // frequency (Hz) when tuning pot is at minimum position
}

void loop() {
  switch (mode) {
    case 0: // for backward compatibility: execute calibration when CAL button is pressed
      if (digitalRead(CAL_BUTTON) == LOW) {
        mode = MODE_CALIBRATE;
        calbutton_prev = LOW;
        factory_settings();
        si5351.set_correction(0);
        printLine1((char *)"Calibrating: Set");
        printLine2((char *)"to zerobeat.    ");
        delay(2000);
        return;
      }
      break;
    case 1: //LSB calibration
      calibrate();
      delay(50);
      return;
    case 2: //USB calibration
      USBoffset();
      delay(50);
      return;
    case 3: //set VFO drive level
      VFOdrive();
      delay(50);
      return;
    case 4: // set tuning range
      set_tune_range();
      delay(50);
      return;
  }
  /*
    checkCW(); */
  checkTX();
  save_frequency();
  checkButton();
  doTuning();
  delay(50);
}
