#ifndef _PIO_ASM_H
#define _PIO_ASM_H

/*
 * PIO "assembly" macro header, written by somewhatlurker
 * Released to public domain/CC0 license.
 * Comes with no guarantees of correctness.
 */

typedef uint16_t pio_instr;

#define PIO_INSTR_JMP  (0b000 << 13)
#define PIO_INSTR_WAIT (0b001 << 13)
#define PIO_INSTR_IN   (0b010 << 13)
#define PIO_INSTR_OUT  (0b011 << 13)
#define PIO_INSTR_PUSH (0b100 << 13)
#define PIO_INSTR_PULL ((0b100 << 13) | (0b1 << 7))
#define PIO_INSTR_MOV  (0b101 << 13)
#define PIO_INSTR_IRQ  (0b110 << 13)
#define PIO_INSTR_SET  (0b111 << 13)

#define PIO_DELAY(x, sideset_count)   (((x) & ((1 << (5 - sideset_count)) - 1)) << 8)
#define PIO_SIDESET(x, sideset_count) (((x) & ((1 << (sideset_count)) - 1)) << (13 - sideset_count))
#define PIO_SIDESET_ENABLE_BIT        (0b1 << 12)

#define PIO_JMP_CND_ALWAYS   (0b000 << 5)
#define PIO_JMP_CND_NOT_X    (0b001 << 5)
#define PIO_JMP_CND_X_DEC    (0b010 << 5)
#define PIO_JMP_CND_NOT_Y    (0b011 << 5)
#define PIO_JMP_CND_Y_DEC    (0b100 << 5)
#define PIO_JMP_CND_X_NE_Y   (0b101 << 5)
#define PIO_JMP_CND_PIN      (0b110 << 5)
#define PIO_JMP_CND_NOT_OSRE (0b111 << 5)
#define PIO_JMP_ADR(x)       ((x) & 0b11111)

#define PIO_WAIT_POLARITY_1 (0b1 << 7)
#define PIO_WAIT_POLARITY_0 (0b0 << 7)
#define PIO_WAIT_SRC_GPIO   (0b00 << 5)
#define PIO_WAIT_SRC_PIN    (0b01 << 5)
#define PIO_WAIT_SRC_IRQ    (0b10 << 5)
#define PIO_WAIT_IDX(x)     ((x) & 0b11111)

#define PIO_IN_SRC_PINS (0b000 << 5)
#define PIO_IN_SRC_X    (0b001 << 5)
#define PIO_IN_SRC_Y    (0b010 << 5)
#define PIO_IN_SRC_NULL (0b011 << 5)
#define PIO_IN_SRC_ISR  (0b110 << 5)
#define PIO_IN_SRC_OSR  (0b111 << 5)
#define PIO_IN_CNT(x)   ((x) & 0b11111)

#define PIO_OUT_DST_PINS    (0b000 << 5)
#define PIO_OUT_DST_X       (0b001 << 5)
#define PIO_OUT_DST_Y       (0b010 << 5)
#define PIO_OUT_DST_NULL    (0b011 << 5)
#define PIO_OUT_DST_PINDIRS (0b100 << 5)
#define PIO_OUT_DST_PC      (0b101 << 5)
#define PIO_OUT_DST_ISR     (0b110 << 5)
#define PIO_OUT_DST_EXEC    (0b111 << 5)
#define PIO_OUT_CNT(x)      ((x) & 0b11111)

#define PIO_PUSH_IFFULL (0b1 << 6)
#define PIO_PUSH_BLOCK  (0b1 << 5)

#define PIO_PULL_IFEMPTY (0b1 << 6)
#define PIO_PULL_BLOCK   (0b1 << 5)

#define PIO_MOV_DST_PINS   (0b000 << 5)
#define PIO_MOV_DST_X      (0b001 << 5)
#define PIO_MOV_DST_Y      (0b010 << 5)
#define PIO_MOV_DST_EXEC   (0b100 << 5)
#define PIO_MOV_DST_PC     (0b101 << 5)
#define PIO_MOV_DST_ISR    (0b110 << 5)
#define PIO_MOV_DST_OSR    (0b111 << 5)
#define PIO_MOV_OP_NONE    (0b00 << 3)
#define PIO_MOV_OP_INVERT  (0b00 << 3)
#define PIO_MOV_OP_REVERSE (0b00 << 3)
#define PIO_MOV_SRC_PINS   (0b000)
#define PIO_MOV_SRC_X      (0b001)
#define PIO_MOV_SRC_Y      (0b010)
#define PIO_MOV_SRC_NULL   (0b011)
#define PIO_MOV_SRC_STATUS (0b101)
#define PIO_MOV_SRC_ISR    (0b110)
#define PIO_MOV_SRC_OSR    (0b111)

#define PIO_IRQ_CLEAR   (0b1 << 6)
#define PIO_IRQ_WAIT    (0b1 << 5)
#define PIO_IRQ_IDX(x)  ((x) & 0b111)
#define PIO_IRQ_IDX_REL (0b1 << 4)

#define PIO_SET_DST_PINS    (0b000 << 5)
#define PIO_SET_DST_X       (0b001 << 5)
#define PIO_SET_DST_Y       (0b010 << 5)
#define PIO_SET_DST_PINDIRS (0b100 << 5)
#define PIO_SET_DATA(x)     ((x) & 0b11111)

#define PIO_NOP (PIO_INSTR_MOV | PIO_MOV_DST_Y | PIO_MOV_SRC_Y)

#endif // _PIO_ASM_H
