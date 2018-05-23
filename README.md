# bitx40

BITX40 sketch for Raduino

This sketch is intended as universal, standard Raduino software that should always work, even on a unmodified out-of-the-box BITX40 + raduino board. Without any hardware modifications the sketch provides the standard basic LSB functionality.
The sketch provides additional features such as USB, CW, RIT/SPLIT, KEYER etc., but these will only become functional when the related (minimal) hardware mods are made. 

![Hardware mod overview](https://github.com/amunters/bitx40/blob/master/hardware%20modification%20overview.PNG) 

See the [operating and modification instructions](https://github.com/amunters/bitx40/blob/master/operating-instructions.md) for full details.

**Note 1:** Since v1.20 it is no longer required to download and install the SI5351 library. Minimalist routines to drive the SI5351 are now embedded in the sketch.

**Note 2:** Since v1.27 the library [PinChangeInterrupt](https://playground.arduino.cc/Main/PinChangeInterrupt) is required for interrupt handling. Use your IDE to [install](https://github.com/amunters/bitx40/blob/master/library-install.md) it before compiling this sketch!

## Donate

I develop and maintain ham radio software as a hobby and distribute it for free. However, if you like this software, please consider to donate a small amount to my son's home who stays in an institute for kids with an intellectual disability and autism. The money will be used for adapted toys, a tricycle, a trampoline or a swing. Your support will be highly appreciated by this group of 6 young adolescents!

 [![Donate](https://www.paypalobjects.com/en_US/GB/i/btn/btn_donateCC_LG.gif)](https://www.paypal.com/cgi-bin/webscr?cmd=_s-xclick&hosted_button_id=PTAMBM6QT8LP8)

## Revision record

1.27.7
- Slightly revised the code so that interrupts are only enabled when the PTTsense mod is installed

1.27.6
- improved the code so that the si5351 does not keep receiving tuning updates once the frequency has reached the upper or lower limit
- corrected a bug that when semiQSK is ON, switching between modes did not work correctly for CWL/CWU.
- increased the default delay time for the Function Button for easier operation
- updated the instructions for the CW-CARRIER mod: Advise to use a 4.7K resistor instead of 10K so as to ensure full output power in CW

1.27.5
- line 33: Added the ability to set the step delay time for fast tuning (when the tuning pot is at the upper/lower limit) (tks Bob, N4FV)

1.27.4
- Line 32: Added the ability to specify text (e.g. your callsign) on the second line of the LCD when it would otherwise be blank (tks Richard, VE3YSH).
- some code clean up

1.27.3
- corrected the si5351 25mhz crystal load capacitance setting (tks Daniel KB3MUN)
- changed the initial calibration value to 180 ppm

1.27.2
- fixed a bug that with short QSK DELAY time, the radio did not return from CW to SSB mode (tks Gary N3GO for testing)

1.27.1
- fixed a bug that the spurious carrier burst was not suppressed in CW-SPLIT mode
- In semiQSK mode, the initial CW element is now delayed by 65ms (to prevent the carrier burst), instead of canceled
- fixed a bug that the radio did not reliably switch from LSB to CWL in semiQSK mode
- fixed a bug that the display got messed up when the VFO is above 10 MHz

1.27
- Improved the suppression of the spurious carrier when switching from RX to TX. This function requires the library [PinChangeInterrupt](https://playground.arduino.cc/Main/PinChangeInterrupt) for interrupt handling. Use your IDE to [install](https://github.com/amunters/bitx40/blob/master/library-install.md) it before compiling this sketch!

v1.26
- Rearranged the menu structure, skip menu items that aren't available when related mods are not installed
- Suppress the spurious carrier burst when the radio swithes to TX (tks Dave M0WID)
- For VFO calibration, use multiplicative correction (ppm) for better accuracy over a wide frequency range (tks Jerry KE7ER)
- Improved method for saving/restoring user settings to EEPROM (tks Pavel CO7WT)

v1.25.1
- some minor bug corrections to the touch keyer calibration code.

v1.25
- Added Capacitive Touch Keyer support.

v1.24
- Optimized CW keyer timing characteristics for smoother keying (tks Hidehiko, JA9MAT)
- Added DIAL LOCK function: Press the Function Button and then the SPOT button simultanuously to lock the dial. Press the FB again to unlock.

v1.23.1
- corrected bug that the 'auto-space' setting interfered with the 'maximum frequency' setting due to incorrect EEPROM location
- corrected bug that the display became cluttered up in the SETTINGS menu (CW parameters), when CW key was down

v1.23
- It is now possible to enable/disable the keyer's auto-space function from the SETTINGS menu (default setting is OFF).
- Added Vibroplex bug emulation to the CW keyer
- Moved all user setting parameters to the top of the sketch for in case you want to edit them manually
- Optimized some code in the keyer routine (tks Pavel CO7WT)

v1.22
- Added some functionality allowing the user to choose "paddle" or "reversed paddle" from the SETTINGS menu

v1.21
- Added automatic CW keyer functionality.
  The default setup is for straight key operation. If you want to use the automatic keyer, connect a paddle keyer to
  pin A1 (dit) and pin D3 (dah). In the SETTINGS menu, set CW-key type to "paddle".
  While keying, the keyer speed can be adjusted by pressing the Function Button (speed up) or the SPOT button (speed down).
  Keyer speed can be set from 1 - 50 WPM.
- It is now possible to set the minimum and maximum tuning frequency via the SETTINGS menu (no longer need to edit the sketch).
- Improved the tuning pot behaviour at the lower and upper ends of the pot.

v1.20.1
- Added some constraints so that frequency limits are respected during fast up/down scanning

v1.20
- Embedded Jerry Gaffke's, KE7ER, "minimalist standalone si5351bx routines". This not only makes the sketch independant from an
  external SI5351 library, but it greatly reduces the memory usage as well. The program space is needed for future development
  of additional features that would otherwise not fit in a Nano. Thanks Jerry!

v1.19
- Improved responsiveness of the tuning pot for smoother tuning (less need to fiddle up and down to set the correct frequency)
- Improved "Fast Tune" (at either ends of the tuning pot).
  The step size is now variable: the closer to the pot limit, the larger the step size.
- In CW SPOT tuning mode, short side tone pulses will be generated instead of a continuous tone.
  This makes SPOT tuning easier when tuning to weak CW signals.
- Calibration can now done at 1 Hz precision

v1.18
- improved CW performance at higher CW speeds:
  reduced the delay at the start of CW transmissions (first dit is no longer lost at high speed CW)
  optimized code so that 1:3 CW-ratio is kept even at high CW speed
- improved FINE TUNE mode so that exact frequency (at 1 Hz precision) is displayed while the SPOT button is held pressed
- added an extra option in the SETTINGS menu for setting semiQSK ON or OFF.
  This may be useful for CW operators who want to manually activate the PTT (e.g. using a foot switch).
  if semiQSK is ON:
    radio will automatically switch to CWL (or CWU), and go into TX mode when the morse key goes down
    go back to RX automatically when the QSKdelay time is exceeded
    radio will switch back to LSB (or USB) when the operator presses the PTT switch
  if semiQSK is OFF:
    operator must activate PTT manually to move the radio in TX
    pressing the PTT does not affect the mode. Use the Function Button to select the desired mode (LSB-USB-CWL-CWU)
- corrected a bug that FINE TUNE was not properly applied in USB mode

v1.17.1
- corrected a bug in v1.17 in the shiftBase() routine that the radio didn't return to the correct frequency after switching 
  VFO's, RIT, SPLIT, FINE TUNE etc.

v1.17
- Added "Fine Tune" capability to SPOT button
  While the SPOT button is held pressed, the radio will temporarily go into "FINE TUNE" mode, allowing the VFO to be set at 1Hz 
  precision. This feature works also in SSB mode (except that no sidetone will be generated then).

v1.16
- Added CW SPOT TONE button for exact zero beating.
  Connect a pushbutton to Arduino pin D4. A SPOT tone will be heard when D4 is connected to ground.
  By aligning the CW Spot tone to match the pitch of an incoming station's signal, you will cause your signal and the
  other station's signal to be exactly on the same frequency (zero beat).

v1.15.1
- RIT offset should only be applied during RX. Due to a small bug the RIT offset was not turned off during transmitting CW.
  (RIT in SSB was OK). This has been corrected - RIT works correctly now in all modes.

v1.15
- Added true RIT functionality (adjustable RX offset while TX frequency remains fixed) (2 Function Button presses)
- The old 'RIT' function, based on switching between VFOs A/B, is now called "SPLIT" (3 presses)
- Mode selection (4 presses) now rotates between LSB-USB-CWL-CWU
- Major code cleanup to reduce memory usage
- Inserted some delay in various routines to prevent annoying buzzing sound in SETTINGS menu

v1.14.1
- Corrected small bug in v1.14 that caused slight ticking noise when the radio was left idle.

v1.14
- added VFO A/B monitoring mode (press Function Button 5 times)
- use RX-offset instead of TX-offset in CW mode - the display now shows the correct TX frequency in CW
- changed the way to switch from CW to SSB mode: press PTT to return to SSB mode (tks Willy W1LY)
- restored the functionality for old way calibration method
- simplified the method for sidetone setting: hold key down to hear sidetone
- improved the display during "fast scan" at tuning pot limits (tks Paul KC8WBK)

v1.13
- added frequency scanning capability
- added functionality so that the user can set the CW timout value via the SETTINGS menu
- added decimal point to the VFO for better readability, like so: A 7.123.4 LSB
- simplified calibration routine and cleaned up the code to preserve memory space 

v1.12
- improved responsiveness of Function Button for better user experience
- corrected Tuning Range and SideTone setting procedures

v1.11
- added menu beeps (needs CW sidetone to be wired up)
- corrected a minor bug that "TX" is always shown when PTT-SENSE line has not been installed

v1.10
- added CW functionality (for straight morse key). This function can also be used just for tuning up.
  This requires the CW-CARRIER line connected to Raduino output D6 (connector P3, pin 15).
  (see https://github.com/amunters/bitx40/blob/master/CW-CARRIER%20wiring.png for wiring instructions).
  The morse key itself shall be connected to Raduino pin A1 (brown wire).
  Both sidebands (CWU or CWL) are available for CW operation.
- Semi break-in for CW
  This requires the TX-RX line from Raduino output D7 (connector P3, pin 16) to override the existing PTT
  switch using a NPN transistor. 
  (see https://github.com/amunters/bitx40/blob/master/TX-RX%20line%20wiring.png for wiring instructions)  
- CW side-tone
  This requires some wiring from Raduino output D5 (connector P3, pin 14) to the speaker.
  (see https://github.com/amunters/bitx40/blob/master/sidetone%20wiring.png for wiring instructions).
  The desired sidetone pitch can be set using the Function Button in the SETTINGS menu.
- Frequency tuning is disabled during TX (to prevent flutter or "FM-ing" during TX).
  This requires the PTT SENSE line connected to pin A0 (black wire).
  (see https://github.com/amunters/bitx40/blob/master/PTT%20SENSE%20wiring.png for wiring instructions).

v1.09
- added RIT (SPLIT) functionality. This requires a PTT sense line connected to pin A0 (black wire)
  (see https://github.com/amunters/bitx40/blob/master/PTT%20SENSE%20wiring.png for wiring instructions)
- simplified tuning range setting in SETTINGS menu
- less frequent EEPROM writes so that EEPROM will last longer

v1.08
- mode (LSB or USB) of each VFO A and B is now also memorized
- the BITX status (VFO frequencies, modes) is now stored in EEPROM every 10 seconds and retrieved during start up
- a warning message "uncalibrated" is displayed when calibration data has been erased

v1.07
- Added functionality via the Function Button:
  Use a pusbutton to momentarily ground pin A3 (orange wire). Do NOT install an external pull-up restistor!
- dual VFO capability (RIT is not yet working though)
- LSB/USB mode
- Settings menu for calibration, tuning range, VFO drive level
- All settings are stored in EEPROM and read during startup

v1.06
- no functional changes in this version, only improved the updateDisplay routine (Jack Purdum, W8TEE)
  (replaced fsprint commmands by str commands for code space reduction)

v1.05
- in setup(): increase the VFO drive level to 4mA to kill the birdie at 7199 kHz (Allard, PE1NWL)
  (4mA seems the optimum value in most cases, but you may try different drive strengths for best results -
  accepted values are 2,4,6,8 mA)

v1.04
- Sketch now allows the (optional) use of a 10-turn potentiometer for complete band coverage (Allard, PE1NWL)
- Standard settings are still for a 1-turn pot.
- But if you want to use a 10-turn pot instead, change the values for 'TUNING_RANGE' and 'baseTune'
  in lines 189 and 190 to your liking
  
v1.03
- improved tuning "flutter fix" (Jerry, KE7ER)

v1.02
- fixed the calibration routine (Allard, PE1NWL)
- fetch the calibration correction factor from EEPROM at startup (Allard, PE1NWL)
- added some changes to comply with si5351 library v2. (Allard, PE1NWL)

v1.01
- original BITX40 sketch (Ashhar Farhan)
