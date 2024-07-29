List of requested UI changes:
+ [x] Change screen from "I" to "A" for current fixed reading
+ [ ] Indication if encoder is mapped to voltage to current setting. With blinking digit can set increment to 1V, 100mV, or 20mV.
+ [x] Bootup voltage 5A with limit at 1A is best.
+ [x] Add mV suffix to set value
+ [ ] Pressing encoder in current mode can change between 50mA to 500mA
+ [x] Skipping bootup screen by pressing any button
+ [ ] Default is PPS but turning knob at bootup screen to select desire profile (PPS, PDO, QC3.0).
+ [ ] Corner of the screen now display protocol PPS, Fixed, or QC3.0
+ [ ] Addition all menu afterboot up to switch between mode if long press Voltage/Current button


Nice to have, add QC3.0 support:
+ [ ] Add QC3.0 detection
+ [ ] Request voltage from QC3.0 charger
+ [ ] Request voltage from QC3.0 charger after PD profile detection
+ [ ] Display QC3.0 profile at boot screen
+ [ ] Press encoder at bootup to select profile between fixed PDO, PPS, or QC3.0

```
//Tested with USB-C to USB-A, and USB-C to USB-C on PC/Mac, Anker PowerCore24k, and UGREEN 140W
#define MEM32(address) (*(volatile uint32_t*)(address))
#define ADDR_ENDP MEM32(USBCTRL_REGS_BASE + 0x00)

value = MEM32(USBCTRL_REGS_BASE + 0x00) & 0x007F; //Masking is only to read bit 0 to 6
if(value != 0)
    //USB is acting as device.
else
    //Not connected to PC
```