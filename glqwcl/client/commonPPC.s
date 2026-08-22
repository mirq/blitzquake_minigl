##
## commonPPC.s
##
## PPC assembler implementations of several functions in common.c
##

.ifdef	WOS
	.macro	funcdef
	.text
	.globl	_\1
	.align	3
_\1:
	.endm

.else
	.macro	funcdef
	.text
	.globl	\1
	.align	4
\1:
	.endm
.endif


# short ShortSwap (short l)
#       swap word (LE->BE)

	funcdef	ShortSwap

	extrwi	r0,r3,8,16
	insrwi	r0,r3,8,16
	extsh	r3,r0
	blr


# int LongSwap (int l)
#     swap longword (LE->BE)

	funcdef	LongSwap

	extrwi	r0,r3,8,0
	rlwimi	r0,r3,24,16,23
	rlwimi	r0,r3,8,8,15
	insrwi	r0,r3,8,0
	mr	r3,r0
	blr


# float FloatSwap (float f)
#       swap float (LE->BE)

	funcdef	FloatSwap

	stwu	r1,-32(r1)
	stfs	f1,24(r1)
	lwz	r3,24(r1)
	li	r0,24
	stwbrx	r3,r1,r0
	lfs	f1,24(r1)
	addi	r1,r1,32
	blr
