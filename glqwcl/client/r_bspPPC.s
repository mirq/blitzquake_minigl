##
## Quake for AMIGA
##
## r_bspPPC.s
##
## Define WOS for PowerOpen ABI, otherwise SVR4-ABI is used.
##

.include        "macrosPPC.i"
.include	"quakedefPPC.i"

#
# external references
#

	xrefv	r_visframecount
	xrefv	r_framecount
	xrefv	r_currentkey
	xrefv	r_drawpolys
	xrefv	r_worldpolysbacktofront
	xrefv	pbtofpolys
	xrefv	numbtofpolys
	xrefv	pfrustum_indexes
	xrefa	modelorg
	xrefa	vpn
	xrefa	vright
	xrefa	vup
	xrefa	view_clipplanes
	xrefv	currententity
	xrefa	screenedge
	xrefa	entity_rotation

.ifdef	__STORM__
	xrefv	sin__r
	xrefv	cos__r
.else
	xrefv	sin
	xrefv	cos
.endif

#
# defines
#

.set	PITCH                ,0
.set	YAW                  ,1
.set	ROLL                 ,2




###########################################################################
#
#       void R_RotateBmodel (void)
#
###########################################################################

	funcdef	R_RotateBmodel

	init	0,0,1,18
	stw	r31,gb(r1)
	stfd	f14,fb+0*8(r1)
	stfd	f15,fb+1*8(r1)
	stfd	f16,fb+2*8(r1)
	stfd	f17,fb+3*8(r1)
	stfd	f18,fb+4*8(r1)
	stfd	f19,fb+5*8(r1)
	stfd	f20,fb+6*8(r1)
	stfd	f21,fb+7*8(r1)
	stfd	f22,fb+8*8(r1)
	stfd	f23,fb+9*8(r1)
	stfd	f24,fb+10*8(r1)
	stfd	f25,fb+11*8(r1)
	stfd	f26,fb+12*8(r1)
	stfd	f27,fb+13*8(r1)
	stfd	f28,fb+14*8(r1)
	stfd	f29,fb+15*8(r1)
	stfd	f30,fb+16*8(r1)
	stfd	f31,fb+17*8(r1)

	ls      f14,cPI
	lw      r31,currententity
	lfs     f1,ENTITY_ANGLES+YAW*4(r31)
	fmuls   f15,f1,f14
	fmr     f1,f15
	mcall	sin
	fmr     f16,f1                  #f16 = s [YAW]
	fmr     f1,f15
	mcall	cos
	fmr     f17,f1                  #f17 = c [YAW]
	lfs     f1,ENTITY_ANGLES+PITCH*4(r31)
	fmuls   f15,f1,f14
	fmr     f1,f15
	mcall	sin
	fmr     f18,f1                  #f18 = s [PITCH]
	fmr     f1,f15
	mcall	cos
	fmr     f19,f1                  #f19 = c [PITCH] = 32(a2)
	lfs     f1,ENTITY_ANGLES+ROLL*4(r31)
	fmuls   f15,f1,f14
	fmr     f1,f15
	mcall	sin
	fmr     f20,f1                  #f20 = s [ROLL]
	fmr     f1,f15
	mcall	cos
	fmuls   f0,f17,f19
	fmr     f15,f1                  #f15 = c [ROLL]
	lxa     r3,entity_rotation 	#r3 = entity_rotation
	stfs    f0,0(r3)                #f0 = entity_rotation [0,0]
	fmuls   f1,f16,f19              #f1 = 4(a2)
	fneg    f2,f18                  #f2 = 8(a2)
	fneg    f3,f16
	fmuls   f4,f17,f18
	stfs    f3,12(r3)               #f3 = entity_rotation[1,0]
	stfs    f4,24(r3)               #f4 = entity_rotation[2,0]
	fmuls   f5,f16,f18              #f5 = 28(a2)
	fmuls   f6,f20,f2
	fmsubs  f7,f15,f1,f6
	fmuls   f8,f20,f1
	stfs    f7,4(r3)                #f7 = entity_rotation[0,1]
	fmuls   f12,f20,f19
	fmadds  f9,f2,f15,f8
	fmuls   f10,f17,f15
	stfs    f9,8(r3)                #f9 = entity_rotation[0,2]
	stfs    f10,16(r3)              #f10 = entity_rotation[1,1]
	fmuls   f2,f19,f15
	fmuls   f11,f17,f20
	fmsubs  f13,f15,f5,f12
	stfs    f11,20(r3)              #f11 = entity_rotation[1,2]
	stfs    f13,28(r3)              #f13 = entity_rotation[2,1]
	fmadds  f1,f20,f5,f2
	stfs    f1,32(r3)               #f1 = entity_rotation[2,2]
	lxa     r7,modelorg
	lfs     f19,0(r7)               #f19 = modelorg[0]
	lfs     f20,4(r7)               #f20 = modelorg[1]
	lfs     f21,8(r7)               #f21 = modelorg[2]
	lxa     r4,vpn
	lfs     f2,0(r4)                #f2 = vpn[0]
	lfs     f6,4(r4)                #f6 = vpn[1]
	lfs     f8,8(r4)                #f8 = vpn[2]
	lxa     r5,vright
	lfs     f12,0(r5)               #f12 = vright[0]
	lfs     f14,4(r5)               #f14 = vright[1]
	lfs     f15,8(r5)               #f15 = vright[2]
	lxa     r6,vup
	lfs     f16,0(r6)               #f16 = vup[0]
	lfs     f17,4(r6)               #f17 = vup[1]
	lfs     f18,8(r6)               #f18 = vup[2]

	fmuls   f22,f19,f0
	fmuls   f25,f2,f0
	fmuls   f28,f12,f0
	fmuls   f31,f16,f0
	fmadds  f22,f20,f7,f22
	fmadds  f25,f6,f7,f25
	fmadds  f28,f14,f7,f28
	fmadds  f31,f17,f7,f31
	fmadds  f22,f21,f9,f22
	fmadds  f25,f8,f9,f25
	fmadds  f28,f15,f9,f28
	fmadds  f31,f18,f9,f31
	fmuls   f23,f19,f3
	fmuls   f26,f2,f3
	fmuls   f29,f12,f3
	fmuls   f0,f16,f3
	fmadds  f23,f20,f10,f23
	fmadds  f26,f6,f10,f26
	fmadds  f29,f14,f10,f29
	fmadds  f0,f17,f10,f0
	fmadds  f23,f21,f11,f23
	fmadds  f26,f8,f11,f26
	fmadds  f29,f15,f11,f29
	fmadds  f0,f18,f11,f0
	fmuls   f24,f19,f4
	fmuls   f27,f2,f4
	fmuls   f30,f12,f4
	fmuls   f7,f16,f4
	fmadds  f24,f20,f13,f24
	fmadds  f27,f6,f13,f27
	fmadds  f30,f14,f13,f30
	fmadds  f7,f17,f13,f7
	fmadds  f21,f21,f1,f24
	fmadds  f8,f8,f1,f27
	fmadds  f15,f15,f1,f30
	fmadds  f18,f18,f1,f7
	fmr     f19,f22
	fmr     f2,f25
	fmr     f12,f28
	fmr     f16,f31
	fmr     f20,f23
	fmr     f6,f26
	fmr     f14,f29
	fmr     f17,f0

	li      r0,4
	mtctr   r0
	lxa     r8,screenedge
	subi    r8,r8,4
	lxa     r9,view_clipplanes
	subi    r9,r9,4
.loop:
	lfsu    f0,4(r8)
	lfsu    f7,4(r8)
	lfsu    f9,4(r8)
	fmuls   f3,f16,f7
	fmuls   f10,f17,f7
	fmuls   f11,f18,f7
	fmadds  f3,f2,f9,f3
	fmadds  f10,f6,f9,f10
	fmadds  f11,f8,f9,f11
	fnmsubs f3,f12,f0,f3
	stfsu   f3,4(r9)
	fmuls   f3,f3,f19
	fnmsubs f10,f14,f0,f10
	stfsu   f10,4(r9)
	fmadds  f3,f10,f20,f3
	fnmsubs f11,f15,f0,f11
	stfsu   f11,4(r9)
	fmadds  f3,f11,f21,f3
	la      r8,MPLANE_SIZEOF-12(r8)
	stfsu   f3,4(r9)
	la      r9,CLIP_SIZEOF-16(r9)
	bdnz    .loop

	stfs    f19,0(r7)
	stfs    f20,4(r7)
	stfs    f21,8(r7)
	stfs    f2,0(r4)
	stfs    f6,4(r4)
	stfs    f8,8(r4)
	stfs    f12,0(r5)
	stfs    f14,4(r5)
	stfs    f15,8(r5)
	stfs    f16,0(r6)
	stfs    f17,4(r6)
	stfs    f18,8(r6)

	lfd	f14,fb+0*8(r1)
	lfd	f15,fb+1*8(r1)
	lfd	f16,fb+2*8(r1)
	lfd	f17,fb+3*8(r1)
	lfd	f18,fb+4*8(r1)
	lfd	f19,fb+5*8(r1)
	lfd	f20,fb+6*8(r1)
	lfd	f21,fb+7*8(r1)
	lfd	f22,fb+8*8(r1)
	lfd	f23,fb+9*8(r1)
	lfd	f24,fb+10*8(r1)
	lfd	f25,fb+11*8(r1)
	lfd	f26,fb+12*8(r1)
	lfd	f27,fb+13*8(r1)
	lfd	f28,fb+14*8(r1)
	lfd	f29,fb+15*8(r1)
	lfd	f30,fb+16*8(r1)
	lfd	f31,fb+17*8(r1)
	lwz	r31,gb(r1)
	exit

	funcend	R_RotateBmodel




.ifdef	WOS
	.tocd
.else
	.data
.endif

lab cPI
	.float	0.017453293		# => M_PI*2/360
