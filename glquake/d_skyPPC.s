##
## Quake for AMIGA
##
## d_skyPPC.s
##
## Define WOS for PowerOpen ABI, otherwise SVR4-ABI is used.
##

.set NOLR,1
.include        "macrosPPC.i"
.include	"quakedefPPC.i"

#
# external references
#

	xrefv	d_viewbuffer
	xrefv	screenwidth
	xrefa	r_refdef
	xrefv	r_skysource
	xrefa	vid
	xrefa	vright
	xrefa	vpn
	xrefa	vup
	xrefv	skytime
	xrefv	skyspeed

	xrefv	INT2DBL_0
	xrefv	c0
	xrefv	c0_5
	xrefv	c65536
	xrefa	_ReciprocTable


#
# defines
#

.set	SKYSHIFT           ,7
.set	SKYSIZE            ,(1<<SKYSHIFT)
.set	SKYMASK            ,(SKYSIZE-1)
.set	SKY_SPAN_SHIFT     ,5
.set	SKY_SPAN_MAX       ,(1<<SKY_SPAN_SHIFT)
.set	R_SKY_SMASK        ,0x7f0000
.set	R_SKY_TMASK        ,0x7f0000





###########################################################################
#
#       void D_DrawSkyScans8 (espan_t *pspan)
#
#       standard scan drawing function for the sky
#
#       D_Sky_uv_To_st is inlined.
#
#       IMPORTANT!! SKY_SPAN_SHIFT must *NOT* exceed 5 (The ReciprocTable
#       has to be extended)
#
###########################################################################

	funcdef	D_DrawSkyScans8

	init	0,16,12,12
	stmw	r20,gb(r1)
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

	lf      f1,INT2DBL_0		#for int2dbl_setup, r4=tmp
	stfd	f1,local(r1)
        lw      r5,d_viewbuffer         #r5 = d_viewbuffer
        lw      r6,screenwidth          #r6 = screenwidth
        lw      r7,r_skysource          #r7 = r_skysource
        lxa     r8,_ReciprocTable
        lxa     r9,r_refdef
        lxa     r12,vid			#r12 = vid
        lxa     r31,vpn
        lxa     r30,vup
        lxa     r29,vright
        lwz     r10,REFDEF_VRECT+VRECT_HEIGHT(r9)
        lwz     r11,REFDEF_VRECT+VRECT_WIDTH(r9)
        cmpw    r11,r10
        blt     .height
        int2dbl f2,r11,r4,local,f1	#f2 = temp = (float)r_refdef...
        b       .width
.height:
        int2dbl f2,r10,r4,local,f1	#f2 = temp = (float)r_refdef...
.width:
        ls      f3,c8192
        ls      f5,c4096                #f5 = 4096
        ls      f0,c3
        ls      f15,ca
        ls      f19,c0
        ls      f20,c0_5
        lfs     f6,0(r31)
        fmuls   f6,f6,f5                #f6 = 4096*vpn[0]
        lfs     f7,4(r31)
        fmuls   f7,f7,f5                #f7 = 4096*vpn[1]
        lfs     f8,8(r31)
        fmuls   f8,f8,f5                #f8 = 4096*vpn[2]
        fdivs   f3,f3,f2                #f3 = 8192 / temp
        subi    r3,r3,4
        lwz     r31,VID_WIDTH(r12)
        srawi   r31,r31,1               #r31 = vid.width >> 1
        lwz     r12,VID_HEIGHT(r12)
        srawi   r12,r12,1               #r12 = vid.height >> 1
        int2dbl f9,r12,r4,local,f1	#f9 = (float)vid.height >> 1

        ls      f14,skytime
        ls      f4,skyspeed
        fmuls   f14,f14,f4
        ls      f4,c65536
        fmuls   f14,f14,f4
        fctiwz  f14,f14
        stfd    f14,local+8(r1)
        lwz     r27,local+12(r1)        #r27 = skytime * skyspeed*$10000
.loop:
        lwzu    r9,4(r3)                #r9 = u
        lwzu    r10,4(r3)               #r10 = v
        int2dbl f4,r10,r4,local,f1	#f4 = (float)pspan->v
        mullw   r0,r10,r6
        add     r0,r0,r9
        add     r11,r5,r0               #r11 = pdest = d_viewbuffer + ...
        subi    r11,r11,1               #prepare for postincrement
        fsubs   f10,f9,f4               #f10 = ((vid.height>>1)-v)
        fmuls   f10,f10,f3              #wv = 8192 * fp0 / temp
        lfs     f11,0(r30)
        fmadds  f23,f11,f10,f6          #f23 = 4096*vpn[0] + wv*vup[0]
        lfs     f11,4(r30)
        fmadds  f24,f11,f10,f7          #f24 = 4096*vpn[1] + wv*vup[1]
        lfs     f11,8(r30)
        fmadds  f25,f11,f10,f8          #f25 = 4096*vpn[2] + wv*vup[2]
        lfs     f4,0(r29)
        fmuls   f4,f4,f3                #f4 = 8192 * vright[0] / temp
        lfs     f12,4(r29)
        fmuls   f12,f12,f3              #f12 = 8192 * vright[1] / temp
        lfs     f13,8(r29)
        fmuls   f13,f13,f3              #f13 = 8192 * vright[2] / temp
        fmuls   f25,f25,f0
        fmuls   f13,f13,f0
        lwzu    r28,4(r3)               #r28 = count = pspan->count
        li      r0,1
        dcbtst  r11,r0

######  D_Sky_uv_To_st (inlined)

        subf    r0,r31,r9
        int2dbl f16,r0,r4,local,f1	#f16 = (float(u-vid.width>>1))
        fmadds  f21,f16,f4,f23          #f17 = end[0]
        fmuls   f17,f21,f21
        fmadds  f22,f16,f12,f24         #f18 = end[1]
        fmadds  f17,f22,f22,f17         #f17 = end[0]^2 + end[1]^2
        fmadds  f18,f16,f13,f25         #f18 = end[2]
        fmadds  f17,f18,f18,f17         #f17 = end[0]^2 + end[1]^2 + end[2]^2
        fcmpo   cr0,f17,f19
        beq     .zero
        frsqrte f16,f17
        fres    f16,f16
        fdivs   f18,f17,f16
        fadd    f16,f18,f16
        fmul    f17,f16,f20             #f17 = sqrt(...)
.zero:
        fdivs   f18,f15,f17
        fmuls   f21,f21,f18             #f21 = 6*(SKYSIZE/2-1)*end[0]
        fmuls   f22,f22,f18             #f22 = 6*(SKYSIZE/2-1)*end[1]
        fctiwz  f21,f21
        stfd    f21,local+8(r1)
        lwz     r26,local+12(r1)
        add     r26,r26,r27             #r26 = s
        fctiwz  f22,f22
        stfd    f22,local+8(r1)
        lwz     r25,local+12(r1)
        add     r25,r25,r27             #r25 = t

######  end of D_Sky_uv_To_st

######  Second loop. In every iteration one part of the whole span is drawn

#                do
#                {
#                        if (count >= SKY_SPAN_MAX)
#                                spancount = SKY_SPAN_MAX;
#                        else
#                                spancount = count;
#
#                        count -= spancount;
#
#                        if (count)
#                        {

.loop2:
        cmpwi   r28,SKY_SPAN_MAX
        bgt     .cont
        mtctr   r28
        subi    r20,r28,1               #r20 = spancountminus1
        li      r28,0                   #r28 = count -= spancount
        b       .finalpart
.cont:
        li      r20,SKY_SPAN_MAX        #r20 = spancount = SKY_SPAN_MAX
        subf    r28,r20,r28             #r28 = count -= spancount
        mtctr   r20


######  Evaluation of the values for the inner loop. This version is used for
######  span size = SKY_SPAN_MAX

#                        // calculate s and t at far end of span,
#                        // calculate s and t steps across span by shifting
#                                u += spancount;
#
#                                D_Sky_uv_To_st (u, v, &snext, &tnext);
#
#                                sstep = (snext - s) >> SKY_SPAN_SHIFT;
#                                tstep = (tnext - t) >> SKY_SPAN_SHIFT;
#                        }


        add     r9,r9,r20               #u += spancount

######  D_Sky_uv_To_st (inlined)

        subf    r0,r31,r9
        int2dbl f16,r0,r4,local,f1	#f16 = (float(u-vid.width>>1))
        fmadds  f21,f16,f4,f23          #f17 = end[0]
        fmuls   f17,f21,f21
        fmadds  f22,f16,f12,f24         #f18 = end[1]
        fmadds  f17,f22,f22,f17         #f17 = end[0]^2 + end[1]^2
        fmadds  f18,f16,f13,f25         #f18 = end[2]
        fmadds  f17,f18,f18,f17         #f17 = end[0]^2 + end[1]^2 + end[2]^2
        fcmpo   cr0,f17,f19
        beq     .zero2
        frsqrte f16,f17
        fres    f16,f16
        fdivs   f18,f17,f16
        fadd    f16,f18,f16
        fmul    f17,f16,f20             #f17 = sqrt(...)
.zero2:
        fdivs   f18,f15,f17
        fmuls   f21,f21,f18             #f21 = 6#(SKYSIZE/2-1)*end[0]
        fctiwz  f21,f21
        stfd    f21,local+8(r1)
        fmuls   f22,f22,f18             #f22 = 6*(SKYSIZE/2-1)*end[1]
        lwz     r24,local+12(r1)
        fctiwz  f22,f22
        stfd    f22,local+8(r1)
        add     r24,r24,r27             #r24 = snext
        lwz     r23,local+12(r1)
        add     r23,r23,r27             #r23 = tnext

######  end of D_Sky_uv_To_st

        subf    r22,r26,r24             #r22 = snext - s
        subf    r21,r25,r23             #r21 = tnext - t
        srawi   r22,r22,SKY_SPAN_SHIFT  #r22 = sstep
        srawi   r21,r21,SKY_SPAN_SHIFT  #r21 = tstep
        b       .mainloop


.finalpart:
        add     r9,r9,r20               #u += spancountminus1

######  D_Sky_uv_To_st (inlined)

        subf    r0,r31,r9
        int2dbl f16,r0,r4,local,f1	#f16 = (float(u-vid.width>>1))
        fmadds  f21,f16,f4,f23          #f17 = end[0]
        fmuls   f17,f21,f21
        fmadds  f22,f16,f12,f24         #f18 = end[1]
        fmadds  f17,f22,f22,f17         #f17 = end[0]^2 + end[1]^2
        fmadds  f18,f16,f13,f25         #f18 = end[2]
        fmadds  f17,f18,f18,f17         #f17 = end[0]^2 + end[1]^2 + end[2]^2
        fcmpo   cr0,f17,f19
        beq     .zero3
        frsqrte f16,f17
        fres    f16,f16
        fdivs   f18,f17,f16
        fadd    f16,f18,f16
        fmul    f17,f16,f20             #f17 = sqrt(...)
.zero3:
        cmplwi  r20,5
        fdivs   f18,f15,f17
        fmuls   f21,f21,f18             #f21 = 6*(SKYSIZE/2-1)*end[0]
        fctiwz  f21,f21
        stfd    f21,local+8(r1)
        fmuls   f22,f22,f18             #f22 = 6*(SKYSIZE/2-1)*end[1]
        lwz     r24,local+12(r1)
        fctiwz  f22,f22
        stfd    f22,local+8(r1)
        add     r24,r24,r27             #r24 = snext
        lwz     r23,local+12(r1)
        add     r23,r23,r27             #r23 = tnext

######  end of D_Sky_uv_To_st

        subf    r22,r26,r24             #r22 = snext - s
        subf    r21,r25,r23             #r21 = tnext - t

        blt     .special
.qdiv:
        slwi    r0,r20,2
        lwzx    r0,r8,r0
        mulhw   r22,r22,r0
        mulhw   r21,r21,r0
        b       .mainloop
.special:
        cmplwi  r20,1
        ble     .mainloop
        cmplwi  r20,3
        beq     .qdiv
        blt     .spec_2
        srawi   r22,r22,2
        srawi   r21,r21,2
        b       .mainloop
.spec_2:
        srawi   r22,r22,1
        srawi   r21,r21,1


######  Main drawing loop.

#                        do
#                        {
#                                *pdest++ = r_skysource[((t & R_SKY_TMASK) >> 8) +
#                                                ((s & R_SKY_SMASK) >> 16)];
#                                s += sstep;
#                                t += tstep;
#                        } while (--spancount > 0);


.mainloop:
        rlwinm  r0,r25,24,17,23         #implied: R_SKY_TMASK
        add     r25,r25,r21             #t += tstep
        rlwimi  r0,r26,16,25,31         #implied: R_SKY_SMASK
        add     r26,r26,r22             #s += sstep
        lbzx    r0,r7,r0                #r_skysource[...]
        stbu    r0,1(r11)
        bdnz    .mainloop

######  loop terminations

        mr      r26,r24                 #s = snext
        mr      r25,r23                 #t = tnext
        mr.     r28,r28
        bgt     .loop2
        lwz     r3,4(r3)
        mr.     r3,r3
        subi    r3,r3,4
        bne     .loop

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
	lmw	r20,gb(r1)
	exit

	funcend	D_DrawSkyScans8




.ifdef	WOS
	.tocd
.else
	.data
.endif

lab c8192
	.float	8192.0
lab c4096
	.float	4096.0
lab c3
	.float	3.0
lab ca
	.float	24772608.0	# => 65536*6*(SKYSIZE/2-1)
