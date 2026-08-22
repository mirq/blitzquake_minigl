##
## Quake for AMIGA
##
## d_polysetPPC.s
##
## Define WOS for PowerOpen ABI, otherwise SVR4-ABI is used.
##

.include        "macrosPPC.i"
.include	"quakedefPPC.i"

#
# external references
#

	xrefv	acolormap
	xrefv	d_aspancount
	xrefv	errorterm
	xrefv	erroradjustup
	xrefv	erroradjustdown
	xrefv	d_countextrastep
	xrefv	ubasestep
	xrefa	r_affinetridesc
	xrefv	a_ststepxwhole
	xrefv	a_sstepxfrac
	xrefv	a_tstepxfrac
	xrefv	r_lstepx
	xrefv	r_lstepy
	xrefv	r_sstepx
	xrefv	r_sstepy
	xrefv	r_tstepx
	xrefv	r_tstepy
	xrefv	r_zistepx
	xrefv	r_zistepy
	xrefa	zspantable
	xrefa	skintable
	xrefa	r_p0
	xrefa	r_p1
	xrefa	r_p2
	xrefv	d_xdenom
	xrefv	d_pcolormap
	xrefa	d_scantable
	xrefv	d_viewbuffer
	xrefv	d_pedgespanpackage
	xrefv	d_pdest
	xrefv	d_pz
	xrefv	d_aspancount
	xrefv	d_ptex
	xrefv	d_sfrac
	xrefv	d_tfrac
	xrefv	d_light
	xrefv	d_zi
	xrefv	d_pdestextrastep
	xrefv	d_pzextrastep
	xrefv	d_countextrastep
	xrefv	d_ptexextrastep
	xrefv	d_sfracextrastep
	xrefv	d_tfracextrastep
	xrefv	d_lightextrastep
	xrefv	d_ziextrastep
	xrefv	d_pdestbasestep
	xrefv	d_pzbasestep
	xrefv	d_ptexbasestep
	xrefv	d_sfracbasestep
	xrefv	d_tfracbasestep
	xrefv	d_lightbasestep
	xrefv	d_zibasestep
	xrefv	d_zwidth
	xrefv	d_pzbuffer
	xrefv	a_spans
	xrefv	pedgetable
	xrefa	edgetables
	xrefv	screenwidth
	xrefv	INT2DBL_0
	xrefv	c2

	xrefv	FloorDivMod
	xrefv	D_RasterizeAliasPolySmooth


#
# defines
#

.set	ALIAS_ONSEAM      ,32		#must match the def. in r_shared.h




###########################################################################
#
#       void D_PolysetDrawSpans8 (spanpackage_t *pspanpackage)
#
#       standard scan drawing function for alias models
#
###########################################################################

	funcdef	D_PolysetDrawSpans8

	init	0,0,12,0
	stmw	r20,gb(r1)

	lw      r4,d_aspancount
	lw      r5,errorterm
	lw      r6,erroradjustup
	lw      r7,erroradjustdown
	lw      r8,d_countextrastep
	lw      r9,ubasestep
	lw      r10,acolormap
	lw      r11,r_zistepx
	lw      r12,a_ststepxwhole
	lw      r31,a_sstepxfrac
	lw      r30,a_tstepxfrac
	lw      r29,r_lstepx
	lxa     r28,r_affinetridesc
	lwz     r28,R_SKINWIDTH(r28)

.ds8loop:

#                lcount = d_aspancount - pspanpackage->count;
#
#                errorterm += erroradjustup;
#                if (errorterm >= 0)
#                {
#                        d_aspancount += d_countextrastep;
#                        errorterm -= erroradjustdown;
#                }
#                else
#                {
#                        d_aspancount += ubasestep;
#                }

	lwz     r0,PSPANP_COUNT(r3)
	subf    r0,r0,r4
	add.    r5,r5,r6
	blt     .ds8else
	add     r4,r4,r8
	subf    r5,r7,r5
	b       .ds8next
.ds8else:
	add     r4,r4,r9
.ds8next:
	mr.     r0,r0
	beq     .ds8loopend
	mtctr   r0

#                        lpdest = pspanpackage->pdest;
#                        lptex = pspanpackage->ptex;
#                        lpz = pspanpackage->pz;
#                        lsfrac = pspanpackage->sfrac;
#                        ltfrac = pspanpackage->tfrac;
#                        llight = pspanpackage->light;
#                        lzi = pspanpackage->zi;

	lwz     r27,PSPANP_PDEST(r3)
	lwz     r26,PSPANP_PZ(r3)
	lwz     r25,PSPANP_PTEX(r3)
	lwz     r24,PSPANP_SFRAC(r3)
	lwz     r23,PSPANP_TFRAC(r3)
	lwz     r22,PSPANP_LIGHT(r3)
	lwz     r21,PSPANP_ZI(r3)

###### main drawing loop

###### r21 = lzi
###### r22 = llight
###### r24 = lsfrac
###### r11 = r_zistepx
###### r23 = ltfrac
###### r30 = a_tstepxfrac
###### r28 = r_affinetridesc.skinwidth
###### r27 -> lpdest
###### r26 -> lpz
###### r25 -> lptex
###### r10 -> acolormap
###### r31 = a_sstepxfrac
###### r12 = a_ststepxwhole
###### r29 = r_lstepx

#                        do
#                        {
#                                if ((lzi >> 16) >= *lpz)
#                                {
#                                        *lpdest = ((byte *)acolormap)[*lptex + (llight & 0xFF00)];
#                                        *lpz = lzi >> 16;
#                                }
#                                lpdest++;
#                                lzi += r_zistepx;
#                                lpz++;
#                                llight += r_lstepx;
#                                lptex += a_ststepxwhole;
#                                lsfrac += a_sstepxfrac;
#                                lptex += lsfrac >> 16;
#                                lsfrac &= 0xFFFF;
#                                ltfrac += a_tstepxfrac;
#                                if (ltfrac & 0x10000)
#                                {
#                                        lptex += r_affinetridesc.skinwidth;
#                                        ltfrac &= 0xFFFF;
#                                }
#                        } while (--lcount);

.ds8loop2:
	srwi    r0,r21,16
	lhz     r20,0(r26)
	cmpw    r0,r20
	blt     .ds8cont
	lbz     r20,0(r25)
	rlwimi  r20,r22,0,16,23
	lbzx    r20,r10,r20
	stb     r20,0(r27)
	sth     r0,0(r26)
.ds8cont:
	add     r24,r24,r31
	addi    r26,r26,2
	addi    r27,r27,1
	srwi    r0,r24,16
	add     r21,r21,r11
	add     r22,r22,r29
	add     r25,r25,r12
	add     r23,r23,r30
	clrlwi  r24,r24,16
	add     r25,r25,r0
	andis.  r0,r23,1
	beq     .ds8cont2
	add     r25,r25,r28
	clrlwi  r23,r23,16
.ds8cont2:
	bdnz    .ds8loop2
.ds8loopend:

#                pspanpackage++;
#        } while (pspanpackage->count != -999999);

	la      r3,PSPANP_SIZEOF(r3)
	lwz     r0,PSPANP_COUNT(r3)
	lis     r20,-999999@h
	ori     r20,r20,-999999@l
	cmpw    r20,r0
	bne     .ds8loop

	lmw	r20,gb(r1)
	exit

	funcend	D_PolysetDrawSpans8




###########################################################################
#
#       void D_PolysetRecursiveTriangle (int *lp1, int *lp2, int *lp3)
#
###########################################################################

	funcdef	D_PolysetRecursiveTriangle

	init	0,0,3,0
	stw	r24,gb+0*4(r1)
	stw	r25,gb+1*4(r1)
	stw	r26,gb+2*4(r1)
	lxa     r6,zspantable
	lw      r7,d_viewbuffer
	lxa     r26,skintable
	lw      r25,d_pcolormap
	lxa     r24,d_scantable
	bl	DoRecursion
	lwz	r24,gb+0*4(r1)
	lwz	r25,gb+1*4(r1)
	lwz	r26,gb+2*4(r1)
	exit


DoRecursion:
	init	0,24,5,0
	stmw	r27,gb(r1)

	mr      r31,r3
	mr      r30,r4
	mr      r29,r5
	la	r28,local(r1)

#        d = lp2[0] - lp1[0];
#        if (d < -1 || d > 1)
#                goto split;
#        d = lp2[1] - lp1[1];
#        if (d < -1 || d > 1)
#                goto split;
#
#        d = lp3[0] - lp2[0];
#        if (d < -1 || d > 1)
#                goto split2;
#        d = lp3[1] - lp2[1];
#        if (d < -1 || d > 1)
#                goto split2;
#
#        d = lp1[0] - lp3[0];
#        if (d < -1 || d > 1)
#                goto split3;
#        d = lp1[1] - lp3[1];
#        if (d < -1 || d > 1)

	lwz     r9,0(r30)
	lwz     r10,0(r31)
	lwz     r11,1*4(r30)
	lwz     r12,1*4(r31)
	subf    r3,r10,r9
	addi    r3,r3,1
	cmplwi  r3,2
	bgt     .rtsplit
	subf    r4,r12,r11
	addi    r4,r4,1
	cmplwi  r4,2
	bgt     .rtsplit
	lwz     r3,0(r29)
	subf    r5,r9,r3
	addi    r5,r5,1
	cmplwi  r5,2
	bgt     .rtsplit2
	lwz     r4,1*4(r29)
	subf    r5,r11,r4
	addi    r5,r5,1
	cmplwi  r5,2
	bgt     .rtsplit2
	subf    r5,r10,r3
	addi    r5,r5,1
	cmplwi  r5,2
	bgt     .rtsplit3
	subf    r5,r12,r4
	addi    r5,r5,1
	cmplwi  r5,2
	ble     .rtexit

#split3:
#                temp = lp1;
#                lp1 = lp3;
#                lp3 = lp2;
#                lp2 = temp;
#
#                goto split;
#
#split2:
#        temp = lp1;
#        lp1 = lp2;
#        lp2 = lp3;
#        lp3 = temp;

.rtsplit3:
	mr      r0,r29
	mr      r29,r30
	mr      r30,r31
	mr      r31,r0
	lwz     r9,0(r30)
	lwz     r10,0(r31)
	lwz     r11,1*4(r30)
	lwz     r12,1*4(r31)
	b       .rtsplit
.rtsplit2:
	mr      r0,r31
	mr      r31,r30
	mr      r30,r29
	mr      r29,r0
	lwz     r9,0(r30)
	lwz     r10,0(r31)
	lwz     r11,1*4(r30)
	lwz     r12,1*4(r31)
.rtsplit:

#        new[0] = (lp1[0] + lp2[0]) >> 1;
#        new[1] = (lp1[1] + lp2[1]) >> 1;
#        new[2] = (lp1[2] + lp2[2]) >> 1;
#        new[3] = (lp1[3] + lp2[3]) >> 1;
#        new[5] = (lp1[5] + lp2[5]) >> 1;

	add     r3,r9,r10
	srawi   r3,r3,1                 #r3 = new[0]
	stw     r3,0(r28)
	add     r4,r11,r12
	srawi   r4,r4,1                 #r4 = new[1]
	stw     r4,4(r28)
	lwz     r5,2*4(r31)
	lwz     r0,2*4(r30)
	add     r5,r5,r0
	srawi   r5,r5,1                 #r5 = new[2]
	stw     r5,8(r28)
	lwz     r8,3*4(r31)
	lwz     r0,3*4(r30)
	add     r8,r8,r0
	srawi   r8,r8,1                 #r8 = new[3]
	stw     r8,12(r28)
	lwz     r27,5*4(r31)
	lwz     r0,5*4(r30)
	add     r27,r27,r0
	srawi   r27,r27,1               #r27 = new[5]
	stw     r27,20(r28)

#        if (lp2[1] > lp1[1])
#                goto nodraw;
#        if ((lp2[1] == lp1[1]) && (lp2[0] < lp1[0]))
#                goto nodraw;


	cmpw    r11,r12
	bgt     .rtnodraw
	bne     .rtdraw
	cmpw    r9,r10
	blt     .rtnodraw
.rtdraw:
	srwi    r27,r27,16              #r27 = z = new[5]>>16

#        z = new[5]>>16;
#        zbuf = zspantable[new[1]] + new[0];
#        if (z >= *zbuf)
#        {
#                int             pix;
#
#                *zbuf = z;
#                pix = d_pcolormap[skintable[new[3]>>16][new[2]>>16]];
#                d_viewbuffer[d_scantable[new[1]] + new[0]] = pix;
#        }

	slwi    r0,r4,2
	lwzx    r9,r6,r0
	add     r10,r3,r3
	lhzx    r11,r9,r10
	cmpw    r27,r11
	blt     .rtnodraw
	sthx    r27,r9,r10
	srawi   r5,r5,16
	srawi   r8,r8,16
	slwi    r8,r8,2
	lwzx    r12,r26,r8
	lbzx    r9,r12,r5
	lbzx    r10,r25,r9
	lwzx    r11,r24,r0
	add     r11,r11,r3
	stbx    r10,r7,r11
.rtnodraw:

#// recursively continue
#        D_PolysetRecursiveTriangle (lp3, lp1, new);
#        D_PolysetRecursiveTriangle (lp3, new, lp2);

	mr      r3,r29
	mr      r4,r31
	mr      r5,r28
	bl      DoRecursion
	mr      r3,r29
	mr      r4,r28
	mr      r5,r30
	bl      DoRecursion

.rtexit:
	lmw	r27,gb(r1)
	exit

	funcend	D_PolysetRecursiveTriangle




###########################################################################
#
#       void D_PolysetSetUpForLineScan (fixed8_t startvertu,
#               fixed8_t startvertv, fixed8_t endvertu, fixed8_t endvertv)
#
#       Parameters are transferred in registers d0-d3
#
###########################################################################

	funcdef	D_PolysetSetUpForLineScan

#        errorterm = -1;
#
#        tm = endvertu - startvertu;
#        tn = endvertv - startvertv;

	li	r0,-1
	sw	r0,errorterm
	subf    r5,r3,r5
	subf    r6,r4,r6
	addi    r5,r5,15
	addi    r6,r6,15

#        if (((tm <= 16) && (tm >= -15)) &&
#                ((tn <= 16) && (tn >= -15)))
#        {
#                ptemp = &adivtab[((tm+15) << 5) + (tn+15)];
#                ubasestep = ptemp->quotient;
#                erroradjustup = ptemp->remainder;
#                erroradjustdown = tn;
#        }

	cmplwi  r5,31
	bgt     .sflselse
	cmplwi  r6,31
	bgt     .sflselse
	lxa     r8,adivtab
	slwi    r5,r5,5
	add     r5,r5,r6
	slwi    r5,r5,3
	add     r8,r8,r5
	lwz     r3,PTEMP_QUOTIENT(r8)
	sw      r3,ubasestep
	lwz     r4,PTEMP_REMAINDER(r8)
	sw      r4,erroradjustup
	subi    r6,r6,15
	sw      r6,erroradjustdown
	b       .sflsexit

#        {
#                dm = (double)tm;
#                dn = (double)tn;
#
#                FloorDivMod (dm, dn, &ubasestep, &erroradjustup);
#
#                erroradjustdown = dn;
#        }

.sflselse:
	subi    r5,r5,15
	subi    r6,r6,15
	sw      r6,erroradjustdown
	mr.     r5,r5
	divw    r7,r5,r6
	mullw   r8,r7,r6
	subf    r8,r8,r5
	bge     .sflscont
	subi    r7,r7,1
	add     r8,r8,r6
.sflscont:
	sw      r7,ubasestep
	sw      r8,erroradjustup
.sflsexit:
	blr

	funcend	D_PolysetSetUpForLineScan




###########################################################################
#
#       void D_PolysetCalcGradients (int skinwidth)
#
###########################################################################

	funcdef	D_PolysetCalcGradients

#        p00_minus_p20 = r_p0[0] - r_p2[0];
#        p01_minus_p21 = r_p0[1] - r_p2[1];
#        p10_minus_p20 = r_p1[0] - r_p2[0];
#        p11_minus_p21 = r_p1[1] - r_p2[1];
#
#        xstepdenominv = 1.0 / (float)d_xdenom;
#
#        ystepdenominv = -xstepdenominv;

	stwu	r1,-64(r1)
	lw      r11,d_xdenom
	ls      f9,c2
	lf      f1,INT2DBL_0            #for int2dbl_setup, r12=tmp
	stfd	f1,32(r1)
	lxa     r4,r_p2
	lxa     r5,r_p1
	lxa     r6,r_p0
	int2dbl	f6,r11,r12,32,f1
	lwz     r7,0(r6)
	lwz     r8,1*4(r6)
	fneg    f6,f6
	lwz     r9,0(r4)
	subf    r7,r9,r7
	fmuls   f7,f6,f6
	int2dbl	f2,r7,r12,32,f1		#f2 = p00_minus_p20
	frsqrte f7,f7
	lwz     r10,1*4(r4)
	subf    r8,r10,r8
	fnmsubs f8,f7,f6,f9
	int2dbl f3,r8,r12,32,f1		#f3 = p01_minus_p21
	fmuls   f7,f7,f8
	lwz     r7,0(r5)
	subf    r7,r9,r7
	fnmsubs f8,f7,f6,f9
	int2dbl f4,r7,r12,32,f1		#f4 = p10_minus_p20
	fmuls   f7,f7,f8
	lwz     r8,1*4(r5)
	subf    r8,r10,r8
	fneg    f6,f7
	int2dbl f5,r8,r12,32,f1		#f5 = p11_minus_p21
	fmuls   f3,f3,f6
	fmuls   f5,f5,f6
	fmuls   f2,f2,f7
	fmuls   f4,f4,f7

#        t0 = r_p0[4] - r_p2[4];
#        t1 = r_p1[4] - r_p2[4];
#        r_lstepx = (int)
#                        ceil((t1 * p01_minus_p21 - t0 * p11_minus_p21) * xstepdenominv);
#        r_lstepy = (int)
#                        ceil((t1 * p00_minus_p20 - t0 * p10_minus_p20) * ystepdenominv);

	mffs    f8
	mtfsfi  7,2
	lwz     r7,4*4(r4)
	lwz     r8,4*4(r6)
	subf    r9,r7,r8
	int2dbl f9,r9,r12,32,f1		#f9 = t0
	lwz     r8,4*4(r5)
	subf    r9,r7,r8
	int2dbl f10,r9,r12,32,f1	#f10 = t1
	fmuls   f11,f9,f5
	fmuls   f12,f9,f4
	fmsubs  f11,f10,f3,f11
	fctiw   f0,f11			#@@@ ab hier ein bißchen optimiert
	stfd    f0,40(r1)
	fmsubs  f12,f10,f2,f12
	lwz     r7,44(r1)
	fctiw   f13,f12
	stfd    f13,48(r1)
	mtfsf   1,f8
	lwz     r8,52(r1)
	sw      r7,r_lstepx
	sw      r8,r_lstepy

#        t0 = r_p0[2] - r_p2[2];
#        t1 = r_p1[2] - r_p2[2];
#        r_sstepx = (int)((t1 * p01_minus_p21 - t0 * p11_minus_p21) *
#                        xstepdenominv);
#        r_sstepy = (int)((t1 * p00_minus_p20 - t0* p10_minus_p20) *
#                        ystepdenominv);

	lwz     r7,2*4(r4)
	lwz     r8,2*4(r6)
	subf    r9,r7,r8
	int2dbl f9,r9,r12,32,f1		#f9 = t0
	lwz     r8,2*4(r5)
	subf    r9,r7,r8
	int2dbl f10,r9,r12,32,f1	#f10 = t1
	fmuls   f11,f9,f5
	fmuls   f12,f9,f4
	fmsubs  f11,f10,f3,f11
	fctiwz  f0,f11			#@@@ ab hier ein bißchen optimiert
	stfd    f0,40(r1)
	fmsubs  f12,f10,f2,f12
	lwz     r10,44(r1)
	fctiwz  f13,f12
	stfd    f13,48(r1)
	lwz     r8,52(r1)
	sw      r10,r_sstepx
	sw      r8,r_sstepy

#        t0 = r_p0[3] - r_p2[3];
#        t1 = r_p1[3] - r_p2[3];
#        r_tstepx = (int)((t1 * p01_minus_p21 - t0 * p11_minus_p21) *
#                        xstepdenominv);
#        r_tstepy = (int)((t1 * p00_minus_p20 - t0 * p10_minus_p20) *
#                        ystepdenominv);

	lwz     r7,3*4(r4)
	lwz     r8,3*4(r6)
	subf    r9,r7,r8
	int2dbl f9,r9,r12,32,f1		#f9 = t0
	lwz     r8,3*4(r5)
	subf    r9,r7,r8
	int2dbl f10,r9,r12,32,f1	#f10 = t1
	fmuls   f11,f9,f5
	fmuls   f12,f9,f4
	fmsubs  f11,f10,f3,f11
	fctiwz  f0,f11			#@@@ ab hier ein bißchen optimiert
	stfd    f0,40(r1)
	fmsubs  f12,f10,f2,f12
	lwz     r11,44(r1)
	fctiwz  f13,f12
	stfd    f13,48(r1)
	lwz     r8,52(r1)
	sw      r11,r_tstepx
	sw      r8,r_tstepy

#        t0 = r_p0[5] - r_p2[5];
#        t1 = r_p1[5] - r_p2[5];
#        r_zistepx = (int)((t1 * p01_minus_p21 - t0 * p11_minus_p21) *
#                        xstepdenominv);
#        r_zistepy = (int)((t1 * p00_minus_p20 - t0 * p10_minus_p20) *
#                        ystepdenominv);

	lwz     r7,5*4(r4)
	lwz     r8,5*4(r6)
	subf    r9,r7,r8
	int2dbl f9,r9,r12,32,f1		#f9 = t0
	lwz     r8,5*4(r5)
	subf    r9,r7,r8
	int2dbl f10,r9,r12,32,f1	#f10 = t1
	fmuls   f11,f9,f5
	fmuls   f12,f9,f4
	fmsubs  f11,f10,f3,f11
	fctiwz  f0,f11			#@@@ ab hier ein bißchen optimiert
	stfd    f0,40(r1)
	fmsubs  f12,f10,f2,f12
	lwz     r7,44(r1)
	fctiwz  f13,f12
	stfd    f13,48(r1)
	lwz     r8,52(r1)
	sw      r7,r_zistepx
	sw      r8,r_zistepy

#        a_sstepxfrac = r_sstepx & 0xFFFF;
#        a_tstepxfrac = r_tstepx & 0xFFFF;
#        a_ststepxwhole = skinwidth * (r_tstepx >> 16) + (r_sstepx >> 16);

	andi.   r4,r10,0xffff
	srawi   r6,r10,16
	sw      r4,a_sstepxfrac
	andi.   r5,r11,0xffff
	srawi   r7,r11,16
	sw      r5,a_tstepxfrac
	mullw   r7,r7,r3
	add     r7,r7,r6
	sw      r7,a_ststepxwhole
	addi	r1,r1,64
	blr

	funcend	D_PolysetCalcGradients




###########################################################################
#
#       void D_PolysetScanLeftEdge (int height)
#
###########################################################################

	funcdef	D_PolysetScanLeftEdge

.ifdef	WOS
	init	0,0,19,0
	stmw	r13,gb(r1)
.else
# r14 is used and save internally, r13 may be used as small data pointer
	init	0,0,17,0
	stmw	r15,gb(r1)
.endif

	mtctr   r3
	lw      r3,d_pedgespanpackage
	subi    r3,r3,4
	lw      r4,d_pdest
	lw      r5,d_pz
	lw      r6,d_aspancount
	lw      r7,d_ptex
	lw      r8,d_sfrac
	lw      r9,d_tfrac
	lw      r10,d_light
	lw      r11,d_zi
	lxa     r12,r_affinetridesc
	lwz     r12,R_SKINWIDTH(r12)
	lw      r31,errorterm
	lw      r30,erroradjustup
	lw      r29,erroradjustdown
	lw      r28,d_pdestextrastep
	lw      r27,d_pzextrastep
	add     r27,r27,r27
	lw      r26,d_countextrastep
	lw      r25,d_ptexextrastep
	lw      r24,d_sfracextrastep
	lw      r23,d_tfracextrastep
	lw      r22,d_lightextrastep
	lw      r21,d_ziextrastep
	lw      r20,d_pdestbasestep
	lw      r19,d_pzbasestep
	add     r19,r19,r19
	lw      r18,ubasestep
	lw      r17,d_ptexbasestep
	lw      r16,d_sfracbasestep
	lw      r15,d_tfracbasestep
.ifdef	WOS
	lw      r14,d_lightbasestep
	lw      r13,d_zibasestep
.endif

.sleloop:

#                d_pedgespanpackage->pdest = d_pdest;
#                d_pedgespanpackage->pz = d_pz;
#                d_pedgespanpackage->count = d_aspancount;
#                d_pedgespanpackage->ptex = d_ptex;
#
#                d_pedgespanpackage->sfrac = d_sfrac;
#                d_pedgespanpackage->tfrac = d_tfrac;
#
#                d_pedgespanpackage->light = d_light;
#                d_pedgespanpackage->zi = d_zi;
#
#                d_pedgespanpackage++;
#
#                errorterm += erroradjustup;
#                if (errorterm >= 0)

	stwu    r4,4(r3)
	add.    r31,r31,r30
	stwu    r5,4(r3)
	stwu    r6,4(r3)
	stwu    r7,4(r3)
	stwu    r8,4(r3)
	stwu    r9,4(r3)
	stwu    r10,4(r3)
	stwu    r11,4(r3)
	blt     .sleelse

#                        d_pdest += d_pdestextrastep;
#                        d_pz += d_pzextrastep;
#                        d_aspancount += d_countextrastep;
#                        d_ptex += d_ptexextrastep;
#                        d_sfrac += d_sfracextrastep;
#                        d_ptex += d_sfrac >> 16;
#
#                        d_sfrac &= 0xFFFF;
#                        d_tfrac += d_tfracextrastep;
#                        if (d_tfrac & 0x10000)
#                        {
#                                d_ptex += r_affinetridesc.skinwidth;
#                                d_tfrac &= 0xFFFF;
#                        }
#                        d_light += d_lightextrastep;
#                        d_zi += d_ziextrastep;
#                        errorterm -= erroradjustdown;

	add     r8,r8,r24
	add     r4,r4,r28
	add     r5,r5,r27
	add     r10,r10,r22
	add     r11,r11,r21
	add     r7,r7,r25
	srawi   r0,r8,16
	add     r6,r6,r26
	add     r9,r9,r23
	add     r7,r7,r0
	andi.   r8,r8,0xffff
	andis.  r0,r9,1
	subf    r31,r29,r31
	beq     .slenext
	add     r7,r7,r12
	andi.   r9,r9,0xffff
	b       .slenext
.sleelse:

#                        d_pdest += d_pdestbasestep;
#                        d_pz += d_pzbasestep;
#                        d_aspancount += ubasestep;
#                        d_ptex += d_ptexbasestep;
#                        d_sfrac += d_sfracbasestep;
#                        d_ptex += d_sfrac >> 16;
#                        d_sfrac &= 0xFFFF;
#                        d_tfrac += d_tfracbasestep;
#                        if (d_tfrac & 0x10000)
#                        {
#                                d_ptex += r_affinetridesc.skinwidth;
#                                d_tfrac &= 0xFFFF;
#                        }
#                        d_light += d_lightbasestep;
#                        d_zi += d_zibasestep;

.ifdef	WOS
	add     r8,r8,r16
	add     r4,r4,r20
	add     r5,r5,r19
	add     r10,r10,r14
	add     r11,r11,r13
	add     r7,r7,r17
.else
	lw      r0,d_lightbasestep
	add     r8,r8,r16
	add     r4,r4,r20
	add     r10,r10,r0
	lw      r0,d_zibasestep
	add     r5,r5,r19
	add     r7,r7,r17
	add     r11,r11,r0
.endif
	srawi   r0,r8,16
	add     r6,r6,r18
	add     r9,r9,r15
	add     r7,r7,r0
	andi.   r8,r8,0xffff
	andis.  r0,r9,1
	beq     .slenext
	add     r7,r7,r12
	andi.   r9,r9,0xffff
.slenext:
	bdnz    .sleloop
	addi    r3,r3,4
	sw      r3,d_pedgespanpackage

.ifdef	WOS
	lmw     r13,gb(r1)
.else
	lmw     r15,gb(r1)
.endif
	exit

	funcend	D_PolysetScanLeftEdge




###########################################################################
#
#       void D_DrawNonSubdiv (void)
#
###########################################################################

	funcdef	D_DrawNonSubdiv

	init	0,0,9,0
	stmw	r23,gb(r1)

#        pfv = r_affinetridesc.pfinalverts;
#        ptri = r_affinetridesc.ptriangles;
#        lnumtriangles = r_affinetridesc.numtriangles;

	lxa     r29,r_affinetridesc
	lwz     r31,R_PFINALVERTS(r29)
	lwz     r30,R_PTRIANGLES(r29)
	lwz     r23,R_NUMTRIANGLES(r29)
	lwz     r29,R_SEAMFIXUP16(r29)
	lxa     r27,r_p0
	lxa     r26,r_p1
	lxa     r25,r_p2
	lxa     r24,edgetables
.nsloop:

#                index0 = pfv + ptri->vertindex[0];
#                index1 = pfv + ptri->vertindex[1];
#                index2 = pfv + ptri->vertindex[2];
#
#                d_xdenom = (index0->v[1]-index1->v[1]) *
#                                (index0->v[0]-index2->v[0]) -
#                                (index0->v[0]-index1->v[0])*(index0->v[1]-index2->v[1]);

	lwz     r6,MT_VERTINDEX+0*4(r30)
	slwi    r6,r6,FV_SIZEOF_EXP
	add     r7,r31,r6               #r7 = index0
	lwz     r6,MT_VERTINDEX+1*4(r30)
	slwi    r6,r6,FV_SIZEOF_EXP
	add     r8,r31,r6               #r8 = index1
	lwz     r6,MT_VERTINDEX+2*4(r30)
	slwi    r6,r6,FV_SIZEOF_EXP
	add     r9,r31,r6               #r9 = index2
	lwz     r3,0(r7)                #r3 = index0->v[0]
	lwz     r4,4(r7)                #r4 = index0->v[1]
	lwz     r5,0(r8)                #r5 = index1->v[0]
	lwz     r6,4(r8)                #r6 = index1->v[1]
	lwz     r10,0(r9)               #r10 = index2->v[0]
	lwz     r11,4(r9)               #r11 = index2->v[1]
	subf    r12,r10,r3
	subf    r0,r6,r4
	mullw   r12,r12,r0
	subf    r0,r5,r3
	subf    r28,r11,r4
	mullw   r28,r28,r0

#                if (d_xdenom >= 0)
#                {
#                        continue;
#                }

	subf.   r12,r28,r12
	bge     .nsnext
	sw      r12,d_xdenom

#                r_p0[0] = index0->v[0];         // u
#                r_p0[1] = index0->v[1];         // v
#                r_p0[2] = index0->v[2];         // s
#                r_p0[3] = index0->v[3];         // t
#                r_p0[4] = index0->v[4];         // light
#                r_p0[5] = index0->v[5];         // iz
#
#                r_p1[0] = index1->v[0];
#                r_p1[1] = index1->v[1];
#                r_p1[2] = index1->v[2];
#                r_p1[3] = index1->v[3];
#                r_p1[4] = index1->v[4];
#                r_p1[5] = index1->v[5];
#
#                r_p2[0] = index2->v[0];
#                r_p2[1] = index2->v[1];
#                r_p2[2] = index2->v[2];
#                r_p2[3] = index2->v[3];
#                r_p2[4] = index2->v[4];
#                r_p2[5] = index2->v[5];

	stw     r3,0(r27)
	stw     r4,4(r27)
	lwz     r12,12(r7)
	stw     r12,12(r27)
	lwz     r0,16(r7)
	stw     r0,16(r27)
	lwz     r12,20(r7)
	stw     r12,20(r27)
	stw     r5,0(r26)
	stw     r6,4(r26)
	lwz     r12,12(r8)
	stw     r12,12(r26)
	lwz     r0,16(r8)
	stw     r0,16(r26)
	lwz     r12,20(r8)
	stw     r12,20(r26)
	stw     r10,0(r25)
	stw     r11,4(r25)
	lwz     r3,MT_FACESFRONT(r30)
	lwz     r12,12(r9)
	stw     r12,12(r25)
	lwz     r0,16(r9)
	stw     r0,16(r25)
	lwz     r12,20(r9)
	stw     r12,20(r25)

	lwz     r5,8(r7)
	lwz     r12,8(r8)
	lwz     r10,8(r9)

#                if (!ptri->facesfront)
#                {
#                        if (index0->flags & ALIAS_ONSEAM)
#                                r_p0[2] += r_affinetridesc.seamfixupX16;
#                        if (index1->flags & ALIAS_ONSEAM)
#                                r_p1[2] += r_affinetridesc.seamfixupX16;
#                        if (index2->flags & ALIAS_ONSEAM)
#                                r_p2[2] += r_affinetridesc.seamfixupX16;
#                }

	mr.     r3,r3
	bne     .nscont
	lwz     r0,FV_FLAGS(r7)
	andi.   r0,r0,ALIAS_ONSEAM
	beq     .ns1
	add     r5,r5,r29
.ns1:
	lwz     r0,FV_FLAGS(r8)
	andi.   r0,r0,ALIAS_ONSEAM
	beq     .ns2
	add     r12,r12,r29
.ns2:
	lwz     r0,FV_FLAGS(r9)
	andi.   r0,r0,ALIAS_ONSEAM
	beq     .nscont
	add     r10,r10,r29
.nscont:
	stw     r5,8(r27)
	stw     r12,8(r26)
	stw     r10,8(r25)

######  D_PolysetSetEdgeTable (inlined)

	cmpw    r4,r6
	blt     .nslt1
	beq     .nseq1
.nsgt1:
	li      r3,ETAB_SIZEOF
	cmpw    cr1,r4,r11
	beq     cr1,.nseq2
	cmpw    r6,r11
	beq     .nseq3
	ble     cr1,.nsskip
	addi    r3,r3,2*ETAB_SIZEOF
.nsskip:
	ble     .nsskip2
	addi    r3,r3,4*ETAB_SIZEOF
.nsskip2:
	add     r3,r3,r24
	sw      r3,pedgetable
	b       .nsdone
.nseq2:
	addi    r3,r24,8*ETAB_SIZEOF
	sw      r3,pedgetable
	b       .nsdone
.nseq3:
	addi    r3,r24,10*ETAB_SIZEOF
	sw      r3,pedgetable
	b       .nsdone
.nseq1:
	cmpw    r4,r11
	bge     .nsge
	addi    r3,r24,2*ETAB_SIZEOF
	sw      r3,pedgetable
	b       .nsdone
.nsge:
	addi    r3,r24,5*ETAB_SIZEOF
	sw      r3,pedgetable
	b       .nsdone
.nslt1:
	li      r3,0
	cmpw    cr1,r4,r11
	beq     cr1,.nseq4
	cmpw    r6,r11
	beq     .nseq5
	ble     cr1,.nsskip3
	addi    r3,r3,2*ETAB_SIZEOF
.nsskip3:
	ble     .nsskip4
	addi    r3,r3,4*ETAB_SIZEOF
.nsskip4:
	add     r3,r3,r24
	sw      r3,pedgetable
	b       .nsdone
.nseq4:
	addi    r3,r24,9*ETAB_SIZEOF
	sw      r3,pedgetable
	b       .nsdone
.nseq5:
	addi    r3,r24,11*ETAB_SIZEOF
	sw      r3,pedgetable
.nsdone:

######  end of D_PolysetSetEdgeTable

	call	D_RasterizeAliasPolySmooth
.nsnext:
	subic.  r23,r23,1
	la      r30,MT_SIZEOF(r30)
	bne     .nsloop

	lmw	r23,gb(r1)
	exit

	funcend	D_DrawNonSubdiv




###########################################################################
#
#       void D_DrawSubdiv (void)
#
###########################################################################

	funcdef	D_DrawSubdiv

	init	0,0,11,0
	stmw	r21,gb(r1)

#        pfv = r_affinetridesc.pfinalverts;
#        ptri = r_affinetridesc.ptriangles;
#        lnumtriangles = r_affinetridesc.numtriangles;

	lxa     r29,r_affinetridesc
	lwz     r31,R_PFINALVERTS(r29)
	lwz     r30,R_PTRIANGLES(r29)
	lwz     r21,R_NUMTRIANGLES(r29)
	lwz     r29,R_SEAMFIXUP16(r29)
	lw      r25,acolormap
.sdloop:

#                index0 = pfv + ptri->vertindex[0];
#                index1 = pfv + ptri->vertindex[1];
#                index2 = pfv + ptri->vertindex[2];
#
#               if (((index0->v[1]-index1->v[1]) *
#                                (index0->v[0]-index2->v[0]) -
#                                (index0->v[0]-index1->v[0])*(index0->v[1]-index2->v[1]);

	lwz     r6,MT_VERTINDEX+0*4(r30)
	slwi    r6,r6,FV_SIZEOF_EXP
	add     r28,r31,r6              #r28 = index0
	lwz     r6,MT_VERTINDEX+1*4(r30)
	slwi    r6,r6,FV_SIZEOF_EXP
	add     r27,r31,r6              #r27 = index1
	lwz     r6,MT_VERTINDEX+2*4(r30)
	slwi    r6,r6,FV_SIZEOF_EXP
	add     r26,r31,r6              #r26 = index2
	lwz     r3,0(r28)               #r3 = index0->v[0]
	lwz     r4,4(r28)               #r4 = index0->v[1]
	lwz     r5,0(r27)               #r5 = index1->v[0]
	lwz     r6,4(r27)               #r6 = index1->v[1]
	lwz     r10,0(r26)              #r10 = index2->v[0]
	lwz     r11,4(r26)              #r11 = index2->v[1]
	subf    r12,r10,r3
	subf    r0,r6,r4
	mullw   r12,r12,r0
	subf    r0,r5,r3
	subf    r7,r11,r4
	mullw   r7,r7,r0
	subf.   r12,r7,r12
	bge     .sdnext
	lwz     r3,MT_FACESFRONT(r30)
	lwz     r8,16(r28)
	andi.   r8,r8,0xff00
	add     r9,r25,r8
	sw      r9,d_pcolormap
	mr.     r3,r3
	beq     .sdelse
	mr      r3,r28
	mr      r4,r27
	mr      r5,r26
	call	D_PolysetRecursiveTriangle
	b       .sdnext
.sdelse:
	lwz     r24,8(r28)              #s0
	lwz     r23,8(r27)              #s1
	lwz     r22,8(r26)              #s2
	lwz     r0,FV_FLAGS(r28)
	andi.   r0,r0,ALIAS_ONSEAM
	beq     .sd1
	add     r5,r24,r29
	stw     r5,8(r28)
.sd1:
	lwz     r0,FV_FLAGS(r27)
	andi.   r0,r0,ALIAS_ONSEAM
	beq     .sd2
	add     r5,r23,r29
	stw     r5,8(r27)
.sd2:
	lwz     r0,FV_FLAGS(r26)
	andi.   r0,r0,ALIAS_ONSEAM
	beq     .sdcont
	add     r5,r22,r29
	stw     r5,8(r26)
.sdcont:
	mr      r3,r28
	mr      r4,r27
	mr      r5,r26
	call	D_PolysetRecursiveTriangle
	stw     r24,8(r28)
	stw     r23,8(r27)
	stw     r22,8(r26)
.sdnext:
	subic.  r21,r21,1
	la      r30,MT_SIZEOF(r30)
	bne     .sdloop

	lmw	r21,gb(r1)
	exit

	funcend	D_DrawSubdiv




.ifdef	WOS
	.tocd
@__adivtab:
	.long	_adivtab
.endif
	.data

lab adivtab
	.long    1,0,1,-1,1,-2,1,-3,1,-4,1
	.long    -5,1,-6,1,-7,2,-1,2,-3,3,0
	.long    3,-3,5,0,7,-1,15,0,0,0,-15
	.long    0,-8,1,-5,0,-4,1,-3,0,-3,3
	.long    -3,6,-2,1,-2,3,-2,5,-2,7,-2
	.long    9,-2,11,-2,13,-1,0,-1,1,0,-14
	.long    1,0,1,-1,1,-2,1,-3,1,-4,1
	.long    -5,1,-6,2,0,2,-2,2,-4,3,-2
	.long    4,-2,7,0,14,0,0,0,-14,0,-7
	.long    0,-5,1,-4,2,-3,1,-3,4,-2,0
	.long    -2,2,-2,4,-2,6,-2,8,-2,10,-2
	.long    12,-1,0,-1,1,-1,2,0,-13,0,-13
	.long    1,0,1,-1,1,-2,1,-3,1,-4,1
	.long    -5,1,-6,2,-1,2,-3,3,-1,4,-1
	.long    6,-1,13,0,0,0,-13,0,-7,1,-5
	.long    2,-4,3,-3,2,-3,5,-2,1,-2,3
	.long    -2,5,-2,7,-2,9,-2,11,-1,0,-1
	.long    1,-1,2,-1,3,0,-12,0,-12,0,-12
	.long    1,0,1,-1,1,-2,1,-3,1,-4,1
	.long    -5,2,0,2,-2,3,0,4,0,6,0
	.long    12,0,0,0,-12,0,-6,0,-4,0,-3
	.long    0,-3,3,-2,0,-2,2,-2,4,-2,6
	.long    -2,8,-2,10,-1,0,-1,1,-1,2,-1
	.long    3,-1,4,0,-11,0,-11,0,-11,0,-11
	.long    1,0,1,-1,1,-2,1,-3,1,-4,1
	.long    -5,2,-1,2,-3,3,-2,5,-1,11,0
	.long    0,0,-11,0,-6,1,-4,1,-3,1,-3
	.long    4,-2,1,-2,3,-2,5,-2,7,-2,9
	.long    -1,0,-1,1,-1,2,-1,3,-1,4,-1
	.long    5,0,-10,0,-10,0,-10,0,-10,0,-10
	.long    1,0,1,-1,1,-2,1,-3,1,-4,2
	.long    0,2,-2,3,-1,5,0,10,0,0,0
	.long    -10,0,-5,0,-4,2,-3,2,-2,0,-2
	.long    2,-2,4,-2,6,-2,8,-1,0,-1,1
	.long    -1,2,-1,3,-1,4,-1,5,-1,6,0
	.long    -9,0,-9,0,-9,0,-9,0,-9,0,-9
	.long    1,0,1,-1,1,-2,1,-3,1,-4,2
	.long    -1,3,0,4,-1,9,0,0,0,-9,0
	.long    -5,1,-3,0,-3,3,-2,1,-2,3,-2
	.long    5,-2,7,-1,0,-1,1,-1,2,-1,3
	.long    -1,4,-1,5,-1,6,-1,7,0,-8,0
	.long    -8,0,-8,0,-8,0,-8,0,-8,0,-8
	.long    1,0,1,-1,1,-2,1,-3,2,0,2
	.long    -2,4,0,8,0,0,0,-8,0,-4,0
	.long    -3,1,-2,0,-2,2,-2,4,-2,6,-1
	.long    0,-1,1,-1,2,-1,3,-1,4,-1,5
	.long    -1,6,-1,7,-1,8,0,-7,0,-7,0
	.long    -7,0,-7,0,-7,0,-7,0,-7,0,-7
	.long    1,0,1,-1,1,-2,1,-3,2,-1,3
	.long    -1,7,0,0,0,-7,0,-4,1,-3,2
	.long    -2,1,-2,3,-2,5,-1,0,-1,1,-1
	.long    2,-1,3,-1,4,-1,5,-1,6,-1,7
	.long    -1,8,-1,9,0,-6,0,-6,0,-6,0
	.long    -6,0,-6,0,-6,0,-6,0,-6,0,-6
	.long    1,0,1,-1,1,-2,2,0,3,0,6
	.long    0,0,0,-6,0,-3,0,-2,0,-2,2
	.long    -2,4,-1,0,-1,1,-1,2,-1,3,-1
	.long    4,-1,5,-1,6,-1,7,-1,8,-1,9
	.long    -1,10,0,-5,0,-5,0,-5,0,-5,0
	.long    -5,0,-5,0,-5,0,-5,0,-5,0,-5
	.long    1,0,1,-1,1,-2,2,-1,5,0,0
	.long    0,-5,0,-3,1,-2,1,-2,3,-1,0
	.long    -1,1,-1,2,-1,3,-1,4,-1,5,-1
	.long    6,-1,7,-1,8,-1,9,-1,10,-1,11
	.long    0,-4,0,-4,0,-4,0,-4,0,-4,0
	.long    -4,0,-4,0,-4,0,-4,0,-4,0,-4
	.long    1,0,1,-1,2,0,4,0,0,0,-4
	.long    0,-2,0,-2,2,-1,0,-1,1,-1,2
	.long    -1,3,-1,4,-1,5,-1,6,-1,7,-1
	.long    8,-1,9,-1,10,-1,11,-1,12,0,-3
	.long    0,-3,0,-3,0,-3,0,-3,0,-3,0
	.long    -3,0,-3,0,-3,0,-3,0,-3,0,-3
	.long    1,0,1,-1,3,0,0,0,-3,0,-2
	.long    1,-1,0,-1,1,-1,2,-1,3,-1,4
	.long    -1,5,-1,6,-1,7,-1,8,-1,9,-1
	.long    10,-1,11,-1,12,-1,13,0,-2,0,-2
	.long    0,-2,0,-2,0,-2,0,-2,0,-2,0
	.long    -2,0,-2,0,-2,0,-2,0,-2,0,-2
	.long    1,0,2,0,0,0,-2,0,-1,0,-1
	.long    1,-1,2,-1,3,-1,4,-1,5,-1,6
	.long    -1,7,-1,8,-1,9,-1,10,-1,11,-1
	.long    12,-1,13,-1,14,0,-1,0,-1,0,-1
	.long    0,-1,0,-1,0,-1,0,-1,0,-1,0
	.long    -1,0,-1,0,-1,0,-1,0,-1,0,-1
	.long    1,0,0,0,-1,0,-1,1,-1,2,-1
	.long    3,-1,4,-1,5,-1,6,-1,7,-1,8
	.long    -1,9,-1,10,-1,11,-1,12,-1,13,-1
	.long    14,-1,15,0,0,0,0,0,0,0,0
	.long    0,0,0,0,0,0,0,0,0,0,0
	.long    0,0,0,0,0,0,0,0,0,0,0
	.long    0,0,0,0,0,0,0,0,0,0,0
	.long    0,0,0,0,0,0,0,0,0,0,0
	.long    0,0,0,0,0,0,0,0,0,0,0
	.long    0,-1,-14,-1,-13,-1,-12,-1,-11,-1,-10
	.long    -1,-9,-1,-8,-1,-7,-1,-6,-1,-5,-1
	.long    -4,-1,-3,-1,-2,-1,-1,-1,0,0,0
	.long    1,0,0,1,0,1,0,1,0,1,0
	.long    1,0,1,0,1,0,1,0,1,0,1
	.long    0,1,0,1,0,1,0,1,0,1,-1
	.long    -13,-1,-12,-1,-11,-1,-10,-1,-9,-1,-8
	.long    -1,-7,-1,-6,-1,-5,-1,-4,-1,-3,-1
	.long    -2,-1,-1,-1,0,-2,0,0,0,2,0
	.long    1,0,0,2,0,2,0,2,0,2,0
	.long    2,0,2,0,2,0,2,0,2,0,2
	.long    0,2,0,2,0,2,0,2,-1,-12,-1
	.long    -11,-1,-10,-1,-9,-1,-8,-1,-7,-1,-6
	.long    -1,-5,-1,-4,-1,-3,-1,-2,-1,-1,-1
	.long    0,-2,-1,-3,0,0,0,3,0,1,1
	.long    1,0,0,3,0,3,0,3,0,3,0
	.long    3,0,3,0,3,0,3,0,3,0,3
	.long    0,3,0,3,0,3,-1,-11,-1,-10,-1
	.long    -9,-1,-8,-1,-7,-1,-6,-1,-5,-1,-4
	.long    -1,-3,-1,-2,-1,-1,-1,0,-2,-2,-2
	.long    0,-4,0,0,0,4,0,2,0,1,1
	.long    1,0,0,4,0,4,0,4,0,4,0
	.long    4,0,4,0,4,0,4,0,4,0,4
	.long    0,4,0,4,-1,-10,-1,-9,-1,-8,-1
	.long    -7,-1,-6,-1,-5,-1,-4,-1,-3,-1,-2
	.long    -1,-1,-1,0,-2,-3,-2,-1,-3,-1,-5
	.long    0,0,0,5,0,2,1,1,2,1,1
	.long    1,0,0,5,0,5,0,5,0,5,0
	.long    5,0,5,0,5,0,5,0,5,0,5
	.long    0,5,-1,-9,-1,-8,-1,-7,-1,-6,-1
	.long    -5,-1,-4,-1,-3,-1,-2,-1,-1,-1,0
	.long    -2,-4,-2,-2,-2,0,-3,0,-6,0,0
	.long    0,6,0,3,0,2,0,1,2,1,1
	.long    1,0,0,6,0,6,0,6,0,6,0
	.long    6,0,6,0,6,0,6,0,6,0,6
	.long    -1,-8,-1,-7,-1,-6,-1,-5,-1,-4,-1
	.long    -3,-1,-2,-1,-1,-1,0,-2,-5,-2,-3
	.long    -2,-1,-3,-2,-4,-1,-7,0,0,0,7
	.long    0,3,1,2,1,1,3,1,2,1,1
	.long    1,0,0,7,0,7,0,7,0,7,0
	.long    7,0,7,0,7,0,7,0,7,-1,-7
	.long    -1,-6,-1,-5,-1,-4,-1,-3,-1,-2,-1
	.long    -1,-1,0,-2,-6,-2,-4,-2,-2,-2,0
	.long    -3,-1,-4,0,-8,0,0,0,8,0,4
	.long    0,2,2,2,0,1,3,1,2,1,1
	.long    1,0,0,8,0,8,0,8,0,8,0
	.long    8,0,8,0,8,0,8,-1,-6,-1,-5
	.long    -1,-4,-1,-3,-1,-2,-1,-1,-1,0,-2
	.long    -7,-2,-5,-2,-3,-2,-1,-3,-3,-3,0
	.long    -5,-1,-9,0,0,0,9,0,4,1,3
	.long    0,2,1,1,4,1,3,1,2,1,1
	.long    1,0,0,9,0,9,0,9,0,9,0
	.long    9,0,9,0,9,-1,-5,-1,-4,-1,-3
	.long    -1,-2,-1,-1,-1,0,-2,-8,-2,-6,-2
	.long    -4,-2,-2,-2,0,-3,-2,-4,-2,-5,0
	.long    -10,0,0,0,10,0,5,0,3,1,2
	.long    2,2,0,1,4,1,3,1,2,1,1
	.long    1,0,0,10,0,10,0,10,0,10,0
	.long    10,0,10,-1,-4,-1,-3,-1,-2,-1,-1
	.long    -1,0,-2,-9,-2,-7,-2,-5,-2,-3,-2
	.long    -1,-3,-4,-3,-1,-4,-1,-6,-1,-11,0
	.long    0,0,11,0,5,1,3,2,2,3,2
	.long    1,1,5,1,4,1,3,1,2,1,1
	.long    1,0,0,11,0,11,0,11,0,11,0
	.long    11,-1,-3,-1,-2,-1,-1,-1,0,-2,-10
	.long    -2,-8,-2,-6,-2,-4,-2,-2,-2,0,-3
	.long    -3,-3,0,-4,0,-6,0,-12,0,0,0
	.long    12,0,6,0,4,0,3,0,2,2,2
	.long    0,1,5,1,4,1,3,1,2,1,1
	.long    1,0,0,12,0,12,0,12,0,12,-1
	.long    -2,-1,-1,-1,0,-2,-11,-2,-9,-2,-7
	.long    -2,-5,-2,-3,-2,-1,-3,-5,-3,-2,-4
	.long    -3,-5,-2,-7,-1,-13,0,0,0,13,0
	.long    6,1,4,1,3,1,2,3,2,1,1
	.long    6,1,5,1,4,1,3,1,2,1,1
	.long    1,0,0,13,0,13,0,13,-1,-1,-1
	.long    0,-2,-12,-2,-10,-2,-8,-2,-6,-2,-4
	.long    -2,-2,-2,0,-3,-4,-3,-1,-4,-2,-5
	.long    -1,-7,0,-14,0,0,0,14,0,7,0
	.long    4,2,3,2,2,4,2,2,2,0,1
	.long    6,1,5,1,4,1,3,1,2,1,1
	.long    1,0,0,14,0,14,-1,0,-2,-13,-2
	.long    -11,-2,-9,-2,-7,-2,-5,-2,-3,-2,-1
	.long    -3,-6,-3,-3,-3,0,-4,-1,-5,0,-8
	.long    -1,-15,0,0,0,15,0,7,1,5,0
	.long    3,3,3,0,2,3,2,1,1,7,1
	.long    6,1,5,1,4,1,3,1,2,1,1
	.long    1,0,0,15,-2,-14,-2,-12,-2,-10,-2
	.long    -8,-2,-6,-2,-4,-2,-2,-2,0,-3,-5
	.long    -3,-2,-4,-4,-4,0,-6,-2,-8,0,-16
	.long    0,0,0,16,0,8,0,5,1,4,0
	.long    3,1,2,4,2,2,2,0,1,7,1
	.long    6,1,5,1,4,1,3,1,2,1,1
	.long    1,0

