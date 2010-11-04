#ifndef __INC_SPIRGB_H
#define __INC_SPIRGB_H
#include <HardwareSerial.h>
#include <WProgram.h>
#include "string.h"
#include <avr/pgmspace.h>

extern "C" { 
  void TIMER1_OVF_vect(void) __attribute__ ((signal,naked,__INTR_ATTRS));
  void SPI_STC_vect(void) __attribute__ ((signal,__INTR_ATTRS));
};

/// A class for controlling a shift-register based LED decoder board such as this one: http://www.usledsupply.com/shop/rgb-32-spi-dmx-decoder.html
/// or using the hl1606 or the lpd6803 controller chips
class CFastSPI_LED { 
public:
  unsigned char clockSelectBits;
  unsigned int m_cpuPercentage;
  
  // computed time to run 10 cycles
  unsigned long m_adjustedUSecTime;
  
  // timer setup functions - will auto-calibrate (timer1, at least) based on other decisions user has made
  void setup_timer1_ovf();
  void setup_hardware_spi();
  
  friend void TIMER1_OVF_vect(void);
  friend void SPI_STC_vect(void);
public:
  enum EChipSet { 
    SPI_595 = 0x01,
    SPI_HL1606 = 0x02,
    SPI_LPD6803 = 0x04,
    SPI_WS2801 = 0x08
  };

public:  
  int m_nLeds;
  unsigned char m_nDataRate;
  unsigned char m_nDirty;
  unsigned char m_eChip;
  unsigned long m_nCounter;
  unsigned char *m_pData;
  unsigned char *m_pDataEnd;
  
public:
  CFastSPI_LED() {m_nDataRate=0; m_cpuPercentage=50; m_nCounter = 0; m_nDirty=0; m_eChip = SPI_595; m_adjustedUSecTime=0;}
 
  // set the number of rgb leds 
  void setLeds(int nLeds) { m_nLeds = nLeds * 3; m_nCounter = 0; m_nDirty = 0; m_pData = (unsigned char*)malloc(m_nLeds); memset(m_pData,0,m_nLeds); m_pDataEnd = m_pData + m_nLeds; }
  
  // set the chipset being used - note: this will reset default values for cpuPercentage and refresh/color level rates
 void setChipset(EChipSet);
 
  // set the desired cpu percentage - changes won't take effect until calling init
  void setCPUPercentage(unsigned int perc) { m_cpuPercentage = perc; }
  
  // set the desired number of color levels (only useful for software pwm chipsets (e.g. 595 or hl1606), chipsets
  // like the lpd6803 offload pwm to the chip, meaning the number of color levels is hardwired to some predefined
  // number, e.g. 32 for the lpd6803.  Note that raising this value will lower the refresh rate in hz that you
  // have.  Maxes out at 255
  void setColorLevels(unsigned int nLevels) { }
  
  // set the desired refresh rate in hz (only useful for software pwm chipsets, e.g. 595 or hl1606).  
  // note that increasining the desired refresh rate may reduce the number of color levels you have available
  // to you.  Note that your requested refresh rate may not be possible.
  void setRefreshRate(unsigned int nDesiredRate) { } 
  
  // initialize the engine - note this will also be re-called if one of the auto-calibration values
  // (e.g. percentage, refresh rate, brightness levels) is changed
  void init();
  
  // start the rgb output 
  void start();
  
  // stop the rgb output 
  void stop();
  
  // call this method whenever you want to output a block of rgb data.  It is assumed that 
  // rgbData is nNumLeds * 3 bytes long.
  void setRGBData(unsigned char *rgbData) { memcpy(m_pData,rgbData,m_nLeds); m_nDirty=1;}
  
  // call this method to get a pointer to the raw rgb data
  unsigned char *getRGBData() { return m_pData; }
  
  // mark the current data as 'dirty' (used by some chipsets) - should be called
  // when writing data in manually using getRGBData instead of setRGBData
  void setDirty();
  
  // 'show' or push the current led data (note, with some chipsets, data shows up in the
  // leds as soon as it is written to the array returned by getRGBData.
  void show() { setDirty(); }
  
  // get a count of how many output cycles have been performed.  Only useful during debugging, this
  // will often return 0
  unsigned long getCounter() { return m_nCounter; }
  
  // clear the output cycle counter.  
  void clearCounter() { m_nCounter=0; }
  
  // get the time in µs to run one cycle of output
  unsigned long getCycleTime() { return m_adjustedUSecTime / 10; } 
  
  // get the target number of cycles per second - cycleTarget / nLeds - total number of full updates for lights
  // available per second.  For manually PWM'd chipsets like 595 or hl1606, that base number is also
  // desiredHz * colorLevels - higher refresh rate means fewer color levels, which may mean more flickering
  unsigned long getCycleTarget() { return (((unsigned long)m_cpuPercentage) * 100000) / m_adjustedUSecTime; }

  // override the spi data rate - the library sets a default data rate for each chipset.  You
  // can forcibly change the data rate that you want used, with 0 being the fastest and 7 being
  // the slowest.  Note that you may need to play around with the max cpu percentage a bit if you
  // change the data rate.  Call this after you have set the chipset to override the chipset's data rate
  void setDataRate(int datarate);

};

extern CFastSPI_LED FastSPI_LED;
#endif

