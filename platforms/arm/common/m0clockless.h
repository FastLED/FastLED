#ifndef __INC_M0_CLOCKLESS_H
#define __INC_M0_CLOCKLESS_H

struct M0ClocklessData {
  uint8_t d[3];
  uint8_t s[3];
  uint8_t e[3];
  uint8_t adj;
};

//
// This macro is to get around some particular stupidisms with the intersection
// of gcc's asm support and gas's macro engironment
#define MK_LABELS(LABEL, VALUE) \
switch(VALUE) { \
  case 0: { asm __volatile__ (".set " str(LABEL) ", 0\n\t"); break; } \
  case 1: { asm __volatile__ (".set " str(LABEL) ", 1\n\t"); break; } \
  case 2: { asm __volatile__ (".set " str(LABEL) ", 2\n\t"); break; } \
  case 3: { asm __volatile__ (".set " str(LABEL) ", 3\n\t"); break; } \
  case 4: { asm __volatile__ (".set " str(LABEL) ", 4\n\t"); break; } \
  case 5: { asm __volatile__ (".set " str(LABEL) ", 5\n\t"); break; } \
  case 6: { asm __volatile__ (".set " str(LABEL) ", 6\n\t"); break; } \
  case 7: { asm __volatile__ (".set " str(LABEL) ", 7\n\t"); break; } \
  case 8: { asm __volatile__ (".set " str(LABEL) ", 8\n\t"); break; } \
  case 9: { asm __volatile__ (".set " str(LABEL) ", 9\n\t"); break; } \
  case 10: { asm __volatile__ (".set " str(LABEL) ", 10\n\t"); break; } \
  case 11: { asm __volatile__ (".set " str(LABEL) ", 11\n\t"); break; } \
  case 12: { asm __volatile__ (".set " str(LABEL) ", 12\n\t"); break; } \
  case 13: { asm __volatile__ (".set " str(LABEL) ", 13\n\t"); break; } \
  case 14: { asm __volatile__ (".set " str(LABEL) ", 14\n\t"); break; } \
  case 15: { asm __volatile__ (".set " str(LABEL) ", 15\n\t"); break; } \
  case 16: { asm __volatile__ (".set " str(LABEL) ", 16\n\t"); break; } \
  case 17: { asm __volatile__ (".set " str(LABEL) ", 17\n\t"); break; } \
  case 18: { asm __volatile__ (".set " str(LABEL) ", 18\n\t"); break; } \
  case 19: { asm __volatile__ (".set " str(LABEL) ", 19\n\t"); break; } \
  case 20: { asm __volatile__ (".set " str(LABEL) ", 20\n\t"); break; } \
  case 21: { asm __volatile__ (".set " str(LABEL) ", 21\n\t"); break; } \
  case 22: { asm __volatile__ (".set " str(LABEL) ", 22\n\t"); break; } \
  case 23: { asm __volatile__ (".set " str(LABEL) ", 23\n\t"); break; } \
  case 24: { asm __volatile__ (".set " str(LABEL) ", 24\n\t"); break; } \
  case 25: { asm __volatile__ (".set " str(LABEL) ", 25\n\t"); break; } \
  case 26: { asm __volatile__ (".set " str(LABEL) ", 26\n\t"); break; } \
  case 27: { asm __volatile__ (".set " str(LABEL) ", 27\n\t"); break; } \
  case 28: { asm __volatile__ (".set " str(LABEL) ", 28\n\t"); break; } \
  case 29: { asm __volatile__ (".set " str(LABEL) ", 29\n\t"); break; } \
  case 30: { asm __volatile__ (".set " str(LABEL) ", 30\n\t"); break; } \
  case 31: { asm __volatile__ (".set " str(LABEL) ", 31\n\t"); break; } \
  case 32: { asm __volatile__ (".set " str(LABEL) ", 32\n\t"); break; } \
  case 33: { asm __volatile__ (".set " str(LABEL) ", 33\n\t"); break; } \
  case 34: { asm __volatile__ (".set " str(LABEL) ", 34\n\t"); break; } \
  case 35: { asm __volatile__ (".set " str(LABEL) ", 35\n\t"); break; } \
  case 36: { asm __volatile__ (".set " str(LABEL) ", 36\n\t"); break; } \
  case 37: { asm __volatile__ (".set " str(LABEL) ", 37\n\t"); break; } \
  case 38: { asm __volatile__ (".set " str(LABEL) ", 38\n\t"); break; } \
  case 39: { asm __volatile__ (".set " str(LABEL) ", 39\n\t"); break; } \
  case 40: { asm __volatile__ (".set " str(LABEL) ", 40\n\t"); break; } \
  case 41: { asm __volatile__ (".set " str(LABEL) ", 41\n\t"); break; } \
  case 42: { asm __volatile__ (".set " str(LABEL) ", 42\n\t"); break; } \
  case 43: { asm __volatile__ (".set " str(LABEL) ", 43\n\t"); break; } \
  case 44: { asm __volatile__ (".set " str(LABEL) ", 44\n\t"); break; } \
  case 45: { asm __volatile__ (".set " str(LABEL) ", 45\n\t"); break; } \
  case 46: { asm __volatile__ (".set " str(LABEL) ", 46\n\t"); break; } \
  case 47: { asm __volatile__ (".set " str(LABEL) ", 47\n\t"); break; } \
  case 48: { asm __volatile__ (".set " str(LABEL) ", 48\n\t"); break; } \
  case 49: { asm __volatile__ (".set " str(LABEL) ", 49\n\t"); break; } \
  case 50: { asm __volatile__ (".set " str(LABEL) ", 50\n\t"); break; } \
  case 51: { asm __volatile__ (".set " str(LABEL) ", 51\n\t"); break; } \
  case 52: { asm __volatile__ (".set " str(LABEL) ", 52\n\t"); break; } \
  case 53: { asm __volatile__ (".set " str(LABEL) ", 53\n\t"); break; } \
  case 54: { asm __volatile__ (".set " str(LABEL) ", 54\n\t"); break; } \
  case 55: { asm __volatile__ (".set " str(LABEL) ", 55\n\t"); break; } \
  case 56: { asm __volatile__ (".set " str(LABEL) ", 56\n\t"); break; } \
  case 57: { asm __volatile__ (".set " str(LABEL) ", 57\n\t"); break; } \
  case 58: { asm __volatile__ (".set " str(LABEL) ", 58\n\t"); break; } \
  case 59: { asm __volatile__ (".set " str(LABEL) ", 59\n\t"); break; } \
  case 60: { asm __volatile__ (".set " str(LABEL) ", 60\n\t"); break; } \
  case 61: { asm __volatile__ (".set " str(LABEL) ", 61\n\t"); break; } \
  case 62: { asm __volatile__ (".set " str(LABEL) ", 62\n\t"); break; } \
  case 63: { asm __volatile__ (".set " str(LABEL) ", 63\n\t"); break; } \
  case 64: { asm __volatile__ (".set " str(LABEL) ", 64\n\t"); break; } \
  case 65: { asm __volatile__ (".set " str(LABEL) ", 65\n\t"); break; } \
  case 66: { asm __volatile__ (".set " str(LABEL) ", 66\n\t"); break; } \
  case 67: { asm __volatile__ (".set " str(LABEL) ", 67\n\t"); break; } \
  case 68: { asm __volatile__ (".set " str(LABEL) ", 68\n\t"); break; } \
  case 69: { asm __volatile__ (".set " str(LABEL) ", 69\n\t"); break; } \
  case 70: { asm __volatile__ (".set " str(LABEL) ", 70\n\t"); break; } \
  case 71: { asm __volatile__ (".set " str(LABEL) ", 71\n\t"); break; } \
  case 72: { asm __volatile__ (".set " str(LABEL) ", 72\n\t"); break; } \
  case 73: { asm __volatile__ (".set " str(LABEL) ", 73\n\t"); break; } \
  case 74: { asm __volatile__ (".set " str(LABEL) ", 74\n\t"); break; } \
  case 75: { asm __volatile__ (".set " str(LABEL) ", 75\n\t"); break; } \
  case 76: { asm __volatile__ (".set " str(LABEL) ", 76\n\t"); break; } \
  case 77: { asm __volatile__ (".set " str(LABEL) ", 77\n\t"); break; } \
  case 78: { asm __volatile__ (".set " str(LABEL) ", 78\n\t"); break; } \
  case 79: { asm __volatile__ (".set " str(LABEL) ", 79\n\t"); break; } \
  case 80: { asm __volatile__ (".set " str(LABEL) ", 80\n\t"); break; } \
  case 81: { asm __volatile__ (".set " str(LABEL) ", 81\n\t"); break; } \
  case 82: { asm __volatile__ (".set " str(LABEL) ", 82\n\t"); break; } \
  case 83: { asm __volatile__ (".set " str(LABEL) ", 83\n\t"); break; } \
  case 84: { asm __volatile__ (".set " str(LABEL) ", 84\n\t"); break; } \
  case 85: { asm __volatile__ (".set " str(LABEL) ", 85\n\t"); break; } \
  case 86: { asm __volatile__ (".set " str(LABEL) ", 86\n\t"); break; } \
  case 87: { asm __volatile__ (".set " str(LABEL) ", 87\n\t"); break; } \
  case 88: { asm __volatile__ (".set " str(LABEL) ", 88\n\t"); break; } \
  case 89: { asm __volatile__ (".set " str(LABEL) ", 89\n\t"); break; } \
  case 90: { asm __volatile__ (".set " str(LABEL) ", 90\n\t"); break; } \
  case 91: { asm __volatile__ (".set " str(LABEL) ", 91\n\t"); break; } \
  case 92: { asm __volatile__ (".set " str(LABEL) ", 92\n\t"); break; } \
  case 93: { asm __volatile__ (".set " str(LABEL) ", 93\n\t"); break; } \
  case 94: { asm __volatile__ (".set " str(LABEL) ", 94\n\t"); break; } \
  case 95: { asm __volatile__ (".set " str(LABEL) ", 95\n\t"); break; } \
  case 96: { asm __volatile__ (".set " str(LABEL) ", 96\n\t"); break; } \
  case 97: { asm __volatile__ (".set " str(LABEL) ", 97\n\t"); break; } \
  case 98: { asm __volatile__ (".set " str(LABEL) ", 98\n\t"); break; } \
  case 99: { asm __volatile__ (".set " str(LABEL) ", 99\n\t"); break; } \
  case 100: { asm __volatile__ (".set " str(LABEL) ", 100\n\t"); break; } \
  case 101: { asm __volatile__ (".set " str(LABEL) ", 101\n\t"); break; } \
  case 102: { asm __volatile__ (".set " str(LABEL) ", 102\n\t"); break; } \
  case 103: { asm __volatile__ (".set " str(LABEL) ", 103\n\t"); break; } \
  case 104: { asm __volatile__ (".set " str(LABEL) ", 104\n\t"); break; } \
  case 105: { asm __volatile__ (".set " str(LABEL) ", 105\n\t"); break; } \
  case 106: { asm __volatile__ (".set " str(LABEL) ", 106\n\t"); break; } \
  case 107: { asm __volatile__ (".set " str(LABEL) ", 107\n\t"); break; } \
  case 108: { asm __volatile__ (".set " str(LABEL) ", 108\n\t"); break; } \
  case 109: { asm __volatile__ (".set " str(LABEL) ", 109\n\t"); break; } \
  case 110: { asm __volatile__ (".set " str(LABEL) ", 110\n\t"); break; } \
  case 111: { asm __volatile__ (".set " str(LABEL) ", 111\n\t"); break; } \
  case 112: { asm __volatile__ (".set " str(LABEL) ", 112\n\t"); break; } \
  case 113: { asm __volatile__ (".set " str(LABEL) ", 113\n\t"); break; } \
  case 114: { asm __volatile__ (".set " str(LABEL) ", 114\n\t"); break; } \
  case 115: { asm __volatile__ (".set " str(LABEL) ", 115\n\t"); break; } \
  case 116: { asm __volatile__ (".set " str(LABEL) ", 116\n\t"); break; } \
  case 117: { asm __volatile__ (".set " str(LABEL) ", 117\n\t"); break; } \
  case 118: { asm __volatile__ (".set " str(LABEL) ", 118\n\t"); break; } \
  case 119: { asm __volatile__ (".set " str(LABEL) ", 119\n\t"); break; } \
  case 120: { asm __volatile__ (".set " str(LABEL) ", 120\n\t"); break; } \
}

template<int HI_OFFSET, int LO_OFFSET, int T1, int T2, int T3, EOrder RGB_ORDER, int WAIT_TIME>int
showLedData(volatile uint32_t *_port, uint32_t _bitmask, const uint8_t *_leds, uint32_t num_leds, struct M0ClocklessData *pData) {
  // Lo register variables
  register uint32_t scratch=0;
  register struct M0ClocklessData *base = pData;
  register volatile uint32_t *port = _port;
  register uint32_t d=0;
  register uint32_t counter=num_leds;
  register uint32_t bn=0;
  register uint32_t b=0;
  register uint32_t bitmask = _bitmask;

  // high register variable
  register const uint8_t *leds = _leds;

  MK_LABELS(T1,T1);
  MK_LABELS(T2,T2);
  MK_LABELS(T3,T3);

  asm __volatile__ (
    ///////////////////////////////////////////////////////////////////////////
    //
    // asm macro definitions - used to assemble the clockless output
    //
    ".ifnotdef fl_delay_def;"
#ifdef FASTLED_ARM_M0_PLUS
    "  .set fl_is_m0p, 1;"
    "  .macro m0pad;"
    "    nop;"
    "  .endm;"
#else
    "  .set fl_is_m0p, 0;"
    "  .macro m0pad;"
    "  .endm;"
#endif
    "  .set fl_delay_def, 1;"
    "  .set fl_delay_mod, 4;"
    "  .if fl_is_m0p == 1;"
    "    .set fl_delay_mod, 3;"
    "  .endif;"
    "  .macro fl_delay dtime, reg=r0;"
    "    .if (\\dtime > 0);"
    "      .set dcycle, (\\dtime / fl_delay_mod);"
    "      .set dwork, (dcycle * fl_delay_mod);"
    "      .set drem, (\\dtime - dwork);"
    "      .rept (drem);"
    "        nop;"
    "      .endr;"
    "      .if dcycle > 0;"
    "        mov \\reg, #dcycle;"
    "        delayloop_\\@:;"
    "        sub \\reg, #1;"
    "        bne delayloop_\\@;"
    "	     .if fl_is_m0p == 0;"
    "          nop;"
    "        .endif;"
    "      .endif;"
    "    .endif;"
    "  .endm;"

    "  .macro mod_delay dtime,b1,b2,reg;"
    "    .set adj, (\\b1 + \\b2);"
    "    .if adj < \\dtime;"
    "      .set dtime2, (\\dtime - adj);"
    "      fl_delay dtime2, \\reg;"
    "    .endif;"
    "  .endm;"

    // check the bit and drop the line low if it isn't set
    "  .macro qlo4 b,bitmask,port,loff	;"
    "    lsl \\b, #1			;"
    "    bcs skip_\\@			;"
    "    str \\bitmask, [\\port, \\loff]	;"
    "    skip_\\@:			;"
    "    m0pad;"
    "  .endm				;"

    // set the pin hi or low (determined by the offset passed in )
    "  .macro qset2 bitmask,port,loff;"
    "    str \\bitmask, [\\port, \\loff];"
    "    m0pad;"
    "  .endm;"

    // Load up the next led byte to work with, put it in bn
    "  .macro loadleds3 leds, bn, rled, scratch;"
    "    mov \\scratch, \\leds;"
    "    ldrb \\bn, [\\scratch, \\rled];"
    "  .endm;"

    // check whether or not we should dither
    "  .macro loaddither7 bn,d,base,rdither;"
    "    ldrb \\d, [\\base, \\rdither];"
    "    lsl \\d, #24;"  //; shift high for the qadd w/bn
    "    lsl \\bn, #24;" //; shift high for the qadd w/d
    "    bne chkskip_\\@;" //; if bn==0, clear d;"
    "    eor \\d, \\d;" //; clear d;"
    "    m0pad;"
    "    chkskip_\\@:;"
    "  .endm;"

    // Do the qadd8 for dithering -- there's two versions of this.  The m0 version
    // takes advantage of the 3 cycle branch to do two things after the branch,
    // while keeping timing constant.  The m0+, however, branches in 2 cycles, so
    // we have to work around that a bit more.  This is one of the few times
    // where the m0 will actually be _more_ efficient than the m0+
    "  .macro dither5 bn,d;"
    "  .syntax unified;"
    "    .if fl_is_m0p == 0;"
    "      adds \\bn, \\d;"         // do the add
    "      bcc dither5_1_\\@;"
    "      mvns \\bn, \\bn;"        // set the low 24bits ot 1's
    "      lsls \\bn, \\bn, #24;"   // move low 8 bits to the high bits
    "      dither5_1_\\@:;"
    "      nop;"                    // nop to keep timing in line
    "    .else;"
    "      adds \\bn, \\d;"         // do the add"
    "      bcc dither5_2_\\@;"
    "      mvns \\bn, \\bn;"        // set the low 24bits ot 1's
    "      dither5_2_\\@:;"
    "      bcc dither5_3_\\@;"
    "      lsls \\bn, \\bn, #24;"   // move low 8 bits to the high bits
    "      dither5_3_\\@:;"
    "    .endif;"
    "  .syntax divided;"
    "  .endm;"

    // Do our scaling
    "  .macro scale4 bn, base, scale, scratch;"
    "    ldrb \\scratch, [\\base, \\scale];"
    "    lsr \\bn, \\bn, #24;"                  // bring bn back down to its low 8 bits
    "    mul \\bn, \\scratch;"                  // do the multiply
    "  .endm;"

    // swap bn into b
    "  .macro swapbbn1 b,bn;"
    "    lsl \\b, \\bn, #16;"  // put the 8 bits we want for output high
    "  .endm;"

    // adjust the dithering value for the next time around (load e from memory
    // to do the math)
    "  .macro adjdither7 base,d,rled,eoffset,scratch;"
    "    ldrb \\d, [\\base, \\rled];"
    "    ldrb \\scratch,[\\base,\\eoffset];"          // load e
    "    .syntax unified;"
    "    subs \\d, \\scratch, \\d;"                   // d=e-d
    "    .syntax divided;"
    "    strb \\d, [\\base, \\rled];"                 // save d
    "  .endm;"

    // increment the led pointer (base+9 has what we're incrementing by)
    "  .macro incleds3   leds, base, scratch;"
    "    ldrb \\scratch, [\\base, #9];"               // load incremen
    "    add \\leds, \\leds, \\scratch;"              // update leds pointer
    "  .endm;"

    // compare and loop
    "  .macro cmploop5 counter,label;"
    "    .syntax unified;"
    "    subs \\counter, #1;"
    "    .syntax divided;"
    "    beq done_\\@;"
    "    m0pad;"
    "    b \\label;"
    "    done_\\@:;"
    "  .endm;"

    " .endif;"
  );

#define M0_ASM_ARGS     :             \
      [base] "+l" (base),             \
      [bitmask] "+l" (bitmask),       \
      [port] "+l" (port),             \
      [leds] "+h" (leds),             \
      [counter] "+l" (counter),       \
      [scratch] "+l" (scratch),       \
      [d] "+l" (d),                   \
      [bn] "+l" (bn),                 \
      [b] "+l" (b)                    \
    :                                 \
      [hi_off] "I" (HI_OFFSET),       \
      [lo_off] "I" (LO_OFFSET),       \
      [led0] "I" (RO(0)),             \
      [led1] "I" (RO(1)),             \
      [led2] "I" (RO(2)),             \
      [scale0] "I" (3+RO(0)),         \
      [scale1] "I" (3+RO(1)),         \
      [scale2] "I" (3+RO(2)),         \
      [e0] "I" (6+RO(0)),             \
      [e1] "I" (6+RO(1)),             \
      [e2] "I" (6+RO(2))              \
    :

    /////////////////////////////////////////////////////////////////////////
    // now for some convinience macros to make building our lines a bit cleaner
#define LOOP            "  loop_%=:"
#define HI2             "  qset2 %[bitmask], %[port], %[hi_off];"
#define D1              "  mod_delay T1,2,0,%[scratch];"
#define QLO4            "  qlo4 %[b],%[bitmask],%[port], %[lo_off];"
#define LOADLEDS3(X)    "  loadleds3 %[leds], %[bn], %[led" #X "] ,%[scratch];"
#define D2(ADJ)         "  mod_delay T2,4," #ADJ ",%[scratch];"
#define LO2             "  qset2 %[bitmask], %[port], %[lo_off];"
#define D3(ADJ)         "  mod_delay T3,2," #ADJ ",%[scratch];"
#define LOADDITHER7(X)  "  loaddither7 %[bn], %[d], %[base], %[led" #X "];"
#define DITHER5         "  dither5 %[bn], %[d];"
#define SCALE4(X)       "  scale4 %[bn], %[base], %[scale" #X "], %[scratch];"
#define SWAPBBN1        "  swapbbn1 %[b], %[bn];"
#define ADJDITHER7(X)   "  adjdither7 %[base],%[d],%[led" #X "],%[e" #X "],%[scratch];"
#define INCLEDS3        "  incleds3 %[leds],%[base],%[scratch];"
#define CMPLOOP5        "  cmploop5 %[counter], loop_%=;"
#define NOTHING         ""

#if !(defined(SEI_CHK) && (FASTLED_ALLOW_INTERRUPTS == 1))
    asm __volatile__ (
      // pre-load byte 0
    LOADLEDS3(0) LOADDITHER7(0) DITHER5 SCALE4(0) ADJDITHER7(0) SWAPBBN1

    // loop over writing out the data
    LOOP
      // Write out byte 0, prepping byte 1
      HI2 D1 QLO4 NOTHING         D2(0) LO2 D3(0)
      HI2 D1 QLO4 LOADLEDS3(1)    D2(3) LO2 D3(0)
      HI2 D1 QLO4 LOADDITHER7(1)  D2(7) LO2 D3(0)
      HI2 D1 QLO4 DITHER5         D2(5) LO2 D3(0)
      HI2 D1 QLO4 SCALE4(1)       D2(4) LO2 D3(0)
      HI2 D1 QLO4 ADJDITHER7(1)   D2(7) LO2 D3(0)
      HI2 D1 QLO4 NOTHING         D2(0) LO2 D3(0)
      HI2 D1 QLO4 SWAPBBN1        D2(1) LO2 D3(0)

      // Write out byte 1, prepping byte 2
      HI2 D1 QLO4 NOTHING         D2(0) LO2 D3(0)
      HI2 D1 QLO4 LOADLEDS3(2)    D2(3) LO2 D3(0)
      HI2 D1 QLO4 LOADDITHER7(2)  D2(7) LO2 D3(0)
      HI2 D1 QLO4 DITHER5         D2(5) LO2 D3(0)
      HI2 D1 QLO4 SCALE4(2)       D2(4) LO2 D3(0)
      HI2 D1 QLO4 ADJDITHER7(2)   D2(7) LO2 D3(0)
      HI2 D1 QLO4 INCLEDS3        D2(3) LO2 D3(0)
      HI2 D1 QLO4 SWAPBBN1        D2(1) LO2 D3(0)

      // Write out byte 2, prepping byte 0
      HI2 D1 QLO4 NOTHING         D2(0) LO2 D3(0)
      HI2 D1 QLO4 LOADLEDS3(0)    D2(3) LO2 D3(0)
      HI2 D1 QLO4 LOADDITHER7(0)  D2(7) LO2 D3(0)
      HI2 D1 QLO4 DITHER5         D2(5) LO2 D3(0)
      HI2 D1 QLO4 SCALE4(0)       D2(4) LO2 D3(0)
      HI2 D1 QLO4 ADJDITHER7(0)   D2(7) LO2 D3(0)
      HI2 D1 QLO4 NOTHING         D2(0) LO2 D3(0)
      HI2 D1 QLO4 SWAPBBN1        D2(1) LO2 D3(5) CMPLOOP5

      M0_ASM_ARGS
    );
#else
    asm __volatile__ (
      // pre-load byte 0
      LOADLEDS3(0) LOADDITHER7(0) DITHER5 SCALE4(0) ADJDITHER7(0) SWAPBBN1
      M0_ASM_ARGS);

    SEI_CHK
    do {
      CLI_CHK;
      asm __volatile__ (
      // Write out byte 0, prepping byte 1
      HI2 D1 QLO4 NOTHING         D2(0) LO2 D3(0)
      HI2 D1 QLO4 LOADLEDS3(1)    D2(3) LO2 D3(0)
      HI2 D1 QLO4 LOADDITHER7(1)  D2(7) LO2 D3(0)
      HI2 D1 QLO4 DITHER5         D2(5) LO2 D3(0)
      HI2 D1 QLO4 SCALE4(1)       D2(4) LO2 D3(0)
      HI2 D1 QLO4 ADJDITHER7(1)   D2(7) LO2 D3(0)
      HI2 D1 QLO4 NOTHING         D2(0) LO2 D3(0)
      HI2 D1 QLO4 SWAPBBN1        D2(1) LO2 D3(0)

      // Write out byte 1, prepping byte 2
      HI2 D1 QLO4 NOTHING         D2(0) LO2 D3(0)
      HI2 D1 QLO4 LOADLEDS3(2)    D2(3) LO2 D3(0)
      HI2 D1 QLO4 LOADDITHER7(2)  D2(7) LO2 D3(0)
      HI2 D1 QLO4 DITHER5         D2(5) LO2 D3(0)
      HI2 D1 QLO4 SCALE4(2)       D2(4) LO2 D3(0)
      HI2 D1 QLO4 ADJDITHER7(2)   D2(7) LO2 D3(0)
      HI2 D1 QLO4 INCLEDS3        D2(3) LO2 D3(0)
      HI2 D1 QLO4 SWAPBBN1        D2(1) LO2 D3(0)

      // Write out byte 2, prepping byte 0
      HI2 D1 QLO4 NOTHING         D2(0) LO2 D3(0)
      HI2 D1 QLO4 LOADLEDS3(0)    D2(3) LO2 D3(0)
      HI2 D1 QLO4 LOADDITHER7(0)  D2(7) LO2 D3(0)
      HI2 D1 QLO4 DITHER5         D2(5) LO2 D3(0)
      HI2 D1 QLO4 SCALE4(0)       D2(4) LO2 D3(0)
      HI2 D1 QLO4 ADJDITHER7(0)   D2(7) LO2 D3(0)
      HI2 D1 QLO4 NOTHING         D2(0) LO2 D3(0)
      HI2 D1 QLO4 SWAPBBN1        D2(1) LO2 D3(5)

      M0_ASM_ARGS
      );
      SEI_CHK; INNER_SEI;
    } while(--counter);
#endif
    return num_leds;
}

#endif
