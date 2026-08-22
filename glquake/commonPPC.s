##
## commonPPC.s
##
## PPC assembler implementations of several functions in common.c
##

.include	"macrosPPC.i"


# void Q_memcpy (void *dest, void *src, int count)

	funcdef	Q_memcpy

	mr.	r5,r5
	or	r0,r3,r4
	or	r0,r0,r5
	beqlr
	andi.   r0,r0,3
	bne     bytecopy
	srawi   r5,r5,2
	subi    r3,r3,4
	subi    r4,r4,4
	mtctr   r5
loop:
	lwzu    r0,4(r4)
	stwu    r0,4(r3)
	bdnz    loop
	blr
bytecopy:
	subi    r3,r3,1
	subi    r4,r4,1
	mtctr   r5
loop2:
	lbzu    r0,1(r4)
	stbu    r0,1(r3)
	bdnz    loop2
	blr


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
