/**
   Raduino_diag for BITX40 - Allard Munters PE1NWL (pe1nwl@gooddx.net)

   This source file is under General Public License version 3.
   
   This sketch tests some basic hardware functions:
   tuning pot reading
   Function Button/ SPOT button switches
   PTT switch
   capacitive touch sensors
   
   Results are shown on the LCD display
   additional messages are output to the serial monitor
   (in the IDE, select tools => serial monitor)
   
*/

#define DEBUG_BOOT                     //uncomment this to print debug messages to the serial monitor
//#define DEBUG_RUN                    //uncomment this to print debug messages to the serial monitor

// *** USER PARAMETERS ***

// tuning range parameters
#define MIN_FREQ 7000000UL        // absolute minimum tuning frequency in Hz
#define MAX_FREQ 7300000UL        // absolute maximum tuning frequency in Hz
#define TUNING_POT_SPAN 50        // tuning pot span in kHz [accepted range 10-500 kHz]
// recommended pot span for a 1-turn pot: 50kHz, for a 10-turn pot: 100 to 200kHz
// recommended pot span when radio is used mainly for CW: 10 to 25 kHz

// USB/LSB parameters
#define CAL_VALUE 0               // VFO calibration value
#define OFFSET_USB 1500           // USB offset in Hz [accepted range -10000Hz to 10000Hz]
#define VFO_DRIVE_LSB 4           // VFO drive level in LSB mode in mA [accepted values 2,4,6,8 mA]
#define VFO_DRIVE_USB 8           // VFO drive level in USB mode in mA [accepted values 2,4,6,8 mA]

// CW parameters
#define CW_SHIFT 800              // RX shift in CW mode in Hz, equal to sidetone pitch [accepted range 200-1200 Hz]
#define SEMI_QSK true             // whether we use semi-QSK (true) or manual PTT (false)
#define CW_TIMEOUT 350            // time delay in ms before radio goes back to receive [accepted range 10-1000 ms]

// CW keyer parameters
#define CW_KEY_TYPE 0             // type of CW-key (0:straight, 1:paddle, 2:rev. paddle, 3:bug, 4:rev. bug)
#define CW_SPEED 16               // CW keyer speed in words per minute [accepted range 1-50 WPM]
#define AUTOSPACE false           // whether or not auto-space is enabled [accepted values: true or false]
#define DIT_DELAY 5               // debounce delay (ms) for DIT contact (affects the DIT paddle sensitivity)
#define DAH_DELAY 15              // debounce delay (ms) for DAH contact (affects the DAH paddle sensitivity)

// Capacitive touch keyer
#define CAP_SENSITIVITY 0         // capacitive touch keyer sensitivity 0 (OFF) [accepted range 0-25]

// frequency scanning parameters
#define SCAN_START 7100           // Scan start frequency in kHz [accepted range MIN_FREQ - MAX_FREQ, see above]
#define SCAN_STOP 7150            // Scan stop frequency in kHz [accepted range SCAN_START - MAX_FREQ, see above]
#define SCAN_STEP 1000            // Scan step size in Hz [accepted range 50Hz to 10000Hz]
#define SCAN_STEP_DELAY 500       // Scan step delay in ms [accepted range 0-2000 ms]

// Function Button
#define CLICK_DELAY 750           // max time (in ms) between function button clicks

// RX-TX burst prevention
#define TX_DELAY 65               // delay (ms) to prevent spurious burst that is emitted when switching from RX to TX

// all variables that will be stored in EEPROM are contained in this 'struct':
struct userparameters {
  byte raduino_version;                            // version identifier
  byte wpm = CW_SPEED;                             // CW keyer speed (words per minute)
  int USB_OFFSET = OFFSET_USB;                     // VFO offset in Hz for USB mode
  int cal = CAL_VALUE;                             // VFO calibration value
  byte LSBdrive = VFO_DRIVE_LSB;                   // VFO drive level in LSB mode
  byte USBdrive = VFO_DRIVE_USB;                   // VFO drive level in USB mode
  bool semiQSK = SEMI_QSK;                         // whether semi QSK is ON or OFF
  unsigned int POT_SPAN = TUNING_POT_SPAN;         // tuning pot span (kHz)
  unsigned int CW_OFFSET = CW_SHIFT;               // RX shift (Hz) in CW mode, equal to side tone pitch
  unsigned long vfoA = 7125000UL;                  // frequency of VFO A
  unsigned long vfoB = 7125000UL;                  // frequency of VFO B
  byte mode_A = 0;                                 // operating mode of VFO A
  byte mode_B = 0;                                 // operating mode of VFO B
  bool vfoActive = false;                          // currently active VFO (A = false, B = true)
  bool splitOn = false;                            // whether SPLIT is ON or OFF
  unsigned int scan_start_freq = SCAN_START;       // scan start frequency (kHz)
  unsigned int scan_stop_freq = SCAN_STOP;         // scan stop frequency (kHz)
  unsigned int scan_step_freq = SCAN_STEP;         // scan step size (Hz)
  unsigned int scan_step_delay = SCAN_STEP_DELAY;  // scan step delay (ms)
  unsigned int QSK_DELAY = CW_TIMEOUT;             // word [accepted range 10-1000 ms]
  byte key_type = CW_KEY_TYPE;                     // CW key type (0:straight, 1:paddle, 2:rev. paddle, 3:bug, 4:rev. bug)
  unsigned long LOWEST_FREQ = MIN_FREQ;            // absolute minimum dial frequency (Hz)
  unsigned long HIGHEST_FREQ = MAX_FREQ;           // absolute maximum dial frequency (Hz)
  bool autospace = AUTOSPACE;                      // whether auto character space is ON or OFF
  byte cap_sens = CAP_SENSITIVITY;                 // sensitivity of the touch keyer sensors
};

// we can access each of the variables inside the above struct by "u.variablename"
struct userparameters u;

#ifdef DEBUG_BOOT
#define DEBUG_PRINT(x)  Serial.print (x)
#define DEBUG_PRINTLN(x)  Serial.println (x)
#else
#define DEBUG_PRINT(x)
#define DEBUG_PRINTLN(x)
#endif

#ifdef DEBUG_RUN
#define DEBUG_PRINT2(x)  Serial.print (x)
#define DEBUG_PRINTLN2(x)  Serial.println (x)
#else
#define DEBUG_PRINT2(x)
#define DEBUG_PRINTLN2(x)
#endif

/**

   Below are the libraries to be included for building the Raduino

   The EEPROM library is used to store settings like the frequency memory, calibration data, etc.
*/

#include <EEPROM.h>

/**
    The Wire.h library is used to talk to the Si5351 and we also declare an instance of
    Si5351 object to control the clocks.
*/
#include <Wire.h>

/**
    The PinChangeInterrupt.h library is used for handling interrupts
*/
#include <PinChangeInterrupt.h> // https://github.com/NicoHood/PinChangeInterrupt

/**
    The main chip which generates upto three oscillators of various frequencies in the
    Raduino is the Si5351a. To learn more about Si5351a you can download the datasheet
    from www.silabs.com although, strictly speaking it is not a requirement to understand this code.

    We no longer use the standard SI5351 library because of its huge overhead due to many unused
    features consuming a lot of program space. Instead of depending on an external library we now use
    Jerry Gaffke's, KE7ER, lightweight standalone mimimalist "si5351bx" routines (see further down the
    code). Here are some defines and declarations used by Jerry's routines:
*/

#define BB0(x) ((uint8_t)x)             // Bust int32 into Bytes
#define BB1(x) ((uint8_t)(x>>8))
#define BB2(x) ((uint8_t)(x>>16))

#define SI5351BX_ADDR 0x60              // I2C address of Si5351   (typical)
#define SI5351BX_XTALPF 2               // 1:6pf  2:8pf  3:10pf

// If using 27mhz crystal, set XTAL=27000000, MSA=33.  Then vco=891mhz
#define SI5351BX_XTAL 25000000          // Crystal freq in Hz
#define SI5351BX_MSA  35                // VCOA is at 25mhz*35 = 875mhz

// User program may have reason to poke new values into these 3 RAM variables
uint32_t si5351bx_vcoa = (SI5351BX_XTAL*SI5351BX_MSA);  // 25mhzXtal calibrate
uint8_t  si5351bx_rdiv = 0;             // 0-7, CLK pin sees fout/(2**rdiv)
uint8_t  si5351bx_drive[3] = {1, 1, 1}; // 0=2ma 1=4ma 2=6ma 3=8ma for CLK 0,1,2
uint8_t  si5351bx_clken = 0xFF;         // Private, all CLK output drivers off

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

char c[17], b[10], printBuff[2][17];

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
   A1 (digital input) is to connect to a straight key, or to the 'Dit' contact of a paddle keyer. Open (HIGH) during key up, switch to ground (LOW) during key down.
   A2 (digital input) can be used for calibration by grounding this line (not required when you have the Function Button at A3)
   A3 (digital input) is connected to a push button that can momentarily ground this line. This Function Button will be used to switch between different modes, etc.
   A4 (already in use for talking to the SI5351)
   A5 (already in use for talking to the SI5351)
   A6 (analog input) is not currently used
   A7 (analog input) is connected to a center pin of good quality 100K or 10K linear potentiometer with the two other ends connected to
       ground and +5v lines available on the connector. This implements the tuning mechanism.
*/

#define PTT_SENSE (A0)
#define KEY (A1)
#define CAL_BUTTON (3)
#define FBUTTON (A3)
#define ANALOG_TUNING (A7)

bool PTTsense_installed; //whether or not the PTT sense line is installed (detected automatically during startup)

/**
    The second set of 16 pins on the bottom connector P3 have the three clock outputs and the digital lines to control the rig.
    This assignment is as follows :
      Pin   1   2    3    4    5    6    7    8    9    10   11   12   13   14   15   16  (connector P3)
         +12V +12V CLK2  GND  GND CLK1  GND  GND  CLK0  GND  D2   D3   D4   D5   D6   D7
    These too are flexible with what you may do with them, for the Raduino, we use them to :

    output D2 - PULSE : is used for the capacitive touch keyer
    input D3  - DAH : is connected to the 'Dah' contact of an paddle keyer (switch to ground).
    input D4  - SPOT : is connected to a push button that can momentarily ground this line. When the SPOT button is pressed a sidetone will be generated for zero beat tuning.
    output D5 - CW_TONE : Side tone output
    output D6 - CW_CARRIER line : turns on the carrier for CW
    output D7 - TX_RX line : Switches between Transmit and Receive in CW mode
*/

#define PULSE (2)
#define DAH (A2)
#define SPOT (4)
#define CW_TONE (5)
#define CW_CARRIER (6)
#define TX_RX (7)

bool TXRX_installed = true; // whether or not the TX_RX mod (PTT bypass transistor) is installed (set automatically at startup)

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
unsigned long vfoA;               // the frequency (Hz) of VFO A
unsigned long vfoB;               // the frequency (Hz) of VFO B
byte mode = LSB;                  // mode of the currently active VFO
bool inTx = false;                // whether or not we are in transmit mode
volatile bool TXstart = false;    // this flag will be set by the ISR as soon as the PTT is keyed


// some variable used by the capacitive touch keyer:

bool CapTouch_installed = true;  // whether or not the capacitive touch modification is installed (detected automatically during startup)
byte base_sens_KEY;              // base delay (in us) when DIT pad is NOT touched (measured automatically by touch sensor calibration routine)
byte base_sens_DAH;              // base delay (in us) when DAH pad is NOT touched (measured automatically by touch sensor calibration routine)
bool capaKEY, capaDAH;

#define bfo_freq (11998000UL)
unsigned long baseTune = 7100000UL; // frequency (Hz) when tuning pot is at minimum position
int old_knob = 0;
unsigned long frequency;    // the 'dial' frequency as shown on the display


// *************  SI5315 routines - tks Jerry Gaffke, KE7ER   ***********************

// An minimalist standalone set of Si5351 routines.
// VCOA is fixed at 875mhz, VCOB not used.
// The output msynth dividers are used to generate 3 independent clocks
// with 1hz resolution to any frequency between 4khz and 109mhz.

// Usage:
// Call si5351bx_init() once at startup with no args;
// Call si5351bx_setfreq(clknum, freq) each time one of the
// three output CLK pins is to be updated to a new frequency.
// A freq of 0 serves to shut down that output clock.

// The global variable si5351bx_vcoa starts out equal to the nominal VCOA
// frequency of 25mhz*35 = 875000000 Hz.  To correct for 25mhz crystal errors,
// the user can adjust this value.  The vco frequency will not change but
// the number used for the (a+b/c) output msynth calculations is affected.
// Example:  We call for a 5mhz signal, but it measures to be 5.001mhz.
// So the actual vcoa frequency is 875mhz*5.001/5.000 = 875175000 Hz,
// To correct for this error:     si5351bx_vcoa=875175000;

// Most users will never need to generate clocks below 500khz.
// But it is possible to do so by loading a value between 0 and 7 into
// the global variable si5351bx_rdiv, be sure to return it to a value of 0
// before setting some other CLK output pin.  The affected clock will be
// divided down by a power of two defined by  2**si5351_rdiv
// A value of zero gives a divide factor of 1, a value of 7 divides by 128.
// This lightweight method is a reasonable compromise for a seldom used feature.

void si5351bx_init() {                  // Call once at power-up, start PLLA
  uint8_t reg;  uint32_t msxp1;
  Wire.begin();
  i2cWrite(149, 0);                     // SpreadSpectrum off
  i2cWrite(3, si5351bx_clken);          // Disable all CLK output drivers
  i2cWrite(183, SI5351BX_XTALPF << 6);  // Set 25mhz crystal load capacitance
  msxp1 = 128 * SI5351BX_MSA - 512;     // and msxp2=0, msxp3=1, not fractional
  uint8_t  vals[8] = {0, 1, BB2(msxp1), BB1(msxp1), BB0(msxp1), 0, 0, 0};
  i2cWriten(26, vals, 8);               // Write to 8 PLLA msynth regs
  i2cWrite(177, 0x20);                  // Reset PLLA  (0x80 resets PLLB)
  // for (reg=16; reg<=23; reg++) i2cWrite(reg, 0x80);    // Powerdown CLK's
  // i2cWrite(187, 0);                  // No fannout of clkin, xtal, ms0, ms4
}

void si5351bx_setfreq(uint8_t clknum, uint32_t fout) {  // Set a CLK to fout Hz
  uint32_t  msa, msb, msc, msxp1, msxp2, msxp3p2top;
  if ((fout < 500000) || (fout > 109000000)) // If clock freq out of range
    si5351bx_clken |= 1 << clknum;      //  shut down the clock
  else {
    msa = si5351bx_vcoa / fout;     // Integer part of vco/fout
    msb = si5351bx_vcoa % fout;     // Fractional part of vco/fout
    msc = fout;             // Divide by 2 till fits in reg
    while (msc & 0xfff00000) {
      msb = msb >> 1;
      msc = msc >> 1;
    }
    msxp1 = (128 * msa + 128 * msb / msc - 512) | (((uint32_t)si5351bx_rdiv) << 20);
    msxp2 = 128 * msb - 128 * msb / msc * msc; // msxp3 == msc;
    msxp3p2top = (((msc & 0x0F0000) << 4) | msxp2);     // 2 top nibbles
    uint8_t vals[8] = { BB1(msc), BB0(msc), BB2(msxp1), BB1(msxp1),
                        BB0(msxp1), BB2(msxp3p2top), BB1(msxp2), BB0(msxp2)
                      };
    i2cWriten(42 + (clknum * 8), vals, 8); // Write to 8 msynth regs
    i2cWrite(16 + clknum, 0x0C | si5351bx_drive[clknum]); // use local msynth
    si5351bx_clken &= ~(1 << clknum);   // Clear bit to enable clock
  }
  i2cWrite(3, si5351bx_clken);        // Enable/disable clock
}

void i2cWrite(uint8_t reg, uint8_t val) {   // write reg via i2c
  Wire.beginTransmission(SI5351BX_ADDR);
  Wire.write(reg);
  Wire.write(val);
  Wire.endTransmission();
}

void i2cWriten(uint8_t reg, uint8_t *vals, uint8_t vcnt) {  // write array
  Wire.beginTransmission(SI5351BX_ADDR);
  Wire.write(reg);
  while (vcnt--) Wire.write(*vals++);
  Wire.endTransmission();
}

// *********** End of Jerry's si5315bx routines *********************************************************

/**
   Display Routine
   This display routine prints a line of characters to the upper or lower line of the 16x2 display
   linenmbr = 0 is the upper line
   linenmbr = 1 is the lower line
*/

void printLine(char linenmbr, char *c) {
  if (strcmp(c, printBuff[linenmbr])) {     // only refresh the display when there was a change
    lcd.setCursor(0, linenmbr);             // place the cursor at the beginning of the selected line
    lcd.print(c);
    strcpy(printBuff[linenmbr], c);

    for (byte i = strlen(c); i < 16; i++) { // add white spaces until the end of the 16 characters line is reached
      lcd.print(' ');
    }
  }
}

/**
   Building upon the previous routine,
   update Display paints the first line as per current state of the radio
*/

void updateDisplay() {
  // tks Jack Purdum W8TEE
  // replaced fsprint commmands by str commands for code size reduction

  memset(c, 0, sizeof(c));
  memset(b, 0, sizeof(b));

  ultoa((frequency + 50), b, DEC); // construct the frequency string
  if (!u.vfoActive)
    strcpy(c, "A ");        // display which VFO is active (A or B)
  else
    strcpy(c, "B ");


  byte p = strlen(b);         // the length of the frequency string (<10 Mhz: 7, >10 MHz: 8)

  strncat(c, &b[0], p - 6);   // display the megahertzes
  strcat(c, ".");
  strncat(c, &b[p - 6], 3);   // display the kilohertzes
  strcat(c, ".");

  strncat(c, &b[p - 3], 1); // display the frequency at 100 Hz precision

  switch (mode) {             // show the operating mode
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

  if (inTx)                   // show the state (TX, SPLIT, or nothing)
    strcat(c, " TX");
  else if (u.splitOn)
    strcat(c, " SP");

  c[16] = '\0';               // cut off any excess characters (string length is max 16 postions)
  printLine(0, c);            // finally print the constructed string to the first line of the display
}

// routine to generate a bleep sound (FB menu)
void bleep(int pitch, int duration, byte repeat) {
  for (byte i = 0; i < repeat; i++) {
    tone(CW_TONE, pitch);
    delay(duration);
    noTone(CW_TONE);
    delay(duration);
  }
}

void setFrequency() {

  DEBUG_PRINT("BFO ");
  DEBUG_PRINT(bfo_freq);
  DEBUG_PRINT(", Si5351: ");
  if (mode & 1) {   // if we are in UPPER side band mode
    si5351bx_setfreq(2, (bfo_freq + frequency - u.USB_OFFSET));
    DEBUG_PRINT(bfo_freq + frequency - u.USB_OFFSET);
    DEBUG_PRINT(", VFO ");
    DEBUG_PRINT(frequency);
  }
  else  {           // if we are in LOWER side band mode
    si5351bx_setfreq(2, (bfo_freq - frequency));
    DEBUG_PRINT(bfo_freq - frequency);
    DEBUG_PRINT(", VFO ");
    DEBUG_PRINT(frequency);
  }
  DEBUG_PRINT(", tuning pot ");
  DEBUG_PRINTLN(analogRead(ANALOG_TUNING));
  updateDisplay();
}

/**
   The checkTX toggles the T/R line. If you would like to make use of RIT, etc,
   you must connect pin A0 (black wire) via a 10K resistor to the output of U3
   This is a voltage regulator LM7805 which goes on during TX. We use the +5V output
   as a PTT sense line (to tell the Raduino that we are in TX).
*/

void SetSideBand(byte drivelevel) {

  set_drive_level(drivelevel);
  setFrequency();
  if (!u.vfoActive)        // if VFO A is active
    u.mode_A = mode;
  else                     // if VFO B is active
    u.mode_B = mode;
}

/* interrupt service routine
  When the PTT is keyed (either manually or by the semiQSK function), normal program execution
  will be interrupted and the interrupt service routine (ISR) will be executed. After that, normal
  program execution will continue from the point it was interrupted.
  The only thing this ISR does, is setting the TXstart flag. This flag is frequently polled by the
  knob_position routine.
  The knob_position routine takes 100 readings from the tuning pot for extra precision. However
  this takes more time (about 10ms). Normally this is not a problem, however when we key the PTT
  we need to quickly turn off the VFO in order to prevent spurious emission. In that case 10ms is
  too much delay.
  In order to prevent this delay, the knob-position routine keeps checking the TXstart flag
  and when it is set it will terminate immediately.
*/

void ISRptt(void) {
  TXstart = true;                       // the TXstart flag will be set as soon as the PTT is keyed
}

// routine to read the position of the tuning knob at high precision (Allard, PE1NWL)
int knob_position() {
  unsigned long knob = 0;
  // the knob value normally ranges from 0 through 1023 (10 bit ADC)
  // in order to increase the precision by a factor 10, we need 10^2 = 100x oversampling
  for (byte i = 0; i < 100; i++) { // it takes about 10 ms to run this loop 100 times
    if (TXstart) {     // when the PTT was keyed, in order to prevent 10 ms delay:
      DEBUG_PRINTLN2("RX => TX");
      TXstart = false; // reset the TXstart flag that was set by the interrupt service routine
      return old_knob; // then exit this routine immediately and return the old knob value
    }
    else
      knob = knob + analogRead(ANALOG_TUNING); // take 100 readings from the ADC
  }
  knob = (knob + 5L) / 10L; // take the average of the 100 readings and multiply the result by 10
  //now the knob value ranges from 0 through 10,230 (10x more precision)
  knob = knob * 10000L / 10230L; // scale the knob range down to 0-10,000
  return (int)knob;
}

/*
  Many BITX40's suffer from a strong birdie at 7199 kHz (LSB).
  This birdie may be eliminated by using a different VFO drive level in LSB mode.
  In USB mode, a high drive level may be needed to compensate for the attenuation of
  higher VFO frequencies.
  The drive level for each mode can be set in the SETTINGS menu
*/

void set_drive_level(byte level) {
  si5351bx_drive[2] = level / 2 - 1;
  setFrequency();
}

/*
   routine to align the current knob position with the current frequency
   If we switch between VFO's A and B, the frequency will change but the tuning knob
   is still in the same position. We need to apply some offset so that the new frequency
   corresponds with the current knob position.
   This routine reads the current knob position, then it shifts the baseTune value up or down
   so that the new frequency matches again with the current knob position.
*/

void shiftBase() {
  setFrequency();
  unsigned long knob = knob_position(); // get the current tuning knob position
  baseTune = frequency - (knob * (unsigned long)u.POT_SPAN / 10UL);
}

/*
   The Tuning mechansim of the Raduino works in a very innovative way. It uses a tuning potentiometer.
   The tuning potentiometer that a voltage between 0 and 5 volts at ANALOG_TUNING pin of the control connector.
   This is read as a value between 0 and 1023. By 100x oversampling this range is expanded by a factor 10.
   Hence, the tuning pot gives you 10,000 steps from one end to the other end of its rotation. Each step is 50 Hz,
   thus giving maximum 500 Khz of tuning range. The tuning range is scaled down depending on the POT_SPAN value.
   The standard tuning range (for the standard 1-turn pot) is 50 Khz. But it is also possible to use a 10-turn pot
   to tune accross the entire 40m band. The desired POT_SPAN can be set via the Function Button in the SETTINGS menu.
   When the potentiometer is moved to either end of the range, the frequency starts automatically moving
   up or down in 10 Khz increments, so it is still possible to tune beyond the range set by POT_SPAN.
*/

void doTuning() {
  int knob = analogRead(ANALOG_TUNING); // get the current tuning knob position
  if (digitalRead(PTT_SENSE)) {
    inTx = true;
    printLine(1, (char *)"PTT is ON");
    DEBUG_PRINTLN("PTT is ON");
    updateDisplay();
    noTone(CW_TONE);                 // stop generating the sidetone
    delay(250);
  }
  else if (!digitalRead(SPOT)) {
    inTx = false;
    printLine(1, (char *)"SPOT pressed");
    DEBUG_PRINTLN("SPOT pressed");
    updateDisplay();
    noTone(CW_TONE);                 // stop generating the sidetone
    delay(250);
  }
  else if (!digitalRead(FBUTTON)) {
    inTx = false;
    printLine(1, (char *)"F-Button pressed");
    DEBUG_PRINTLN("F-Button pressed");
    updateDisplay();
    noTone(CW_TONE);                 // stop generating the sidetone
    delay(250);
  }
  else if (!CapTouch_installed && !digitalRead(KEY)) {
    inTx = false;
    printLine(1, (char *)"KEY/DIT is ON");
    DEBUG_PRINTLN("KEY/DIT is ON");
    updateDisplay();
    tone(CW_TONE, 800);      // generate sidetone
    delay(250);
  }
  else if (!CapTouch_installed && !digitalRead(DAH)) {
    inTx = false;
    printLine(1, (char *)"DAH is ON");
    DEBUG_PRINTLN("DAH is ON");
    updateDisplay();
    tone(CW_TONE, 800);      // generate sidetone
    delay(250);
  }
  else if (CapTouch_installed && capaKEY) {
    inTx = false;
    printLine(1, (char *)"DIT sensor");
    DEBUG_PRINTLN("DIT sensor touched");
    updateDisplay();
    tone(CW_TONE, 800);      // generate sidetone
    delay(250);
  }
  else if (CapTouch_installed && capaDAH) {
    inTx = false;
    printLine(1, (char *)"DAH sensor");
    DEBUG_PRINTLN("DAH sensor touched");
    updateDisplay();
    tone(CW_TONE, 800);      // generate sidetone
    delay(250);
  }
  else {
    inTx = false;
    itoa(knob, b, DEC);
    strcpy(c, "tuning pot ");
    strcat(c, b);
    printLine(1, c);
    noTone(CW_TONE);                 // stop generating the sidetone
    updateDisplay();
  }
  knob = knob * 100000 / 10230; // get the current tuning knob position

  // tuning is disabled during TX (only when PTT sense line is installed)
  if (inTx && abs(knob - old_knob) > 20) {
    printLine(1, (char *)"dial is locked");
    shiftBase();
    return;
  }
  else if (inTx) {           // no tuning in TX or when dial is locked
    return;
  }
  else if (abs(knob - old_knob) < 20)
    save_frequency();                 // only save VFO frequencies to EEPROM when tuning pot is not being turned

  knob = knob_position(); // get the precise tuning knob position
  // the knob is fully on the low end, do fast tune: move down by 10 Khz and wait for 300 msec
  // if the POT_SPAN is very small (less than 25 kHz) then use 1 kHz steps instead

  if (knob == 0) {
    if (frequency > u.LOWEST_FREQ) {
      if (u.POT_SPAN < 25)
        baseTune = baseTune - 1000UL; // fast tune down in 1 kHz steps
      else
        baseTune = baseTune - 10000UL; // fast tune down in 10 kHz steps
      frequency = baseTune + (unsigned long)knob * (unsigned long)u.POT_SPAN / 10UL;
      delay(300);
    }
    if (frequency <= u.LOWEST_FREQ)
      baseTune = frequency = u.LOWEST_FREQ;
    setFrequency();
    old_knob = 0;
  }

  // the knob is full on the high end, do fast tune: move up by 10 Khz and wait for 300 msec
  // if the POT_SPAN is very small (less than 25 kHz) then use 1 kHz steps instead

  else if (knob == 10000) {
    if (frequency < u.HIGHEST_FREQ) {
      if (u.POT_SPAN < 25)
        baseTune = baseTune + 1000UL; // fast tune up in 1 kHz steps
      else
        baseTune = baseTune + 10000UL; // fast tune up in 10 kHz steps
      frequency = baseTune + (unsigned long)knob * (unsigned long)u.POT_SPAN / 10UL;
      delay(300);
    }
    if (frequency >= u.HIGHEST_FREQ) {
      baseTune = u.HIGHEST_FREQ - (u.POT_SPAN * 1000UL);
      frequency = u.HIGHEST_FREQ;
    }
    setFrequency();
    old_knob = 10000;
  }

  // the tuning knob is at neither extremities, tune the signals as usual
  else {
    if (abs(knob - old_knob) > 4) { // improved "flutter fix": only change frequency when the current knob position is more than 4 steps away from the previous position
      knob = (knob + old_knob) / 2; // tune to the midpoint between current and previous knob reading
      old_knob = knob;
      frequency = constrain(baseTune + (unsigned long)knob * (unsigned long)u.POT_SPAN / 10UL, u.LOWEST_FREQ, u.HIGHEST_FREQ);
      setFrequency();
      delay(10);
    }
  }

  if (!u.vfoActive) // VFO A is active
    vfoA = frequency;
  else
    vfoB = frequency;
}

void factory_settings() {

  printLine(0, (char *)"loading standard");
  printLine(1, (char *)"settings...");

  EEPROM.put(0, u); // save all user parameters to EEPROM
  delay(1000);
}

// routine to save both VFO frequencies when they haven't changed for more than 500Hz in the past 30 seconds

void save_frequency() {
  static unsigned long t3;
  if (abs(vfoA - u.vfoA) > 500UL || abs(vfoB - u.vfoB) > 500UL) {
    if (millis() - t3 > 30000UL) {
      u.vfoA = vfoA;
      u.vfoB = vfoB;
      t3 = millis();
    }
  }
  else
    t3 = millis();
}

/*
  Routine to calibrate the touch key pads
  (measure the time it takes the capacitance to discharge while the touch pads are NOT touched)
  Even when the touch pads are NOT touched, there is some internal capacitance
  The internal capacitance may vary depending on the length and routing of the wiring
  We measure the base capacitance so that we can use it as a baseline reference
  (threshold delay when the pads are not touched).
*/

void calibrate_touch_pads() {
  // disable the internal pullups
  pinMode(KEY, INPUT);
  pinMode(DAH, INPUT);

  bool triggered;
  // first we calibrate the KEY (DIT) touch pad
  base_sens_KEY = 0;                    // base capacity of the KEY (DIT) touch pad
  do {
    base_sens_KEY++;                    // increment the delay time until the KEY touch pad is no longer triggered by the base capacitance
    triggered = false;
    for (int i = 0; i < 100; i++) {
      digitalWrite(PULSE, HIGH);        // bring the KEY input to a digital HIGH level
      delayMicroseconds(50);            // wait a short wile to allow voltage on the KEY input to stabalize
      digitalWrite(PULSE, LOW);         // now bring the KEY input to a digital LOW value
      delayMicroseconds(base_sens_KEY); // wait few microseconds
      if (digitalRead(KEY)) {           // check 100 times if KEY input is still HIGH
        triggered = true;               // if KEY is still high, then it was triggered by the base capacitance
        i = 100;
      }
    }
  } while (triggered && base_sens_KEY != 255);               // keep trying until KEY input is no longer triggered
  DEBUG_PRINT("DIT touch sensor: ");
  DEBUG_PRINT(base_sens_KEY);
  DEBUG_PRINTLN(" us");

  // Next we calibrate the DAH pad
  base_sens_DAH = 0;                    // base capacity of the DAH touch pad
  do {
    base_sens_DAH++;                    // increment the delay time until the DAH touch pad is no longer triggered by the base capacitance
    triggered = false;
    for (int i = 0; i < 100; i++) {
      digitalWrite(PULSE, HIGH);        // bring the KEY input to a digital HIGH level
      delayMicroseconds(50);            // wait a short wile to allow voltage on the DAH input to stabalize
      digitalWrite(PULSE, LOW);         // now bring the DAH input to a digital LOW value
      delayMicroseconds(base_sens_DAH); // wait few microseconds
      if (digitalRead(DAH)) {           // check 100 times if DAH input is still HIGH
        triggered = true;               // if KEY is still high, then it was triggered by the base capacitance
        i = 100;
      }
    }
  } while (triggered && base_sens_DAH != 255);                 // keep trying until KEY input is no longer triggered
  DEBUG_PRINT("DAH touch sensor: ");
  DEBUG_PRINT(base_sens_DAH);
  DEBUG_PRINTLN(" us");
  DEBUG_PRINT("touch sensor detected: ");

  if (base_sens_KEY == 255 || base_sens_DAH == 255 || base_sens_KEY == 1 || base_sens_DAH == 1) {   // if inputs are still triggered even with max delay (255 us)
    CapTouch_installed = false;                         // then the base capacitance is too high (or the mod is not installed) so we can't use the touch keyer
    u.cap_sens = 0;                                     // turn capacitive touch keyer OFF
    printLine(0, (char *)"touch sensors");
    printLine(1, (char *)"not detected");
    DEBUG_PRINTLN("NO");
  }
  else if (u.cap_sens > 0) {
    DEBUG_PRINTLN("YES");
    printLine(0, (char *)"touch key calibr");
    strcpy(c, "DIT ");
    itoa(base_sens_KEY, b, DEC);
    strcat(c, b);
    strcat(c, ", DAH ");
    itoa(base_sens_DAH, b, DEC);
    strcat(c, b);
    strcat(c, " us");
    printLine(1, c);
  }
  else {
    printLine(1, (char *)"touch keyer OFF");
    DEBUG_PRINTLN("YES");
  }

  delay(2000);
  updateDisplay();

  //configure the morse keyer inputs
  if (u.cap_sens == 0) {       // enable the internal pull-ups if touch keyer is disabled
    pinMode(KEY, INPUT_PULLUP);
    pinMode(DAH, INPUT_PULLUP);
  }
}

void touch_key() {
  static unsigned long KEYup = 0;
  static unsigned long KEYdown = 0;
  static unsigned long DAHup = 0;
  static unsigned long DAHdown = 0;

  digitalWrite(PULSE, LOW);                            // send LOW to the PULSE output
  delayMicroseconds(base_sens_KEY + 25 - u.cap_sens);  // wait few microseconds for the KEY capacitance to discharge
  if (digitalRead(KEY)) {                              // test if KEY input is still HIGH
    KEYdown = millis();                                // KEY touch pad was touched
    if (!capaKEY && millis() - KEYup > 1) {
      capaKEY = true;
    }
  }
  else {                                               // KEY touch pad was not touched
    KEYup = millis();
    if (capaKEY && millis() - KEYdown > 10)
      capaKEY = false;
  }

  digitalWrite(PULSE, HIGH);                           // send HIGH to the PULSE output
  delayMicroseconds(50);                               // allow the capacitance to recharge

  digitalWrite(PULSE, LOW);                            // send LOW to the PULSE output
  delayMicroseconds(base_sens_DAH + 25 - u.cap_sens);  // wait few microseconds for the DAH capacitance to discharge

  if (digitalRead(DAH)) {                              // test if DAH input is still HIGH
    DAHdown = millis();                                // DAH touch pad was touched
    if (!capaDAH && millis() - DAHup > 1) {
      capaDAH = true;
    }
  }
  else {                                               // DAH touch pad was not touched
    DAHup = millis();
    if (capaDAH && millis() - DAHdown > 10)
      capaDAH = false;
  }

  digitalWrite(PULSE, HIGH);                           // send HIGH to the PULSE output
  delayMicroseconds(50);                               // allow the capacitance to recharge

  // put the touch sensors in the correct position
  if (u.key_type == 2 || u.key_type == 4) {
    // swap capaKEY and capaDAH if paddle is reversed
    capaKEY = capaKEY xor capaDAH;
    capaDAH = capaKEY xor capaDAH;
    capaKEY = capaKEY xor capaDAH;
  }
}

/*
   setup is called on boot up
   It setups up the modes for various pins as inputs or outputs
   initiliaizes the Si5351 and sets various variables to initial state

   Just in case the LCD display doesn't work well, the debug log is dumped on the serial monitor
   Choose Serial Monitor from Arduino IDE's Tools menu to see the Serial.print messages
*/

void setup() {
  u.raduino_version = 27;
  strcpy (c, "Raduino Diag v1");

  lcd.begin(16, 2);

  Serial.begin(9600);
  DEBUG_PRINTLN(c);
  printLine(0, c);
  delay(1000);


  analogReference(DEFAULT);
  DEBUG_PRINTLN("starting up...");

  //configure the function button to use the internal pull-up
  pinMode(FBUTTON, INPUT_PULLUP);
  //configure the CAL button to use the internal pull-up
  pinMode(CAL_BUTTON, INPUT_PULLUP);
  //configure the SPOT button to use the internal pull-up
  pinMode(SPOT, INPUT_PULLUP);

  pinMode(TX_RX, INPUT_PULLUP);
  DEBUG_PRINT("TX-RX line installed: ");
  if (digitalRead(TX_RX)) { // Test if TX_RX line is installed
    pinMode(TX_RX, OUTPUT);
    digitalWrite(TX_RX, 0);
    TXRX_installed = false;
    u.semiQSK = false;
    u.QSK_DELAY = 10;
    DEBUG_PRINTLN("NO");
  }
  else {
    pinMode(TX_RX, OUTPUT);
    digitalWrite(TX_RX, 0);
    DEBUG_PRINTLN("YES");
    printLine(0, (char *)"TX-RX line");
    printLine(1, (char *)"installed");
    delay(1000);
  }

  pinMode(CW_CARRIER, OUTPUT);
  digitalWrite(CW_CARRIER, 0);
  pinMode(CW_TONE, OUTPUT);
  digitalWrite(CW_TONE, 0);
  pinMode(PULSE, OUTPUT);
  digitalWrite(PULSE, 0);



  //configure the PTT SENSE to use the internal pull-up
  pinMode(PTT_SENSE, INPUT_PULLUP);
  delay(100);
  // check if PTT sense line is installed
  PTTsense_installed = !digitalRead(PTT_SENSE);
  pinMode(PTT_SENSE, INPUT); //disable the internal pull-up

  // attach interrupt to the PTT_SENSE input
  // when PTT_SENSE goes from LOW to HIGH (so when PTT is keyed), execute the ISRptt routine
  attachPCINT(digitalPinToPCINT(PTT_SENSE), ISRptt, RISING);
  delay(1000);

  //retrieve user settings from EEPROM
  EEPROM.get(0, u);

  DEBUG_PRINT("PTTsense installed: ");
  if (PTTsense_installed) {
    DEBUG_PRINTLN("YES");
    printLine(0, (char *)"PTT_SENSE mod");
    printLine(1, (char *)"installed");
    delay(1000);
    calibrate_touch_pads();  // measure the base capacitance of the touch pads while they're not being touched
    u.cap_sens = 22;
  }
  else
    DEBUG_PRINTLN("NO");
#ifdef DEBUG_BOOT
  DEBUG_PRINT("Function button pressed: ");
  if (digitalRead(FBUTTON))
    DEBUG_PRINTLN("NO");
  else
    DEBUG_PRINTLN("YES");
  DEBUG_PRINT("SPOT button pressed: ");
  if (digitalRead(SPOT))
    DEBUG_PRINTLN("NO");
  else
    DEBUG_PRINTLN("YES");
  DEBUG_PRINT("analog reading tuning pot: ");
  DEBUG_PRINTLN(analogRead(ANALOG_TUNING));
#endif


  //initialize the SI5351
  si5351bx_init();
  //si5351bx_vcoa = (SI5351BX_XTAL * SI5351BX_MSA) + u.cal * 100L; // apply the calibration correction factor

  vfoA = u.vfoA;
  vfoB = u.vfoB;

  frequency = vfoA;
  mode = LSB;

  SetSideBand(u.LSBdrive);

  shiftBase(); //align the current knob position with the current frequency
  DEBUG_PRINT("display line 1: ");
  DEBUG_PRINTLN(c);
  DEBUG_PRINT("display line 2: ");

  //display warning message when VFO calibration data was erased
  if ((u.cal == 0) && (u.USB_OFFSET == 1500)) {
    printLine(1, (char *)"VFO uncalibrated");
    DEBUG_PRINTLN("VFO uncalibrated");
    delay(1000);
  }
  else
    DEBUG_PRINTLN();

  DEBUG_PRINT("VFO: ");
  DEBUG_PRINTLN(frequency);
  DEBUG_PRINT("BFO: ");
  DEBUG_PRINTLN(bfo_freq);

  DEBUG_PRINT("Si5351: ");
  DEBUG_PRINTLN(bfo_freq - frequency);

  bleep(u.CW_OFFSET, 60, 3);
  bleep(u.CW_OFFSET, 180, 1);

  DEBUG_PRINTLN();
  DEBUG_PRINTLN("boot completed");

  si5351bx_drive[0] = 0;  // lowest BFO drive level for less tuning clicks
  si5351bx_setfreq(0, bfo_freq - 1350);
}

void loop() {
  touch_key();
  doTuning();
}
