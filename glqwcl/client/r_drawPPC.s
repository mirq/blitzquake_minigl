##
## Quake for AMIGA
##
## r_drawPPC.s
##
## Define WOS for PowerOpen ABI, otherwise SVR4-ABI is used.
##

.include        "macrosPPC.i"
.include	"quakedefPPC.i"

#
# external references
#
	xrefv	r_lastvertvalid
	xrefv	r_u1
	xrefv	r_v1
	xrefv	r_lzi1
	xrefv	r_ceilv1
	xrefv	r_nearzi
	xrefv	r_nearzionly
	xrefv	r_emitted
	xrefv	r_framecount
	xrefv	r_pedge
	xrefa	r_refdef
	xrefv	r_leftclipped
	xrefv	r_rightclipped
	xrefa	r_leftexit
	xrefa	r_rightexit
	xrefa	r_leftenter
	xrefa	r_rightenter
	xrefv	r_edges
	xrefv	r_outofsurfaces
	xrefv	r_outofedges
	xrefv	r_pcurrentvertbase
	xrefv	r_polycount
	xrefv	r_currentkey
	xrefv	c_faceclip
	xrefa	modelorg
	xrefv	xscale
	xrefv	yscale
	xrefv	xscaleinv
	xrefv	yscaleinv
	xrefv	xcenter
	xrefv	ycenter
	xrefa	vright
	xrefa	vup
	xrefa	vpn
	xrefv	cacheoffset
	xrefv	edge_p
	xrefv	surfaces
	xrefv	surface_p
	xrefv	surf_max
	xrefv	edge_max
	xrefa	newedges
	xrefa	removeedges
	xrefa	view_clipplanes
	xrefv	insubmodel
	xrefv	currententity
	xrefv	makeleftedge
	xrefv	makerightedge
	xrefv	INT2DBL_0
	xrefv	c0
	xrefv	c2


#
# defines
#

.set	FULLY_CLIPPED_CACHED   ,0x80000000
.set	FRAMECOUNT_MASK        ,0x7fffffff




###########################################################################
#
#       void R_EmitEdge (mvertex_t *pv0, mvertex_t *pv1)
#
###########################################################################

	funcdef	R_EmitEdge

	init	0,16,0,7
	stfd	f14,fb+0*8(r1)
	stfd	f15,fb+1*8(r1)
	stfd	f16,fb+2*8(r1)
	stfd	f17,fb+3*8(r1)
	stfd	f18,fb+4*8(r1)
	stfd	f19,fb+5*8(r1)
	stfd	f20,fb+6*8(r1)
	ls	f0,c2
	lf      f11,INT2DBL_0           #for int2dbl_setup
	stfd	f11,local(r1)

        lxa     r12,r_refdef
        lxa     r6,vright
        lfs     f14,0(r6)
        lfs     f15,4(r6)
        lfs     f16,8(r6)
        lxa     r6,vup
        lfs     f17,0(r6)
        lfs     f18,4(r6)
        lfs     f19,8(r6)
        lxa     r6,vpn
        lfs     f20,0(r6)
        lfs     f12,4(r6)
        lfs     f13,8(r6)
        lxa     r9,modelorg
        ls      f10,cNEAR_CLIP

#        if (r_lastvertvalid)
#        {
#                u0 = r_u1;
#                v0 = r_v1;
#                lzi0 = r_lzi1;
#                ceilv0 = r_ceilv1;
#        }

        lw      r6,r_lastvertvalid
        mr.     r6,r6
        beq     .EEelse
        ls      f1,r_u1                 #f1 = u0
        ls      f2,r_v1                 #f2 = v0
        ls      f3,r_lzi1               #f3 = lzi0
        lw      r7,r_ceilv1             #r7 = ceilv0
        b       .EEnext

#                world = &pv0->position[0];
#
#                VectorSubtract (world, modelorg, local);
#                TransformVector (local, transformed);
#
#                if (transformed[2] < NEAR_CLIP)
#                        transformed[2] = NEAR_CLIP;
#
#                lzi0 = 1.0 / transformed[2];
#
#                scale = xscale * lzi0;
#                u0 = (xcenter + scale*transformed[0]);
#                if (u0 < r_refdef.fvrectx_adj)
#                        u0 = r_refdef.fvrectx_adj;
#                if (u0 > r_refdef.fvrectright_adj)
#                        u0 = r_refdef.fvrectright_adj;
#
#                scale = yscale * lzi0;
#                v0 = (ycenter - scale*transformed[1]);
#                if (v0 < r_refdef.fvrecty_adj)
#                        v0 = r_refdef.fvrecty_adj;
#                if (v0 > r_refdef.fvrectbottom_adj)
#                        v0 = r_refdef.fvrectbottom_adj;
#
#                ceilv0 = (int) ceil(v0);

.EEelse:

######  VectorSubtract (inlined)

        lfs     f1,0(r3)
        lfs     f7,0(r9)
        fsubs   f1,f1,f7                #f1 = local[0]
        lfs     f2,4(r3)
        lfs     f8,4(r9)
        fsubs   f2,f2,f8                #f2 = local[1]
        lfs     f3,8(r3)
        lfs     f9,8(r9)
        fsubs   f3,f3,f9                #f3 = local[2]

######  TransformVector (inlined)

        fmuls   f7,f1,f14
        fmuls   f8,f1,f17
        fmuls   f9,f1,f20
        fmadds  f7,f2,f15,f7
        fmadds  f8,f2,f18,f8
        fmadds  f9,f2,f12,f9
        fmadds  f7,f3,f16,f7            #f7 = transformed[0]
        fmadds  f8,f3,f19,f8            #f8 = transformed[1]
        fmadds  f9,f3,f13,f9            #f9 = transformed[2]

######  end of TransformVector

        fsubs   f1,f9,f10
        ls      f2,xscale
        fsel    f9,f1,f9,f10
        ls      f4,yscale
        fres    f3,f9                   #f3 = lzi0
        ls      f5,ycenter
        fmuls   f7,f7,f3
        fmuls   f8,f8,f3
        ls      f6,xcenter
        lfs     f1,REFDEF_FVRECTX_ADJ(r12)
        fmadds  f7,f2,f7,f6
        lfs     f2,REFDEF_FVRECTY_ADJ(r12)
        fnmsubs f8,f4,f8,f5
        fsubs   f5,f7,f1
        fsubs   f4,f8,f2
        fsel    f7,f5,f7,f1
        fsel    f8,f4,f8,f2
        lfs     f1,REFDEF_FVRECTRIGHT_ADJ(r12)
        fsubs   f5,f1,f7
        lfs     f2,REFDEF_FVRECTBOTTOM_ADJ(r12)
        fsubs   f4,f2,f8
        fsel    f1,f5,f7,f1             #f1 = u0
        fsel    f2,f4,f8,f2             #f2 = v0
        mffs    f7
        mtfsfi  cr7,2
        fctiw   f4,f2
        stfd    f4,local+8(r1)
        lwz     r7,local+12(r1)         #r7 = ceilv0
        mtfsf   1,f7
.EEnext:

#        world = &pv1->position[0];
#
#// transform and project
#        VectorSubtract (world, modelorg, local);
#        TransformVector (local, transformed);
#
#        if (transformed[2] < NEAR_CLIP)
#                transformed[2] = NEAR_CLIP;
#
#        r_lzi1 = 1.0 / transformed[2];
#
#        scale = xscale * r_lzi1;
#        r_u1 = (xcenter + scale*transformed[0]);
#        if (r_u1 < r_refdef.fvrectx_adj)
#                r_u1 = r_refdef.fvrectx_adj;
#        if (r_u1 > r_refdef.fvrectright_adj)
#                r_u1 = r_refdef.fvrectright_adj;
#
#        scale = yscale * r_lzi1;
#        r_v1 = (ycenter - scale*transformed[1]);
#        if (r_v1 < r_refdef.fvrecty_adj)
#                r_v1 = r_refdef.fvrecty_adj;
#        if (r_v1 > r_refdef.fvrectbottom_adj)
#                r_v1 = r_refdef.fvrectbottom_adj;

######  VectorSubtract (inlined)

        lfs     f4,0(r4)
        lfs     f7,0(r9)
        fsubs   f4,f4,f7                #f4 = local[0]
        lfs     f5,4(r4)
        lfs     f8,4(r9)
        fsubs   f5,f5,f8                #f5 = local[1]
        lfs     f6,8(r4)
        lfs     f9,8(r9)
        fsubs   f6,f6,f9                #f6 = local[2]

######  TransformVector (inlined)

        fmuls   f7,f4,f14
        fmuls   f8,f4,f17
        fmuls   f9,f4,f20
        fmadds  f7,f5,f15,f7
        fmadds  f8,f5,f18,f8
        fmadds  f9,f5,f12,f9
        fmadds  f7,f6,f16,f7            #f7 = transformed[0]
        fmadds  f8,f6,f19,f8            #f8 = transformed[1]
        fmadds  f9,f6,f13,f9            #f9 = transformed[2]

######  end of TransformVector

        fsubs   f5,f9,f10
        fsel    f9,f5,f9,f10
        ls      f6,ycenter
        fres    f4,f9                   #f4 = r_lzi1
        ls      f5,xscale
        fmuls   f7,f7,f4
        ls      f9,xcenter
        fmuls   f8,f8,f4
        ls      f10,yscale
        fmadds  f7,f5,f7,f9
        fnmsubs f8,f10,f8,f6
        lfs     f10,REFDEF_FVRECTX_ADJ(r12)
        fsubs   f5,f7,f10
        lfs     f9,REFDEF_FVRECTY_ADJ(r12)
        ss      f4,r_lzi1
        fsubs   f6,f8,f9
        fsel    f7,f5,f7,f10
        fsel    f8,f6,f8,f9
        lfs     f9,REFDEF_FVRECTRIGHT_ADJ(r12)
        fsubs   f5,f9,f7
        lfs     f10,REFDEF_FVRECTBOTTOM_ADJ(r12)
        fsubs   f6,f10,f8
        fsel    f5,f5,f7,f9             #f5 = r_u1
        fsel    f6,f6,f8,f10            #f6 = r_v1
        ss      f5,r_u1
        mffs    f7
        mtfsfi  cr7,2
        fctiw   f8,f6
        ss      f6,r_v1
        stfd    f8,local+8(r1)
        lwz     r10,local+12(r1)        #r10 = r_ceilv1
        mtfsf   1,f7


#        if (r_lzi1 > lzi0)
#                lzi0 = r_lzi1;
#
#        if (lzi0 > r_nearzi)    // for mipmap finding
#                r_nearzi = lzi0;
#
#// for right edges, all we want is the effect on 1/z
#        if (r_nearzionly)
#                return;
#
#        r_emitted = 1;
#
#        r_ceilv1 = (int) ceil(r_v1);

        fsubs   f8,f3,f4
        fsel    f3,f8,f3,f4
        ls      f8,r_nearzi
        fsubs   f7,f8,f3
        fsel    f8,f7,f8,f3
        ss      f8,r_nearzi
        lw      r11,r_nearzionly
        mr.     r11,r11
        bne     .EEexit1
        sw      r10,r_ceilv1
        li      r12,1
        sw      r12,r_emitted

#        if (ceilv0 == r_ceilv1)
#        {
#        // we cache unclipped horizontal edges as fully clipped
#                if (cacheoffset != 0x7FFFFFFF)
#                {
#                        cacheoffset = FULLY_CLIPPED_CACHED |
#                                        (r_framecount & FRAMECOUNT_MASK);
#                }
#
#                return;         // horizontal edge
#        }

        cmpw    r10,r7
        bne     .EEcont13
        lw      r12,cacheoffset
        lis     r11,0x7fffffff@h
        ori     r11,r11,0x7fffffff@l
        cmpw    r11,r12
        beq     .EEexit1
        lw      r11,r_framecount
        lis     r4,FRAMECOUNT_MASK@h
        ori     r4,r4,FRAMECOUNT_MASK@l
        and     r11,r11,r4
        lis     r3,FULLY_CLIPPED_CACHED@h
        ori     r3,r3,FULLY_CLIPPED_CACHED@l
        or      r11,r11,r3
        sw      r11,cacheoffset
        b       .EEexit1

#        side = ceilv0 > r_ceilv1;
#
#        edge = edge_p++;
#
#        edge->owner = r_pedge;
#
#        edge->nearzi = lzi0;

.EEcont13:
        fsubs   f9,f2,f6
        lw      r9,edge_p
        fsubs   f8,f1,f5
        stfs    f3,EDGE_NEARZI(r9)
        addi    r11,r9,EDGE_SIZEOF
        fneg    f7,f8
        sw      r11,edge_p
        lw      r12,r_pedge
        fsel    f8,f9,f8,f7
        stw     r12,EDGE_OWNER(r9)
        fneg    f7,f9

#        if (side == 0)
#        {
#        // trailing edge (go from p1 to p2)
#                v = ceilv0;
#                v2 = r_ceilv1 - 1;
#
#                edge->surfs[0] = surface_p - surfaces;
#                edge->surfs[1] = 0;
#
#                u_step = ((r_u1 - u0) / (r_v1 - v0));
#                u = u0 + ((float)v - v0) * u_step;
#        }

        cmpw    r7,r10
        fsel    f9,f9,f9,f7
        lw      r5,surface_p
        bgt     .EEelse2
        mr      r11,r7                  #r11 = v = ceilv0
        lw      r6,surfaces
        fmuls   f7,f9,f9
        subf    r5,r6,r5
        subi    r12,r10,1               #r12 = r_ceilv1 - 1
        slwi    r5,r5,10
        frsqrte f7,f7
        lxa     r10,r_refdef
        stw     r5,EDGE_SURFS(r9)
        fnmsubs f4,f7,f9,f0
	int2dbl	f5,r11,r8,local,f11
        fmuls   f7,f7,f4
        li      r4,1
        lwz     r5,REFDEF_VRECTXR_ADJ_S20(r10)
        fnmsubs f4,f7,f9,f0
        fsubs   f5,f5,f2
        lwz     r10,REFDEF_VRECTX_ADJ_S20(r10)
        fmuls   f7,f7,f4
        slwi    r11,r11,2
        fmuls   f8,f7,f8
        slwi    r12,r12,2
        ls      f9,c16times65536
        fmadds  f7,f5,f8,f1             #f7 = u
        b       .EEcont14

#        {
#        // leading edge (go from p2 to p1)
#                v2 = ceilv0 - 1;
#                v = r_ceilv1;
#
#                edge->surfs[0] = 0;
#                edge->surfs[1] = surface_p - surfaces;
#
#                u_step = ((u0 - r_u1) / (v0 - r_v1));
#                u = r_u1 + ((float)v - r_v1) * u_step;
#        }

.EEelse2:
        fsel    f9,f9,f9,f7
        mr      r11,r10                 #r11 = v = r_ceilv1
        lw      r6,surfaces
        fmuls   f7,f9,f9
        subf    r5,r6,r5
        subi    r12,r7,1                #r12 = ceilv0 - 1
        srawi   r5,r5,6
        frsqrte f7,f7
        clrlwi  r5,r5,16
        stw     r5,EDGE_SURFS(r9)
        fnmsubs f4,f7,f9,f0
        lxa     r10,r_refdef
	int2dbl	f1,r11,r8,local,f11
        fmuls   f7,f7,f4
        li      r4,0
        lwz     r5,REFDEF_VRECTXR_ADJ_S20(r10)
        fnmsubs f4,f7,f9,f0
        fsubs   f1,f1,f6
        lwz     r10,REFDEF_VRECTX_ADJ_S20(r10)
        fmuls   f7,f7,f4
        slwi    r11,r11,2
        fmuls   f8,f7,f8
        slwi    r12,r12,2
        ls      f9,c16times65536
        fmadds  f7,f1,f8,f5             #f7 = u

#        edge->u_step = u_step*0x100000;
#        edge->u = u*0x100000 + 0xFFFFF;

.EEcont14:
        fmuls   f8,f8,f9
        lis     r3,0xfffff@h
        ori     r3,r3,0xfffff@l
        fctiwz  f1,f8
        stfd    f1,local+8(r1)
        lwz     r0,local+12(r1)
        stw     r0,EDGE_U_STEP(r9)
        fmuls   f7,f7,f9
        fctiwz  f1,f7
        stfd    f1,local+8(r1)
        lwz     r0,local+12(r1)
        add     r6,r0,r3

#        if (edge->u < r_refdef.vrect_x_adj_shift20)
#                edge->u = r_refdef.vrect_x_adj_shift20;
#        if (edge->u > r_refdef.vrectright_adj_shift20)
#                edge->u = r_refdef.vrectright_adj_shift20;

        cmpw    r6,r10
        bge     .EEcont15
        mr      r6,r10
.EEcont15:
        cmpw    r6,r5
        ble     .EEcont16
        mr      r6,r5

#        u_check = edge->u;
#        if (edge->surfs[0])
#                u_check++;      // sort trailers after leaders
#
#        if (!newedges[v] || newedges[v]->u >= u_check)
#        {
#                edge->next = newedges[v];
#                newedges[v] = edge;
#        }
#        else
#        {
#                pcheck = newedges[v];
#                while (pcheck->next && pcheck->next->u < u_check)
#                        pcheck = pcheck->next;
#                edge->next = pcheck->next;
#                pcheck->next = edge;
#        }

.EEcont16:
        lxa     r5,newedges
        stw     r6,EDGE_U(r9)
        lxa     r7,removeedges
        add     r6,r6,r4
        lwzx    r3,r5,r11
        mr.     r3,r3
        beq     .EEcont18
        lwz     r4,EDGE_U(r3)
        cmpw    r6,r4
        bgt     .EEloop
.EEcont18:
        stw     r3,EDGE_NEXT(r9)
        stwx    r9,r5,r11
        b       .EEcont21
.EEloop:
        lwz     r5,EDGE_NEXT(r3)
        mr.     r5,r5
        beq     .EEcont20
        mr      r0,r3
        mr      r3,r5
        lwz     r4,EDGE_U(r5)
        cmpw    r6,r4
        bgt     .EEloop
        mr      r3,r0
.EEcont20:
        stw     r5,EDGE_NEXT(r9)
        stw     r9,EDGE_NEXT(r3)

#        edge->nextremove = removeedges[v2];
#        removeedges[v2] = edge;

.EEcont21:
        lwzx    r3,r7,r12
        stw     r3,EDGE_NEXTREMOVE(r9)
        stwx    r9,r7,r12

.EEexit1:
	lfd	f14,fb+0*8(r1)
	lfd	f15,fb+1*8(r1)
	lfd	f16,fb+2*8(r1)
	lfd	f17,fb+3*8(r1)
	lfd	f18,fb+4*8(r1)
	lfd	f19,fb+5*8(r1)
	lfd	f20,fb+6*8(r1)
	exit

	funcend	R_EmitEdge




###########################################################################
#
#       void R_ClipEdge (mvertex_t *pv0, mvertex_t *pv1, clipplane_t *clip)
#
###########################################################################

	funcdef	R_ClipEdge

	init	0,16,0,0
	la	r6,local(r1)

        mr.     r5,r5
        beq     .CEadd
        ls      f10,c0
.CEloop:

#       d0 = DotProduct (pv0->position, clip->normal) - clip->dist;
#       d1 = DotProduct (pv1->position, clip->normal) - clip->dist;

        lfs     f1,CLIP_NORMAL(r5)
        lfs     f2,CLIP_NORMAL+4(r5)
        lfs     f3,CLIP_NORMAL+8(r5)
        lfs     f9,CLIP_DIST(r5)
        lfs     f4,0(r3)                #f4 = pv0->position[0]
        fmsubs  f7,f4,f1,f9
        lfs     f5,4(r3)                #f5 = pv0->position[1]
        fmadds  f7,f5,f2,f7
        lfs     f6,8(r3)                #f6 = pv0->position[2]
        fmadds  f7,f6,f3,f7             #f7 = d0
        lfs     f8,0(r4)                #f8 = pv1->position[0]
        fmsubs  f1,f8,f1,f9
        lfs     f9,4(r4)                #f9 = pv1->position[1]
        fmadds  f1,f9,f2,f1
        lfs     f2,8(r4)                #f2 = pv1->position[2]
        fmadds  f1,f2,f3,f1             #f1 = d1

#                        if (d0 >= 0)
#                        {
#                        // point 0 is unclipped
#                                if (d1 >= 0)
#                                {
#                                // both points are unclipped
#                                        continue;
#                                }

        fcmpo   cr0,f7,f10
        blt     .CEless
        fcmpo   cr0,f1,f10
        bge     .CEloopend

#                                cacheoffset = 0x7FFFFFFF;
#
#                                f = d0 / (d0 - d1);
#                                clipvert.position[0] = pv0->position[0] +
#                                                f * (pv1->position[0] - pv0->position[0]);
#                                clipvert.position[1] = pv0->position[1] +
#                                                f * (pv1->position[1] - pv0->position[1]);
#                                clipvert.position[2] = pv0->position[2] +
#                                                f * (pv1->position[2] - pv0->position[2]);

        lis     r7,0x7fffffff@h
        ori     r7,r7,0x7fffffff@l
        sw      r7,cacheoffset
        fsubs   f3,f7,f1
        fdivs   f3,f7,f3
        fsubs   f8,f8,f4
        fsubs   f9,f9,f5
        fsubs   f2,f2,f6
        fmadds  f8,f8,f3,f4
        stfs    f8,0(r6)
        fmadds  f9,f9,f3,f5
        stfs    f9,4(r6)
        fmadds  f2,f2,f3,f6
        stfs    f2,8(r6)

#                                if (clip->leftedge)
#                                {
#                                        r_leftclipped = true;
#                                        r_leftexit = clipvert;
#                                }

        lbz     r7,CLIP_LEFTEDGE(r5)
        mr.     r7,r7
        beq     .CEelse
	li	r0,1
	sw	r0,r_leftclipped
        lxa     r11,r_leftexit
        lwz     r7,0(r6)
        stw     r7,0(r11)
        lwz     r8,4(r6)
        stw     r8,4(r11)
        lwz     r9,8(r6)
        stw     r9,8(r11)
        b       .CEcont

#                                else if (clip->rightedge)
#                                {
#                                        r_rightclipped = true;
#                                        r_rightexit = clipvert;
#                                }

.CEelse:
	li	r0,1
	sw	r0,r_rightclipped
        lxa     r11,r_rightexit
        lwz     r7,0(r6)
        stw     r7,0(r11)
        lwz     r8,4(r6)
        stw     r8,4(r11)
        lwz     r9,8(r6)
        stw     r9,8(r11)

#                                R_ClipEdge (pv0, &clipvert, clip->next);
#                                return;

.CEcont:
        mr      r4,r6
        lwz     r5,CLIP_NEXT(r5)
        call    R_ClipEdge
        b       .CEexit

#                                if (d1 < 0)
#                                {
#                                // both points are clipped
#                                // we do cache fully clipped edges
#                                        if (!r_leftclipped)
#                                                cacheoffset = FULLY_CLIPPED_CACHED |
#                                                                (r_framecount & FRAMECOUNT_MASK);
#                                        return;
#                                }

.CEless:
        fcmpo   cr0,f1,f10
        bge     .CEcont2
        lw      r7,r_leftclipped
        mr.     r7,r7
        bne     .CEexit
        lis     r9,FULLY_CLIPPED_CACHED@h
        ori     r9,r9,FULLY_CLIPPED_CACHED@l
        lis     r7,FRAMECOUNT_MASK@h
        ori     r7,r7,FRAMECOUNT_MASK@l
        lw      r8,r_framecount
        and     r8,r8,r7
        or      r8,r8,r9
        sw      r8,cacheoffset
        b       .CEexit

#                                r_lastvertvalid = false;
#
#                        // we don't cache partially clipped edges
#                                cacheoffset = 0x7FFFFFFF;
#
#                                f = d0 / (d0 - d1);
#                                clipvert.position[0] = pv0->position[0] +
#                                                f * (pv1->position[0] - pv0->position[0]);
#                                clipvert.position[1] = pv0->position[1] +
#                                                f * (pv1->position[1] - pv0->position[1]);
#                                clipvert.position[2] = pv0->position[2] +
#                                                f * (pv1->position[2] - pv0->position[2]);

.CEcont2:
	li	r0,0
        sw	r0,r_lastvertvalid
        lis     r7,0x7fffffff@h
        ori     r7,r7,0x7fffffff@l
        sw      r7,cacheoffset
        fsubs   f3,f7,f1
        fdivs   f3,f7,f3
        fsubs   f8,f8,f4
        fsubs   f9,f9,f5
        fsubs   f2,f2,f6
        fmadds  f8,f8,f3,f4
        stfs    f8,0(r6)
        fmadds  f9,f9,f3,f5
        stfs    f9,4(r6)
        fmadds  f2,f2,f3,f6
        stfs    f2,8(r6)

#                                if (clip->leftedge)
#                                {
#                                        r_leftclipped = true;
#                                        r_leftenter = clipvert;
#                                }

        lbz     r7,CLIP_LEFTEDGE(r5)
        mr.     r7,r7
        beq     .CEelse2
	li	r0,1
        sw	r0,r_leftclipped
        lxa     r11,r_leftenter
        lwz     r7,0(r6)
        stw     r7,0(r11)
        lwz     r8,4(r6)
        stw     r8,4(r11)
        lwz     r9,8(r6)
        stw     r9,8(r11)
        b       .CEcont3

#                                else if (clip->rightedge)
#                                {
#                                        r_rightclipped = true;
#                                        r_rightenter = clipvert;
#                                }

.CEelse2:
	li	r0,1
        sw	r0,r_rightclipped
        lxa     r11,r_rightenter
        lwz     r7,0(r6)
        stw     r7,0(r11)
        lwz     r8,4(r6)
        stw     r8,4(r11)
        lwz     r9,8(r6)
        stw     r9,8(r11)

#                                R_ClipEdge (&clipvert, pv1, clip->next);
#                                return;

.CEcont3:
        mr      r3,r6
        lwz     r5,CLIP_NEXT(r5)
        call    R_ClipEdge
        b       .CEexit

#                } while ((clip = clip->next) != NULL);

.CEloopend:
        lwz     r5,CLIP_NEXT(r5)
        mr.     r5,r5
        bne     .CEloop

#        R_EmitEdge (pv0, pv1);

.CEadd:
        call    R_EmitEdge

.CEexit:
	exit

	funcend	R_ClipEdge




.ifdef	WOS
	.tocd
.else
	.data
.endif

lab cNEAR_CLIP
	.float	0.01			#must match the def. in r_local.h
lab c16times65536
	.float	1048576.0
