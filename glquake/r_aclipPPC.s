##
## Quake for AMIGA
##
## r_aclipPPC.s
##
## Define WOS for PowerOpen ABI, otherwise SVR4-ABI is used.
##

.include	"macrosPPC.i"
.include	"quakedefPPC.i"

#
# external references
#

	xrefa	r_refdef
	xrefv	INT2DBL_0
	xrefv	c0_5


#
# defines
#

.set	ALIAS_LEFT_CLIP        ,1
.set	ALIAS_TOP_CLIP         ,2
.set	ALIAS_RIGHT_CLIP       ,4
.set	ALIAS_BOTTOM_CLIP      ,8




###########################################################################
#
#       void _R_Alias_clip_left (finalvert_t *pfv0, finalvert_t *pfv1,
#                                finalvert_t *out)
#
###########################################################################

	funcdef	R_Alias_clip_left

	init	0,16,0,0

	lf      f1,INT2DBL_0		#for int2dbl setup, tmp=r6
	stfd	f1,local(r1)
	lxa     r7,r_refdef
	lwz     r7,REFDEF_ALIASVRECT+VRECT_X(r7)
	ls      f2,c0_5
	subi    r3,r3,4
	subi    r4,r4,4
	subi    r5,r5,4
	lwzu    r8,4(r3)
	lwzu    r9,4(r3)
	lwzu    r10,4(r4)
	lwzu    r11,4(r4)
	cmpw    r9,r11
	blt     .lcont
	subf    r12,r8,r7
	int2dbl f3,r12,r6,local,f1
	fadd    f4,f2,f3
	subf    r12,r8,r10
	int2dbl f5,r12,r6,local,f1
	fdivs   f6,f3,f5
	fctiwz  f0,f4
	stfd    f0,local+8(r1)
	lwz     r12,local+12(r1)
	add     r12,r12,r8
	stwu    r12,4(r5)
	li      r0,5
	mtctr   r0
	b       .lentry
.lloop:
	lwzu    r9,4(r3)
	lwzu    r11,4(r4)
.lentry:
	subf    r12,r9,r11
	int2dbl f3,r12,r6,local,f1
	fmadds  f3,f3,f6,f2
	fctiwz  f0,f3
	stfd    f0,local+8(r1)
	lwz     r12,local+12(r1)
	add     r12,r12,r9
	stwu    r12,4(r5)
	bdnz    .lloop
	b       .lexit
.lcont:
	subf    r12,r10,r7
	int2dbl f3,r12,r6,local,f1
	fadd    f4,f2,f3
	subf    r12,r10,r8
	int2dbl f5,r12,r6,local,f1
	fdivs   f6,f3,f5
	fctiwz  f0,f4
	stfd    f0,local+8(r1)
	lwz     r12,local+12(r1)
	add     r12,r12,r10
	stwu    r12,4(r5)
	li      r0,5
	mtctr   r0
	b       .lentry2
.lloop2:
	lwzu    r9,4(r3)
	lwzu    r11,4(r4)
.lentry2:
	subf    r12,r11,r9
	int2dbl f3,r12,r6,local,f1
	fmadds  f3,f3,f6,f2
	fctiwz  f0,f3
	stfd    f0,local+8(r1)
	lwz     r12,local+12(r1)
	add     r12,r12,r11
	stwu    r12,4(r5)
	bdnz    .lloop2
.lexit:
	exit

	funcend	R_Alias_clip_left




###########################################################################
#
#       void R_Alias_clip_right (finalvert_t *pfv0, finalvert_t *pfv1,
#                                finalvert_t *out)
#
###########################################################################

	funcdef	R_Alias_clip_right

	init	0,16,0,0

	lf      f1,INT2DBL_0		#for int2dbl_setup, tmp=r6
	stfd	f1,local(r1)
	lxa     r7,r_refdef
	lwz     r7,REFDEF_ALIASVRECTRIGHT(r7)
	ls      f2,c0_5
	subi    r3,r3,4
	subi    r4,r4,4
	subi    r5,r5,4
	lwzu    r8,4(r3)
	lwzu    r9,4(r3)
	lwzu    r10,4(r4)
	lwzu    r11,4(r4)
	cmpw    r9,r11
	blt     .rcont
	subf    r12,r8,r7
	int2dbl f3,r12,r6,local,f1
	fadd    f4,f2,f3
	subf    r12,r8,r10
	int2dbl f5,r12,r6,local,f1
	fdivs   f6,f3,f5
	fctiwz  f0,f4
	stfd    f0,local+8(r1)
	lwz     r12,local+12(r1)
	add     r12,r12,r8
	stwu    r12,4(r5)
	li      r0,5
	mtctr   r0
	b       .rentry
.rloop:
	lwzu    r9,4(r3)
	lwzu    r11,4(r4)
.rentry:
	subf    r12,r9,r11
	int2dbl f3,r12,r6,local,f1
	fmadds  f3,f3,f6,f2
	fctiwz  f0,f3
	stfd    f0,local+8(r1)
	lwz     r12,local+12(r1)
	add     r12,r12,r9
	stwu    r12,4(r5)
	bdnz    .rloop
	b       .rexit
.rcont:
	subf    r12,r10,r7
	int2dbl f3,r12,r6,local,f1
	fadd    f4,f2,f3
	subf    r12,r10,r8
	int2dbl f5,r12,r6,local,f1
	fdivs   f6,f3,f5
	fctiwz  f0,f4
	stfd    f0,local+8(r1)
	lwz     r12,local+12(r1)
	add     r12,r12,r10
	stwu    r12,4(r5)
	li      r0,5
	mtctr   r0
	b       .rentry2
.rloop2:
	lwzu    r9,4(r3)
	lwzu    r11,4(r4)
.rentry2:
	subf    r12,r11,r9
	int2dbl f3,r12,r6,local,f1
	fmadds  f3,f3,f6,f2
	fctiwz  f0,f3
	stfd    f0,local+8(r1)
	lwz     r12,local+12(r1)
	add     r12,r12,r11
	stwu    r12,4(r5)
	bdnz    .rloop2
.rexit:
	exit

	funcend	R_Alias_clip_right




###########################################################################
#
#       void R_Alias_clip_top (finalvert_t *pfv0, finalvert_t *pfv1,
#                                finalvert_t *out)
#
###########################################################################

	funcdef	R_Alias_clip_top

	init	0,16,0,0

	lf      f1,INT2DBL_0		#for int2dbl_setup, tmp=r6
	stfd	f1,local(r1)
	lxa     r7,r_refdef
	lwz     r7,REFDEF_ALIASVRECT+VRECT_Y(r7)
	ls      f2,c0_5
	subi    r3,r3,4
	subi    r4,r4,4
	subi    r5,r5,4
	lwzu    r8,4(r3)
	lwzu    r9,4(r3)
	lwzu    r10,4(r4)
	lwzu    r11,4(r4)
	cmpw    r9,r11
	blt     .tcont
	subf    r12,r9,r7
	int2dbl f3,r12,r6,local,f1
	fadd    f4,f2,f3
	subf    r12,r9,r11
	int2dbl f5,r12,r6,local,f1
	fdivs   f6,f3,f5
	fctiwz  f0,f4
	stfd    f0,local+8(r1)
	lwz     r12,local+12(r1)
	add     r12,r12,r9
	stwu    r12,4(r5)
	li      r0,5
	mtctr   r0
	b       .tentry
.tloop:
	lwzu    r8,4(r3)
	lwzu    r10,4(r4)
.tentry:
	subf    r12,r8,r10
	int2dbl f3,r12,r6,local,f1
	fmadds  f3,f3,f6,f2
	fctiwz  f0,f3
	stfd    f0,local+8(r1)
	lwz     r12,local+12(r1)
	add     r12,r12,r8
	stwu    r12,4(r5)
	bdnz    .tloop
	b       .texit
.tcont:
	subf    r12,r11,r7
	int2dbl f3,r12,r6,local,f1
	fadd    f4,f2,f3
	subf    r12,r11,r9
	int2dbl f5,r12,r6,local,f1
	fdivs   f6,f3,f5
	fctiwz  f0,f4
	stfd    f0,local+8(r1)
	lwz     r12,local+12(r1)
	add     r12,r12,r11
	stwu    r12,4(r5)
	li      r0,5
	mtctr   r0
	b       .tentry2
.tloop2:
	lwzu    r8,4(r3)
	lwzu    r10,4(r4)
.tentry2:
	subf    r12,r10,r8
	int2dbl f3,r12,r6,local,f1
	fmadds  f3,f3,f6,f2
	fctiwz  f0,f3
	stfd    f0,local+8(r1)
	lwz     r12,local+12(r1)
	add     r12,r12,r10
	stwu    r12,4(r5)
	bdnz    .tloop2
.texit:
	lwz     r3,-20(r5)
	lwz     r4,-16(r5)
	stw     r4,-20(r5)
	stw     r3,-16(r5)
	exit

	funcend	R_Alias_clip_top




###########################################################################
#
#       void R_Alias_clip_bottom (finalvert_t *pfv0, finalvert_t *pfv1,
#                                finalvert_t *out)
#
###########################################################################

	funcdef	R_Alias_clip_bottom

	init	0,16,0,0

	lf      f1,INT2DBL_0		#for int2dbl_setup, tmp=r6
	stfd	f1,local(r1)
	lxa     r7,r_refdef
	lwz     r7,REFDEF_ALIASVRECTBOTTOM(r7)
	ls      f2,c0_5
	subi    r3,r3,4
	subi    r4,r4,4
	subi    r5,r5,4
	lwzu    r8,4(r3)
	lwzu    r9,4(r3)
	lwzu    r10,4(r4)
	lwzu    r11,4(r4)
	cmpw    r9,r11
	blt     .bcont
	subf    r12,r9,r7
	int2dbl f3,r12,r6,local,f1
	fadd    f4,f2,f3
	subf    r12,r9,r11
	int2dbl f5,r12,r6,local,f1
	fdivs   f6,f3,f5
	fctiwz  f0,f4
	stfd    f0,local+8(r1)
	lwz     r12,local+12(r1)
	add     r12,r12,r9
	stwu    r12,4(r5)
	li      r0,5
	mtctr   r0
	b       .bentry
.bloop:
	lwzu    r8,4(r3)
	lwzu    r10,4(r4)
.bentry:
	subf    r12,r8,r10
	int2dbl f3,r12,r6,local,f1
	fmadds  f3,f3,f6,f2
	fctiwz  f0,f3
	stfd    f0,local+8(r1)
	lwz     r12,local+12(r1)
	add     r12,r12,r8
	stwu    r12,4(r5)
	bdnz    .bloop
	b       .bexit
.bcont:
	subf    r12,r11,r7
	int2dbl f3,r12,r6,local,f1
	fadd    f4,f2,f3
	subf    r12,r11,r9
	int2dbl f5,r12,r6,local,f1
	fdivs   f6,f3,f5
	fctiwz  f0,f4
	stfd    f0,local+8(r1)
	lwz     r12,local+12(r1)
	add     r12,r12,r11
	stwu    r12,4(r5)
	li      r0,5
	mtctr   r0
	b       .bentry2
.bloop2:
	lwzu    r8,4(r3)
	lwzu    r10,4(r4)
.bentry2:
	subf    r12,r10,r8
	int2dbl f3,r12,r6,local,f1
	fmadds  f3,f3,f6,f2
	fctiwz  f0,f3
	stfd    f0,-8(r1)
	lwz     r12,-4(r1)
	add     r12,r12,r10
	stwu    r12,4(r5)
	bdnz    .bloop2
.bexit:
	lwz     r3,-20(r5)
	lwz     r4,-16(r5)
	stw     r4,-20(r5)
	stw     r3,-16(r5)
	exit

	funcend	R_Alias_clip_bottom




###########################################################################
#
#       int R_AliasClip  (finalvert_t *in, finalvert_t *out, int flag,
#                          int count, void(*clip)(...)
#
###########################################################################

	funcdef	R_AliasClip

	init	0,0,13,0
	stmw	r19,gb(r1)

#        j = count-1;
#        k = 0;
#        for (i=0 ; i<count ; j = i, i++)
#        {

	mr      r20,r6
	subic.  r8,r6,1
	blt     .acskip
	slwi    r8,r8,FV_SIZEOF_EXP
	add     r31,r3,r8               #r31 = in[j]
	lxa     r10,r_refdef
	lwz     r30,REFDEF_ALIASVRECT+VRECT_X(r10)
	lwz     r29,REFDEF_ALIASVRECT+VRECT_Y(r10)
	lwz     r28,REFDEF_ALIASVRECTRIGHT(r10)
	lwz     r27,REFDEF_ALIASVRECTBOTTOM(r10)
	mr      r26,r3
	mr      r25,r3
	mr      r24,r5
	mr      r22,r7
	mr      r21,r4
	mr      r19,r4

#                oldflags = in[j].flags & flag;
#                flags = in[i].flags & flag;
#                if (flags && oldflags)
#                        continue;

.acloop:
	lwz     r0,FV_FLAGS(r31)
	and     r0,r0,r24
	lwz     r23,FV_FLAGS(r25)
	and.    r23,r23,r24
	beq     .acdo
	mr.     r0,r0
	bne     .acnext
.acdo:

#                if (oldflags ^ flags)
#                {
#                        clip (&in[j], &in[i], &out[k]);
#                        out[k].flags = 0;
#                        if (out[k].v[0] < r_refdef.aliasvrect.x)
#                                out[k].flags |= ALIAS_LEFT_CLIP;
#                        if (out[k].v[1] < r_refdef.aliasvrect.y)
#                                out[k].flags |= ALIAS_TOP_CLIP;
#                        if (out[k].v[0] > r_refdef.aliasvrectright)
#                                out[k].flags |= ALIAS_RIGHT_CLIP;
#                        if (out[k].v[1] > r_refdef.aliasvrectbottom)
#                                out[k].flags |= ALIAS_BOTTOM_CLIP;
#                        k++;
#                }

	xor.    r0,r0,r23
	beq     .accont
	mtlr    r22
	mr      r3,r31
	mr      r4,r25
	mr      r5,r21
	blrl
	li      r3,0
	lwz     r0,0(r21)
	cmpw    r0,r30
	bge     .ac1
	ori     r3,r3,ALIAS_LEFT_CLIP
.ac1:
	cmpw    r0,r28
	ble     .ac2
	ori     r3,r3,ALIAS_RIGHT_CLIP
.ac2:
	lwz     r0,4(r21)
	cmpw    r0,r29
	bge     .ac3
	ori     r3,r3,ALIAS_TOP_CLIP
.ac3:
	cmpw    r0,r27
	ble     .ac4
	ori     r3,r3,ALIAS_BOTTOM_CLIP
.ac4:
	stw     r3,FV_FLAGS(r21)
	la      r21,FV_SIZEOF(r21)

#                if (!flags)
#                {
#                        out[k] = in[i];
#                        k++;
#                }

.accont:
	mr.     r23,r23
	bne     .acnext
	lwz     r0,0(r25)
	stw     r0,0(r21)
	lwz     r3,4(r25)
	stw     r3,4(r21)
	lwz     r0,8(r25)
	stw     r0,8(r21)
	lwz     r3,12(r25)
	stw     r3,12(r21)
	lwz     r0,16(r25)
	stw     r0,16(r21)
	lwz     r3,20(r25)
	stw     r3,20(r21)
	lwz     r0,24(r25)
	stw     r0,24(r21)
	la      r21,FV_SIZEOF(r21)
.acnext:
	mr      r31,r26
	la      r26,FV_SIZEOF(r26)
	la      r25,FV_SIZEOF(r25)
	subic.  r20,r20,1
	bne     .acloop

#        return k;

.acskip:
	subf    r3,r19,r21
	srawi   r3,r3,FV_SIZEOF_EXP

	lmw	r19,gb(r1)
	exit

	funcend	R_AliasClip
