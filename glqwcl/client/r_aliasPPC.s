##
## Quake for AMIGA
##
## r_aliasPPC.s
##
## Define WOS for PowerOpen ABI, otherwise SVR4-ABI is used.
##

.set NOLR,1
.include        "macrosPPC.i"
.include	"quakedefPPC.i"

#
# external references
#

	xrefa	aliastransform
	xrefa	r_avertexnormals
	xrefa	r_plightvec
	xrefv	r_ambientlight
	xrefv	r_shadelight
	xrefv	ziscale
	xrefv	aliasxscale
	xrefv	aliasyscale
	xrefv	aliasxcenter
	xrefv	aliasycenter
	xrefv	INT2DBL_0
	xrefv	c0




###########################################################################
#
#       void R_AliasTransformVector (vec3_t in, vec3_t out)
#
###########################################################################

	funcdef	R_AliasTransformVector

        lxa     r5,aliastransform
        lfs     f3,12(r5)
        lfs     f1,0(r3)
        lfs     f2,0(r5)
        fmadds  f0,f1,f2,f3
        lfs     f4,4(r3)
        lfs     f5,4(r5)
        fmadds  f0,f4,f5,f0
        lfs     f6,8(r3)
        lfs     f7,8(r5)
        fmadds  f0,f6,f7,f0
        stfs    f0,0(r4)
        lfs     f3,12+16(r5)
        lfs     f2,0+16(r5)
        fmadds  f0,f1,f2,f3
        lfs     f5,4+16(r5)
        fmadds  f0,f4,f5,f0
        lfs     f7,8+16(r5)
        fmadds  f0,f6,f7,f0
        stfs    f0,4(r4)
        lfs     f3,12+2*16(r5)
        lfs     f2,0+2*16(r5)
        fmadds  f0,f1,f2,f3
        lfs     f5,4+2*16(r5)
        fmadds  f0,f4,f5,f0
        lfs     f7,8+2*16(r5)
        fmadds  f0,f6,f7,f0
        stfs    f0,8(r4)
        blr

	funcend	R_AliasTransformVector




###########################################################################
#
#       void R_AliasTransformFinalVert (finalvert_t *fv, auxvert_t *av,
#                                      trivertx_t *pverts, stvert_t *pstverts)
#
###########################################################################

	funcdef	R_AliasTransformFinalVert

	init	0,16,0,0

	ls	f13,c0
	lxa	r7,aliastransform
	lf      f11,INT2DBL_0              #for int2dbl_setup, tmp=r11
	stfd	f11,local(r1)

#        av->fv[0] = DotProduct(pverts->v, aliastransform[0]) +
#                        aliastransform[0][3];
#        av->fv[1] = DotProduct(pverts->v, aliastransform[1]) +
#                        aliastransform[1][3];
#        av->fv[2] = DotProduct(pverts->v, aliastransform[2]) +
#                        aliastransform[2][3];

         lbz     r8,0(r5)
         int2dbl f1,r8,r11,local,f11
         lfs     f3,12(r7)
         lfs     f2,0(r7)
         fmadds  f0,f1,f2,f3
         lbz     r8,1(r5)
         int2dbl f4,r8,r11,local,f11
         lfs     f5,4(r7)
         fmadds  f0,f4,f5,f0
         lbz     r8,2(r5)
         int2dbl f6,r8,r11,local,f11
         lfs     f7,8(r7)
         fmadds  f0,f6,f7,f0
         stfs    f0,0(r4)
         lfs     f3,12+16(r7)
         lfs     f2,0+16(r7)
         fmadds  f0,f1,f2,f3
         lfs     f5,4+16(r7)
         fmadds  f0,f4,f5,f0
         lfs     f7,8+16(r7)
         fmadds  f0,f6,f7,f0
         stfs    f0,4(r4)
         lfs     f3,12+2*16(r7)
         lfs     f2,0+2*16(r7)
         fmadds  f0,f1,f2,f3
         lfs     f5,4+2*16(r7)
         fmadds  f0,f4,f5,f0
         lfs     f7,8+2*16(r7)
         fmadds  f0,f6,f7,f0
         stfs    f0,8(r4)

#        fv->v[2] = pstverts->s;
#        fv->v[3] = pstverts->t;
#
#        fv->flags = pstverts->onseam;

        lwz     r0,SV_S(r6)
        stw     r0,8(r3)
        lwz     r8,SV_T(r6)
        stw     r8,12(r3)
        lwz     r9,SV_ONSEAM(r6)
        stw     r9,FV_FLAGS(r3)

#        plightnormal = r_avertexnormals[pverts->lightnormalindex];
#        lightcos = DotProduct (plightnormal, r_plightvec);
#        temp = r_ambientlight;

        lxa     r7,r_avertexnormals
        lbz     r8,TV_LIGHTNORMALINDEX(r5)
        mulli   r8,r8,12
        add     r7,r7,r8
        lxa     r9,r_plightvec
        lfs     f1,0(r7)
        lfs     f2,0(r9)
        fmuls   f3,f1,f2
        lfs     f4,4(r7)
        lfs     f5,4(r9)
        fmadds  f3,f4,f5,f3
        lfs     f6,8(r7)
        lfs     f7,8(r9)
        fmadds  f3,f6,f7,f3
        lw      r10,r_ambientlight

#        if (lightcos < 0)
#        {
#                temp += (int)(r_shadelight * lightcos);
#
#        // clamp; because we limited the minimum ambient and shading light, we
#        // don't have to clamp low light, just bright
#                if (temp < 0)
#                        temp = 0;
#        }
#
#        fv->v[4] = temp;

        fcmpo   cr0,f3,f13
        bge     .cont
        ls      f12,r_shadelight
        fmuls   f3,f3,f12
        fctiwz  f0,f3
        stfd    f0,local+8(r1)
        lwz     r11,local+12(r1)
        add.    r10,r10,r11
        bge     .cont
        li      r10,0
.cont:
        stw     r10,16(r3)
	exit

	funcend	R_AliasTransformFinalVert




###########################################################################
#
#       void R_AliasProjectFinalVert (finalvert_t *fv, auxvert_t *av,)
#
###########################################################################

	funcdef	R_AliasProjectFinalVert

	init	0,8,0,0

        lfs     f1,8(r4)
        ls      f2,ziscale
        fres    f1,f1
        fmuls   f3,f1,f2
        fctiwz  f0,f3
        stfd    f0,local(r1)
        lwz     r5,local+4(r1)
        stw     r5,20(r3)
        lfs     f4,0(r4)
        lfs     f5,4(r4)
        fmuls   f4,f4,f1
        fmuls   f5,f5,f1
        ls      f6,aliasxscale
        ls      f7,aliasxcenter
        fmadds  f4,f4,f6,f7
        fctiwz  f0,f4
        stfd    f0,local(r1)
        lwz     r5,local+4(r1)
        stw     r5,0(r3)
        ls      f6,aliasyscale
        ls      f7,aliasycenter
        fmadds  f5,f5,f6,f7
        fctiwz  f0,f5
        stfd    f0,local(r1)
        lwz     r5,local+4(r1)
        stw     r5,4(r3)
	exit

	funcend	R_AliasProjectFinalVert
