#if defined(ARDUINO) && ARDUINO >= 100
  #include "Arduino.h"
  #include <pins_arduino.h>
#else
  #include "WProgram.h"
  #include <pins_arduino.h>
#include "wiring.h"
#endif
#include "avr/interrupt.h"
#include "avr/io.h"
#include "FastSPI_LED.h"

// #define DEBUG_SPI
#ifdef DEBUG_SPI
#  define DPRINT Serial.print
#  define DPRINTLN Serial.println
#else
#  define DPRINT(x)
#  define DPRINTLN(x)
#endif

// #define COUNT_ROUNDS 1

// some spi defines
// Duemilanove and mini w/328
#if defined(__AVR_ATmega328P__) || defined(__AVR_ATmega168__)
#define SPI_PORT PORTB
#define SPI_DDR  DDRB
#define SPI_PIN  PINB
#define SPI_MOSI 3       // Arduino pin 11.
#define SPI_MISO 4       // Arduino pin 12.
#define SPI_SCK  5       // Arduino pin 13.
#define SPI_SSN  2       // Arduino pin 10.
#define DATA_PIN 11
#define SLAVE_PIN 12
#define CLOCK_PIN 13
#define LATCH_PIN 10
#define TIMER_AVAILABLE 1

// Mega.
#elif defined(__AVR_ATmega1280__) || defined(__AVR_ATmega2560__)
#define SPI_PORT PORTB
#define SPI_DDR  DDRB
#define SPI_PIN  PINB
#define SPI_MOSI 2       // Arduino pin 51.
#define SPI_MISO 3       // Arduino pin 50.
#define SPI_SCK  1       // Arduino pin 52.
#define SPI_SSN  0       // Arduino pin 53.
#define DATA_PIN 51
#define SLAVE_PIN 50
#define CLOCK_PIN 52
#define LATCH_PIN 53
#define TIMER_AVAILABLE 1

// Leonardo, teensy, blinkm
#elif defined(__AVR_ATmega32U4__)

#define SPI_PORT PORTB
#define SPI_DDR  DDRB
#define SPI_PIN  PINB
#define SPI_MOSI 2       // Arduino pin 10.
#define SPI_MISO 3       // Arduino pin 11.
#define SPI_SCK  1       // Arduino pin 9.
#define SPI_SSN  0       // Arduino pin 8.
#define DATA_PIN 16      // PB2, pin 10, Digital16
#define SLAVE_PIN 14     // PB3, pin 11, Digital14
#define CLOCK_PIN 15     // PB1, pin 9, Digital15
#define LATCH_PIN 17     // PB0, pin 8, Digital17
#define TIMER_AVAILABLE 1

#elif defined(__MK20DX128__)   // for Teensy 3.0
#define SPI_PORT PORTB
#define SPI_DDR  DDRB
#define SPI_PIN  PINB
#define SPI_MOSI 2       // Arduino pin 10.
#define SPI_MISO 3       // Arduino pin 11.
#define SPI_SCK  1       // Arduino pin 9.
#define SPI_SSN  0       // Arduino pin 8.
#define DATA_PIN 11      // PB2, pin 10, Digital16
#define SLAVE_PIN 12     // PB3, pin 11, Digital14
#define CLOCK_PIN 13     // PB1, pin 9, Digital15
#define LATCH_PIN 10      // PB0, pin 8, Digital17

#define NOT_A_PIN NULL
#define TIMER_AVAILABLE 0
#endif

#define BIT_HI(R, P) (R) |= _BV(P)
#define BIT_LO(R, P) (R) &= ~_BV(P)

#define MASK_HI(R, P) (R) |= (P)
#define MASK_LO(R, P) (R) &= ~(P)

// HL1606 defines
//Commands for each LED to be ORd together.

#define TM_1606 153

#define Command     B10000000
#define Commandx2   B11000000   //Use this one to make dimming twice as fast.
#define BlueOff    B00000000   
#define BlueOn     B00010000
#define BlueUp     B00100000
#define BlueDown   B00110000
#define RedOff      B00000000
#define RedOn       B00000100
#define RedUp       B00001000
#define RedDown     B00001100
#define GreenOff     B00000000
#define GreenOn      B00000001
#define GreenUp      B00000010
#define GreenDown    B00000011

#define BRIGHT_MAX 256

// Some ASM defines
#if defined(__arm__) 
# define NOP __asm__ __volatile__ ("nop\n");
#else
#  define NOP __asm__ __volatile__ ("cp r0,r0\n");
#endif

#define NOP2 NOP NOP
#define NOP1 NOP
#define NOP3 NOP NOP2
#define NOP4 NOP NOP3
#define NOP5 NOP NOP4
#define NOP6 NOP NOP5
#define NOP7 NOP NOP6
#define NOP8 NOP NOP7
#define NOP9 NOP NOP8
#define NOP10 NOP NOP9
#define NOP11 NOP NOP10
#define NOP12 NOP NOP11
#define NOP13 NOP NOP12
#define NOP14 NOP NOP13
#define NOP15 NOP10 NOP5
#define NOP16 NOP14 NOP2
#define NOP20 NOP10 NOP10
#define NOP22 NOP20 NOP2

#define NOP_SHORT NOP2
#define NOP_LONG NOP5

// TODO: Better: MASH_HI(_PORT,PIN); NOP; NOP; MASK_SET(_PORT, PIN, X & (1<<N)); NOP; NOP; MASK_LO(_PORT, PIN); (loop logic)
// TODO: Best - interleave X lines 
#define TM1809_BIT_SET(X,N,_PORT) if( X & (1<<N) ) { MASK_HI(_PORT,PIN); NOP_LONG; MASK_LO(_PORT,PIN); NOP_SHORT; } else { MASK_HI(_PORT,PIN); NOP_SHORT; MASK_LO(_PORT,PIN); NOP_LONG; }

#define TM1809_BIT_ALL(_PORT)   \
TM1809_BIT_SET(x,7,_PORT); \
TM1809_BIT_SET(x,6,_PORT); \
TM1809_BIT_SET(x,5,_PORT); \
TM1809_BIT_SET(x,4,_PORT); \
TM1809_BIT_SET(x,3,_PORT); \
TM1809_BIT_SET(x,2,_PORT); \
TM1809_BIT_SET(x,1,_PORT); \
TM1809_BIT_SET(x,0,_PORT);

#define TM1809_ALL(_PORT,PTR, END) \
while(PTR != END) { register unsigned char x = *PTR++;  TM1809_BIT_ALL(_PORT); \
 x = *PTR++; TM1809_BIT_ALL(_PORT); \
 x = *PTR++; TM1809_BIT_ALL(_PORT); }

#define NOP_SHORT_1903 NOP2
#define NOP_LONG_1903 NOP15

#define UCS1903_BIT_SET(X,N,_PORT) if( X & (1<<N) ) { MASK_HI(_PORT,PIN); NOP_LONG_1903; MASK_LO(_PORT,PIN); NOP_SHORT_1903; } else { MASK_HI(_PORT,PIN); NOP_SHORT_1903; MASK_LO(_PORT,PIN); NOP_LONG_1903; }

#define UCS1903_BIT_ALL(_PORT)   \
 UCS1903_BIT_SET(x,7,_PORT); \
 UCS1903_BIT_SET(x,6,_PORT); \
 UCS1903_BIT_SET(x,5,_PORT); \
 UCS1903_BIT_SET(x,4,_PORT); \
 UCS1903_BIT_SET(x,3,_PORT); \
 UCS1903_BIT_SET(x,2,_PORT); \
 UCS1903_BIT_SET(x,1,_PORT); \
 UCS1903_BIT_SET(x,0,_PORT);

#define UCS1903_ALL(_PORT,PTR, END) \
 while(PTR != END) { register unsigned char x = *PTR++;  UCS1903_BIT_ALL(_PORT); \
   x = *PTR++; UCS1903_BIT_ALL(_PORT); \
   x = *PTR++; UCS1903_BIT_ALL(_PORT); }

#define TM1809_BIT_ALLD TM1809_BIT_ALL(PORTD);
#define TM1809_BIT_ALLB TM1809_BIT_ALL(PORTB);

#define SPI_A(data) SPDR=data;
#define SPI_B while(!(SPSR & (1<<SPIF))); 
#define SPI_TRANSFER(data) { SPDR=data; while(!(SPSR & (1<<SPIF))); } 
#define SPI_BIT(bit) digitalWrite(DATA_PIN, bit); digitalWrite(CLOCK_PIN, HIGH); digitalWrite(CLOCK_PIN, LOW);


   CFastSPI_LED FastSPI_LED;

// local prototyps
   extern "C" { 
    void spi595(void);
    void spihl1606(void);
    void spilpd6803(void);
  };

// local variables used for state tracking and pre-computed values
// TODO: move these into the class as necessary
  static unsigned char *pData;
  static unsigned char nBrightIdx=0;
  static unsigned char nBrightMax=0;
  static unsigned char nCountBase=0;
  static unsigned char nCount=0;
  static unsigned char nLedBlocks=0;
  unsigned char nChip=0;
//static unsigned long adjustedUSecTime;

#define USE_TIMER (m_eChip == SPI_595 || m_eChip == SPI_HL1606 || m_eChip == SPI_LPD6803) 
#define USE_SPI (m_eChip != SPI_TM1809 && m_eChip != SPI_UCS1903)

  void CFastSPI_LED::setDirty() { m_nDirty = 1; }

  void CFastSPI_LED::init() { 
  // store some static locals (makes lookup faster)
    pData = m_pDataEnd;
    nCountBase = m_nLeds / 3;
  // set up the spi timer - also do some initial timing loops to get base adjustments
  // for the timer below  
    setup_hardware_spi();
    if(USE_TIMER) {
      delay(10);
      setup_timer1_ovf();
    }

    if(m_eChip == SPI_LPD8806) { 
      // write out the initial set of 0's to latch out the world
      int n = (m_nLeds + 191 / 192);
      while(n--) { 
        SPI_A(0);
        SPI_B; SPI_A(0);
        SPI_B; SPI_A(0);
        SPI_B;
      }
    }
  }

// 
  void CFastSPI_LED::start() {
#if TIMER_AVAILABLE
    if(USE_TIMER) {
    TCCR1B |= clockSelectBits;                                                     // reset clock select register
  }
#endif
}

void CFastSPI_LED::stop() {
#if TIMER_AVAILABLE
  if(USE_TIMER) { 
    // clear the clock select bits
    TCCR1B &= ~(_BV(CS10) | _BV(CS11) | _BV(CS12));
  }
#endif
}


void CFastSPI_LED::setChipset(EChipSet eChip) {
  m_eChip = eChip;
  nChip = eChip;
  switch(eChip) { 
    case SPI_595: 
      nBrightIdx = 256 / 128; 
      nBrightMax = 256 - nBrightIdx;
        // Set some info used for taking advantage of extreme loop unrolling elsewhere
      if(m_nLeds % 24 == 0 ) { 
       nLedBlocks = m_nLeds / 24;
       if(nLedBlocks > 4) { nLedBlocks = 0; }
     } else { 
       nLedBlocks = 0;
     }
     break;
   case SPI_HL1606: 
      // nTimerKick = 153; // shooting for ~ 125,000 rounds/second - 66% cpu
     nBrightIdx = (m_nLeds <= 20) ? (256 / 80) : (256 / 32); 
     nBrightMax = 256 - nBrightIdx;
     nCount = nCountBase;
     break;
   case SPI_LPD6803: 
     nBrightIdx = 0; 
     break;
   default:
     // no one else does anything 'special' here
   break;

 }

  // set default cpu percentage targets  
 switch(FastSPI_LED.m_eChip) { 
  case CFastSPI_LED::SPI_595: m_cpuPercentage = 53; break;
  case CFastSPI_LED::SPI_LPD6803: m_cpuPercentage = 50; break;
  case CFastSPI_LED::SPI_HL1606: m_cpuPercentage = 65; break;
  case CFastSPI_LED::SPI_LPD8806: 
  case CFastSPI_LED::SPI_SM16716:
  case CFastSPI_LED::SPI_WS2801: m_cpuPercentage = 25; break;
  case CFastSPI_LED::SPI_TM1809: m_cpuPercentage = 5; break;
  case CFastSPI_LED::SPI_UCS1903: m_cpuPercentage = 5; break;
}  

  // set default spi rates
switch(m_eChip) { 
  case CFastSPI_LED::SPI_HL1606:
  m_nDataRate = 2;
  if(m_nLeds > 20) { 
   m_nDataRate = 3;
 }
 break;
 case CFastSPI_LED::SPI_595:  
 case CFastSPI_LED::SPI_LPD6803:
 case CFastSPI_LED::SPI_LPD8806:
 case CFastSPI_LED::SPI_WS2801:
 case CFastSPI_LED::SPI_SM16716:
 case CFastSPI_LED::SPI_TM1809:
 case CFastSPI_LED::SPI_UCS1903:
 m_nDataRate = 0;
 break;
}
}

#if TIMER_AVAILABLE
// Why not use function pointers?  They're expensive!  Having TIMER1_OVF_vect call a chip
// specific interrupt function through a pointer adds approximately 1.3µs over the if/else blocks 
// below per cycle.  That doesn't sound like a lot, though, right?  Wrong.  For the HL-1606, with
// 40 leds, you may want to push up to 125,000 cycles/second.  1.3µs extra per cycle means 162.5
// milliseconds.  Put a different way, you just cost yourself 16.25% of your cpu time.  This is on 
// a 16Mhz processor.  On an 8Mhz processor, the hit is even more severe, percentage wise.  1.3µs
// is also, for another point of data, only 20 clocks on a 20Mhz processor.  Every extra register push/pop
// that needs to be added to the function is 4 cycles (2 for push, 2 for pop) - or .25µs.  To call through
// a function pointer requires two pushes and two pops (to save r30/r31) or 8 cycles, plus the actual call
// which is 4 clocks, and then a reti which is another 4 clocks, plus the loading of the function pointer 
// which is itself 4 clocks (2 clocks for each of loading the high and low bytes of the function's address)
// The asm below only adds 12 clocks (and did some other tightening in the hl1606 code to get us closer
// to being able to saturate as well) - wouldn't have to worry push/pop'ing r1 if someone out there wasn't
// misbehaving and using r1 for something other than zero!
extern "C"  void TIMER1_OVF_vect(void) __attribute__ ((signal,naked,__INTR_ATTRS));
void TIMER1_OVF_vect(void) {
#if 1
  __asm__ __volatile__ ( "push r1");
  __asm__ __volatile__ ( "lds r1, nChip" );
  __asm__ __volatile__ ( "sbrc r1, 1" );          // if(nChip == SPI_HL1606) { 
  __asm__ __volatile__ ( "rjmp do1606" );         //   do1606();
  __asm__ __volatile__ ( "sbrs r1, 0" );          // } else if(nChip != SPI_595) { 
  __asm__ __volatile__ ( "rjmp do6803" );         //   do6803(); 
  __asm__ __volatile__ ( "do595: pop r1" );       // } else if(nChip == SPI_595) { 
  __asm__ __volatile__ ( "  jmp spi595" );        //   do595();
  __asm__ __volatile__ ( "do6803: pop r1" );      // }
  __asm__ __volatile__ ( "  jmp spilpd6803" );
  __asm__ __volatile__ ( "do1606: pop r1" );
  __asm__ __volatile__ ( "  jmp spihl1606" );
#else
  __asm__ __volatile__ ( "lds r1, nChip" );
  __asm__ __volatile__ ( "sbrc r1, 1" );
  __asm__ __volatile__ ( "jmp spihl1606" );
  __asm__ __volatile__ ( "sbrc r1, 2" );
  __asm__ __volatile__ ( "jmp spilpd6803");
  __asm__ __volatile__ ( "jmp spi595" );
#endif
}

//  if(FastSPI_LED.m_eChip == CFastSPI_LED::SPI_HL1606)
ISR(spihl1606)
{
 static unsigned char nBrightness = 1;
 register unsigned char aByte = Command;

    //if(pData == FastSPI_LED.m_pData) 
 if(nCount != 0)
 {  
  register unsigned char nCheck = nBrightness;
  if(*(--pData) > nCheck) { aByte |= BlueOn; } if(*(--pData) > nCheck) { aByte |= GreenOn; } if(*(--pData) > nCheck) { aByte |= RedOn; } 
      // SPI_B;
  SPI_A(aByte);
  nCount--;
  return;
}
else
{ 
  BIT_HI(SPI_PORT,SPI_SSN);
  pData = FastSPI_LED.m_pDataEnd;
  BIT_LO(SPI_PORT,SPI_SSN);
  if (nBrightness <= nBrightMax) { nBrightness += nBrightIdx; } 
  else { nBrightness = 1; }
  BIT_HI(SPI_PORT,SPI_SSN);
      // if( (nBrightness += nBrightIdx) > BRIGHT_MAX) { nBrightness = 1; }  
  nCount = nCountBase;
  BIT_LO(SPI_PORT,SPI_SSN);
  SPI_A(aByte);
  return;
} 
} 
  //else if(FastSPI_LED.m_eChip == CFastSPI_LED::SPI_595)
ISR(spi595)
{
  static unsigned char nBrightness = 1;
  if(nBrightness > nBrightMax) { nBrightness = 1; } 
  else { nBrightness += nBrightIdx; }
    // register unsigned char nCheck = nBrightness;
  register unsigned char aByte;
    //register unsigned char *
  { 
    BIT_LO(SPI_PORT,SPI_SSN);

#define BIT_SET(nBit) if(*(--pData) >= nBrightness) { aByte |= nBit; }
#define BLOCK8 aByte=0; BIT_SET(0x80); BIT_SET(0x40); BIT_SET(0x20); BIT_SET(0x10); BIT_SET(0x08); BIT_SET(0x04); BIT_SET(0x02); BIT_SET(0x01);
#define COMMANDA BLOCK8 ; SPI_A(aByte);
#define COMMANDB BLOCK8 ; SPI_B; SPI_A(aByte);
#define COM3A COMMANDA ; COMMANDB ; COMMANDB
#define COM3B COMMANDB ; COMMANDB ; COMMANDB
#define COM12 COM3A ; COM3B ; COM3B ; COM3B

      // If we have blocks of 3 8 bit shift registers, that gives us 8 rgb leds, we'll do some aggressive
      // loop unrolling for cases where we have multiples of 3 shift registers - i should expand this out to
      // handle a wider range of cases at some point
    switch(nLedBlocks) { 
     case 4: COM12; break;
     case 3: COM3A; COM3B; COM3B; break;
     case 2: COM3A; COM3B; break;
     case 1: COM3A; break;
     default:
     BLOCK8;
     SPI_A(aByte);
     for(register char i = FastSPI_LED.m_nLeds; i > 8; i-= 8) { 
       BLOCK8;
       SPI_B;
       SPI_A(aByte);
     }
   }
   BIT_HI(SPI_PORT,SPI_SSN);
   pData = FastSPI_LED.m_pDataEnd;
   return;
 }
}
  //else // if(FastSPI_LED.m_eChip == CFastSPI_LED::SPI_LPD6803)
ISR(spilpd6803)
{
  static unsigned char nState=1;
  if(FastSPI_LED.m_eChip == CFastSPI_LED::SPI_LPD6803)
  {
    if(nState==1) 
    {
     SPI_A(0); 
     if(FastSPI_LED.m_nDirty==1) {
       nState = 0;
       FastSPI_LED.m_nDirty = 0;
       SPI_B; 
       SPI_A(0);
       pData = FastSPI_LED.m_pData;
       return;
     }
     SPI_B;
     SPI_A(0);
     return;
   }
   else
   {
     register unsigned int command;
     command = 0x8000;
	command |= (*(pData++) & 0xF8) << 7; // red is the high 5 bits
	command |= (*(pData++) & 0xF8) << 2; // green is the middle 5 bits
	command |= *(pData++) >> 3 ; // blue is the low 5 bits
	SPI_B;
	SPI_A( (command>>8) &0xFF);
	if(pData == FastSPI_LED.m_pDataEnd) { 
   nState = 1;
 }
 SPI_B;
 SPI_A( command & 0xFF);
 return;
} 
}
}
#endif

void CFastSPI_LED::show() { 
  static byte run=0;

  setDirty();
  if(FastSPI_LED.m_eChip == CFastSPI_LED::SPI_WS2801)
  {
    cli();
    register byte *p = m_pData;
    register byte *e = m_pDataEnd;

	// If we haven't run through yet - nothing has primed the SPI bus,
	// and the first SPI_B will block.  
    if(!run) { 
      run = 1;
      SPI_A(*p++);
      SPI_B; SPI_A(*p++);
      SPI_B; SPI_A(*p++);
    }
    while(p != e) { 
     SPI_B; SPI_A(*p++);
     SPI_B; SPI_A(*p++);
     SPI_B; SPI_A(*p++);
   }
   m_nDirty = 0;
   sei();
 }
 else if(FastSPI_LED.m_eChip == CFastSPI_LED::SPI_SM16716)
 {
  cli();
  register byte *p = m_pData;
  register byte *e = m_pDataEnd;

    // SM167176 starts with a control block of 50 bits
  SPI_A(0);
  SPI_B; SPI_A(0);
  SPI_B; SPI_A(0);
  SPI_B; SPI_A(0);
  SPI_B; SPI_A(0);
  SPI_B; SPI_A(0);
  SPI_B; 
  SPI_BIT(0);
  SPI_BIT(0);

  while(p != e) {
      // every 24 bit block starts with a 1 bit being sent
   SPI_BIT(1); 
   SPI_A(*p++); SPI_B;
   SPI_A(*p++); SPI_B;
   SPI_A(*p++); SPI_B;
 }
 m_nDirty = 0;
 sei();
}else if(FastSPI_LED.m_eChip == CFastSPI_LED::SPI_UCS1903)
{
  cli();
  m_nDirty = 0;
  register byte *pData = m_pData;
  for(int iPins = 0; iPins < m_nPins; iPins++) { 
    register byte *pEnd = pData + m_pPinLengths[iPins];
    register unsigned char PIN = digitalPinToBitMask(FastSPI_LED.m_pPins[iPins]); 
    register volatile uint8_t *pPort = m_pPorts[iPins];

        if(pPort == NOT_A_PIN) { /* do nothing */ } 
    else { UCS1903_ALL(*pPort, pData, pEnd); }
  }
  sei();
}
else if(FastSPI_LED.m_eChip == CFastSPI_LED::SPI_TM1809)
{
  cli();
  m_nDirty = 0;
  register byte *pData = m_pData;
  for(int iPins = 0; iPins < m_nPins; iPins++) { 
    register byte *pEnd = pData + m_pPinLengths[iPins];
    register unsigned char PIN = digitalPinToBitMask(FastSPI_LED.m_pPins[iPins]); 
    register volatile uint8_t *pPort = m_pPorts[iPins];

        if(pPort == NOT_A_PIN) { /* do nothing */ } 
    else { TM1809_ALL(*pPort, pData, pEnd); }
  }
  sei();
}
else if(FastSPI_LED.m_eChip == CFastSPI_LED::SPI_LPD8806) 
{
  cli();
  register byte *p = m_pData;
  register byte *e = m_pDataEnd;

      // The LPD8806 requires the high bit to be set
  while(p != e) { 
    SPI_B; SPI_A( *p++ >> 1 | 0x80);
    SPI_B; SPI_A( *p++ >> 1 | 0x80);
    SPI_B; SPI_A( *p++ >> 1 | 0x80);
  }

      // Latch out our 0's to set the data stream
  int n = (m_nLeds + 191 )/ 192;
  while(n--) { 
    SPI_B; SPI_A(0);
    SPI_B; SPI_A(0);
    SPI_B; SPI_A(0);
  }
  m_nDirty=0;
  sei();
}
}

void CFastSPI_LED::setDataRate(int datarate) {
  m_nDataRate = datarate;
}

void CFastSPI_LED::setPinCount(int nPins) {
  m_nPins = nPins;
  m_pPins = (unsigned int*)malloc(sizeof(unsigned int) * nPins);
  m_pPinLengths = (unsigned int*)malloc(sizeof(unsigned int) * nPins);
  m_pPorts = (uint8_t**)malloc(sizeof(uint8_t*) * nPins);
  for(int i = 0; i < nPins; i++) { 
    m_pPins[i] = m_pPinLengths[i] = 0;
    m_pPorts[i] = NULL;
  }
}

void CFastSPI_LED::setPin(int iPins, int nPin, int nLength) {
  m_pPins[iPins] = nPin;
  m_pPinLengths[iPins] = nLength*3;
  m_pPorts[iPins] = (uint8_t*)portOutputRegister(digitalPinToPort(nPin));
}

void CFastSPI_LED::setup_hardware_spi(void) {
  byte clr;
  
  if(USE_SPI) { 
    pinMode(DATA_PIN,OUTPUT);
    pinMode(LATCH_PIN,OUTPUT);
    pinMode(CLOCK_PIN,OUTPUT);
    pinMode(SLAVE_PIN,OUTPUT);
    digitalWrite(DATA_PIN,LOW);
    digitalWrite(LATCH_PIN,LOW);
    digitalWrite(CLOCK_PIN,LOW);
    digitalWrite(SLAVE_PIN,LOW);

    // spi prescaler: 
    // SPI2X SPR1 SPR0
    //   0     0     0    fosc/4
    //   0     0     1    fosc/16
    //   0     1     0    fosc/64
    //   0     1     1    fosc/128
    //   1     0     0    fosc/2
    //   1     0     1    fosc/8
    //   1     1     0    fosc/32
    //   1     1     1    fosc/64
    SPCR |= ( (1<<SPE) | (1<<MSTR) ); // enable SPI as master
    SPCR &= ~ ( (1<<SPR1) | (1<<SPR0) ); // clear prescaler bits
    clr=SPSR; // clear SPI status reg
    clr=SPDR; // clear SPI data reg

    // set the prescalar bits based on the chosen data rate values
    bool b2x = false;
    switch(m_nDataRate) { 
      /* fosc/2   */ case 0: b2x=true; break;
      /* fosc/4   */ case 1: break;
      /* fosc/8   */ case 2: SPCR |= (1<<SPR0); b2x=true; break;
      /* fosc/16  */ case 3: SPCR |= (1<<SPR0); break;
      /* fosc/32  */ case 4: SPCR |= (1<<SPR1); b2x=true; break;
      /* fosc/64  */ case 5: SPCR |= (1<<SPR1); break;
      /* fosc/64  */ case 6: SPCR |= (1<<SPR1); SPCR |= (1<<SPR0); b2x=true; break;
      /* fosc/128 */ default: SPCR |= (1<<SPR1); SPCR |= (1<<SPR0); break;
    }
    if(b2x) { SPSR |= (1<<SPI2X); }
    else { SPSR &= ~ (1<<SPI2X); }

  } else { 
    for(int i = 0; i < m_nPins; i++) { 
      pinMode(m_pPins[i], OUTPUT);
      digitalWrite(m_pPins[i],LOW);
    }
  }

  // dig out some timing info 
#if TIMER_AVAILABLE
  if(USE_TIMER && USE_SPI) { 
    SPI_A(0);
    TIMER1_OVF_vect();

    // First thing to do is count our cycles to figure out how to line
    // up the desired performance percentages
    unsigned long nRounds=0;
    volatile int x = 0;
    unsigned long mCStart = millis();
    for(volatile int i = 0 ; i < 10000; i++) {
      ;
#ifdef COUNT_ROUNDS
      m_nCounter++;
#endif
    }
    unsigned long mCEnd = millis();
    m_nCounter=0;
    nRounds = 10000;
    DPRINT("10000 round empty loop in ms: "); DPRINTLN(mCEnd - mCStart);
    unsigned long mStart,mStop;
    if(!USE_TIMER) { 
#ifdef DEBUG_SPI
      mStart = millis();
      for(volatile int i = 0; i < 10000; i++) { 
        show(); 
      }
      mStop = millis();
#endif
    } else { 
      mStart = millis();
      for(volatile int i = 0; i < 10000; i++) { 
        TIMER1_OVF_vect(); 
      }
      mStop = millis();
    }
    DPRINT(nRounds); DPRINT(" rounds of rgb out in ms: "); DPRINTLN(mStop - mStart); 

    // This gives us the time for 10 rounds in µs
    m_adjustedUSecTime = (mStop-mStart) - (mCEnd - mCStart);
  } 
#endif
}

// Core borrowed and adapted from the Timer1 Arduino library - by Jesse Tane
#define RESOLUTION 65536

void CFastSPI_LED::setup_timer1_ovf(void) {
#if TIMER_AVAILABLE
  // Now that we have our per-cycle timing, figure out how to set up the timer to
  // match the desired cpu percentage
  TCCR1A = 0;
  TCCR1B = _BV(WGM13);
  
  unsigned long baseCounts = (((unsigned long)m_cpuPercentage) * 100000) / m_adjustedUSecTime;
  unsigned long us10;
  switch(FastSPI_LED.m_eChip) { 
    case CFastSPI_LED::SPI_LPD6803: us10 = (1000000 ) / baseCounts; break;
    case CFastSPI_LED::SPI_HL1606: us10 = (1000000 ) / baseCounts; break;
    case CFastSPI_LED::SPI_595: us10 = (1000000 ) / baseCounts; break; 
  }
  
  DPRINT("bc:"); DPRINTLN(baseCounts);
  DPRINT("us*10:"); DPRINTLN(us10);
  
  long cycles = ( (F_CPU)*us10) / (2000000);                                            // the counter runs backwards after TOP, interrupt is at BOTTOM so divide microseconds by 2

  if(FastSPI_LED.m_eChip == SPI_HL1606) { 
    // can't have a cycle at or less than 66 cycles on a 16Mhz system, or 33 cycles on an 8Mhz system
    // with the hl1606's - at least with 40 leds.  Stupid timing constraints.  The chips whig out if you send
    // data at a faster rate.  This rate limit means we don't waste time waiting on SPSR
    if(F_CPU == 16000000 && cycles < 67) { cycles = 67; }
    if(F_CPU == 8000000 && cycles < 34) { cycles = 34; }
  }
  DPRINT("cy:"); DPRINTLN(cycles);

  if(cycles < RESOLUTION)              clockSelectBits = _BV(CS10);              // no prescale, full xtal/TCCR
  else if((cycles >>= 3) < RESOLUTION) clockSelectBits = _BV(CS11);              // prescale by /8
  else if((cycles >>= 3) < RESOLUTION) clockSelectBits = _BV(CS11) | _BV(CS10);  // prescale by /64
  else if((cycles >>= 2) < RESOLUTION) clockSelectBits = _BV(CS12);              // prescale by /256
  else if((cycles >>= 2) < RESOLUTION) clockSelectBits = _BV(CS12) | _BV(CS10);  // prescale by /1024
  else        cycles = RESOLUTION - 1, clockSelectBits = _BV(CS12) | _BV(CS10);  // request was out of bounds, set as maximum
  ICR1 = cycles;                                                     // ICR1 is TOP in p & f correct pwm mode
  TCCR1B &= ~(_BV(CS10) | _BV(CS11) | _BV(CS12));
  // call start explicitly TCCR1B |= clockSelectBits;                                                     // reset clock select register
  
  TIMSK1 = _BV(TOIE1);
  sei();
#endif
}

