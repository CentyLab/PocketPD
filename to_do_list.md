List of requested UI changes:
+ [x] Change screen from "I" to "A" for current fixed reading
+ [x] Indication if encoder is mapped to voltage to current setting. With blinking digit can set increment to 1V, 100mV, or 20mV.
+ [x] Bootup voltage 5A with limit at 1A is best.
+ [x] Add mV suffix to set value
+ [x] Pressing encoder in current mode can change between 50mA to 200mA
+ [x] Skipping bootup screen by pressing any button
+ [x] Default is PPS but turning knob at bootup screen to select desire profile (PPS, PDO, QC3.0).
+ [x] Corner of the screen now display protocol PPS, Fixed, or QC3.0
+ [x] Addition all menu afterboot up to switch between mode if long press Voltage/Current button


+ [x] Calibrate current sensor
+ [x] Delay CC/CV update to reduce mode flicker.
+ [x] Fix CC/CV bug at no load.


+ [x] Add EEPROM function to remember last used voltage/current and PD mode settings.

+ [x] Change Startup PPS to be Corse **#issue #17**
+ [x] Fix quickly turning the encoder makes the power supply not delivering the set voltage. **#issue #11**
+ [x] Fix Voltage selection bug near 3.3V **#issue #3**
+ [x] Add counter for mAh or Wh used **#issue #13**

Backlog:
+ [ ] Add more cable loss when connect through Android adapter, detect computer connection.
+ [ ] Add reminder boot screen for buttons function **#issue #14**

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


Code structure note:
Right now the statemachine is handlding the voltage calling as well as OLED update. 
OLED update function is a one big function that is now taking in 1 flag to determine what to display and what not to display.

Increments.. is currently one single global variable but PPS and QC has different adjustment.

Best if each stage has its own OLED print method, and increment setting or button mapping.
