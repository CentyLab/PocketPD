/* Structs and Resgister list ported from "AP33772 I2C Command Tester" by Joseph Liang
 * Created 11 April 2022
 * Added class and class functions by VicentN for PicoPD evaluation board
 * Created 8 August 2023
 */

#include "AP33772_PocketPD.h"

/**
 * @brief Class constuctor
 * @param &wire reference of Wire class. Pass in Wire or Wire1
 */
AP33772::AP33772(TwoWire &wire)
{
    _i2cPort = &wire;
}

/**
 * @brief Check if power supply is good and fetch the PDO profile
 */
void AP33772::begin()
{
    i2c_read(AP33772_ADDRESS, CMD_STATUS, 1); // CMD: Read Status
    ap33772_status.readStatus = readBuf[0];

    if (ap33772_status.isOvp)
        event_flag.ovp = 1;
    if (ap33772_status.isOcp)
        event_flag.ocp = 1;

    if (ap33772_status.isReady) // negotiation finished
    {
        if (ap33772_status.isNewpdo) // new PDO
        {
            if (ap33772_status.isSuccess) // negotiation success
                event_flag.newNegoSuccess = 1;
            else
                event_flag.newNegoFail = 1;
        }
        else
        {
            if (ap33772_status.isSuccess)
                event_flag.negoSuccess = 1;
            else
                event_flag.negoFail = 1;
        }
    }
    delay(10);

    // If negotiation is good, let's load in some PDOs from charger
    if (event_flag.newNegoSuccess | event_flag.negoSuccess)
    {
        event_flag.newNegoSuccess = 0;

        i2c_read(AP33772_ADDRESS, CMD_PDONUM, 1); // CMD: Read PDO Number
        numPDO = readBuf[0];

        i2c_read(AP33772_ADDRESS, CMD_SRCPDO, SRCPDO_LENGTH); // CMD: Read PDOs
        // copy PDOs to pdoData[]
        for (byte i = 0; i < numPDO; i++)
        {
            pdoData[i].byte0 = readBuf[i * 4];
            pdoData[i].byte1 = readBuf[i * 4 + 1];
            pdoData[i].byte2 = readBuf[i * 4 + 2];
            pdoData[i].byte3 = readBuf[i * 4 + 3];

            if ((pdoData[i].byte3 & 0xF0) == 0xC0) // PPS profile found
            {
                PPSindex = i; // Store index
                existPPS = 1; // Turn on flag
            }
        }
    }
}

/**
 * @brief Set VBUS voltage and max current
 * Current will automatically be in limit mode
 * @param targetVoltage, targetCurrent, - mV and mA
 */
void AP33772::setSupplyVoltageCurrent(int targetVoltage, int targetCurrent)
{
    if ((existPPS) && (pdoData[PPSindex].pps.maxVoltage * 100 >= targetVoltage) && (pdoData[PPSindex].pps.minVoltage * 100 <= targetVoltage))
    {
        indexPDO = PPSindex;
        reqPpsVolt = targetVoltage / 20;        // Unit in 20mV/LBS
        rdoData.pps.objPosition = PPSindex + 1; // index 1
        // rdoData.pps.opCurrent = pdoData[PPSindex].pps.maxCurrent;
        rdoData.pps.opCurrent = targetCurrent / 50; // Current limit
        rdoData.pps.voltage = reqPpsVolt;
        writeRDO();
        return;
    }
}

/**
 * @brief Set VBUS voltage
 * Current is set to MAX all the time
 * @param targetVoltage in mV
 */
void AP33772::setVoltage(int targetVoltage)
{
    /*
    Step 1: Check if PPS can satify request voltage
    Step 2: Scan PDOs to see what is the lower closest voltage to request
    Step 3: Compare found PDOs votlage and PPS max voltage
    */
    byte tempIndex = 0;
    if ((existPPS) && (pdoData[PPSindex].pps.maxVoltage * 100 >= targetVoltage) && (pdoData[PPSindex].pps.minVoltage * 100 <= targetVoltage))
    {
        indexPDO = PPSindex;
        reqPpsVolt = targetVoltage / 20;        // Unit in 20mV/LBS
        rdoData.pps.objPosition = PPSindex + 1; // index 1
        rdoData.pps.opCurrent = pdoData[PPSindex].pps.maxCurrent;
        rdoData.pps.voltage = reqPpsVolt;
        writeRDO();
        return;
    }
    else
    {
        // Step 2: Scan PDOs to see what is the lower closest voltage to request
        for (byte i = 0; i < numPDO - existPPS; i++)
        {
            if (pdoData[i].fixed.voltage * 50 <= targetVoltage)
                tempIndex = i;
        }
        // Step 3: Compare found PDOs votlage and PPS max voltage
        if (pdoData[tempIndex].fixed.voltage * 50 > pdoData[PPSindex].pps.maxVoltage * 100)
        {
            indexPDO = tempIndex;
            rdoData.fixed.objPosition = tempIndex + 1; // Index 0 to Index 1
            rdoData.fixed.maxCurrent = pdoData[indexPDO].fixed.maxCurrent;
            rdoData.fixed.opCurrent = pdoData[indexPDO].fixed.maxCurrent;
            writeRDO();
            return;
        }
        else // If PPS voltage larger or equal to Fixed PDO
        {
            indexPDO = PPSindex;
            reqPpsVolt = pdoData[PPSindex].pps.maxVoltage * 5; // Convert unit: 100mV -> 20mV
            rdoData.pps.objPosition = PPSindex + 1;            // index 1
            rdoData.pps.opCurrent = pdoData[PPSindex].pps.maxCurrent;
            rdoData.pps.voltage = reqPpsVolt;
            writeRDO();
            return;
        }
    }
}

/**
 * @brief Request PDO profile.
 * @param PDOindex start from index 0 to (PDONum - 1) if no PPS, (PDONum -2) if PPS found
 */
void AP33772::setPDO(uint8_t PDOindex)
{
    uint8_t guarding;

    if (PPSindex == 1)
        {guarding = numPDO - 2;}
    else
        guarding = numPDO - 1; // Example array[4] only exist index 0,1,2,3

    if (PDOindex <= guarding)
    {
        rdoData.fixed.objPosition = PDOindex + 1; // Index 0 to Index 1
        rdoData.fixed.maxCurrent = pdoData[PDOindex].fixed.maxCurrent;
        rdoData.fixed.opCurrent = pdoData[PDOindex].fixed.maxCurrent;
        writeRDO();
    }
}

/**
 * @brief Set max current before tripping at wall plug
 * @param targetMaxCurrent in mA
 */
void AP33772::setMaxCurrent(int targetMaxCurrent)
{
    /*
    Step 1: Check if current profile is PPS, check if max current is lower than request
        If yes, set new max current
        If no, report fault
    Step 2: If profile is PDO, check if max current is lower than request
        If yes, set new max current
        If no, report fault
    */
    if (indexPDO == PPSindex)
    {
        if (targetMaxCurrent <= pdoData[PPSindex].pps.maxCurrent * 50)
        {
            rdoData.pps.objPosition = PPSindex + 1;        // index 1
            rdoData.pps.opCurrent = targetMaxCurrent / 50; // 50mA/LBS
            rdoData.pps.voltage = reqPpsVolt;
            writeRDO();
        }
        else
        {
        } // Do nothing
    }
    else
    {
        if (targetMaxCurrent <= pdoData[indexPDO].fixed.maxCurrent * 10)
        {
            rdoData.fixed.objPosition = indexPDO + 1;         // Index 0 to Index 1
            rdoData.fixed.maxCurrent = targetMaxCurrent / 10; // 10mA/LBS
            rdoData.fixed.opCurrent = targetMaxCurrent / 10;  // 10mA/LBS
            writeRDO();
        }
        else
        {
        } // Do nothing
    }
}

/**
 * @brief Set resistance value of 10K NTC at 25C, 50C, 75C and 100C.
 *          Default is 10000, 4161, 1928, 974Ohm
 * @param TR25, TR50, TR75, TR100 unit in Ohm
 * @attention Blocking function due to long I2C write, min blocking time 15ms
 */
void AP33772::setNTC(int TR25, int TR50, int TR75, int TR100) // Parameter NOT DONE
{
    writeBuf[0] = TR25 & 0xff;
    writeBuf[1] = (TR25 >> 8) & 0xff;
    i2c_write(AP33772_ADDRESS, 0x28, 2);
    delay(5);
    writeBuf[0] = TR50 & 0xff;
    writeBuf[1] = (TR50 >> 8) & 0xff;
    i2c_write(AP33772_ADDRESS, 0x2A, 2);
    delay(5);
    writeBuf[0] = TR75 & 0xff;
    writeBuf[1] = (TR75 >> 8) & 0xff;
    i2c_write(AP33772_ADDRESS, 0x2C, 2);
    delay(5);
    writeBuf[0] = TR100 & 0xff;
    writeBuf[1] = (TR100 >> 8) & 0xff;
    i2c_write(AP33772_ADDRESS, 0x2E, 2);
}

/**
 * @brief Set target temperature (C) when output power through USB-C is reduced
 *          Default is 120 C
 * @param temperature (unit in Celcius)
 */
void AP33772::setDeratingTemp(int temperature)
{
    writeBuf[0] = temperature;
    i2c_write(AP33772_ADDRESS, CMD_DRTHR, 1);
}

void AP33772::setMask(AP33772_MASK flag)
{
    // First read in what is currently in the MASK
    i2c_read(AP33772_ADDRESS, CMD_MASK, 1);
    writeBuf[0] = readBuf[0] | flag;
    delay(5); // Short break between read/write
    i2c_write(AP33772_ADDRESS, CMD_MASK, 1);
}

void AP33772::clearMask(AP33772_MASK flag)
{
    // First read in what is currently in the MASK
    i2c_read(AP33772_ADDRESS, CMD_MASK, 1);
    writeBuf[0] = readBuf[0] & ~flag;
    delay(5); // Short break between read/write
    i2c_write(AP33772_ADDRESS, CMD_MASK, 1);
}

void AP33772::i2c_read(byte slvAddr, byte cmdAddr, byte len)
{
    // clear readBuffer
    for (byte i = 0; i < READ_BUFF_LENGTH; i++)
    {
        readBuf[i] = 0;
    }
    byte i = 0;
    Wire.beginTransmission(slvAddr); // transmit to device SLAVE_ADDRESS
    Wire.write(cmdAddr);             // sets the command register
    Wire.endTransmission();          // stop transmitting

    Wire.requestFrom(slvAddr, len); // request len bytes from peripheral device
    if (len <= Wire.available())
    { // if len bytes were received
        while (Wire.available())
        {
            readBuf[i] = (byte)Wire.read();
            i++;
        }
    }
}

void AP33772::i2c_write(byte slvAddr, byte cmdAddr, byte len)
{
    Wire.beginTransmission(slvAddr); // transmit to device SLAVE_ADDRESS
    Wire.write(cmdAddr);             // sets the command register
    Wire.write(writeBuf, len);       // write data with len
    Wire.endTransmission();          // stop transmitting

    // clear readBuffer
    for (byte i = 0; i < WRITE_BUFF_LENGTH; i++)
    {
        writeBuf[i] = 0;
    }
}

/**
 * @brief Write the desire power profile back to the power source
 */
void AP33772::writeRDO()
{
    writeBuf[3] = rdoData.byte3;
    writeBuf[2] = rdoData.byte2;
    writeBuf[1] = rdoData.byte1;
    writeBuf[0] = rdoData.byte0;
    i2c_write(AP33772_ADDRESS, CMD_RDO, 4); // CMD: Write RDO
}

/**
 * @brief Read VBUS voltage
 * @return voltage in mV
 */
int AP33772::readVoltage()
{
    i2c_read(AP33772_ADDRESS, CMD_VOLTAGE, 1);
    return readBuf[0] * 80; // I2C read return 80mV/LSB
}

/**
 * @brief Read VBUS current
 * @return current in mA
 */
int AP33772::readCurrent()
{
    i2c_read(AP33772_ADDRESS, CMD_CURRENT, 1);
    return readBuf[0] * 16; // I2C read return 24mA/LSB
}

/**
 * @brief Read maximum VBUS current
 * @return current in mA
 */
int AP33772::getMaxCurrent() const
{
    if (indexPDO == PPSindex)
    {
        return pdoData[PPSindex].pps.maxCurrent * 50;
    }
    else
    {
        return pdoData[indexPDO].fixed.maxCurrent * 10;
    }
}

/**
 * @brief Read NTC temperature
 * @return tempearture in C
 */
int AP33772::readTemp()
{
    i2c_read(AP33772_ADDRESS, CMD_TEMP, 1);
    return readBuf[0]; // I2C read return 1C/LSB
}

/**
 * @brief Hard reset the power supply. Will temporary cause power outage
 */
void AP33772::reset()
{
    writeBuf[0] = 0x00;
    writeBuf[1] = 0x00;
    writeBuf[2] = 0x00;
    writeBuf[3] = 0x00;
    i2c_write(AP33772_ADDRESS, CMD_RDO, 4);
}

/**
 * @brief Debug code to quickly check power supply profile PDOs
 */
void AP33772::printPDO()
{
    Serial.print("Source PDO Number = ");
    Serial.print(numPDO);
    Serial.println();

    for (byte i = 0; i < numPDO; i++)
    {
        if ((pdoData[i].byte3 & 0xF0) == 0xC0) // PPS PDO
        {
            Serial.print("PDO[");
            Serial.print(i + 1); // PDO position start from 1
            Serial.print("] - PPS : ");
            Serial.print((float)(pdoData[i].pps.minVoltage) * 100 / 1000);
            Serial.print("V~");
            Serial.print((float)(pdoData[i].pps.maxVoltage) * 100 / 1000);
            Serial.print("V @ ");
            Serial.print((float)(pdoData[i].pps.maxCurrent) * 50 / 1000);
            Serial.println("A");
        }
        else if ((pdoData[i].byte3 & 0xC0) == 0x00) // Fixed PDO
        {
            Serial.print("PDO[");
            Serial.print(i + 1);
            Serial.print("] - Fixed : ");
            Serial.print((float)(pdoData[i].fixed.voltage) * 50 / 1000);
            Serial.print("V @ ");
            Serial.print((float)(pdoData[i].fixed.maxCurrent) * 10 / 1000);
            Serial.println("A");
        }
    }
    Serial.println("===============================================");
}

/**
 * Add on for PPS Bench Power Supply
 */

/**
 * @brief Get the number of power profile, include PPS if exist
 */
int AP33772::getNumPDO()
{
    return numPDO;
}

/**
 * @brief Get index of PPS profile
 */
int AP33772::getPPSIndex()
{
    return PPSindex;
}

/**
 * @brief MaxCurrent for fixed voltage PDO
 * @return Current in mAmp
 */
int AP33772::getPDOMaxcurrent(uint8_t PDOindex)
{
    return pdoData[PDOindex].fixed.maxCurrent * 10;
}

/**
 * @brief Get fixed PDO voltage
 * @return Voltage in mVolt
 */
int AP33772::getPDOVoltage(uint8_t PDOindex)
{
    return pdoData[PDOindex].fixed.voltage * 50;
}

/**
 * @brief Get PPS min votlage
 * @return Voltage in mVolt
 */
int AP33772::getPPSMinVoltage(uint8_t PPSindex)
{
    return pdoData[PPSindex].pps.minVoltage * 100;
}

/**
 * @brief Get PPS max votlage
 * @return Voltage in mVolt
 */
int AP33772::getPPSMaxVoltage(uint8_t PPSindex)
{
    return pdoData[PPSindex].pps.maxVoltage * 100;
}

/**
 * @brief Get PPS max current
 * @return Current in mAmp
 */
int AP33772::getPPSMaxCurrent(uint8_t PPSindex)
{
    return pdoData[PPSindex].pps.maxCurrent * 50;
}