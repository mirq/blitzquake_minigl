##
##  Sound mixing routines for Amiga PPC
##

.include        "macrosPPC.i"
.include	"quakedefPPC.i"


	.text


# external references
	xrefv	shm
        xrefv	paintedtime
        xrefa	paintbuffer
        xrefa	volume


	funcdef	S_TransferPaintBufferASM

# The Amiga PPC version only supports 8 bit samples
	stwu	r1,-32(r1)
	lw	r4,shm			# r4 shm (struct dma_t)
	lxa	r5,paintbuffer		# r5 paintbuffer (int left,int right)
	lxa	r6,volume
	lfs	f1,16(r6)		# volume.value * 256
	ls	f2,c256
	fmuls	f1,f1,f2
	lw	r7,paintedtime
	subf.	r8,r7,r3		# r8 count
	beq	exit
	lwz	r9,dma_buffer(r4)	# r9 dma buffer start address
	fctiwz	f1,f1
	stfd	f1,24(r1)
	lwz	r10,28(r1)		# r10 = snd_vol
	lwz	r11,dma_samples(r4)
	add	r12,r9,r11
	subi	r11,r11,1
	and	r7,r7,r11
	add	r3,r7,r9		# r3 out
	lis	r4,0x7fffff@h		# r4 max val
	ori	r4,r4,0x7fffff@l
	not	r6,r4			# r6 min val
loop:
	lwz	r0,0(r5)
	mullw	r0,r0,r10
	addi	r5,r5,8
	cmpw	r0,r4
	ble	.1
	mr	r0,r4
	b	.2
.1:	cmpw	r0,r6
	bge	.2
	mr	r0,r6
.2:	srawi	r0,r0,16
	stb	r0,0(r3)
	addi	r3,r3,1
	cmpw	r3,r12
	bge	.3
	subic.	r8,r8,1
	bne	loop
	b	exit
.3:	mr	r3,r9
	subic.	r8,r8,1
	bne	loop
exit:
	addi	r1,r1,32
	blr

	funcend	S_TransferPaintBufferASM



.ifdef	WOS
	.tocd
.else
        .data
.endif
	.align	2
lab c256
	.float	256.0
