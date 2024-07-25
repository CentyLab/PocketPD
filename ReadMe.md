List of requested UI changes:
+ [x] Change screen from "I" to "A" for current fixed reading
+ [ ] Indication if encoder is mapped to voltage to current setting. With blinking digit can set increment to 1V, 100mV, or 20mV.
+ [x] Bootup voltage 5A with limit at 1A is best.
+ [ ] Add mV suffix to set value
+ [ ] Pressing encoder in current mode can change between 50mA to 500mA
+ [ ] Skipping bootup screen by pressing any button
+ [ ] Default is PPS but turning knob at bootup screen to select desire profile (PPS, PDO, QC3.0).
+ [ ] Corner of the screen now display protocol PPS, Fixed, or QC3.0
+ [ ] Addition all menu afterboot up to switch between mode if long press Voltage/Current button


Add QC3.0 support:
+ [ ] Add QC3.0 detection
+ [ ] Request voltage from QC3.0 charger
+ [ ] Request voltage from QC3.0 charger after PD profile detection
+ [ ] Display QC3.0 profile at boot screen
+ [ ] Press encoder at bootup to select profile between fixed PDO, PPS, or QC3.0

Before letting Platform IO pulling the pico-sdk file. Follow [Important steps for Windows users, before installing](https://arduino-pico.readthedocs.io/en/latest/platformio.html#important-steps-for-windows-users-before-installing)
Else you will encounter:

```
VCSBaseException: VCS: Could not process command ['git', 'clone', '--recursive', 'https://github.com/earlephilhower/arduino-pico.git', 'C:\\Users\\keylo\\.platformio\\.cache\\tmp\\pkg-installing-iypaogfn']
```
