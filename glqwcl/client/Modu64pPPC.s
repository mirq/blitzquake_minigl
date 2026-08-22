#
# PowerUp fix which implements the missing Modu64p() function. :P
#
	.file	"Modu64pPPC.s"

	.extern	PPCDivu64


	.text

	.globl	PPCModu64p
PPCModu64p:
	mflr	r0
	stw	r0,4(r1)
	stwu	r1,-16(r1)
	stw	r31,12(r1)
	mr	r31,r3
	lwz	r5,0(r4)
	lwz	r6,4(r4)
	lwz	r4,4(r3)
	lwz	r3,0(r3)
	bl	PPCDivu64
	stw	r5,0(r31)
	stw	r6,4(r31)
	lwz	r31,12(r1)
	addi	r1,r1,16
	lwz	r0,4(r1)
	mtlr	r0
	blr

	.type	PPCModu64p,@function
	.size	PPCModu64p,$-PPCModu64p
