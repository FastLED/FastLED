@/* ***** BEGIN LICENSE BLOCK ***** 
@ * Version: RCSL 1.0/RPSL 1.0 
@ *  
@ * Portions Copyright (c) 1995-2002 RealNetworks, Inc. All Rights Reserved. 
@ *      
@ * The contents of this file, and the files included with this file, are 
@ * subject to the current version of the RealNetworks Public Source License 
@ * Version 1.0 (the "RPSL") available at 
@ * http://www.helixcommunity.org/content/rpsl unless you have licensed 
@ * the file under the RealNetworks Community Source License Version 1.0 
@ * (the "RCSL") available at http://www.helixcommunity.org/content/rcsl, 
@ * in which case the RCSL will apply. You may also obtain the license terms 
@ * directly from RealNetworks.  You may not use this file except in 
@ * compliance with the RPSL or, if you have a valid RCSL with RealNetworks 
@ * applicable to this file, the RCSL.  Please see the applicable RPSL or 
@ * RCSL for the rights, obligations and limitations governing use of the 
@ * contents of the file.  
@ *  
@ * This file is part of the Helix DNA Technology. RealNetworks is the 
@ * developer of the Original Code and owns the copyrights in the portions 
@ * it created. 
@ *  
@ * This file, and the files included with this file, is distributed and made 
@ * available on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER 
@ * EXPRESS OR IMPLIED, AND REALNETWORKS HEREBY DISCLAIMS ALL SUCH WARRANTIES, 
@ * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY, FITNESS 
@ * FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT. 
@ * 
@ * Technology Compatibility Kit Test Suite(s) Location: 
@ *    http://www.helixcommunity.org/content/tck 
@ * 
@ * Contributor(s): 
@ *  
@ * ***** END LICENSE BLOCK ***** */ 

# a lot faster in RAM (1-2 ms per frame)
.data

	.GLOBAL xmp3_PolyphaseStereo
xmp3_PolyphaseStereo:
	STMFD    r13!,{r4-r11,r14}
	SUB      r13,r13,#8
	STR      r0,[r13,#4]
	MOV      r4,#0x2000000
	MOV      r8,#0x2000000
	MOV      r5,#0
	MOV      r9,#0
	LDR      r12,[r2],#4
	LDR      r14,[r2],#4
	LDR      r0,[r1,#0]
	LDR      r3,[r1,#0x5c]
	SMLAL    r4,r5,r0,r12
	LDR      r0,[r1,#0x80]
	RSB      r14,r14,#0
	SMLAL    r4,r5,r3,r14
	LDR      r3,[r1,#0xdc]
	SMLAL    r8,r9,r0,r12
	SMLAL    r8,r9,r3,r14
	LDR      r12,[r2],#4
	LDR      r14,[r2],#4
	LDR      r0,[r1,#4]
	LDR      r3,[r1,#0x58]
	SMLAL    r4,r5,r0,r12
	LDR      r0,[r1,#0x84]
	RSB      r14,r14,#0
	SMLAL    r4,r5,r3,r14
	LDR      r3,[r1,#0xd8]
	SMLAL    r8,r9,r0,r12
	SMLAL    r8,r9,r3,r14
	LDR      r12,[r2],#4
	LDR      r14,[r2],#4
	LDR      r0,[r1,#8]
	LDR      r3,[r1,#0x54]
	SMLAL    r4,r5,r0,r12
	LDR      r0,[r1,#0x88]
	RSB      r14,r14,#0
	SMLAL    r4,r5,r3,r14
	LDR      r3,[r1,#0xd4]
	SMLAL    r8,r9,r0,r12
	SMLAL    r8,r9,r3,r14
	LDR      r12,[r2],#4
	LDR      r14,[r2],#4
	LDR      r0,[r1,#0xc]
	LDR      r3,[r1,#0x50]
	SMLAL    r4,r5,r0,r12
	LDR      r0,[r1,#0x8c]
	RSB      r14,r14,#0
	SMLAL    r4,r5,r3,r14
	LDR      r3,[r1,#0xd0]
	SMLAL    r8,r9,r0,r12
	SMLAL    r8,r9,r3,r14
	LDR      r12,[r2],#4
	LDR      r14,[r2],#4
	LDR      r0,[r1,#0x10]
	LDR      r3,[r1,#0x4c]
	SMLAL    r4,r5,r0,r12
	LDR      r0,[r1,#0x90]
	RSB      r14,r14,#0
	SMLAL    r4,r5,r3,r14
	LDR      r3,[r1,#0xcc]
	SMLAL    r8,r9,r0,r12
	SMLAL    r8,r9,r3,r14
	LDR      r12,[r2],#4
	LDR      r14,[r2],#4
	LDR      r0,[r1,#0x14]
	LDR      r3,[r1,#0x48]
	SMLAL    r4,r5,r0,r12
	LDR      r0,[r1,#0x94]
	RSB      r14,r14,#0
	SMLAL    r4,r5,r3,r14
	LDR      r3,[r1,#0xc8]
	SMLAL    r8,r9,r0,r12
	SMLAL    r8,r9,r3,r14
	LDR      r12,[r2],#4
	LDR      r14,[r2],#4
	LDR      r0,[r1,#0x18]
	LDR      r3,[r1,#0x44]
	SMLAL    r4,r5,r0,r12
	LDR      r0,[r1,#0x98]
	RSB      r14,r14,#0
	SMLAL    r4,r5,r3,r14
	LDR      r3,[r1,#0xc4]
	SMLAL    r8,r9,r0,r12
	SMLAL    r8,r9,r3,r14
	LDR      r12,[r2],#4
	LDR      r14,[r2],#4
	LDR      r0,[r1,#0x1c]
	LDR      r3,[r1,#0x40]
	SMLAL    r4,r5,r0,r12
	LDR      r0,[r1,#0x9c]
	RSB      r14,r14,#0
	SMLAL    r4,r5,r3,r14
	LDR      r3,[r1,#0xc0]
	SMLAL    r8,r9,r0,r12
	SMLAL    r8,r9,r3,r14
	LDR      r0,[r13,#4]
	MOV      r14,#0x7f00
	ORR      r14,r14,#0xff
	MOV      r4,r4,LSR #26
	ORR      r4,r4,r5,LSL #6
	MOV      r12,r4,ASR #31
	CMP      r12,r4,ASR #15
	EORNE    r4,r12,r14
	MOV      r8,r8,LSR #26
	ORR      r8,r8,r9,LSL #6
	MOV      r12,r8,ASR #31
	CMP      r12,r8,ASR #15
	EORNE    r8,r12,r14
	STRH     r4,[r0,#0]
	STRH     r8,[r0,#2]
	ADD      r2,r2,#0x3c0
	ADD      r1,r1,#0x1000
	MOV      r4,#0x2000000
	MOV      r8,#0x2000000
	MOV      r5,#0
	MOV      r9,#0
	LDR      r12,[r2],#4
	LDR      r0,[r1,#0]
	LDR      r3,[r1,#0x80]
	SMLAL    r4,r5,r0,r12
	SMLAL    r8,r9,r3,r12
	LDR      r12,[r2],#4
	LDR      r0,[r1,#4]
	LDR      r3,[r1,#0x84]
	SMLAL    r4,r5,r0,r12
	SMLAL    r8,r9,r3,r12
	LDR      r12,[r2],#4
	LDR      r0,[r1,#8]
	LDR      r3,[r1,#0x88]
	SMLAL    r4,r5,r0,r12
	SMLAL    r8,r9,r3,r12
	LDR      r12,[r2],#4
	LDR      r0,[r1,#0xc]
	LDR      r3,[r1,#0x8c]
	SMLAL    r4,r5,r0,r12
	SMLAL    r8,r9,r3,r12
	LDR      r12,[r2],#4
	LDR      r0,[r1,#0x10]
	LDR      r3,[r1,#0x90]
	SMLAL    r4,r5,r0,r12
	SMLAL    r8,r9,r3,r12
	LDR      r12,[r2],#4
	LDR      r0,[r1,#0x14]
	LDR      r3,[r1,#0x94]
	SMLAL    r4,r5,r0,r12
	SMLAL    r8,r9,r3,r12
	LDR      r12,[r2],#4
	LDR      r0,[r1,#0x18]
	LDR      r3,[r1,#0x98]
	SMLAL    r4,r5,r0,r12
	SMLAL    r8,r9,r3,r12
	LDR      r12,[r2],#4
	LDR      r0,[r1,#0x1c]
	LDR      r3,[r1,#0x9c]
	SMLAL    r4,r5,r0,r12
	SMLAL    r8,r9,r3,r12
	LDR      r0,[r13,#4]
	MOV      r14,#0x7f00
	ORR      r14,r14,#0xff
	MOV      r4,r4,LSR #26
	ORR      r4,r4,r5,LSL #6
	MOV      r12,r4,ASR #31
	CMP      r12,r4,ASR #15
	EORNE    r4,r12,r14
	MOV      r8,r8,LSR #26
	ORR      r8,r8,r9,LSL #6
	MOV      r12,r8,ASR #31
	CMP      r12,r8,ASR #15
	EORNE    r8,r12,r14
	STRH     r4,[r0,#0x40]
	STRH     r8,[r0,#0x42]
	SUB      r2,r2,#0x3e0
	SUB      r1,r1,#0xf00
	MOV      r12,#0xf
	ADD      r0,r0,#4
LoopPS:
	STR      r12,[r13,#0]
	STR      r0,[r13,#4]
	MOV      r4,#0x2000000
	MOV      r8,#0x2000000
	MOV      r6,#0x2000000
	MOV      r10,#0x2000000
	MOV      r5,#0
	MOV      r9,#0
	MOV      r7,#0
	MOV      r11,#0
	LDR      r12,[r2],#4
	LDR      r14,[r2],#4
	LDR      r0,[r1,#0]
	LDR      r3,[r1,#0x5c]
	SMLAL    r4,r5,r0,r12
	SMLAL    r6,r7,r0,r14
	RSB      r14,r14,#0
	SMLAL    r6,r7,r3,r12
	SMLAL    r4,r5,r3,r14
	LDR      r3,[r1,#0xdc]
	LDR      r0,[r1,#0x80]
	SMLAL    r8,r9,r3,r14
	SMLAL    r10,r11,r3,r12
	RSB      r14,r14,#0
	SMLAL    r8,r9,r0,r12
	SMLAL    r10,r11,r0,r14
	LDR      r12,[r2],#4
	LDR      r14,[r2],#4
	LDR      r0,[r1,#4]
	LDR      r3,[r1,#0x58]
	SMLAL    r4,r5,r0,r12
	SMLAL    r6,r7,r0,r14
	RSB      r14,r14,#0
	SMLAL    r6,r7,r3,r12
	SMLAL    r4,r5,r3,r14
	LDR      r3,[r1,#0xd8]
	LDR      r0,[r1,#0x84]
	SMLAL    r8,r9,r3,r14
	SMLAL    r10,r11,r3,r12
	RSB      r14,r14,#0
	SMLAL    r8,r9,r0,r12
	SMLAL    r10,r11,r0,r14
	LDR      r12,[r2],#4
	LDR      r14,[r2],#4
	LDR      r0,[r1,#8]
	LDR      r3,[r1,#0x54]
	SMLAL    r4,r5,r0,r12
	SMLAL    r6,r7,r0,r14
	RSB      r14,r14,#0
	SMLAL    r6,r7,r3,r12
	SMLAL    r4,r5,r3,r14
	LDR      r3,[r1,#0xd4]
	LDR      r0,[r1,#0x88]
	SMLAL    r8,r9,r3,r14
	SMLAL    r10,r11,r3,r12
	RSB      r14,r14,#0
	SMLAL    r8,r9,r0,r12
	SMLAL    r10,r11,r0,r14
	LDR      r12,[r2],#4
	LDR      r14,[r2],#4
	LDR      r0,[r1,#0xc]
	LDR      r3,[r1,#0x50]
	SMLAL    r4,r5,r0,r12
	SMLAL    r6,r7,r0,r14
	RSB      r14,r14,#0
	SMLAL    r6,r7,r3,r12
	SMLAL    r4,r5,r3,r14
	LDR      r3,[r1,#0xd0]
	LDR      r0,[r1,#0x8c]
	SMLAL    r8,r9,r3,r14
	SMLAL    r10,r11,r3,r12
	RSB      r14,r14,#0
	SMLAL    r8,r9,r0,r12
	SMLAL    r10,r11,r0,r14
	LDR      r12,[r2],#4
	LDR      r14,[r2],#4
	LDR      r0,[r1,#0x10]
	LDR      r3,[r1,#0x4c]
	SMLAL    r4,r5,r0,r12
	SMLAL    r6,r7,r0,r14
	RSB      r14,r14,#0
	SMLAL    r6,r7,r3,r12
	SMLAL    r4,r5,r3,r14
	LDR      r3,[r1,#0xcc]
	LDR      r0,[r1,#0x90]
	SMLAL    r8,r9,r3,r14
	SMLAL    r10,r11,r3,r12
	RSB      r14,r14,#0
	SMLAL    r8,r9,r0,r12
	SMLAL    r10,r11,r0,r14
	LDR      r12,[r2],#4
	LDR      r14,[r2],#4
	LDR      r0,[r1,#0x14]
	LDR      r3,[r1,#0x48]
	SMLAL    r4,r5,r0,r12
	SMLAL    r6,r7,r0,r14
	RSB      r14,r14,#0
	SMLAL    r6,r7,r3,r12
	SMLAL    r4,r5,r3,r14
	LDR      r3,[r1,#0xc8]
	LDR      r0,[r1,#0x94]
	SMLAL    r8,r9,r3,r14
	SMLAL    r10,r11,r3,r12
	RSB      r14,r14,#0
	SMLAL    r8,r9,r0,r12
	SMLAL    r10,r11,r0,r14
	LDR      r12,[r2],#4
	LDR      r14,[r2],#4
	LDR      r0,[r1,#0x18]
	LDR      r3,[r1,#0x44]
	SMLAL    r4,r5,r0,r12
	SMLAL    r6,r7,r0,r14
	RSB      r14,r14,#0
	SMLAL    r6,r7,r3,r12
	SMLAL    r4,r5,r3,r14
	LDR      r3,[r1,#0xc4]
	LDR      r0,[r1,#0x98]
	SMLAL    r8,r9,r3,r14
	SMLAL    r10,r11,r3,r12
	RSB      r14,r14,#0
	SMLAL    r8,r9,r0,r12
	SMLAL    r10,r11,r0,r14
	LDR      r12,[r2],#4
	LDR      r14,[r2],#4
	LDR      r0,[r1,#0x1c]
	LDR      r3,[r1,#0x40]
	SMLAL    r4,r5,r0,r12
	SMLAL    r6,r7,r0,r14
	RSB      r14,r14,#0
	SMLAL    r6,r7,r3,r12
	SMLAL    r4,r5,r3,r14
	LDR      r3,[r1,#0xc0]
	LDR      r0,[r1,#0x9c]
	SMLAL    r8,r9,r3,r14
	SMLAL    r10,r11,r3,r12
	RSB      r14,r14,#0
	SMLAL    r8,r9,r0,r12
	SMLAL    r10,r11,r0,r14
	ADD      r1,r1,#0x100
	LDR      r0,[r13,#4]
	MOV      r14,#0x7f00
	ORR      r14,r14,#0xff
	MOV      r4,r4,LSR #26
	ORR      r4,r4,r5,LSL #6
	MOV      r12,r4,ASR #31
	CMP      r12,r4,ASR #15
	EORNE    r4,r12,r14
	MOV      r8,r8,LSR #26
	ORR      r8,r8,r9,LSL #6
	MOV      r12,r8,ASR #31
	CMP      r12,r8,ASR #15
	EORNE    r8,r12,r14
	MOV      r6,r6,LSR #26
	ORR      r6,r6,r7,LSL #6
	MOV      r12,r6,ASR #31
	CMP      r12,r6,ASR #15
	EORNE    r6,r12,r14
	MOV      r10,r10,LSR #26
	ORR      r10,r10,r11,LSL #6
	MOV      r12,r10,ASR #31
	CMP      r12,r10,ASR #15
	EORNE    r10,r12,r14
	LDR      r12,[r13,#0]
	ADD      r14,r0,r12,LSL #3
	STRH     r6,[r14],#2
	STRH     r10,[r14],#2
	STRH     r4,[r0],#2
	STRH     r8,[r0],#2
	SUBS     r12,r12,#1
	BNE      LoopPS
	ADD      r13,r13,#8
	LDMFD    r13!,{r4-r11,pc}

	.GLOBAL xmp3_PolyphaseMono
xmp3_PolyphaseMono:
	STMFD    r13!,{r4-r11,r14}
	SUB      r13,r13,#8
	STR      r0,[r13,#4]
	MOV      r4,#0x2000000
	MOV      r5,#0
	LDR      r12,[r2],#4
	LDR      r14,[r2],#4
	LDR      r0,[r1,#0]
	LDR      r3,[r1,#0x5c]
	RSB      r14,r14,#0
	SMLAL    r4,r5,r0,r12
	SMLAL    r4,r5,r3,r14
	LDR      r12,[r2],#4
	LDR      r14,[r2],#4
	LDR      r0,[r1,#4]
	LDR      r3,[r1,#0x58]
	RSB      r14,r14,#0
	SMLAL    r4,r5,r0,r12
	SMLAL    r4,r5,r3,r14
	LDR      r12,[r2],#4
	LDR      r14,[r2],#4
	LDR      r0,[r1,#8]
	LDR      r3,[r1,#0x54]
	RSB      r14,r14,#0
	SMLAL    r4,r5,r0,r12
	SMLAL    r4,r5,r3,r14
	LDR      r12,[r2],#4
	LDR      r14,[r2],#4
	LDR      r0,[r1,#0xc]
	LDR      r3,[r1,#0x50]
	RSB      r14,r14,#0
	SMLAL    r4,r5,r0,r12
	SMLAL    r4,r5,r3,r14
	LDR      r12,[r2],#4
	LDR      r14,[r2],#4
	LDR      r0,[r1,#0x10]
	LDR      r3,[r1,#0x4c]
	RSB      r14,r14,#0
	SMLAL    r4,r5,r0,r12
	SMLAL    r4,r5,r3,r14
	LDR      r12,[r2],#4
	LDR      r14,[r2],#4
	LDR      r0,[r1,#0x14]
	LDR      r3,[r1,#0x48]
	RSB      r14,r14,#0
	SMLAL    r4,r5,r0,r12
	SMLAL    r4,r5,r3,r14
	LDR      r12,[r2],#4
	LDR      r14,[r2],#4
	LDR      r0,[r1,#0x18]
	LDR      r3,[r1,#0x44]
	RSB      r14,r14,#0
	SMLAL    r4,r5,r0,r12
	SMLAL    r4,r5,r3,r14
	LDR      r12,[r2],#4
	LDR      r14,[r2],#4
	LDR      r0,[r1,#0x1c]
	LDR      r3,[r1,#0x40]
	RSB      r14,r14,#0
	SMLAL    r4,r5,r0,r12
	SMLAL    r4,r5,r3,r14
	LDR      r0,[r13,#4]
	MOV      r14,#0x7f00
	ORR      r14,r14,#0xff
	MOV      r4,r4,LSR #26
	ORR      r4,r4,r5,LSL #6
	MOV      r12,r4,ASR #31
	CMP      r12,r4,ASR #15
	EORNE    r4,r12,r14
	STRH     r4,[r0,#0]
	ADD      r2,r2,#0x3c0
	ADD      r1,r1,#0x1000
	MOV      r4,#0x2000000
	MOV      r5,#0
	LDR      r12,[r2],#4
	LDR      r0,[r1,#0]
	SMLAL    r4,r5,r0,r12
	LDR      r12,[r2],#4
	LDR      r0,[r1,#4]
	SMLAL    r4,r5,r0,r12
	LDR      r12,[r2],#4
	LDR      r0,[r1,#8]
	SMLAL    r4,r5,r0,r12
	LDR      r12,[r2],#4
	LDR      r0,[r1,#0xc]
	SMLAL    r4,r5,r0,r12
	LDR      r12,[r2],#4
	LDR      r0,[r1,#0x10]
	SMLAL    r4,r5,r0,r12
	LDR      r12,[r2],#4
	LDR      r0,[r1,#0x14]
	SMLAL    r4,r5,r0,r12
	LDR      r12,[r2],#4
	LDR      r0,[r1,#0x18]
	SMLAL    r4,r5,r0,r12
	LDR      r12,[r2],#4
	LDR      r0,[r1,#0x1c]
	SMLAL    r4,r5,r0,r12
	LDR      r0,[r13,#4]
	MOV      r14,#0x7f00
	ORR      r14,r14,#0xff
	MOV      r4,r4,LSR #26
	ORR      r4,r4,r5,LSL #6
	MOV      r12,r4,ASR #31
	CMP      r12,r4,ASR #15
	EORNE    r4,r12,r14
	STRH     r4,[r0,#0x20]
	SUB      r2,r2,#0x3e0
	SUB      r1,r1,#0xf00
	MOV      r12,#0xf
	ADD      r0,r0,#2
LoopPM:
	STR      r12,[r13,#0]
	STR      r0,[r13,#4]
	MOV      r4,#0x2000000
	MOV      r6,#0x2000000
	MOV      r5,#0
	MOV      r7,#0
	LDR      r12,[r2],#4
	LDR      r14,[r2],#4
	LDR      r0,[r1,#0]
	LDR      r3,[r1,#0x5c]
	SMLAL    r4,r5,r0,r12
	SMLAL    r6,r7,r0,r14
	RSB      r14,r14,#0
	SMLAL    r4,r5,r3,r14
	SMLAL    r6,r7,r3,r12
	LDR      r12,[r2],#4
	LDR      r14,[r2],#4
	LDR      r0,[r1,#4]
	LDR      r3,[r1,#0x58]
	SMLAL    r4,r5,r0,r12
	SMLAL    r6,r7,r0,r14
	RSB      r14,r14,#0
	SMLAL    r4,r5,r3,r14
	SMLAL    r6,r7,r3,r12
	LDR      r12,[r2],#4
	LDR      r14,[r2],#4
	LDR      r0,[r1,#8]
	LDR      r3,[r1,#0x54]
	SMLAL    r4,r5,r0,r12
	SMLAL    r6,r7,r0,r14
	RSB      r14,r14,#0
	SMLAL    r4,r5,r3,r14
	SMLAL    r6,r7,r3,r12
	LDR      r12,[r2],#4
	LDR      r14,[r2],#4
	LDR      r0,[r1,#0xc]
	LDR      r3,[r1,#0x50]
	SMLAL    r4,r5,r0,r12
	SMLAL    r6,r7,r0,r14
	RSB      r14,r14,#0
	SMLAL    r4,r5,r3,r14
	SMLAL    r6,r7,r3,r12
	LDR      r12,[r2],#4
	LDR      r14,[r2],#4
	LDR      r0,[r1,#0x10]
	LDR      r3,[r1,#0x4c]
	SMLAL    r4,r5,r0,r12
	SMLAL    r6,r7,r0,r14
	RSB      r14,r14,#0
	SMLAL    r4,r5,r3,r14
	SMLAL    r6,r7,r3,r12
	LDR      r12,[r2],#4
	LDR      r14,[r2],#4
	LDR      r0,[r1,#0x14]
	LDR      r3,[r1,#0x48]
	SMLAL    r4,r5,r0,r12
	SMLAL    r6,r7,r0,r14
	RSB      r14,r14,#0
	SMLAL    r4,r5,r3,r14
	SMLAL    r6,r7,r3,r12
	LDR      r12,[r2],#4
	LDR      r14,[r2],#4
	LDR      r0,[r1,#0x18]
	LDR      r3,[r1,#0x44]
	SMLAL    r4,r5,r0,r12
	SMLAL    r6,r7,r0,r14
	RSB      r14,r14,#0
	SMLAL    r4,r5,r3,r14
	SMLAL    r6,r7,r3,r12
	LDR      r12,[r2],#4
	LDR      r14,[r2],#4
	LDR      r0,[r1,#0x1c]
	LDR      r3,[r1,#0x40]
	SMLAL    r4,r5,r0,r12
	SMLAL    r6,r7,r0,r14
	RSB      r14,r14,#0
	SMLAL    r4,r5,r3,r14
	SMLAL    r6,r7,r3,r12
	ADD      r1,r1,#0x100
	LDR      r0,[r13,#4]
	MOV      r14,#0x7f00
	ORR      r14,r14,#0xff
	MOV      r4,r4,LSR #26
	ORR      r4,r4,r5,LSL #6
	MOV      r12,r4,ASR #31
	CMP      r12,r4,ASR #15
	EORNE    r4,r12,r14
	MOV      r6,r6,LSR #26
	ORR      r6,r6,r7,LSL #6
	MOV      r12,r6,ASR #31
	CMP      r12,r6,ASR #15
	EORNE    r6,r12,r14
	LDR      r12,[r13,#0]
	ADD      r14,r0,r12,LSL #2
	STRH     r6,[r14],#2
	STRH     r4,[r0],#2
	SUBS     r12,r12,#1
	BNE      LoopPM
	ADD      r13,r13,#8
	LDMFD    r13!,{r4-r11,pc}

	.END
