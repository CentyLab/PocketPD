#include <Menu.h>

/**
 * Constructor
 * Pass in pointer to local variable to simplified future funtion calls
 */
Menu::Menu(U8G2_SSD1306_128X64_NONAME_F_HW_I2C* oled, AP33772* usb, RotaryEncoder* encoder, Button* button_encoder, Button* button_output, Button* button_selectVI)
{
    u8g2 = oled;
    usbpd = usb;
    _encoder = encoder;
    _button_encoder = button_encoder;
    _button_output = button_output;
    _button_selectVI = button_selectVI;

    menuPosition = 0;
    _numPDO = 0;
    qc3_0available = 0;
}

void Menu::page_selectCapability()
{ 
    uint8_t linelocation;
    static int val;

    if(val = (int8_t)_encoder->getDirection())
    {
        menuPosition = menuPosition - val;
        if(menuPosition < 0) menuPosition = _numPDO + qc3_0available - 1; //wrap around
        if(menuPosition > (_numPDO + qc3_0available - 1)) menuPosition = 0; //wrap around
    }
    
    u8g2->clearBuffer();
    for(byte i = 0; i< usbpd->getNumPDO(); i++){
        if(i != usbpd->getPPSIndex()){
        linelocation = 9*(i+1);
        u8g2->setCursor(5, linelocation);
        u8g2->print("PDO: ");
        u8g2->print(usbpd->getPDOVoltage(i)/1000.0,0);
        u8g2->print("V @ ");
        u8g2->print(usbpd->getPDOMaxcurrent(i)/1000.0,0);
        u8g2->print("A");
        }
        else if(usbpd->existPPS){
        linelocation = 9*(i+1);
        u8g2->setCursor(5, linelocation);
        u8g2->print("PPS: ");
        u8g2->print(usbpd->getPPSMinVoltage(i)/1000.0,1);
        u8g2->print("V~");
        u8g2->print(usbpd->getPPSMaxVoltage(i)/1000.0,1);
        u8g2->print("V @ ");
        u8g2->print(usbpd->getPPSMaxCurrent(i)/1000.0,0);
        u8g2->print("A");
        }
        if(i == menuPosition)
        {
            u8g2->setCursor(0, linelocation);
            u8g2->print(">");
        }
    }

    u8g2->sendBuffer();
}


void Menu::page_bootProfile()
{
    
}

/**
 * Call this function after usbpd.begin()
 */
void Menu::get_numPDO()
{
    //Pass in AP33772 flag
    _numPDO = usbpd->getNumPDO();
}