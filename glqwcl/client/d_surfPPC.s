##
## Quake for AMIGA
##
## d_surfPPC.s
##
## Define WOS for PowerOpen ABI, otherwise SVR4-ABI is used.
##

.include        "macrosPPC.i"
.include	"quakedefPPC.i"

#
# external references
#

	xrefa	r_drawsurf
	xrefa	d_lightstylevalue
	xrefv	r_framecount
	xrefv	surfscale
	xrefv	c_surf

	xrefv	R_TextureAnimation
	xrefv	R_DrawSurface
	xrefv	D_SCAlloc




###########################################################################
#
#       surfcache_t *D_CacheSurface (msurface_t *surface, int miplevel)
#
###########################################################################

	funcdef	D_CacheSurface

	init	0,4,4,1
	stmw	r28,gb(r1)
	stfd	f14,fb(r1)

	mr      r31,r3
	mr      r30,r4
	lwz     r3,MSURFACE_TEXINFO(r31)
	lwz     r3,MTEXINFO_TEXTURE(r3)
	call	R_TextureAnimation
	lxa     r29,r_drawsurf
	stw     r3,DRAWSURF_TEXTURE(r29)
	mr      r9,r3
	lxa     r3,d_lightstylevalue
	lbz     r4,MSURFACE_STYLES(r31)
	slwi    r4,r4,2
	lwzx    r5,r3,r4
	stw     r5,DRAWSURF_LIGHTADJ(r29)
	lbz     r4,MSURFACE_STYLES+1(r31)
	slwi    r4,r4,2
	lwzx    r6,r3,r4
	stw     r6,DRAWSURF_LIGHTADJ+1*4(r29)
	lbz     r4,MSURFACE_STYLES+2(r31)
	slwi    r4,r4,2
	lwzx    r7,r3,r4
	stw     r7,DRAWSURF_LIGHTADJ+2*4(r29)
	lbz     r4,MSURFACE_STYLES+3(r31)
	slwi    r4,r4,2
	lwzx    r8,r3,r4
	stw     r8,DRAWSURF_LIGHTADJ+3*4(r29)
	slwi    r0,r30,2
	la      r4,MSURFACE_CACHESPOTS(r31)
	lwzx    r3,r4,r0                #r3 = cache
	mr.     r3,r3
	lw      r28,r_framecount
	beq     .cont2
	lwz     r10,SURFCACHE_DLIGHT(r3)
	cmpwi   cr1,r10,0
	lwz     r12,MSURFACE_DLIGHTFRAME(r31)
	cmpw    cr2,r28,r12
	lwz     r4,SURFCACHE_TEXTURE(r3)
	crandc  eq,4*cr1+eq,4*cr2+eq
	cmpw    cr3,r4,r9
	lwz     r11,SURFCACHE_LIGHTADJ(r3)
	crand   eq,eq,4*cr3+eq
	cmpw    cr4,r11,r5
	lwz     r4,SURFCACHE_LIGHTADJ+1*4(r3)
	crand   eq,eq,4*cr4+eq
	cmpw    cr5,r4,r6
	lwz     r11,SURFCACHE_LIGHTADJ+2*4(r3)
	crand   eq,eq,4*cr5+eq
	cmpw    cr6,r11,r7
	lwz     r4,SURFCACHE_LIGHTADJ+3*4(r3)
	crand   eq,eq,4*cr6+eq
	cmpw    cr7,r4,r8
	crand   eq,eq,4*cr7+eq
	beq     .exit
.cont2:
	mr.     r3,r3
	slwi    r0,r30,23
	lis     r11,0x3f800000@h
	ori     r11,r11,0x3f800000@l
	subf    r0,r0,r11
	stw     r0,local(r1)
	lfs     f14,local(r1)		#f14 = surfscale
	stw     r30,DRAWSURF_SURFMIP(r29)
	lha     r4,MSURFACE_EXTENTS(r31)
	sraw    r4,r4,r30
	stw     r4,DRAWSURF_SURFWIDTH(r29)
	stw     r4,DRAWSURF_ROWBYTES(r29)
	lha     r11,MSURFACE_EXTENTS+1*2(r31)
	sraw    r11,r11,r30
	stw     r11,DRAWSURF_SURFHEIGHT(r29)
	bne     .nocache
	mr      r3,r4
	mullw   r4,r4,r11
	call	D_SCAlloc
	slwi    r4,r30,2
	addi    r0,r4,MSURFACE_CACHESPOTS
	add     r4,r31,r0
	stw     r3,0(r4)
	stw     r4,SURFCACHE_OWNER(r3)
	stfs    f14,SURFCACHE_MIPSCALE(r3)
.nocache:
	lwz     r4,MSURFACE_DLIGHTFRAME(r31)
	cmpw    r4,r28
	bne     .else
	li	r0,1
	stw	r0,SURFCACHE_DLIGHT(r3)
	b       .cont
.else:
	li	r0,0
	stw	r0,SURFCACHE_DLIGHT(r3)
.cont:
	la      r4,SURFCACHE_DATA(r3)
	stw     r4,DRAWSURF_SURFDAT(r29)
	stw     r9,SURFCACHE_TEXTURE(r3)
	stw     r5,SURFCACHE_LIGHTADJ(r3)
	stw     r6,SURFCACHE_LIGHTADJ+1*4(r3)
	stw     r7,SURFCACHE_LIGHTADJ+2*4(r3)
	stw     r8,SURFCACHE_LIGHTADJ+3*4(r3)
	stw     r31,DRAWSURF_SURF(r29)
	lw      r5,c_surf
	mr      r31,r3
	addi    r5,r5,1
	sw      r5,c_surf
	call	R_DrawSurface
	mr      r3,r31

.exit:
	lfd	f14,fb(r1)
	lmw	r28,gb(r1)
	exit

	funcend	D_CacheSurface
