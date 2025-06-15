#include <U8g2lib.h>
#include <Button.h>
#include <RotaryEncoder.h>
#include <AP33772_PocketPD.h>

#ifndef MENU_H
#define MENU_H

/**
 * Need to pass through
 * u8g2 instance
 * button and encoder detection for menu interaction
 */

/**
 * How do I build a menu list
 * Ref code: https://github.com/shuzonudas/monoview/blob/master/U8g2/Examples/Menu/simpleMenu/simpleMenu.ino#L278
 */

class Menu
{
public:
    Menu(U8G2_SSD1306_128X64_NONAME_F_HW_I2C *oled, AP33772 *usb, RotaryEncoder *encoder, Button *button_encoder, Button *button_output, Button *button_selectVI);
    uint8_t numPDO; // include both fixed PDO and PPS
    int menuPosition;
    // void get_numPDO();
    void set_qc3flag(bool flag);
    void page_selectCapability();
    void page_bootProfile();
    void checkMenuPosition(bool wrapAround); // Check if menuPosition is within range, wrap around if not

private:
    U8G2_SSD1306_128X64_NONAME_F_HW_I2C *u8g2;
    AP33772 *usbpd;
    RotaryEncoder *_encoder;
    Button *_button_encoder;
    Button *_button_output;
    Button *_button_selectVI;

    bool qc3_0available;
};

#endif