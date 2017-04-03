# bitx40
BITX40 sketch for Raduino

Important:
This sketch is confirmed working OK with the si5351 library v2.01.
Older library version definitely don't work (compilation errors).
Recently released versions v2.02 and v2.03 compile OK but give strong pops/clicks in the speaker during tuning.
This issue is still under investigation.
# Until this is solved, please use v2.01 of the si5351 library.
The si5351 library v2.01 can be downloaded from https://github.com/etherkit/Si5351Arduino/releases/tag/v2.0.1

Revision record

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
