##
## Quake for AMIGA
##
## d_scanPPC.s
##
## Define WOS for PowerOpen ABI, otherwise SVR4-ABI is used.
##

.set NOLR,1
.include        "macrosPPC.i"
.include	"quakedefPPC.i"

.macro	rsreset
.set	rscnt,0
.endm

.macro	rs
.set	\1,rscnt
.set	rscnt,rscnt+\2
.endm

#
# external references
#
	xrefv	cacheblock
	xrefv	d_sdivzorigin
	xrefv	d_sdivzstepu
	xrefv	d_sdivzstepv
	xrefv	d_tdivzorigin
	xrefv	d_tdivzstepu
	xrefv	d_tdivzstepv
	xrefv	d_ziorigin
	xrefv	d_zistepu
	xrefv	d_zistepv
	xrefv	sadjust
	xrefv	tadjust
	xrefv	sdivz
	xrefv	tdivz
	xrefv	bbextents
	xrefv	bbextentt
	xrefv	d_viewbuffer
	xrefv	screenwidth
	xrefv	cachewidth
	xrefv	d_zwidth
	xrefv	d_pzbuffer
	xrefv	sintable

	xrefa	d_subdiv16
	xrefa	cl
	xrefa	intsintable
	xrefa	sintable
	xrefa	vid
	xrefa	scr_vrect
	xrefa	r_refdef

	xrefv	INT2DBL_0
	xrefv	c0
	xrefv	c2
	xrefv	c65536
	xrefa	_ReciprocTable


#
# defines
#

# MUST match the #define in d_iface.h!
.set	CYCLE,128
.set	AMP2,3
.set	SPEED,20




###########################################################################
#
#       void D_WarpScreen (void)
#
#       water effect algorithm
#
###########################################################################

	funcdef	D_WarpScreen

	rsreset
	rs	int2dbl_tmp1,8
	rs	dbl2int_tmp1,8
	rs	rowptr,1024*4
	rs	column,1280*4

	init	0,rscnt+512,8,0
	stmw	r24,gb(r1)
	la	r4,local+rowptr(r1)		#r4 -> rowptr[1024]
	la	r5,local+column(r1)		#r5 -> column[1280]
	lf	f1,INT2DBL_0			#f1 = 0x4330000080000000
	stfd	f1,local+int2dbl_tmp1(r1)	#int2dbl_tmp1 = 0x43300000...

	lxa	r6,vid				#r6 -> vid
	lxa	r7,r_refdef			#r7 -> r_refdef
	lxa	r8,scr_vrect			#r8 -> scr_vrect
	lwz	r9,VRECT_WIDTH(r8)		#r9 = scr_vrect.width
	lwz	r10,VRECT_HEIGHT(r8)		#r10 = scr_vrect.height
	lwz	r11,REFDEF_VRECT+VRECT_X(r7)	#r11 = r_refdef.vrect.x
	lwz	r12,REFDEF_VRECT+VRECT_Y(r7)	#r12 = r_refdef.vrect.y
	lwz	r0,REFDEF_VRECT+VRECT_WIDTH(r7)
	int2dbl	f2,r0,r0,local+int2dbl_tmp1,f1	#f2 = (float)r_refdef.vrect.width
	lwz	r0,REFDEF_VRECT+VRECT_HEIGHT(r7)
	int2dbl f3,r0,r0,local+int2dbl_tmp1,f1	#f3 = (float)r_refdef.vrect.height
	lw      r31,screenwidth			#r31 = screenwidth
	ls      f4,cAMP2times2			#f4 = AMP2*2

#        w = r_refdef.vrect.width;
#        h = r_refdef.vrect.height;
#
#        wratio = w / (float)scr_vrect.width;
#        hratio = h / (float)scr_vrect.height;

	fadds	f5,f2,f4                #f5 = w + AMP2*2
	fadds	f6,f3,f4                #f6 = h + AMP2*2
	int2dbl f0,r9,r0,local+int2dbl_tmp1,f1
	fmuls	f5,f5,f0		#* (float)scr_vrect.width
	int2dbl	f0,r10,r0,local+int2dbl_tmp1,f1
	fmuls	f6,f6,f0		#* (float)scr_vrect.height
	fmuls	f2,f2,f2		#w*w
	fmuls	f3,f3,f3		#h*h
	fdivs	f2,f2,f5		#f2 = wratio*w/(w+AMP2*2)
	fdivs	f3,f3,f6		#f3 = hratio*h/(h+AMP2*2)
	mullw	r12,r12,r31		#r12 = r_refdef.vrect.y*screenwidth
	lw	r29,d_viewbuffer
	add	r12,r12,r29		#r12 = d_viewbuffer + r12
	lwz	r29,VID_ROWBYTES(r6)	#r29 = vid.rowbytes
	addi	r27,r9,AMP2*2
	addi	r28,r10,AMP2*2

#        for (v=0 ; v<scr_vrect.height+AMP2*2 ; v++)
#        {
#                rowptr[v] = d_viewbuffer + (r_refdef.vrect.y * screenwidth) +
#                                 (screenwidth * (int)((float)v * hratio * h / (h + AMP2 * 2)));
#        }

	li	r26,0			#v = 0
	subi	r25,r4,4		#r25 -> rowptr[0] - 4
.wsloop:
	int2dbl	f7,r26,r0,local+int2dbl_tmp1,f1 #f7 = (float)v
	fmuls	f7,f7,f3                #(float)v*hratio*h/(h+AMP2*2)
	fctiwz	f0,f7
	stfd	f0,local+dbl2int_tmp1(r1)
	lwz	r24,local+dbl2int_tmp1+4(r1) #r24 = (int)f7
	mullw	r24,r24,r31
	add	r24,r24,r12
	addi	r26,r26,1               #v++
	stwu	r24,4(r25)              #rowptr[v] = r24
	cmpw	r26,r28
	blt	.wsloop

#        for (u=0 # u<scr_vrect.width+AMP2*2 ; u++)
#        {
#                column[u] = r_refdef.vrect.x +
#                                (int)((float)u * wratio * w / (w + AMP2 * 2));
#        }
	li	r26,0                   #u = 0
	subi	r25,r5,4                #r25 -> column[0]
.wsloop2:
	int2dbl	f7,r26,r0,local+int2dbl_tmp1,f1 #f7 = (float)u
	fmuls	f7,f7,f2		#(float)u*wratio*w/(w+AMP2*2)
	fctiwz	f0,f7
	stfd	f0,local+dbl2int_tmp1(r1)
	lwz	r24,local+dbl2int_tmp1+4(r1) #r24 = (int)f7
	add	r24,r24,r11
	addi	r26,r26,1		#u++
	stwu	r24,4(r25)		#column[u] = r24
	cmpw	r26,r27
	blt	.wsloop2

#        turb = intsintable + ((int)(cl.time*SPEED)&(CYCLE-1));
#        dest = vid.buffer + scr_vrect.y * vid.rowbytes + scr_vrect.x;
#        for (v=0 ; v<scr_vrect.height ; v++, dest += vid.rowbytes)
#        {
#                col = &column[turb[v]];
#                row = &rowptr[v];
#                for (u=0 ; u<scr_vrect.width ; u+=4)
#                {
#                        dest[u+0] = row[turb[u+0]][col[u+0]];
#                        dest[u+1] = row[turb[u+1]][col[u+1]];
#                        dest[u+2] = row[turb[u+2]][col[u+2]];
#                        dest[u+3] = row[turb[u+3]][col[u+3]];
#                }
#        }

	srawi	r9,r9,2
	lxa	r28,cl
	lfd	f7,CL_TIME(r28)
	ls	f0,cSPEED
	fmuls	f7,f7,f0
	fctiwz	f0,f7
	stfd	f0,local+dbl2int_tmp1(r1)
	lwz	r27,local+dbl2int_tmp1+4(r1)
	andi.	r27,r27,CYCLE-1
	slwi	r27,r27,2
	lxa	r26,intsintable
	add	r27,r27,r26		#r27 = turb = sintable + ...

	lwz	r28,VRECT_Y(r8)
	mullw	r28,r28,r29
	lwz	r26,VRECT_X(r8)
	add	r26,r26,r28
	lwz	r28,VID_BUFFER(r6)
	add	r26,r26,r28		#r26 = dest = vid.buffer + ...

	li	r6,0
.wsloop3:
	slwi	r0,r6,2
	mtctr	r9
	subi	r7,r27,4		#r7 -> turb[u] - 4
	lwzx	r8,r27,r0		#r8 = turb[v]
	subi	r11,r26,1		#r11 -> dest[u] - 1
	slwi	r8,r8,2
	add	r12,r5,r8		#r12 = col = &column[turb[v]]
	subi	r12,r12,4
	add	r31,r4,r0		#r31 = row = &rowptr[v]
.wsloop4:
	lwzu	r0,4(r7)		#r0 = turb[u+0]
	slwi	r0,r0,2
	lwzx	r30,r31,r0		#r30 = row[turb[u+0]]
	lwzu	r0,4(r12)		#r0 = col[u+0]
	lbzx	r0,r30,r0		#r0 = row[turb[u+0][col[u+0]]
	stbu	r0,1(r11)		#dest[u+0] = r0
	lwzu	r0,4(r7)		#r0 = turb[u+0]
	slwi	r0,r0,2
	lwzx	r30,r31,r0		#r30 = row[turb[u+0]]
	lwzu	r0,4(r12)		#r0 = col[u+0]
	lbzx	r0,r30,r0		#r0 = row[turb[u+0][col[u+0]]
	stbu	r0,1(r11)		#dest[u+0] = r0
	lwzu	r0,4(r7)		#r0 = turb[u+0]
	slwi	r0,r0,2
	lwzx	r30,r31,r0		#r30 = row[turb[u+0]]
	lwzu	r0,4(r12)		#r0 = col[u+0]
	lbzx	r0,r30,r0		#r0 = row[turb[u+0][col[u+0]]
	stbu	r0,1(r11)		#dest[u+0] = r0
	lwzu	r0,4(r7)		#r0 = turb[u+0]
	slwi	r0,r0,2
	lwzx	r30,r31,r0		#r30 = row[turb[u+0]]
	lwzu	r0,4(r12)		#r0 = col[u+0]
	lbzx	r0,r30,r0		#r0 = row[turb[u+0][col[u+0]]
	stbu	r0,1(r11)		#dest[u+0] = r0
	bdnz	.wsloop4
	add	r26,r26,r29		#dest += vid.rowbytes
	addi	r6,r6,1
	cmpw	r6,r10
	blt	.wsloop3

	lmw	r24,gb(r1)
	exit

	funcend	D_WarpScreen




###########################################################################
#
#       void Turbulent8 (espan_t *pspan)
#
#       standard scan drawing function for animated textures
#       Note: The function D_DrawTurbulent8Span was inlined into this
#       function, because it's never used anywhere else.
#
###########################################################################

	funcdef	Turbulent8

	init	0,16,7,16		# 16 local bytes for int2dbl/dbl2int
	stmw	r25,gb(r1)
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

	lxa	r26,_ReciprocTable
	lw      r27,cacheblock          #r27 = r_turb_pbase
	lxa     r5,cl
	lfd     f1,CL_TIME(r5)
	ls      f2,cSPEED
	fmuls   f1,f1,f2
	fctiwz  f0,f1
	stfd    f0,local+8(r1)
	lwz     r4,local+12(r1)
	andi.   r4,r4,CYCLE-1
	slwi    r4,r4,2
	lxa     r5,sintable
	add     r4,r4,r5                #r4 = r_turb_turb = sintable + ...
	ls      f1,d_sdivzstepu         #f1 = d_sdivzstepu
	ls      f2,d_sdivzstepv         #f2 = d_sdivzstepv
	ls      f3,d_sdivzorigin        #f3 = d_sdivzorigin
	ls      f4,d_tdivzstepu         #f4 = d_tdivzstepu
	ls      f5,d_tdivzstepv         #f5 = d_tdivzstepv
	ls      f6,d_tdivzorigin        #f6 = d_tdivzorigin
	ls      f7,d_zistepu            #f7 = d_zistepu
	ls      f8,d_zistepv            #f8 = d_zistepv
	ls      f9,d_ziorigin           #f9 = d_ziorigin
	lf      f10,INT2DBL_0           #for int2dbl_setup
	stfd	f10,local(r1)
	lw      r6,sadjust
	int2dbl f11,r6,r0,local,f10	#f11 = sadjust
	lw      r6,tadjust
	int2dbl f12,r6,r0,local,f10	#f12 = tadjust
	lw      r6,bbextents
	int2dbl f13,r6,r0,local,f10	#f13 = bbextents
	lw      r6,bbextentt
	int2dbl f14,r6,r0,local,f10	#f14 = bbextentt
	lw      r6,d_viewbuffer         #r6 = d_viewbuffer
	lw      r7,screenwidth          #r7 = screenwidth
	ls      f15,c65536              #f15 = 65536
	ls      f16,c16	                #f16 = 16
	ls      f25,c0                  #f25 = 0
	ls      f28,c2                  #f28 = 2
	fmuls   f17,f1,f16              #f17 = sdivz16stepu
	fmuls   f18,f4,f16              #f18 = tdivz16stepu
	fmuls   f19,f7,f16              #f19 = zi16stepu
	subi    r3,r3,4                 #prepare for postincrement

######  First loop. In every iteration one complete span is drawn

#        pbase = (unsigned char *)cacheblock;
#
#        sdivz16stepu = d_sdivzstepu * 16;
#        tdivz16stepu = d_tdivzstepu * 16;
#        zi16stepu = d_zistepu * 16;
#
#        do
#        {
#                r_turb_pdest = (unsigned char *)((byte *)d_viewbuffer +
#                                (screenwidth * pspan->v) + pspan->u);
#
#                count = pspan->count;
#
#        // calculate the initial s/z, t/z, 1/z, s, and t and clamp
#                du = (float)pspan->u;
#                dv = (float)pspan->v;
#
#                sdivz = d_sdivzorigin + dv*d_sdivzstepv + du*d_sdivzstepu;
#                tdivz = d_tdivzorigin + dv*d_tdivzstepv + du*d_tdivzstepu;
#                zi = d_ziorigin + dv*d_zistepv + du*d_zistepu;
#                z = (float)0x10000 / zi;        // prescale to 16.16 fixed-point
#

.t8loop:
	lwzu    r8,4(r3)
	int2dbl f20,r8,r0,local,f10	#f20 = du
	lwzu    r9,4(r3)
	int2dbl f21,r9,r0,local,f10	#f21 = dv
	fmadds  f24,f21,f8,f9
	fmadds  f22,f21,f2,f3
	fmadds  f23,f21,f5,f6
	fmadds  f26,f20,f7,f24          #f26 = zi
	fmadds  f22,f20,f1,f22          #f22 = sdivz
	fmadds  f23,f20,f4,f23          #f23 = tdivz
	fmuls   f0,f26,f26
	frsqrte f0,f0
	mullw   r10,r9,r7               #r10 = pspan->v * screenwidth
	fnmsubs f29,f0,f26,f28
	add     r10,r10,r8              #r10 = r10 + pspan->u
	fmuls   f0,f0,f29
	add     r11,r6,r10              #r11 = pdest
	fnmsubs f29,f0,f26,f28
	subi    r11,r11,1               #prepare for postincrement
	fmuls   f0,f0,f29
	lwzu    r12,4(r3)               #r12 = count
	fmuls   f24,f0,f15
	fmadds  f20,f22,f24,f11         #f20 = s
	fsubs   f0,f20,f13
	fsel    f20,f0,f13,f20          #if (s>bb...) s = bbextents
	fsel    f20,f20,f20,f25         #if (s<0) s = 0
	fctiwz  f0,f20
	stfd    f0,local+8(r1)
	lwz     r8,local+12(r1)         #r8 = (int)s
	fmadds  f21,f23,f24,f12         #f21 = t
	fsubs   f0,f21,f14
	fsel    f21,f0,f14,f21          #if (t>bb...) t = bbextentt
	fsel    f21,f21,f21,f25         #if (t<0) t = 0
	fctiwz  f0,f21
	stfd    f0,local+8(r1)
	lwz     r9,local+12(r1)         #r9 = (int)t


######  Second loop. In every iteration one part of the whole span is drawn

#                do
#                {
#                // calculate s and t at the far end of the span
#                        if (count >= 16)
#                                r_turb_spancount = 16;
#                        else
#                                r_turb_spancount = count;
#
#                        count -= r_turb_spancount;
#
#                        if (count)
#                        {

.t8loop2:
	cmpwi   r12,16
	bgt     .t8cont
	mtctr   r12
	subi    r10,r12,1
	int2dbl f21,r10,r0,local,f10	#spancountminus1 = (float)...
	li      r12,0                   #r12 = count -= spancount
	fmadds  f26,f21,f7,f26          #zi += d_zistepu * ...
	b       .t8finalpart
.t8cont:
	fadds   f26,f26,f19             #zi += zi16stepu
	li      r10,16                  #r10 = spancount = 16
	fmuls   f0,f26,f26
	subf    r12,r10,r12             #r12 = count -= spancount
	frsqrte f0,f0
	mtctr   r10

######  Evaluation of the values for the inner loop. This version is used for
######  span size = 16

#                        // calculate s/z, t/z, zi->fixed s and t at far end of span,
#                        // calculate s and t steps across span by shifting
#                                sdivz += sdivz16stepu;
#                                tdivz += tdivz16stepu;
#                                zi += zi16stepu;
#                                z = (float)0x10000 / zi;        // prescale to 16.16 fixed-point
#                                snext = (int)(sdivz * z) + sadjust;
#                                if (snext > bbextents)
#                                        snext = bbextents;
#                                else if (snext < 16)
#                                        snext = 16;      // prevent round-off error on <0 steps from
#                                                                //  from causing overstepping & running off the
#                                                                //  edge of the texture
#                                tnext = (int)(tdivz * z) + tadjust;
#                                if (tnext > bbextentt)
#                                        tnext = bbextentt;
#                                else if (tnext < 16)
#                                        tnext = 16;      // guard against round-off error on <0 steps
#                                r_turb_sstep = (snext - r_turb_s) >> 4;
#                                r_turb_tstep = (tnext - r_turb_t) >> 4;
#                        }

	fnmsubs f20,f0,f26,f28
	fadds   f22,f22,f17             #sdivz += sdivz16stepu
	fmuls   f0,f0,f20
	fadds   f23,f23,f18             #tdivz += tdivz16stepu
	fnmsubs f20,f0,f26,f28
	fmuls   f0,f0,f20
	fmuls   f24,f0,f15
	fmadds  f20,f22,f24,f11         #f20 = snext
	fsubs   f0,f20,f13
	fsel    f20,f0,f13,f20          #if (snext>bb...) snext = bbextents
	fsubs   f0,f20,f16
	fsel    f20,f0,f20,f16          #if (snext<16) snext = 16
	fctiwz  f0,f20
	stfd    f0,local+8(r1)
	lwz     r31,local+12(r1)        #r31 = (int)snext
	fmadds  f21,f23,f24,f12         #f21 = tnext
	fsubs   f0,f21,f14
	fsel    f21,f0,f14,f21          #if (tnext>bb...) tnext = bbextentt
	subf    r29,r8,r31
	fsubs   f0,f21,f16
	srawi   r29,r29,4               #r29 = sstep = (snext - s) >> 4
	fsel    f21,f0,f21,f16          #if (tnext<16) tnext = 16
	fctiwz  f0,f21
	stfd    f0,local+8(r1)
	lwz     r30,local+12(r1)        #r30 = (int)tnext
	subf    r28,r9,r30
	srawi   r28,r28,4               #r28 = tstep = (tnext - t) >> 4
	b       .t8mainloop

######  Evaluation of the values for the inner loop. This version is used for
######  span size < 16

######  The original algorithm has two ugly divisions at the end of this part.
######  These are removed by the following optimization:
######  First, the divisors 1,2 and 4 are handled specially to gain speed. The
######  other divisors are handled using a reciprocal table.

#                        // calculate s/z, t/z, zi->fixed s and t at last pixel in span (so
#                        // can't step off polygon), clamp, calculate s and t steps across
#                        // span by division, biasing steps low so we don't run off the
#                        // texture
#                                spancountminus1 = (float)(r_turb_spancount - 1);
#                                sdivz += d_sdivzstepu * spancountminus1;
#                                tdivz += d_tdivzstepu * spancountminus1;
#                                zi += d_zistepu * spancountminus1;
#                                z = (float)0x10000 / zi;        // prescale to 16.16 fixed-point
#                                snext = (int)(sdivz * z) + sadjust;
#                                if (snext > bbextents)
#                                        snext = bbextents;
#                                else if (snext < 16)
#                                        snext = 16;      // prevent round-off error on <0 steps from
#                                                                //  from causing overstepping & running off the
#                                                                //  edge of the texture
#
#                                tnext = (int)(tdivz * z) + tadjust;
#                                if (tnext > bbextentt)
#                                        tnext = bbextentt;
#                                else if (tnext < 16)
#                                        tnext = 16;      // guard against round-off error on <0 steps
#
#                                if (r_turb_spancount > 1)
#                                {
#                                        r_turb_sstep = (snext - r_turb_s) / (spancount - 1);
#                                        r_turb_tstep = (tnext - r_turb_t) / (spancount - 1);
#                                }
#                        }

.t8finalpart:
	fmuls   f0,f26,f26
	fmadds  f22,f21,f1,f22          #sdivz += d_sdivzstepu * ...
	frsqrte f0,f0
	fmadds  f23,f21,f4,f23          #tdivs += d_tdivzstepu * ...
	fnmsubs f20,f0,f26,f28
	cmplwi  r10,5
	fmuls   f0,f0,f20
	fnmsubs f20,f0,f26,f28
	fmuls   f0,f0,f20
	fmuls   f24,f0,f15
	fmadds  f20,f22,f24,f11         #f27 = snext
	fsubs   f0,f20,f13
	fsel    f20,f0,f13,f20          #if (snext>bb...) snext = bbextents
	fsubs   f0,f20,f16
	fsel    f20,f0,f20,f16          #if (snext<16) snext = 16
	fctiwz  f0,f20
	stfd    f0,local+8(r1)
	lwz     r31,local+12(r1)        #r31 = (int)snext
	fmadds  f21,f23,f24,f12         #f21 = tnext
	fsubs   f0,f21,f14
	fsel    f21,f0,f14,f21          #if (tnext>bb...) tnext = bbextentt
	subf    r29,r8,r31
	fsubs   f0,f21,f16
	fsel    f21,f0,f21,f16          #if (tnext<16) tnext = 16
	fctiwz  f0,f21
	stfd    f0,local+8(r1)
	lwz     r30,local+12(r1)        #r30 = (int)tnext
	subf    r28,r9,r30
	blt     .t8special
.t8qdiv:
	slwi    r0,r10,2
	lwzx    r0,r26,r0
	mulhw   r29,r29,r0
	mulhw   r28,r28,r0
	b       .t8mainloop
.t8special:
	cmplwi  r10,1
	ble     .t8mainloop
	cmplwi  r10,3
	beq     .t8qdiv
	blt     .t8spec_2
	srawi   r29,r29,2
	srawi   r28,r28,2
	b       .t8mainloop
.t8spec_2:
	srawi   r29,r29,1
	srawi   r28,r28,1

######  D_DrawTurbulent8Span (inlined)
######  Main drawing loop.

#        do
#        {
#                sturb = ((r_turb_s + r_turb_turb[(r_turb_t>>16)&(CYCLE-1)])>>16)&63;
#                tturb = ((r_turb_t + r_turb_turb[(r_turb_s>>16)&(CYCLE-1)])>>16)&63;
#                *r_turb_pdest++ = *(r_turb_pbase + (tturb<<6) + sturb);
#                r_turb_s += r_turb_sstep;
#                r_turb_t += r_turb_tstep;
#        } while (--r_turb_spancount > 0);


.t8mainloop:
	andis.  r8,r8,CYCLE-1
	andis.  r9,r9,CYCLE-1
.t8draw:
	rlwinm  r0,r9,18,23,29		#implicit: CYCLE = 128
	rlwinm  r25,r8,18,23,29		#implicit: CYCLE = 128
	lwzx    r10,r4,r0
	add     r10,r10,r8
	lwzx    r25,r4,r25
	add     r8,r8,r29
	add     r25,r25,r9
	extrwi  r10,r10,6,10
	rlwinm  r25,r25,22,20,25
	add     r10,r10,r25
	add     r9,r9,r28
	lbzx    r0,r27,r10
	stbu    r0,1(r11)
	bdnz    .t8draw

	mr      r8,r31
	mr      r9,r30
	mr.	r12,r12
	bgt     .t8loop2
	lwz     r3,4(r3)                #while (...)
	mr.	r3,r3
	subi    r3,r3,4
	bne     .t8loop

	lmw	r25,gb(r1)
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
	exit

	funcend	Turbulent8




###########################################################################
#
#       void D_DrawSpans8 (espan_t *pspan)
#
#       standard scan drawing function (8 pixel subdivision)
#
###########################################################################

	funcdef	D_DrawSpans8

	lxa     r4,d_subdiv16		# subdiv16? then call DrawSpans16!
	lfs     f0,CVAR_VALUE(r4)
	ls      f1,c0
	fcmpo   0,f0,f1
.ifdef	WOS
	bne     _D_DrawSpans16
.else
	bne	D_DrawSpans16
.endif

	init	0,16,6,16		# 16 local bytes for int2dbl/dbl2int
	stmw	r26,gb(r1)
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

	lxa     r26,_ReciprocTable
	lw      r27,cacheblock          #r27 = pbase
	lw      r4,cachewidth           #r4 = cachewidth
	ls      f1,d_sdivzstepu         #f1 = d_sdivzstepu
	ls      f2,d_sdivzstepv         #f2 = d_sdivzstepv
	ls      f3,d_sdivzorigin        #f3 = d_sdivzorigin
	ls      f4,d_tdivzstepu         #f4 = d_tdivzstepu
	ls      f5,d_tdivzstepv         #f5 = d_tdivzstepv
	ls      f6,d_tdivzorigin        #f6 = d_tdivzorigin
	ls      f7,d_zistepu            #f7 = d_zistepu
	ls      f8,d_zistepv            #f8 = d_zistepv
	ls      f9,d_ziorigin           #f9 = d_ziorigin
	lf      f10,INT2DBL_0           #for int2dbl_setup
	stfd	f10,local(r1)
	lw      r6,sadjust
	int2dbl f11,r6,r0,local,f10	#f11 = sadjust
	lw      r6,tadjust
	int2dbl f12,r6,r0,local,f10	#f12 = tadjust
	lw      r6,bbextents
	int2dbl f13,r6,r0,local,f10	#f13 = bbextents
	lw      r6,bbextentt
	int2dbl f14,r6,r0,local,f10	#f14 = bbextentt
	lw      r6,d_viewbuffer         #r6 = d_viewbuffer
	lw      r7,screenwidth          #r7 = screenwidth
	ls      f15,c65536              #f15 = 65536
	ls      f16,c8			#f16 = 8
	ls      f25,c0                  #f25 = 0
	ls      f28,c2                  #f28 = 2
	fmuls   f17,f1,f16              #f17 = sdivz8stepu
	fmuls   f18,f4,f16              #f18 = tdivz8stepu
	fmuls   f19,f7,f16              #f19 = zi8stepu
	subi    r3,r3,4                 #prepare for postincrement

######  First loop. In every iteration one complete span is drawn

#        pbase = (unsigned char *)cacheblock;
#
#        sdivz8stepu = d_sdivzstepu * 8;
#        tdivz8stepu = d_tdivzstepu * 8;
#        zi8stepu = d_zistepu * 8;
#
#        do
#        {
#                pdest = (unsigned char *)((byte *)d_viewbuffer +
#                                (screenwidth * pspan->v) + pspan->u);
#
#                count = pspan->count;
#
#        // calculate the initial s/z, t/z, 1/z, s, and t and clamp
#                du = (float)pspan->u;
#                dv = (float)pspan->v;
#
#                sdivz = d_sdivzorigin + dv*d_sdivzstepv + du*d_sdivzstepu;
#                tdivz = d_tdivzorigin + dv*d_tdivzstepv + du*d_tdivzstepu;
#                zi = d_ziorigin + dv*d_zistepv + du*d_zistepu;
#                z = (float)0x10000 / zi;        // prescale to 16.16 fixed-point
#

.d8loop:
	lwzu    r8,4(r3)
	int2dbl f20,r8,r0,local,f10	#f20 = du
	lwzu    r9,4(r3)
	int2dbl f21,r9,r0,local,f10	#f21 = dv
	fmadds  f24,f21,f8,f9
	fmadds  f22,f21,f2,f3
	fmadds  f23,f21,f5,f6
	fmadds  f26,f20,f7,f24          #f26 = zi
	fmadds  f22,f20,f1,f22          #f22 = sdivz
	fmadds  f23,f20,f4,f23          #f23 = tdivz
	fmuls   f0,f26,f26
	frsqrte f0,f0
	mullw   r10,r9,r7               #r10 = pspan->v * screenwidth
	fnmsubs f29,f0,f26,f28
	add     r10,r10,r8              #r10 = r10 + pspan->u
	fmuls   f0,f0,f29
	add     r11,r6,r10              #r11 = pdest
	fnmsubs f29,f0,f26,f28
	subi    r11,r11,1               #prepare for postincrement
	fmuls   f0,f0,f29
	lwzu    r12,4(r3)               #r12 = count
	fmuls   f24,f0,f15
	fmadds  f20,f22,f24,f11         #f20 = s
	fsubs   f0,f20,f13
	fsel    f20,f0,f13,f20          #if (s>bb...) s = bbextents
	fsel    f20,f20,f20,f25         #if (s<0) s = 0
	fctiwz  f0,f20
	stfd    f0,local+8(r1)
	lwz     r8,local+12(r1)         #r8 = (int)s
	fmadds  f21,f23,f24,f12         #f21 = t
	fsubs   f0,f21,f14
	fsel    f21,f0,f14,f21          #if (t>bb...) t = bbextentt
	fsel    f21,f21,f21,f25         #if (t<0) t = 0
	fctiwz  f0,f21
	stfd    f0,local+8(r1)
	lwz     r9,local+12(r1)         #r9 = (int)t
	li      r0,1
	dcbtst  r11,r0


######  Second loop. In every iteration one part of the whole span is drawn

#                do
#                {
#                // calculate s and t at the far end of the span
#                        if (count >= 8)
#                                spancount = 8;
#                        else
#                                spancount = count;
#
#                        count -= spancount;
#
#                        if (count)
#                        {

.d8loop2:
	cmpwi   r12,8
	bgt     .d8cont
	mtctr   r12
	subi    r10,r12,1
	int2dbl f21,r10,r0,local,f10	#spancountminus1 = (float)...
	li      r12,0                   #r12 = count -= spancount
	fmadds  f26,f21,f7,f26          #zi += d_zistepu # ...
	b       .d8finalpart
.d8cont:
	fadds   f26,f26,f19             #zi += zi8stepu
	li      r10,8                   #r10 = spancount = 8
	fmuls   f0,f26,f26
	subf    r12,r10,r12             #r12 = count -= spancount
	frsqrte f0,f0
	mtctr   r10

######  Evaluation of the values for the inner loop. This version is used for
######  span size = 8

#                        // calculate s/z, t/z, zi->fixed s and t at far end of span,
#                        // calculate s and t steps across span by shifting
#                                sdivz += sdivz8stepu;
#                                tdivz += tdivz8stepu;
#                                zi += zi8stepu;
#                                z = (float)0x10000 / zi;        // prescale to 16.16 fixed-point
#                                snext = (int)(sdivz * z) + sadjust;
#                                if (snext > bbextents)
#                                        snext = bbextents;
#                                else if (snext < 8)
#                                        snext = 8;      // prevent round-off error on <0 steps from
#                                                                //  from causing overstepping & running off the
#                                                                //  edge of the texture
#                                tnext = (int)(tdivz * z) + tadjust;
#                                if (tnext > bbextentt)
#                                        tnext = bbextentt;
#                                else if (tnext < 8)
#                                        tnext = 8;      // guard against round-off error on <0 steps
#                                sstep = (snext - s) >> 3;
#                                tstep = (tnext - t) >> 3;
#                        }

	fnmsubs f20,f0,f26,f28
	fadds   f22,f22,f17             #sdivz += sdivz8stepu
	fmuls   f0,f0,f20
	fadds   f23,f23,f18             #tdivz += tdivz8stepu
	fnmsubs f20,f0,f26,f28
	fmuls   f0,f0,f20
	fmuls   f24,f0,f15
	fmadds  f20,f22,f24,f11         #f20 = snext
	fsubs   f0,f20,f13
	fsel    f20,f0,f13,f20          #if (snext>bb...) snext = bbextents
	fsubs   f0,f20,f16
	fsel    f20,f0,f20,f16          #if (snext<8) snext = 8
	fctiwz  f0,f20
	stfd    f0,local+8(r1)
	lwz     r31,local+12(r1)        #r31 = (int)snext
	fmadds  f21,f23,f24,f12         #f21 = tnext
	fsubs   f0,f21,f14
	fsel    f21,f0,f14,f21          #if (tnext>bb...) tnext = bbextentt
	subf    r29,r8,r31
	fsubs   f0,f21,f16
	srawi   r29,r29,3               #r29 = sstep = (snext - s) >> 3
	fsel    f21,f0,f21,f16          #if (tnext<8) tnext = 8
	fctiwz  f0,f21
	stfd    f0,local+8(r1)
	lwz     r30,local+12(r1)        #r30 = (int)tnext
	subf    r28,r9,r30
	srawi   r28,r28,3               #r28 = tstep = (tnext - t) >> 3
	b       .d8mainloop

######  Evaluation of the values for the inner loop. This version is used for
######  span size < 8

######  The original algorithm has two ugly divisions at the end of this part.
######  These are removed by the following optimization:
######  First, the divisors 1,2 and 4 are handled specially to gain speed. The
######  other divisors are handled using a reciprocal table.

#                        // calculate s/z, t/z, zi->fixed s and t at last pixel in span (so
#                        // can't step off polygon), clamp, calculate s and t steps across
#                        // span by division, biasing steps low so we don't run off the
#                        // texture
#                                spancountminus1 = (float)(spancount - 1);
#                                sdivz += d_sdivzstepu * spancountminus1;
#                                tdivz += d_tdivzstepu * spancountminus1;
#                                zi += d_zistepu * spancountminus1;
#                                z = (float)0x10000 / zi;        // prescale to 16.16 fixed-point
#                                snext = (int)(sdivz * z) + sadjust;
#                                if (snext > bbextents)
#                                        snext = bbextents;
#                                else if (snext < 8)
#                                        snext = 8;      // prevent round-off error on <0 steps from
#                                                                //  from causing overstepping & running off the
#                                                                //  edge of the texture
#
#                                tnext = (int)(tdivz * z) + tadjust;
#                                if (tnext > bbextentt)
#                                        tnext = bbextentt;
#                                else if (tnext < 8)
#                                        tnext = 8;      // guard against round-off error on <0 steps
#
#                                if (spancount > 1)
#                                {
#                                        sstep = (snext - s) / (spancount - 1);
#                                        tstep = (tnext - t) / (spancount - 1);
#                                }
#                        }

.d8finalpart:
	fmuls   f0,f26,f26
	fmadds  f22,f21,f1,f22          #sdivz += d_sdivzstepu * ...
	frsqrte f0,f0
	fmadds  f23,f21,f4,f23          #tdivs += d_tdivzstepu * ...
	fnmsubs f20,f0,f26,f28
	cmplwi  r10,5
	fmuls   f0,f0,f20
	fnmsubs f20,f0,f26,f28
	fmuls   f0,f0,f20
	fmuls   f24,f0,f15
	fmadds  f20,f22,f24,f11         #f27 = snext
	fsubs   f0,f20,f13
	fsel    f20,f0,f13,f20          #if (snext>bb...) snext = bbextents
	fsubs   f0,f20,f16
	fsel    f20,f0,f20,f16          #if (snext<8) snext = 8
	fctiwz  f0,f20
	stfd    f0,local+8(r1)
	lwz     r31,local+12(r1)        #r31 = (int)snext
	fmadds  f21,f23,f24,f12         #f21 = tnext
	fsubs   f0,f21,f14
	fsel    f21,f0,f14,f21          #if (tnext>bb...) tnext = bbextentt
	subf    r29,r8,r31
	fsubs   f0,f21,f16
	fsel    f21,f0,f21,f16          #if (tnext<8) tnext = 8
	fctiwz  f0,f21
	stfd    f0,local+8(r1)
	lwz     r30,local+12(r1)        #r30 = (int)tnext
	subf    r28,r9,r30
	blt     .d8special
.d8qdiv:
	slwi    r0,r10,2
	lwzx    r0,r26,r0
	mulhw   r29,r29,r0
	mulhw   r28,r28,r0
	b       .d8mainloop
.d8special:
	cmplwi  r10,1
	ble     .d8mainloop
	cmplwi  r10,3
	beq     .d8qdiv
	blt     .d8spec_2
	srawi   r29,r29,2
	srawi   r28,r28,2
	b       .d8mainloop
.d8spec_2:
	srawi   r29,r29,1
	srawi   r28,r28,1

######  Main drawing loop. Here lies the speed.

#                        do
#                        {
#                                *pdest++ = *(pbase + (s >> 16) + (t >> 16) * cachewidth);
#                                s += sstep;
#                                t += tstep;
#                        } while (--spancount > 0);

.d8mainloop:
	srawi   r0,r9,16
	srawi   r10,r8,16
	mullw   r0,r0,r4
	add     r9,r9,r28
	add     r0,r0,r10
	add     r8,r8,r29
	lbzx    r0,r27,r0
	stbu    r0,1(r11)
	bdnz    .d8mainloop
	mr      r8,r31
	mr      r9,r30
	mr.	r12,r12
	bgt     .d8loop2
	lwz     r3,4(r3)                #while (...)
	mr.	r3,r3
	subi    r3,r3,4
	bne     .d8loop

	lmw	r26,gb(r1)
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
	exit

	funcend	D_DrawSpans8




###########################################################################
#
#       void D_DrawSpans16 (espan_t *pspan)
#
#       standard scan drawing function (16 pixel subdivision)
#
###########################################################################

	funcdef	D_DrawSpans16

	init	0,16,6,16		# 16 local bytes for int2dbl/dbl2int
	stmw	r26,gb(r1)
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

	lxa     r26,_ReciprocTable
	lw      r27,cacheblock          #r27 = pbase
	lw      r4,cachewidth           #r4 = cachewidth
	ls      f1,d_sdivzstepu         #f1 = d_sdivzstepu
	ls      f2,d_sdivzstepv         #f2 = d_sdivzstepv
	ls      f3,d_sdivzorigin        #f3 = d_sdivzorigin
	ls      f4,d_tdivzstepu         #f4 = d_tdivzstepu
	ls      f5,d_tdivzstepv         #f5 = d_tdivzstepv
	ls      f6,d_tdivzorigin        #f6 = d_tdivzorigin
	ls      f7,d_zistepu            #f7 = d_zistepu
	ls      f8,d_zistepv            #f8 = d_zistepv
	ls      f9,d_ziorigin           #f9 = d_ziorigin
	lf      f10,INT2DBL_0           #for int2dbl_setup
	stfd	f10,local(r1)
	lw      r6,sadjust
	int2dbl f11,r6,r0,local,f10	#f11 = sadjust
	lw      r6,tadjust
	int2dbl f12,r6,r0,local,f10	#f12 = tadjust
	lw      r6,bbextents
	int2dbl f13,r6,r0,local,f10	#f13 = bbextents
	lw      r6,bbextentt
	int2dbl f14,r6,r0,local,f10	#f14 = bbextentt
	lw      r6,d_viewbuffer         #r6 = d_viewbuffer
	lw      r7,screenwidth          #r7 = screenwidth
	ls      f15,c65536              #f15 = 65536
	ls      f16,c16                 #f16 = 16
	ls      f25,c0                  #f25 = 0
	ls      f28,c2                  #f28 = 2
	fmuls   f17,f1,f16              #f17 = sdivz16stepu
	fmuls   f18,f4,f16              #f18 = tdivz16stepu
	fmuls   f19,f7,f16              #f19 = zi16stepu
	subi    r3,r3,4                 #prepare for postincrement

######  First loop. In every iteration one complete span is drawn

#        pbase = (unsigned char *)cacheblock;
#
#        sdivz16stepu = d_sdivzstepu * 16;
#        tdivz16stepu = d_tdivzstepu * 16;
#        zi16stepu = d_zistepu * 16;
#
#        do
#        {
#                pdest = (unsigned char *)((byte *)d_viewbuffer +
#                                (screenwidth * pspan->v) + pspan->u);
#
#                count = pspan->count;
#
#        // calculate the initial s/z, t/z, 1/z, s, and t and clamp
#                du = (float)pspan->u;
#                dv = (float)pspan->v;
#
#                sdivz = d_sdivzorigin + dv*d_sdivzstepv + du*d_sdivzstepu;
#                tdivz = d_tdivzorigin + dv*d_tdivzstepv + du*d_tdivzstepu;
#                zi = d_ziorigin + dv*d_zistepv + du*d_zistepu;
#                z = (float)0x10000 / zi;        // prescale to 16.16 fixed-point
#

.d16loop:
	lwzu    r8,4(r3)
	int2dbl f20,r8,r0,local,f10	#f20 = du
	lwzu    r9,4(r3)
	int2dbl f21,r9,r0,local,f10	#f21 = dv
	fmadds  f24,f21,f8,f9
	fmadds  f22,f21,f2,f3
	fmadds  f23,f21,f5,f6
	fmadds  f26,f20,f7,f24          #f26 = zi
	fmadds  f22,f20,f1,f22          #f22 = sdivz
	fmadds  f23,f20,f4,f23          #f23 = tdivz
	fmuls   f0,f26,f26
	frsqrte f0,f0
	mullw   r10,r9,r7               #r10 = pspan->v * screenwidth
	fnmsubs f29,f0,f26,f28
	add     r10,r10,r8              #r10 = r10 + pspan->u
	fmuls   f0,f0,f29
	add     r11,r6,r10              #r11 = pdest
	fnmsubs f29,f0,f26,f28
	subi    r11,r11,1               #prepare for postincrement
	fmuls   f0,f0,f29
	lwzu    r12,4(r3)               #r12 = count
	fmuls   f24,f0,f15
	fmadds  f20,f22,f24,f11         #f20 = s
	fsubs   f0,f20,f13
	fsel    f20,f0,f13,f20          #if (s>bb...) s = bbextents
	fsel    f20,f20,f20,f25         #if (s<0) s = 0
	fctiwz  f0,f20
	stfd    f0,local+8(r1)
	lwz     r8,local+12(r1)         #r8 = (int)s
	fmadds  f21,f23,f24,f12         #f21 = t
	fsubs   f0,f21,f14
	fsel    f21,f0,f14,f21          #if (t>bb...) t = bbextentt
	fsel    f21,f21,f21,f25         #if (t<0) t = 0
	fctiwz  f0,f21
	stfd    f0,local+8(r1)
	lwz     r9,local+12(r1)         #r9 = (int)t
	li      r0,1
	dcbtst  r11,r0

######  Second loop. In every iteration one part of the whole span is drawn

#                do
#                {
#                // calculate s and t at the far end of the span
#                        if (count >= 16)
#                                spancount = 16;
#                        else
#                                spancount = count;
#
#                        count -= spancount;
#
#                        if (count)
#                        {

.d16loop2:
	cmpwi   r12,16
	bgt     .d16cont
	mtctr   r12
	subi    r10,r12,1
	int2dbl f21,r10,r0,local,f10	#spancountminus1 = (float)...
	li      r12,0                   #r12 = count -= spancount
	fmadds  f26,f21,f7,f26          #zi += d_zistepu * ...
	b       .d16finalpart
.d16cont:
	fadds   f26,f26,f19             #zi += zi16stepu
	li      r10,16                  #r10 = spancount = 16
	fmuls   f0,f26,f26
	subf    r12,r10,r12             #r12 = count -= spancount
	frsqrte f0,f0
	mtctr   r10

#######  Evaluation of the values for the inner loop. This version is used for
#######  span size = 16

#                        // calculate s/z, t/z, zi->fixed s and t at far end of span,
#                        // calculate s and t steps across span by shifting
#                                sdivz += sdivz16stepu;
#                                tdivz += tdivz16stepu;
#                                zi += zi16stepu;
#                                z = (float)0x10000 / zi;        // prescale to 16.16 fixed-point
#                                snext = (int)(sdivz * z) + sadjust;
#                                if (snext > bbextents)
#                                        snext = bbextents;
#                                else if (snext < 16)
#                                        snext = 16;      // prevent round-off error on <0 steps from
#                                                                //  from causing overstepping & running off the
#                                                                //  edge of the texture
#                                tnext = (int)(tdivz * z) + tadjust;
#                                if (tnext > bbextentt)
#                                        tnext = bbextentt;
#                                else if (tnext < 16)
#                                        tnext = 16;      // guard against round-off error on <0 steps
#                                sstep = (snext - s) >> 4;
#                                tstep = (tnext - t) >> 4;
#                        }

	fnmsubs f20,f0,f26,f28
	fadds   f22,f22,f17             #sdivz += sdivz16stepu
	fmuls   f0,f0,f20
	fadds   f23,f23,f18             #tdivz += tdivz16stepu
	fnmsubs f20,f0,f26,f28
	fmuls   f0,f0,f20
	fmuls   f24,f0,f15
	fmadds  f20,f22,f24,f11         #f20 = snext
	fsubs   f0,f20,f13
	fsel    f20,f0,f13,f20          #if (snext>bb...) snext = bbextents
	fsubs   f0,f20,f16
	fsel    f20,f0,f20,f16          #if (snext<16) snext = 16
	fctiwz  f0,f20
	stfd    f0,local+8(r1)
	lwz     r31,local+12(r1)        #r31 = (int)snext
	fmadds  f21,f23,f24,f12         #f21 = tnext
	fsubs   f0,f21,f14
	fsel    f21,f0,f14,f21          #if (tnext>bb...) tnext = bbextentt
	subf    r29,r8,r31
	fsubs   f0,f21,f16
	srawi   r29,r29,4               #r29 = sstep = (snext - s) >> 4
	fsel    f21,f0,f21,f16          #if (tnext<16) tnext = 16
	fctiwz  f0,f21
	stfd    f0,local+8(r1)
	lwz     r30,local+12(r1)        #r30 = (int)tnext
	subf    r28,r9,r30
	srawi   r28,r28,4               #r28 = tstep = (tnext - t) >> 4
	b       .d16mainloop

######  Evaluation of the values for the inner loop. This version is used for
######  span size < 16

######  The original algorithm has two ugly divisions at the end of this part.
######  These are removed by the following optimization:
######  First, the divisors 1,2 and 4 are handled specially to gain speed. The
######  other divisors are handled using a reciprocal table.

#                        // calculate s/z, t/z, zi->fixed s and t at last pixel in span (so
#                        // can't step off polygon), clamp, calculate s and t steps across
#                        // span by division, biasing steps low so we don't run off the
#                        // texture
#                                spancountminus1 = (float)(spancount - 1);
#                                sdivz += d_sdivzstepu * spancountminus1;
#                                tdivz += d_tdivzstepu * spancountminus1;
#                                zi += d_zistepu * spancountminus1;
#                                z = (float)0x10000 / zi;        // prescale to 16.16 fixed-point
#                                snext = (int)(sdivz * z) + sadjust;
#                                if (snext > bbextents)
#                                        snext = bbextents;
#                                else if (snext < 16)
#                                        snext = 16;      // prevent round-off error on <0 steps from
#                                                                //  from causing overstepping & running off the
#                                                                //  edge of the texture
#
#                                tnext = (int)(tdivz * z) + tadjust;
#                                if (tnext > bbextentt)
#                                        tnext = bbextentt;
#                                else if (tnext < 16)
#                                        tnext = 16;      // guard against round-off error on <0 steps
#
#                                if (spancount > 1)
#                                {
#                                        sstep = (snext - s) / (spancount - 1);
#                                        tstep = (tnext - t) / (spancount - 1);
#                                }
#                        }

.d16finalpart:
	fmuls   f0,f26,f26
	fmadds  f22,f21,f1,f22          #sdivz += d_sdivzstepu * ...
	frsqrte f0,f0
	fmadds  f23,f21,f4,f23          #tdivs += d_tdivzstepu * ...
	fnmsubs f20,f0,f26,f28
	cmplwi  r10,5
	fmuls   f0,f0,f20
	fnmsubs f20,f0,f26,f28
	fmuls   f0,f0,f20
	fmuls   f24,f0,f15
	fmadds  f20,f22,f24,f11         #f27 = snext
	fsubs   f0,f20,f13
	fsel    f20,f0,f13,f20          #if (snext>bb...) snext = bbextents
	fsubs   f0,f20,f16
	fsel    f20,f0,f20,f16          #if (snext<16) snext = 16
	fctiwz  f0,f20
	stfd    f0,local+8(r1)
	lwz     r31,local+12(r1)        #r31 = (int)snext
	fmadds  f21,f23,f24,f12         #f21 = tnext
	fsubs   f0,f21,f14
	fsel    f21,f0,f14,f21          #if (tnext>bb...) tnext = bbextentt
	subf    r29,r8,r31
	fsubs   f0,f21,f16
	fsel    f21,f0,f21,f16          #if (tnext<16) tnext = 16
	fctiwz  f0,f21
	stfd    f0,local+8(r1)
	lwz     r30,local+12(r1)        #r30 = (int)tnext
	subf    r28,r9,r30
	blt     .d16special
.d16qdiv:
	slwi    r0,r10,2
	lwzx    r0,r26,r0
	mulhw   r29,r29,r0
	mulhw   r28,r28,r0
	b       .d16mainloop
.d16special:
	cmplwi  r10,1
	ble     .d16mainloop
	cmplwi  r10,3
	beq     .d16qdiv
	blt     .d16spec_2
	srawi   r29,r29,2
	srawi   r28,r28,2
	b       .d16mainloop
.d16spec_2:
	srawi   r29,r29,1
	srawi   r28,r28,1

######  Main drawing loop. Here lies the speed.

#                        do
#                        {
#                                *pdest++ = *(pbase + (s >> 16) + (t >> 16) * cachewidth);
#                                s += sstep;
#                                t += tstep;
#                        } while (--spancount > 0);

.d16mainloop:
	srawi   r0,r9,16
	srawi   r10,r8,16
	mullw   r0,r0,r4
	add     r9,r9,r28
	add     r0,r0,r10
	add     r8,r8,r29
	lbzx    r0,r27,r0
	stbu    r0,1(r11)
	bdnz    .d16mainloop
	mr      r8,r31
	mr      r9,r30
	mr.	r12,r12
	bgt     .d16loop2
	lwz     r3,4(r3)                #while (...)
	mr.	r3,r3
	subi    r3,r3,4
	bne     .d16loop

	lmw	r26,gb(r1)
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
	exit

	funcend	D_DrawSpans16




###########################################################################
#
#       void D_DrawZSpans (espan_t *pspan)
#                          r3
#
#       standard z-scan drawing function
#
###########################################################################

	funcdef	D_DrawZSpans

	init	0,16,0,0		# 16 local bytes for int2dbl/dbl2int
	lf      f1,INT2DBL_0            #for int2dbl_setup
	stfd	f1,local(r1)

	ls      f2,cHUGE                #f2 = 0x8000*0x10000
	ls      f3,d_zistepu            #f3 = d_zistepu
	ls      f4,d_zistepv            #f4 = d_zistepv
	ls      f5,d_ziorigin           #f5 = d_ziorigin
	lw      r5,d_pzbuffer           #r5 = d_pzbuffer
	lw      r6,d_zwidth             #r6 = d_zwidth

#        izistep = (int)(d_zistepu * 0x8000 * 0x10000);

	fmuls   f6,f3,f2
	fctiwz  f0,f6
	stfd    f0,local+8(r1)
	lwz     r7,local+12(r1)         #r7 = izistep
	subi    r3,r3,4                 #prepare for postincrement

#                pdest = d_pzbuffer + (d_zwidth * pspan->v) + pspan->u;
#
#                count = pspan->count;
#
#        // calculate the initial 1/z
#                du = (float)pspan->u;
#                dv = (float)pspan->v;
#
#                zi = d_ziorigin + dv*d_zistepv + du*d_zistepu;
#        // we count on FP exceptions being turned off to avoid range problems
#                izi = (int)(zi * 0x8000 * 0x10000);


.zloop:
	lwzu    r8,4(r3)                #r8 = pspan->u
	lwzu    r9,4(r3)                #r9 = pspan->v
	lwzu    r10,4(r3)               #r10 = pspan->count
	mullw   r11,r9,r6
	add     r11,r11,r8
	add     r11,r11,r11
	add     r12,r5,r11              #r12 = pdest = d_pzbuffer + ...
	int2dbl f7,r8,r0,local,f1	#f7 = du
	int2dbl f8,r9,r0,local,f1	#f8 = dv
	fmadds  f10,f8,f4,f5
	subi    r12,r12,4
	fmadds  f10,f7,f3,f10           #zi = d_ziorigin + ...
	fmuls   f7,f10,f2
	fctiwz  f0,f7
	stfd    f0,local+8(r1)
	lwz     r8,local+12(r1)         #r8 = izi = (int)(zi * ...)

#                if ((long)pdest & 0x02)
#                {
#                        *pdest++ = (short)(izi >> 16);
#                        izi += izistep;
#                        count--;
#                }

	andi.   r0,r12,2                #if (long)pdest & 0x02
	beq     .zcont
	srawi   r0,r8,16
	sth     r0,4(r12)               #*pdest++ (short)(izi >> 16)
	addi    r12,r12,2
	add     r8,r8,r7                #izi += izistep
	subi    r10,r10,1               #count--
.zcont:


#                if ((doublecount = count >> 1) > 0)
#                {
#                        do
#                        {
#                                ltemp = izi >> 16;
#                                izi += izistep;
#                                ltemp |= izi & 0xFFFF0000;
#                                izi += izistep;
#                                *(int *)pdest = ltemp;
#                                pdest += 2;
#                        } while (--doublecount > 0);
#                }

	srawi.  r0,r10,1                #if ((doublecount = count >> 1))
	ble     .zcont2
	mtctr   r0
.zloop2:
	srawi   r0,r8,16                #ltemp = izi >> 16
	add     r8,r8,r7                #izi += izistep
	inslwi  r0,r8,16,0              #ltemp |= izi & 0xFFFF0000
	add     r8,r8,r7                #izi += izistep
	stwu    r0,4(r12)               #*(int *)pdest = ltemp
	bdnz    .zloop2
.zcont2:

#                if (count & 1)
#                        *pdest = (short)(izi >> 16);

	andi.   r0,r10,1                #if (count & 1)
	beq     .zcont3
	srawi   r0,r8,16                #*pdest = (short)(izi >> 16)
	sth     r0,4(r12)
.zcont3:

#        } while ((pspan = pspan->pnext) != NULL);

	lwz     r3,4(r3)                #while (...)
	mr.	r3,r3
	subi    r3,r3,4
	bne     .zloop

	exit

	funcend	D_DrawZSpans




.ifdef	WOS
	.tocd
.else
	.data
.endif

lab c8
	.float	8.0
lab c16
	.float	16.0
lab cHUGE
	.float	2147483648.0
lab cSPEED
	.float	20
lab cAMP2times2
	.float	6.0	# 2 * AMP2
.ifne AMP2-3
.fail "AMP2 must be 3!"
.endif
