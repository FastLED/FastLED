#include <WProgram.h>
#include <avr/interrupt.h>
#include <avr/io.h>
#include "FastSPI_LED.h"
#include "wiring.h"

/* */
#define DPRINT Serial.print
#define DPRINTLN Serial.println
/* /
#define DPRINT(x)
#define DPRINTLN(x)
/* */

// #define COUNT_ROUNDS 1

// some spi defines
// Duemilanove and mini w/328
#if defined(__AVR_ATmega328P__)
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

// Mega.
#elif defined(__AVR_ATmega1280__)
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
#endif

#define BIT_HI(R, P) (R) |= _BV(P)
#define BIT_LO(R, P) (R) &= ~_BV(P)

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


void CFastSPI_LED::setDirty() { m_nDirty = 1; }

void CFastSPI_LED::init() { 
  // store some static locals (makes lookup faster)
  pData = m_pDataEnd;
  nCountBase = m_nLeds / 3;
  // set up the spi timer - also do some initial timing loops to get base adjustments
  // for the timer below  
  setup_hardware_spi();
  delay(10);
  setup_timer1_ovf();
}

// 
void CFastSPI_LED::start() {
  TCCR1B |= clockSelectBits;                                                     // reset clock select register
}

void CFastSPI_LED::stop() {
  // clear the clock select bits
  TCCR1B &= ~(_BV(CS10) | _BV(CS11) | _BV(CS12));
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
  }

  // set default cpu percentage targets  
  switch(FastSPI_LED.m_eChip) { 
    case CFastSPI_LED::SPI_595: m_cpuPercentage = 53; break;
    case CFastSPI_LED::SPI_LPD6803: m_cpuPercentage = 50; break;
    case CFastSPI_LED::SPI_HL1606: m_cpuPercentage = 65; break;
    case CFastSPI_LED::SPI_WS2801: m_cpuPercentage = 25; break;
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
    case CFastSPI_LED::SPI_WS2801:
      m_nDataRate = 0;
      break;
  }
}

#define SPI_A(data) SPDR=data;
#define SPI_B while(!(SPSR & (1<<SPIF))); 
#define SPI_TRANSFER(data) { SPDR=data; while(!(SPSR & (1<<SPIF))); } 

#if 0
static int in=0;
ISR(SPI_STC_vect) {
       static unsigned char nBrightness = 1;
    if(pData == FastSPI_LED.m_pData) 
    { 
      BIT_HI(SPI_PORT,SPI_SSN);
  
      pData = FastSPI_LED.m_pDataEnd;
      if(nBrightness > nBrightMax) { nBrightness = 1; } 
      else { nBrightness += nBrightIdx; }
      // if( (nBrightness += nBrightIdx) > BRIGHT_MAX) { nBrightness = 1; }  
      // nCount = FastSPI_LED.m_nLeds+1;
      BIT_LO(SPI_PORT,SPI_SSN);
      //return;
    } 
    {  
      register unsigned char nCheck = nBrightness;
      register unsigned char aByte = Command;
      if(*(--pData) > nCheck) { aByte |= BlueOn; } if(*(--pData) > nCheck) { aByte |= GreenOn; } if(*(--pData) > nCheck) { aByte |= RedOn; } 
      SPI_A(aByte);
#ifdef COUNT_ROUNDS
      FastSPI_LED.m_nCounter++;
#endif
    }
}
#endif

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
  __asm__ __volatile__ ( "sbrc r1, 1" );
  __asm__ __volatile__ ( "rjmp do1606" );
  __asm__ __volatile__ ( "sbrs r1, 0" );
  __asm__ __volatile__ ( "rjmp do6803" );
  __asm__ __volatile__ ( "do595: pop r1" );
  __asm__ __volatile__ ( "  jmp spi595" );
  __asm__ __volatile__ ( "do6803: pop r1" );
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
//ISR(TIMER1_OVF_vect) {
//#ifdef COUNT_ROUNDS
//  FastSPI_LED.m_nCounter++;
//#endif
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
  else // if(FastSPI_LED.m_eChip == CFastSPI_LED::SPI_WS2801)
  {
    if(nState==1) {
      if(FastSPI_LED.m_nDirty==1) {
        nState = 0;
        FastSPI_LED.m_nDirty = 0;
        pData = FastSPI_LED.m_pData;
        return;
      }
    } else {
      while(pData != FastSPI_LED.m_pDataEnd) { 
        SPI_B; SPI_A(*pData++); 
      }
      nState = 1; 
      return;
    }
  }
}
//}

void CFastSPI_LED::setDataRate(int datarate) {
  m_nDataRate = datarate;
}

void CFastSPI_LED::setup_hardware_spi(void) {
  byte clr;
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

  // dig out some timing info 
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
  mStart = millis();
  for(volatile int i = 0; i < 10000; i++) { 
    TIMER1_OVF_vect(); 
  }
  mStop = millis();
  DPRINT(nRounds); DPRINT(" rounds of rgb out in ms: "); DPRINTLN(mStop - mStart); 
  
  // This gives us the time for 10 rounds in µs
  m_adjustedUSecTime = (mStop-mStart) - (mCEnd - mCStart);

}

// Core borrowed and adapted from the Timer1 Arduino library - by Jesse Tane
#define RESOLUTION 65536

void CFastSPI_LED::setup_timer1_ovf(void) {
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
    case CFastSPI_LED::SPI_WS2801: us10 = (1000000) / baseCounts; break;
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
}

