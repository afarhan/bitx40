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

v1.03
- improved tuning "flutter fix" (Jerry, KE7ER)

v1.02
- fixed the calibration routine (Allard, PE1NWL)
- fetch the calibration correction factor from EEPROM at startup (Allard, PE1NWL)
- added some changes to comply with si5351 library v2. (Allard, PE1NWL)

v1.01
- original BITX40 sketch (Ashhar Farhan)
