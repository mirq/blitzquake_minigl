##
## Quake for AMIGA
##
## d_edgePPC.s
##
## Define WOS for PowerOpen ABI, otherwise SVR4-ABI is used.
##

.include        "macrosPPC.i"
.include	"quakedefPPC.i"

#
# external references
#

	xrefa	vright
	xrefa	vup
	xrefa	vpn
	xrefv	xscaleinv
	xrefv	yscaleinv
	xrefv	xcenter
	xrefv	ycenter
	xrefv	d_zistepu
	xrefv	d_zistepv
	xrefv	d_ziorigin
	xrefv	d_sdivzstepu
	xrefv	d_tdivzstepu
	xrefv	d_sdivzstepv
	xrefv	d_tdivzstepv
	xrefv	d_sdivzorigin
	xrefv	d_tdivzorigin
	xrefv	sadjust
	xrefv	tadjust
	xrefv	bbextents
	xrefv	bbextentt
	xrefa	transformed_modelorg
	xrefv	miplevel
	xrefa	cl_entities
	xrefv	currententity
	xrefa	r_drawflat
	xrefv	surfaces
	xrefv	surface_p
	xrefv	r_drawnpolycount
	xrefv	r_skymade
	xrefa	r_clearcolor
	xrefv	cacheblock
	xrefv	cachewidth
	xrefa	r_origin
	xrefa	base_vpn
	xrefa	base_vup
	xrefa	base_vright
	xrefa	base_modelorg
	xrefv	d_drawspans
	xrefv	scale_for_mip
	xrefa	d_scalemip
	xrefv	d_minmip
	xrefa	modelorg
	xrefa	screenedge
	xrefa	view_clipplanes

	xrefv	D_DrawSolidSurface
	xrefv	D_DrawZSpans
	xrefv	R_MakeSky
	xrefv	D_DrawSkyScans8
	xrefv	D_DrawZSpans
	xrefv	Turbulent8
	xrefv	R_RotateBmodel
	xrefv	D_CacheSurface

	xrefv	c0
	xrefv	c0_5
	xrefv	c65536


#
# defines
#

.set	SURF_DRAWSKY        ,4
.set	SURF_DRAWTURB       ,0x10
.set	SURF_DRAWBACKGROUND ,0x40




###########################################################################
#
#       void _D_CalcGradients (msurface_t *pface)
#
###########################################################################

	funcdef	D_CalcGradients

	init	0,24,0,9
	stfd	f14,fb+0*8(r1)
	stfd	f15,fb+1*8(r1)
	stfd	f16,fb+2*8(r1)
	stfd	f17,fb+3*8(r1)
	stfd	f18,fb+4*8(r1)
	stfd	f19,fb+5*8(r1)
	stfd	f20,fb+6*8(r1)
	stfd	f22,fb+8*8(r1)

	lwz     r4,MSURFACE_TEXINFO(r3)
	la      r5,16(r4)
	lxa     r6,vright
	lxa     r7,vup
	lxa     r8,vpn
	lfs     f1,0(r6)
	lfs     f10,0(r4)
	fmuls   f0,f10,f1
	lfs     f2,4(r6)
	lfs     f11,4(r4)
	fmadds  f0,f11,f2,f0
	lfs     f3,8(r6)
	lfs     f12,8(r4)
	fmadds  f0,f12,f3,f0            #f0 = p_saxis[0]
	lfs     f4,0(r7)
	fmuls   f13,f10,f4
	lfs     f5,4(r7)
	fmadds  f13,f11,f5,f13
	lfs     f6,8(r7)
	fmadds  f13,f12,f6,f13          #f13 = p_saxis[1]
	lfs     f7,0(r8)
	fmuls   f14,f10,f7
	lfs     f8,4(r8)
	fmadds  f14,f11,f8,f14
	lfs     f9,8(r8)
	fmadds  f14,f12,f9,f14          #f14 = p_saxis[2]

	lfs     f15,0(r5)
	fmuls   f18,f15,f1
	lw      r9,miplevel
	fmuls   f19,f15,f4
	slwi    r0,r9,23
	fmuls   f20,f15,f7
	lfs     f16,4(r5)
	fmadds  f18,f16,f2,f18
	lis     r11,0x3f800000@h
	ori     r11,r11,0x3f800000@l
	fmadds  f19,f16,f5,f19
	subf    r0,r0,r11
	fmadds  f20,f16,f8,f20
	lfs     f17,8(r5)
	fmadds  f18,f17,f3,f18          #f18 = p_taxis[0]
	stw     r0,local(r1)
	fmadds  f19,f17,f6,f19          #f19 = p_taxis[1]
	lfs     f22,local(r1)           #f22 = mipscale
	fmadds  f20,f17,f9,f20          #f20 = p_taxis[2]

	ls      f1,xscaleinv
	ls      f5,yscaleinv
	fmuls   f1,f1,f22
	fmuls   f5,f5,f22
	fmuls   f2,f1,f0
	fmuls   f6,f5,f13
	ss      f2,d_sdivzstepu
	fneg    f6,f6
	ss      f6,d_sdivzstepv
	fmuls   f3,f1,f18
	fmuls   f7,f5,f19
	ss      f3,d_tdivzstepu
	fneg    f7,f7
	ss      f7,d_tdivzstepv
	ls      f4,xcenter
	ls      f8,ycenter
	fmuls   f2,f2,f4
	lxa     r11,transformed_modelorg
	fmadds  f6,f6,f8,f2
	fmuls   f3,f3,f4
	fmadds  f7,f7,f8,f3
	fmsubs  f2,f14,f22,f6
	ss      f2,d_sdivzorigin
	fmsubs  f3,f20,f22,f7
	ss      f3,d_tdivzorigin

	lfs     f1,0(r11)
	lfs     f2,4(r11)
	lfs     f3,8(r11)

	fmuls   f4,f1,f0
	fmuls   f5,f1,f18
	ls      f6,c65536
	fmadds  f4,f2,f13,f4
	fmadds  f5,f2,f19,f5
	fmadds  f4,f3,f14,f4
	fmadds  f5,f3,f20,f5
	ls      f7,c0_5
	fmuls   f4,f4,f22
	fmuls   f5,f5,f22

	fmuls   f22,f22,f6
	lwz     r8,MSURFACE_TEXTUREMINS(r3)

	fmadds  f4,f6,f4,f7
	fmadds  f5,f6,f5,f7
	fctiwz  f0,f4
	fctiwz  f2,f5
	stfd    f0,local(r1)
	stfd    f2,local+8(r1)
	lwz     r6,local+4(r1)
	lwz     r12,local+12(r1)
	lfs     f8,12(r4)
	lfs     f3,12(r5)
	fmuls   f8,f8,f22
	fmuls   f3,f3,f22
	fctiwz  f0,f8
	fctiwz  f2,f3
	stfd    f0,local(r1)
	stfd    f2,local+8(r1)
	lwz     r7,local+4(r1)
	lwz     r4,local+12(r1)
	clrrwi  r11,r8,16
	slwi    r5,r8,16
	sraw    r11,r11,r9
	sraw    r5,r5,r9
	subf    r11,r7,r11
	subf    r5,r4,r5
	subf    r6,r11,r6
	subf    r12,r5,r12
	sw      r6,sadjust
	sw      r12,tadjust

	lwz     r8,MSURFACE_EXTENTS(r3)
	clrrwi  r6,r8,16
	slwi    r5,r8,16
	sraw    r6,r6,r9
	sraw    r5,r5,r9
	subi    r6,r6,1
	subi    r5,r5,1
	sw      r6,bbextents
	sw      r5,bbextentt

	lfd	f14,fb+0*8(r1)
	lfd	f15,fb+1*8(r1)
	lfd	f16,fb+2*8(r1)
	lfd	f17,fb+3*8(r1)
	lfd	f18,fb+4*8(r1)
	lfd	f19,fb+5*8(r1)
	lfd	f20,fb+6*8(r1)
	lfd	f22,fb+8*8(r1)
	exit

	funcend	D_CalcGradients




###########################################################################
#
#       void _D_DrawSurfaces (void)
#
###########################################################################

	funcdef	D_DrawSurfaces

.ifdef	WOS
	init	0,8,18,9
	stmw	r14,gb(r1)
.else
	init	0,8,17,9
	stmw	r15,gb(r1)
.endif
	stfd	f14,fb+0*8(r1)
	stfd	f15,fb+1*8(r1)
	stfd	f16,fb+2*8(r1)
	stfd	f17,fb+3*8(r1)
	stfd	f18,fb+4*8(r1)
	stfd	f19,fb+5*8(r1)
	stfd	f20,fb+6*8(r1)
	stfd	f21,fb+7*8(r1)
	stfd	f22,fb+8*8(r1)

#        currententity = &cl_entities[0];
	lxa     r12,cl_entities
	ls      f17,c0
	ls      f18,cM0_9
	sw      r12,currententity

	lxa     r31,modelorg
	lxa     r30,transformed_modelorg
	lxa     r29,vright
	lxa     r28,vup
	lxa     r27,vpn
	lxa     r24,r_origin
	lxa     r23,base_vright
	lxa     r22,base_vup
	lxa     r21,base_vpn
	lxa     r19,screenedge
	lxa     r18,d_scalemip
	lw      r17,d_minmip
	lw      r15,d_drawspans
	subi    r19,r19,4
	lxa     r20,view_clipplanes
	subi    r20,r20,4

#        TransformVector (modelorg, transformed_modelorg);
#        VectorCopy (transformed_modelorg, world_transformed_modelorg);

######  TransformVector (inlined)

	lfs     f1,0(r31)
	lfs     f4,0(r29)
	fmuls   f0,f1,f4
	lfs     f2,4(r31)
	lfs     f5,4(r29)
	fmadds  f0,f2,f5,f0
	lfs     f3,8(r31)
	lfs     f6,8(r29)
	fmadds  f14,f3,f6,f0
	stfs    f14,0(r30)              #f14 = world_transformed[0]
	lfs     f4,0(r28)
	fmuls   f7,f1,f4
	lfs     f5,4(r28)
	fmadds  f7,f2,f5,f7
	lfs     f6,8(r28)
	fmadds  f15,f3,f6,f7
	stfs    f15,4(r30)              #f15 = world_transformed[1]
	lfs     f4,0(r27)
	fmuls   f8,f1,f4
	lfs     f5,4(r27)
	fmadds  f8,f2,f5,f8
	lfs     f6,8(r27)
	fmadds  f16,f3,f6,f8
	stfs    f16,8(r30)              #f16 = world_transformed[2]

######  end of TransformVector

#        if (r_drawflat.value)
#        {
#                for (s = &surfaces[1] ; s<surface_p ; s++)

	lxa     r3,r_drawflat
	lfs     f0,CVAR_VALUE(r3)
	fcmpo   cr0,f0,f17
	beq     .dsnotflat
	lw      r26,surfaces
	la      r26,SURF_SIZEOF(r26)
	lw      r25,surface_p
.dsloop:
	cmpw    r26,r25
	bge     .dsend

#     if (!s->spans)
#             continue;

	lwz     r0,SURF_SPANS(r26)
	mr.     r0,r0
	beq     .dsnext

#       d_zistepu = s->d_zistepu;
#       d_zistepv = s->d_zistepv;
#       d_ziorigin = s->d_ziorigin;
#
#       D_DrawSolidSurface (s, (long)s->data & 0xFF);
#       D_DrawZSpans (s->spans);

	lwz     r3,SURF_D_ZISTEPU(r26)
	sw      r3,d_zistepu
	lwz     r3,SURF_D_ZISTEPV(r26)
	sw      r3,d_zistepv
	lwz     r3,SURF_D_ZIORIGIN(r26)
	sw      r3,d_ziorigin
	lwz     r4,SURF_DATA(r26)
	andi.   r4,r4,0xff
	mr      r3,r26
	call	D_DrawSolidSurface
	lwz     r3,SURF_SPANS(r26)
	call	D_DrawZSpans
.dsnext:
	la      r26,SURF_SIZEOF(r26)
	b       .dsloop

.dsnotflat:
	lw      r26,surfaces
	la      r26,SURF_SIZEOF(r26)
	lw      r25,surface_p

#         for (s = &surfaces[1] ; s<surface_p ; s++)
#         {
#                 if (!s->spans)
#                         continue;

.dsloop2:
	cmpw    r26,r25
	bge     .dsend
	lwz     r0,SURF_SPANS(r26)
	mr.     r0,r0
	beq     .dsnext2

#     r_drawnpolycount++;
#
#     d_zistepu = s->d_zistepu;
#     d_zistepv = s->d_zistepv;
#     d_ziorigin = s->d_ziorigin;

	lw      r3,r_drawnpolycount
	addi    r3,r3,1
	sw      r3,r_drawnpolycount
	lwz     r3,SURF_D_ZISTEPU(r26)
	sw      r3,d_zistepu
	lwz     r3,SURF_D_ZISTEPV(r26)
	sw      r3,d_zistepv
	lwz     r3,SURF_D_ZIORIGIN(r26)
	sw      r3,d_ziorigin

#     if (s->flags & SURF_DRAWSKY)
#     {
#             if (!r_skymade)
#             {
#                     R_MakeSky ();
#             }
#
#             D_DrawSkyScans8 (s->spans);
#             D_DrawZSpans (s->spans);
#     }

	lwz     r5,SURF_FLAGS(r26)
	andi.   r0,r5,SURF_DRAWSKY
	beq     .dsnosky
	lw      r3,r_skymade
	mr.     r3,r3
	bne     .dsskymade
	call	R_MakeSky
.dsskymade:
	lwz     r3,SURF_SPANS(r26)
	call	D_DrawSkyScans8
	lwz     r3,SURF_SPANS(r26)
	call	D_DrawZSpans
	b       .dsnext2

#   else if (s->flags & SURF_DRAWBACKGROUND)
#   {
#   // set up a gradient for the background surface that places it
#   // effectively at infinity distance from the viewpoint
#           d_zistepu = 0;
#           d_zistepv = 0;
#           d_ziorigin = -0.9;
#
#           D_DrawSolidSurface (s, (int)r_clearcolor.value & 0xFF);
#           D_DrawZSpans (s->spans);
#   }

.dsnosky:
	andi.   r0,r5,SURF_DRAWBACKGROUND
	beq     .dsnobackground
	ss      f17,d_zistepu
	ss      f17,d_zistepv
	ss      f18,d_ziorigin
	lxa     r3,r_clearcolor
	lfs     f0,CVAR_VALUE(r3)
	fctiwz  f0,f0
	stfd    f0,local(r1)
	lwz     r4,local+4(r1)
	andi.   r4,r4,0xff
	mr      r3,r26
	call	D_DrawSolidSurface
	lwz     r3,SURF_SPANS(r26)
	call	D_DrawZSpans
	b       .dsnext2

#   else if (s->flags & SURF_DRAWTURB)
#   {
#           pface = s->data;
#           miplevel = 0;
#           cacheblock = (pixel_t *)
#                           ((byte *)pface->texinfo->texture +
#                           pface->texinfo->texture->offsets[0]);
#           cachewidth = 64;

.dsnobackground:
	andi.   r0,r5,SURF_DRAWTURB
	beq     .dsnoturb
.ifdef	WOS
	lwz     r14,SURF_DATA(r26)
	li	r0,0
	sw	r0,miplevel
	lwz     r7,MSURFACE_TEXINFO(r14)
.else
	lwz     r7,SURF_DATA(r26)
	li	r0,0
	sw	r0,miplevel
	lwz     r7,MSURFACE_TEXINFO(r7)
.endif
	lwz     r7,MTEXINFO_TEXTURE(r7)
	lwz     r0,TEXTURE_OFFSETS(r7)
	add     r7,r7,r0
	sw      r7,cacheblock
	li	r0,64
	sw	r0,cachewidth

#      if (s->insubmodel)
#      {
#      // FIXME: we don't want to do all this for every polygon!
#      // TODO: store once at start of frame
#              currententity = s->entity;      //FIXME: make this passed in to
#                                                                      // R_RotateBmodel ()
#              VectorSubtract (r_origin, currententity->origin,
#                              local_modelorg);
#              TransformVector (local_modelorg, transformed_modelorg);
#
#              R_RotateBmodel ();      // FIXME: don't mess with the frustum,
#                                                      // make entity passed in
#      }

	lwz     r0,SURF_INSUBMODEL(r26)
	mr.     r0,r0
	beq     .dsnosub
	lwz     r3,SURF_ENTITY(r26)
	sw      r3,currententity
	la      r3,ENTITY_ORIGIN(r3)

######  VectorSubtract (inlined)

	lfs     f1,0(r24)
	lfs     f5,0(r3)
	fsubs   f1,f1,f5
	lfs     f2,4(r24)
	lfs     f6,4(r3)
	fsubs   f2,f2,f6
	lfs     f3,8(r24)
	lfs     f7,8(r3)
	fsubs   f3,f3,f7

######  end of VectorSubtract (inlined)

######  TransformVector (inlined)

	lfs     f4,0(r29)
	fmuls   f7,f4,f1
	lfs     f5,4(r29)
	fmadds  f7,f5,f2,f7
	lfs     f6,8(r29)
	fmadds  f7,f6,f3,f7
	stfs    f7,0(r30)
	lfs     f4,0(r28)
	fmuls   f7,f4,f1
	lfs     f5,4(r28)
	fmadds  f7,f5,f2,f7
	lfs     f6,8(r28)
	fmadds  f7,f6,f3,f7
	stfs    f7,4(r30)
	lfs     f4,0(r27)
	fmuls   f7,f4,f1
	lfs     f5,4(r27)
	fmadds  f7,f5,f2,f7
	lfs     f6,8(r27)
	fmadds  f7,f6,f3,f7
	stfs    f7,8(r30)

###### end of TransformVector

	call	R_RotateBmodel

#              D_CalcGradients (pface);
#              Turbulent8 (s->spans);
#              D_DrawZSpans (s->spans);

.dsnosub:
.ifdef	WOS
	mr      r3,r14
.else
	lwz     r3,SURF_DATA(r26)
.endif
	call	D_CalcGradients
	lwz     r3,SURF_SPANS(r26)
	call	Turbulent8
	lwz     r3,SURF_SPANS(r26)
	call	D_DrawZSpans

#          if (s->insubmodel)
#          {
#          //
#          // restore the old drawing state
#          // FIXME: we don't want to do this every time!
#          // TODO: speed up
#          //
#                  currententity = &cl_entities[0];
#                  VectorCopy (world_transformed_modelorg,
#                                          transformed_modelorg);
#                  VectorCopy (base_vpn, vpn);
#                  VectorCopy (base_vup, vup);
#                  VectorCopy (base_vright, vright);
#                  VectorCopy (base_modelorg, modelorg);
#                  R_TransformFrustum ();
#          }

	lwz     r0,SURF_INSUBMODEL(r26)
	mr.     r0,r0
	beq     .dsnext2
	lxa     r12,cl_entities
	sw      r12,currententity

	stfs    f14,0(r30)
	stfs    f15,4(r30)
	stfs    f16,8(r30)
	lxa	r11,base_modelorg
	lfs     f0,0(r23)
	stfs    f0,0(r29)               #f0 = vright[0]
	lfs     f1,0(r22)
	stfs    f1,0(r28)               #f1 = vup[0]
	lfs     f2,0(r21)
	stfs    f2,0(r27)               #f2 = vpn[0]
	lfs	f9,0(r11)
	stfs	f9,0(r31)
	lfs     f3,4(r23)
	stfs    f3,4(r29)               #f3 = vright[1]
	lfs     f4,4(r22)
	stfs    f4,4(r28)               #f4 = vup[1]
	lfs     f5,4(r21)
	stfs    f5,4(r27)               #f5 = vpn[1]
	lfs	f9,4(r11)
	stfs	f9,4(r31)
	lfs     f6,8(r23)
	stfs    f6,8(r29)               #f6 = vright[2]
	lfs     f7,8(r22)
	stfs    f7,8(r28)               #f7 = vup[2]
	lfs     f8,8(r21)
	stfs    f8,8(r27)               #f8 = vpn[2]
	lfs	f9,8(r11)
	stfs	f9,8(r31)

###### R_TransformFrustum (inlined)

	li      r0,4
	mtctr   r0
	lfs     f20,0(r31)
	lfs     f21,4(r31)
	lfs     f22,8(r31)
.dsloop3:
	lfsu    f9,4(r19)
	lfsu    f10,4(r19)
	lfsu    f11,4(r19)
	fmuls   f12,f1,f10
	fmuls   f13,f4,f10
	fmuls   f19,f7,f10
	fmadds  f12,f2,f11,f12
	fmadds  f13,f5,f11,f13
	fmadds  f19,f8,f11,f19
	fnmsubs f12,f0,f9,f12
	stfsu   f12,4(r20)
	fmuls   f12,f12,f20
	fnmsubs f13,f3,f9,f13
	stfsu   f13,4(r20)
	fmadds  f12,f13,f21,f12
	fnmsubs f19,f6,f9,f19
	stfsu   f19,4(r20)
	fmadds  f12,f19,f22,f12
	la      r19,MPLANE_SIZEOF-12(r19)
	stfsu   f12,4(r20)
	la      r20,CLIP_SIZEOF-16(r20)
	bdnz    .dsloop3
	la      r19,-4*MPLANE_SIZEOF(r19)
	la      r20,-4*CLIP_SIZEOF(r20)

######  end of R_TransformFrustum

	b       .dsnext2
.dsnoturb:

#          if (s->insubmodel)
#          {
#          // FIXME: we don't want to do all this for every polygon!
#          // TODO: store once at start of frame
#                  currententity = s->entity;      //FIXME: make this passed in to
#                                                                          // R_RotateBmodel ()
#                  VectorSubtract (r_origin, currententity->origin, local_modelorg);
#                  TransformVector (local_modelorg, transformed_modelorg);
#
#                  R_RotateBmodel ();      // FIXME: don't mess with the frustum,
#                                                          // make entity passed in
#          }

	lwz     r0,SURF_INSUBMODEL(r26)
	mr.     r0,r0
	beq     .dsnosub2
	lwz     r3,SURF_ENTITY(r26)
	sw      r3,currententity
	la      r3,ENTITY_ORIGIN(r3)

###### VectorSubtract (inlined)

	lfs     f1,0(r24)
	lfs     f5,0(r3)
	fsubs   f1,f1,f5
	lfs     f2,4(r24)
	lfs     f6,4(r3)
	fsubs   f2,f2,f6
	lfs     f3,8(r24)
	lfs     f7,8(r3)
	fsubs   f3,f3,f7

###### end of VectorSubtract

###### TransformVector (inlined)

	lfs     f4,0(r29)
	fmuls   f7,f4,f1
	lfs     f5,4(r29)
	fmadds  f7,f5,f2,f7
	lfs     f6,8(r29)
	fmadds  f7,f6,f3,f7
	stfs    f7,0(r30)
	lfs     f4,0(r28)
	fmuls   f7,f4,f1
	lfs     f5,4(r28)
	fmadds  f7,f5,f2,f7
	lfs     f6,8(r28)
	fmadds  f7,f6,f3,f7
	stfs    f7,4(r30)
	lfs     f4,0(r27)
	fmuls   f7,f4,f1
	lfs     f5,4(r27)
	fmadds  f7,f5,f2,f7
	lfs     f6,8(r27)
	fmadds  f7,f6,f3,f7
	stfs    f7,8(r30)

######  end of TransformVector

	call	R_RotateBmodel
.dsnosub2:

#                                pface = s->data;
#                                miplevel = D_MipLevelForScale (s->nearzi * scale_for_mip
#                                # pface->texinfo->mipadjust);
#
#                        // FIXME: make this passed in to D_CacheSurface
#                                pcurrentcache = D_CacheSurface (pface, miplevel);
#
#                                cacheblock = (pixel_t *)pcurrentcache->data;
#                                cachewidth = pcurrentcache->width;

	lwz     r16,SURF_DATA(r26)
	lfs     f0,SURF_NEARZI(r26)
	ls      f1,scale_for_mip
	fmuls   f0,f0,f1
	lwz     r3,MSURFACE_TEXINFO(r16)
	lfs     f1,MTEXINFO_MIPADJUST(r3)
	fmuls   f0,f0,f1

######  D_MipLevelForScale (inlined)
	li      r4,0
	lfs     f1,0(r18)
	fcmpo   cr0,f0,f1
	bge     .dsfound
	li      r4,1
	lfs     f1,4(r18)
	fcmpo   cr0,f0,f1
	bge     .dsfound
	li      r4,2
	lfs     f1,8(r18)
	fcmpo   cr0,f0,f1
	bge     .dsfound
	li      r4,3
.dsfound:
	cmpw    r4,r17
	bge     .dsmiplevok
	mr      r4,r17
.dsmiplevok:
######  end of D_MipLevelForScale

	sw      r4,miplevel
	mr      r3,r16
	call	D_CacheSurface
	la      r4,SURFCACHE_DATA(r3)
	sw      r4,cacheblock
	lwz     r3,SURFCACHE_WIDTH(r3)
	sw      r3,cachewidth

#                                D_CalcGradients (pface);
#
#                                (*d_drawspans) (s->spans);
#
#                                D_DrawZSpans (s->spans);

	mr      r3,r16
	call	D_CalcGradients
	lwz     r3,SURF_SPANS(r26)
	mtlr    r15
	blrl
	lwz     r3,SURF_SPANS(r26)
	call	D_DrawZSpans

#          if (s->insubmodel)
#          {
#          //
#          // restore the old drawing state
#          // FIXME: we don't want to do this every time!
#          // TODO: speed up
#          //
#                  currententity = &cl_entities[0];
#                  VectorCopy (world_transformed_modelorg,
#                                          transformed_modelorg);
#                  VectorCopy (base_vpn, vpn);
#                  VectorCopy (base_vup, vup);
#                  VectorCopy (base_vright, vright);
#                  VectorCopy (base_modelorg, modelorg);
#                  R_TransformFrustum ();
#          }

	lwz     r0,SURF_INSUBMODEL(r16)
	mr.     r0,r0
	beq     .dsnext2

	lxa     r12,cl_entities
	sw      r12,currententity
	stfs    f14,0(r30)
	stfs    f15,4(r30)
	stfs    f16,8(r30)
	lxa	r11,base_modelorg
	lfs     f0,0(r23)
	stfs    f0,0(r29)               #f0 = vright[0]
	lfs     f1,0(r22)
	stfs    f1,0(r28)               #f1 = vup[0]
	lfs     f2,0(r21)
	stfs    f2,0(r27)               #f2 = vpn[0]
	lfs	f9,0(r11)
	stfs	f9,0(r31)
	lfs     f3,4(r23)
	stfs    f3,4(r29)               #f3 = vright[1]
	lfs     f4,4(r22)
	stfs    f4,4(r28)               #f4 = vup[1]
	lfs     f5,4(r21)
	stfs    f5,4(r27)               #f5 = vpn[1]
	lfs	f9,4(r11)
	stfs	f9,4(r31)
	lfs     f6,8(r23)
	stfs    f6,8(r29)               #f6 = vright[2]
	lfs     f7,8(r22)
	stfs    f7,8(r28)               #f7 = vup[2]
	lfs     f8,8(r21)
	stfs    f8,8(r27)               #f8 = vpn[2]
	lfs	f9,8(r11)
	stfs	f9,8(r31)

###### R_TransformFrustum (inlined)

	li      r0,4
	mtctr   r0
	lfs     f20,0(r31)
	lfs     f21,4(r31)
	lfs     f22,8(r31)
.dsloop4:
	lfsu    f9,4(r19)
	lfsu    f10,4(r19)
	lfsu    f11,4(r19)
	fmuls   f12,f1,f10
	fmuls   f13,f4,f10
	fmuls   f19,f7,f10
	fmadds  f12,f2,f11,f12
	fmadds  f13,f5,f11,f13
	fmadds  f19,f8,f11,f19
	fnmsubs f12,f0,f9,f12
	stfsu   f12,4(r20)
	fmuls   f12,f12,f20
	fnmsubs f13,f3,f9,f13
	stfsu   f13,4(r20)
	fmadds  f12,f13,f21,f12
	fnmsubs f19,f6,f9,f19
	stfsu   f19,4(r20)
	fmadds  f12,f19,f22,f12
	la      r19,MPLANE_SIZEOF-12(r19)
	stfsu   f12,4(r20)
	la      r20,CLIP_SIZEOF-16(r20)
	bdnz    .dsloop4
	la      r19,-4*MPLANE_SIZEOF(r19)
	la      r20,-4*CLIP_SIZEOF(r20)

######  end of R_TransformFrustum

.dsnext2:
	la      r26,SURF_SIZEOF(r26)
	b       .dsloop2

.dsend:
	lfd	f14,fb+0*8(r1)
	lfd	f15,fb+1*8(r1)
	lfd	f16,fb+2*8(r1)
	lfd	f17,fb+3*8(r1)
	lfd	f18,fb+4*8(r1)
	lfd	f19,fb+5*8(r1)
	lfd	f20,fb+6*8(r1)
	lfd	f21,fb+7*8(r1)
	lfd	f22,fb+8*8(r1)
.ifdef	WOS
	lmw	r14,gb(r1)
.else
	lmw	r15,gb(r1)
.endif
	exit

	funcend	D_DrawSurfaces




.ifdef	WOS
	.tocd
.else
	.data
.endif

lab cM0_9
	.float	-0.9
