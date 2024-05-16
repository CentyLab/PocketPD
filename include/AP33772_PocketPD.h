/* Structs and Resgister list ported from "AP33772 I2C Command Tester" by Joseph Liang
 * Created 11 April 2022
 * Added class and class functions by VicentN for PicoPD evaluation board
 */
#ifndef __AP33772_POCKETPD__
#define __AP33772_POCKETPD__

#include <Arduino.h>
#include <Wire.h>

#define CMD_SRCPDO 0x00
#define CMD_PDONUM 0x1C
#define CMD_STATUS 0x1D
#define CMD_MASK 0x1E
#define CMD_VOLTAGE 0x20
#define CMD_CURRENT 0x21
#define CMD_TEMP 0x22
#define CMD_OCPTHR 0x23
#define CMD_OTPTHR 0x24
#define CMD_DRTHR 0x25
#define CMD_RDO 0x30

#define AP33772_ADDRESS 0x51
#define READ_BUFF_LENGTH     30
#define WRITE_BUFF_LENGTH    6
#define SRCPDO_LENGTH        28

typedef enum{
  READY_EN    = 1 << 0, // 0000 0001
  SUCCESS_EN  = 1 << 1, // 0000 0010
  NEWPDO_EN   = 1 << 2, // 0000 0100
  OVP_EN      = 1 << 4, // 0001 0000
  OCP_EN      = 1 << 5, // 0010 0000
  OTP_EN      = 1 << 6, // 0100 0000
  DR_EN       = 1 << 7  // 1000 0000
} AP33772_MASK;

typedef struct
{
  union
  {
    struct
    {
      byte isReady : 1;
      byte isSuccess : 1;
      byte isNewpdo : 1;
      byte reserved : 1;
      byte isOvp : 1;
      byte isOcp : 1;
      byte isOtp : 1;
      byte isDr : 1;
    };
    byte readStatus;
  };
  byte readVolt; // LSB: 80mV
  byte readCurr; // LSB: 24mA
  byte readTemp; // unit: 1C
} AP33772_STATUS_T;

typedef struct
{
  union
  {
    struct
    {
      byte newNegoSuccess : 1;
      byte newNegoFail : 1;
      byte negoSuccess : 1;
      byte negoFail : 1;
      byte reserved_1 : 4;
    };
    byte negoEvent;
  };
  union
  {
    struct
    {
      byte ovp : 1;
      byte ocp : 1;
      byte otp : 1;
      byte dr : 1;
      byte reserved_2 : 4;
    };
    byte protectEvent;
  };
} EVENT_FLAG_T;

typedef struct
{
  union
  {
    struct
    {
      unsigned int maxCurrent : 10; // unit: 10mA
      unsigned int voltage : 10;    // unit: 50mV
      unsigned int reserved_1 : 10;
      unsigned int type : 2;
    } fixed;
    struct
    {
      unsigned int maxCurrent : 7; // unit: 50mA
      unsigned int reserved_1 : 1;
      unsigned int minVoltage : 8; // unit: 100mV
      unsigned int reserved_2 : 1;
      unsigned int maxVoltage : 8; // unit: 100mV
      unsigned int reserved_3 : 3;
      unsigned int apdo : 2;
      unsigned int type : 2;
    } pps;
    struct
    {
      byte byte0;
      byte byte1;
      byte byte2;
      byte byte3;
    };
    unsigned long data;
  };
} PDO_DATA_T;

typedef struct
{
  union
  {
    struct
    {
      unsigned int maxCurrent : 10; // unit: 10mA
      unsigned int opCurrent : 10;  // unit: 10mA
      unsigned int reserved_1 : 8;
      unsigned int objPosition : 3;
      unsigned int reserved_2 : 1;
    } fixed;
    struct
    {
      unsigned int opCurrent : 7; // unit: 50mA
      unsigned int reserved_1 : 2;
      unsigned int voltage : 11; // unit: 20mV
      unsigned int reserved_2 : 8;
      unsigned int objPosition : 3;
      unsigned int reserved_3 : 1;
    } pps;
    struct
    {
      byte byte0;
      byte byte1;
      byte byte2;
      byte byte3;
    };
    unsigned long data;
  };
} RDO_DATA_T;

class AP33772
{
public:
  AP33772(TwoWire &wire = Wire);
  void begin();
  void setVoltage(int targetVoltage); // Unit in mV
  void setMaxCurrent(int targetMaxCurrent); // Unit in mA
  void setNTC(int TR25, int TR50, int TR75, int TR100);
  void setDeratingTemp(int temperature);
  void setMask(AP33772_MASK flag);
  void clearMask(AP33772_MASK flag);
  void writeRDO();
  int readVoltage();
  int readCurrent();
  int getMaxCurrent() const;
  int readTemp();
  void printPDO();
  void reset();

  int getNumPDO();
  int getPPSIndex();
  int getPDOMaxcurrent(uint8_t PDOindex);
  int getPDOVoltage(uint8_t PDOindex);
  int getPPSMinVoltage(uint8_t PPSindex);
  int getPPSMaxVoltage(uint8_t PPSindex);
  int getPPSMaxCurrent(uint8_t PPSindex);
  void setSupplyVoltageCurrent(int targetVoltage, int targetCurrent);
  byte existPPS = 0;  // PPS flag for setVoltage()

private:
  void i2c_read(byte slvAddr, byte cmdAddr, byte len);
  void i2c_write(byte slvAddr, byte cmdAddr, byte len);
  TwoWire *_i2cPort;
  byte readBuf[READ_BUFF_LENGTH] = {0};
  byte writeBuf[WRITE_BUFF_LENGTH] = {0};
  byte numPDO = 0;    // source PDO number
  byte indexPDO = 0;  // PDO index, start from index 0
  int reqPpsVolt = 0; // requested PPS voltage, unit:20mV
  
  int8_t PPSindex = 8; 
  
  AP33772_STATUS_T ap33772_status = {0};
  EVENT_FLAG_T event_flag = {0};
  RDO_DATA_T rdoData = {0};
  PDO_DATA_T pdoData[7] = {0};
};

#endif
