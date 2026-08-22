##
## Quake for AMIGA
##
## r_surfPPC.s
##
## Define WOS for PowerOpen ABI, otherwise SVR4-ABI is used.
##

.set NOLR,1
.include        "macrosPPC.i"
.include	"quakedefPPC.i"

#
# external references
#

	xrefv	pbasesource
	xrefv	prowdestbase
	xrefv	r_lightptr
	xrefv	r_lightwidth
	xrefv	r_numvblocks
	xrefv	r_sourcemax
	xrefv	r_stepback
	xrefv	sourcetstep
	xrefv	surfrowbytes
	xrefa	vid




###########################################################################
#
#       void R_DrawSurfaceBlock8_mip0
#
###########################################################################

	funcdef	R_DrawSurfaceBlock8_mip0

	init	0,0,7,0
	stmw	r25,gb(r1)

	lxa     r11,vid
	lw      r3,pbasesource
	lw      r4,prowdestbase
	lw      r5,r_lightptr
	lwz     r11,VID_COLORMAP(r11)
	lw      r31,r_numvblocks
	lw      r28,sourcetstep
	lw      r27,surfrowbytes
	lw      r26,r_sourcemax
	lw      r25,r_stepback
	lw      r8,r_lightwidth
	slwi    r8,r8,2
.m0loop:
	li      r0,16
	lwz     r6,0(r5)                #r6 = lightleft
	lwz     r7,4(r5)                #r7 = lightright
	add     r5,r5,r8
	lwz     r9,0(r5)
	lwz     r10,4(r5)
	subf    r9,r6,r9
	subf    r10,r7,r10
	srawi   r9,r9,4                 #r9 = lightleftstep
	srawi   r10,r10,4               #r10 = lightrightstep
	mtctr   r0
.m0loop2:
	li      r0,15
	subf    r12,r7,r6
	srawi   r12,r12,4               #r12 = lightstep
	mr      r30,r7                  #r30 = light
.m0loop3:
	lbzx    r29,r3,r0               #r29 = pix
	rlwimi  r29,r30,0,16,23
	lbzx    r29,r11,r29
	stbx    r29,r4,r0
	subic.  r0,r0,1
	add     r30,r30,r12
	bge     .m0loop3
	add     r3,r3,r28
	add     r6,r6,r9
	add     r7,r7,r10
	add     r4,r4,r27
	bdnz    .m0loop2
	cmpw    r3,r26
	blt     .m0next
	sub     r3,r3,r25
.m0next:
	subic.  r31,r31,1
	bne     .m0loop
	lmw	r25,gb(r1)
	exit

	funcend	R_DrawSurfaceBlock8_mip0




###########################################################################
#
#       void R_DrawSurfaceBlock8_mip1
#
###########################################################################

	funcdef	R_DrawSurfaceBlock8_mip1

	init	0,0,7,0
	stmw	r25,gb(r1)

	lxa     r11,vid
	lw      r3,pbasesource
	lw      r4,prowdestbase
	lw      r5,r_lightptr
	lwz     r11,VID_COLORMAP(r11)
	lw      r31,r_numvblocks
	lw      r28,sourcetstep
	lw      r27,surfrowbytes
	lw      r26,r_sourcemax
	lw      r25,r_stepback
	lw      r8,r_lightwidth
	slwi    r8,r8,2
.m1loop:
	li      r0,8
	lwz     r6,0(r5)                #r6 = lightleft
	lwz     r7,4(r5)                #r7 = lightright
	add     r5,r5,r8
	lwz     r9,0(r5)
	lwz     r10,4(r5)
	subf    r9,r6,r9
	subf    r10,r7,r10
	srawi   r9,r9,3                 #r9 = lightleftstep
	srawi   r10,r10,3               #r10 = lightrightstep
	mtctr   r0
.m1loop2:
	li      r0,7
	subf    r12,r7,r6
	srawi   r12,r12,3               #r12 = lightstep
	mr      r30,r7                  #r30 = light
.m1loop3:
	lbzx    r29,r3,r0               #r29 = pix
	rlwimi  r29,r30,0,16,23
	lbzx    r29,r11,r29
	stbx    r29,r4,r0
	subic.  r0,r0,1
	add     r30,r30,r12
	bge     .m1loop3
	add     r3,r3,r28
	add     r6,r6,r9
	add     r7,r7,r10
	add     r4,r4,r27
	bdnz    .m1loop2
	cmpw    r3,r26
	blt     .m1next
	sub     r3,r3,r25
.m1next:
	subic.  r31,r31,1
	bne     .m1loop
	lmw	r25,gb(r1)
	exit

	funcend	R_DrawSurfaceBlock8_mip1




###########################################################################
#
#       void R_DrawSurfaceBlock8_mip2
#
###########################################################################

	funcdef	R_DrawSurfaceBlock8_mip2

	init	0,0,7,0
	stmw	r25,gb(r1)

	lxa     r11,vid
	lw      r3,pbasesource
	lw      r4,prowdestbase
	lw      r5,r_lightptr
	lwz     r11,VID_COLORMAP(r11)
	lw      r31,r_numvblocks
	lw      r28,sourcetstep
	lw      r27,surfrowbytes
	lw      r26,r_sourcemax
	lw      r25,r_stepback
	lw      r8,r_lightwidth
	slwi    r8,r8,2
.m2loop:
	li      r0,4
	lwz     r6,0(r5)                #r6 = lightleft
	lwz     r7,4(r5)                #r7 = lightright
	add     r5,r5,r8
	lwz     r9,0(r5)
	lwz     r10,4(r5)
	subf    r9,r6,r9
	subf    r10,r7,r10
	srawi   r9,r9,2                 #r9 = lightleftstep
	srawi   r10,r10,2               #r10 = lightrightstep
	mtctr   r0
.m2loop2:
	li      r0,3
	subf    r12,r7,r6
	srawi   r12,r12,2               #r12 = lightstep
	mr      r30,r7                  #r30 = light
.m2loop3:
	lbzx    r29,r3,r0               #r29 = pix
	rlwimi  r29,r30,0,16,23
	lbzx    r29,r11,r29
	stbx    r29,r4,r0
	subic.  r0,r0,1
	add     r30,r30,r12
	bge     .m2loop3
	add     r3,r3,r28
	add     r6,r6,r9
	add     r7,r7,r10
	add     r4,r4,r27
	bdnz    .m2loop2
	cmpw    r3,r26
	blt     .m2next
	sub     r3,r3,r25
.m2next:
	subic.  r31,r31,1
	bne     .m2loop
	lmw	r25,gb(r1)
	exit

	funcend	R_DrawSurfaceBlock8_mip2




###########################################################################
#
#       void R_DrawSurfaceBlock8_mip3
#
###########################################################################

	funcdef	R_DrawSurfaceBlock8_mip3

	init	0,0,7,0
	stmw	r25,gb(r1)

	lxa     r11,vid
	lw      r3,pbasesource
	lw      r4,prowdestbase
	lw      r5,r_lightptr
	lwz     r11,VID_COLORMAP(r11)
	lw      r31,r_numvblocks
	lw      r28,sourcetstep
	lw      r27,surfrowbytes
	lw      r26,r_sourcemax
	lw      r25,r_stepback
	lw      r8,r_lightwidth
	slwi    r8,r8,2
.m3loop:
	li      r0,2
	lwz     r6,0(r5)                #r6 = lightleft
	lwz     r7,4(r5)                #r7 = lightright
	add     r5,r5,r8
	lwz     r9,0(r5)
	lwz     r10,4(r5)
	subf    r9,r6,r9
	subf    r10,r7,r10
	srawi   r9,r9,1                 #r9 = lightleftstep
	srawi   r10,r10,1               #r10 = lightrighctstep
	mtctr   r0
.m3loop2:
	subf    r12,r7,r6
	srawi   r12,r12,1               #r12 = lightstep

	lbz     r29,1(r3)               #r29 = pix
	rlwimi  r29,r7,0,16,23
	lbzx    r29,r11,r29
	stb     r29,1(r4)
	add     r30,r7,r12
	lbz     r29,0(r3)               #r29 = pix
	rlwimi  r29,r30,0,16,23
	lbzx    r29,r11,r29
	stb     r29,0(r4)

	add     r3,r3,r28
	add     r6,r6,r9
	add     r7,r7,r10
	add     r4,r4,r27
	bdnz    .m3loop2
	cmpw    r3,r26
	blt     .m3next
	sub     r3,r3,r25
.m3next:
	subic.  r31,r31,1
	bne     .m3loop
	lmw	r25,gb(r1)
	exit

	funcend	R_DrawSurfaceBlock8_mip3
