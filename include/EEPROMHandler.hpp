#ifndef EEPROMHANDLER_HPP
#define EEPROMHANDLER_HPP

#include <Arduino.h>
#include <EEPROM.h>

#define EEPROM_SIZE          4096              // Size of EEPROM in bytes
#define EEPROM_SAVE_INTERVAL 2000              // Time interval to save settings to EEPROM in milliseconds


struct Settings {
        uint32_t           crc;                // CRC for settings validation
        int32_t            targetVoltage;      // Target voltage in mV
        int32_t            targetCurrent;      // Target current in mA
        int32_t            menuPosition;       // Current menu position
};

class EEPROMHandler {
    bool     initialized   = false;            // Flag to check if EEPROM is initialized
    bool     firstReadOk   = false;            // Flag to check if the first read from EEPROM was successful
    Settings settings;
 public:
    EEPROMHandler() { }
    virtual ~EEPROMHandler() { }
    
    void begin() {
        if(!initialized) {
            EEPROM.begin(EEPROM_SIZE);
            Serial.printf("EEPROM initialized %4d bytes | Settings size: %4d\n\r", EEPROM_SIZE, sizeof(Settings));
            initialized = true;
        }
    }

    uint32_t calculateCRC() {
        uint32_t crc = 0;
        for(size_t i = 1; i < sizeof(Settings)/sizeof(uint32_t); ++i) {
            crc ^= ((uint32_t*)&settings)[i];
        }
        return crc;
    }

    bool loadSettings(Settings &outSettings) {
        begin();

        if(firstReadOk) {
            Serial.println("Using cached settings.");
            outSettings = settings;
            return true;
        }
        
        EEPROM.get(0, settings);
        uint32_t crc = calculateCRC();
        
        Serial.printf("Crc calculated: %08X, stored crc: %08X\n\r", crc, settings.crc);
        Serial.printf("Target Voltage: %d mV, Target Current: %d mA, Menu Position: %d\n\r", 
                       settings.targetVoltage, settings.targetCurrent, settings.menuPosition);

        if(settings.crc != 0 && settings.crc == crc) {
            outSettings = settings;
            Serial.println("Settings loaded successfully.");
            firstReadOk = true;
        } else {
            Serial.println("Invalid settings in EEPROM.");
            firstReadOk = false;
        }
        return firstReadOk;
    }

    bool saveSettings(Settings &newSettings) {
        if(!initialized) {
            Serial.println("EEPROM not initialized, cannot save settings.");
            return false;
        }

        newSettings.crc = calculateCRC();
        // Compare new settings with current settings
        if(firstReadOk && memcmp(&settings, &newSettings, sizeof(Settings)) == 0) {
            Serial.println("No changes in settings, skipping save.");
            return true;
        }

        settings = newSettings;
        
        EEPROM.put(0, settings);
        if(EEPROM.commit()) {
            Serial.println("Settings saved successfully.");
            return true;
        } else {
            Serial.println("Failed to save settings.");
            return false;
        }
    }
};
#endif // EEPROMHANDLER_HPP