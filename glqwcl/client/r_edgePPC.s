##
## Quake for AMIGA
##
## r_edgePPC.s
##
## Define WOS for PowerOpen ABI, otherwise SVR4-ABI is used.
##

.include        "macrosPPC.i"
.include	"quakedefPPC.i"

#
# external references
#

	xrefv	r_bmodelactive
	xrefv	surfaces
	xrefv	span_p
	xrefv	current_iv
	xrefv	fv
	xrefv	edge_head_u_shift20
	xrefa	edge_head
	xrefa	edge_tail
	xrefa	edge_aftertail
	xrefv   R_CleanupSpan
	xrefv	INT2DBL_0




###########################################################################
#
#       void R_InsertNewEdges (edge_t *edgestoadd, edge_t *edgelist)
#
###########################################################################

	funcdef	R_InsertNewEdges

#        do
#        {
#                next_edge = edgestoadd->next;
#edgesearch:
#                if (edgelist->u >= edgestoadd->u)
#                        goto addedge;
#                edgelist=edgelist->next;
#                if (edgelist->u >= edgestoadd->u)
#                        goto addedge;
#                edgelist=edgelist->next;
#                if (edgelist->u >= edgestoadd->u)
#                        goto addedge;
#                edgelist=edgelist->next;
#                if (edgelist->u >= edgestoadd->u)
#                        goto addedge;
#                edgelist=edgelist->next;
#                goto edgesearch;
#
#        // insert edgestoadd before edgelist
#addedge:
#                edgestoadd->next = edgelist;
#                edgestoadd->prev = edgelist->prev;
#                edgelist->prev->next = edgestoadd;
#                edgelist->prev = edgestoadd;
#        } while ((edgestoadd = next_edge) != NULL);

        lwz     r5,EDGE_U(r3)
.INEloop:
        lwz     r6,EDGE_U(r4)
        cmpw    r5,r6
        ble     .INEaddedge
        lwz     r4,EDGE_NEXT(r4)
        lwz     r6,EDGE_U(r4)
        cmpw    r5,r6
        ble     .INEaddedge
        lwz     r4,EDGE_NEXT(r4)
        lwz     r6,EDGE_U(r4)
        cmpw    r5,r6
        ble     .INEaddedge
        lwz     r4,EDGE_NEXT(r4)
        lwz     r6,EDGE_U(r4)
        cmpw    r5,r6
        ble     .INEaddedge
        lwz     r4,EDGE_NEXT(r4)
        b       .INEloop
.INEaddedge:
        lwz     r7,EDGE_NEXT(r3)
        stw     r4,EDGE_NEXT(r3)
        lwz     r5,EDGE_PREV(r4)
        mr.     r7,r7
        stw     r5,EDGE_PREV(r3)
        stw     r3,EDGE_NEXT(r5)
        stw     r3,EDGE_PREV(r4)
        beq     .INEend
        mr      r3,r7
        lwz     r5,EDGE_U(r3)
        b       .INEloop
.INEend:
	blr

	funcend	R_InsertNewEdges




###########################################################################
#
#       void R_RemoveEdges (edge_t *pedge)
#
###########################################################################

	funcdef	R_RemoveEdges

#        do
#        {
#                pedge->next->prev = pedge->prev;
#                pedge->prev->next = pedge->next;
#        } while ((pedge = pedge->nextremove) != NULL);

.REloop:
        lwz     r4,EDGE_NEXT(r3)
        lwz     r5,EDGE_PREV(r3)
        lwz     r3,EDGE_NEXTREMOVE(r3)
        mr.     r3,r3
        stw     r5,EDGE_PREV(r4)
        stw     r4,EDGE_NEXT(r5)
        bne     .REloop
	blr

	funcend	R_RemoveEdges




###########################################################################
#
#       void R_StepActiveU (edge_t *pedge)
#
###########################################################################

	funcdef	R_StepActiveU

#nextedge:
#                pedge->u += pedge->u_step;
#                if (pedge->u < pedge->prev->u)
#                        goto pushback;
#                pedge = pedge->next;
#
#                pedge->u += pedge->u_step;
#                if (pedge->u < pedge->prev->u)
#                        goto pushback;
#                pedge = pedge->next;
#
#                pedge->u += pedge->u_step;
#                if (pedge->u < pedge->prev->u)
#                        goto pushback;
#                pedge = pedge->next;
#
#                pedge->u += pedge->u_step;
#                if (pedge->u < pedge->prev->u)
#                        goto pushback;
#                pedge = pedge->next;
#
#                goto nextedge;

        lwz     r4,EDGE_PREV(r3)
        lwz     r5,EDGE_U(r4)
        lxa     r6,edge_aftertail
        lxa     r7,edge_tail
.SAloop:
        lwz     r8,EDGE_U(r3)
        lwz     r9,EDGE_U_STEP(r3)
        add     r8,r8,r9
        stw     r8,EDGE_U(r3)
        cmpw    r8,r5
        blt     .SApushback
        mr      r5,r8
        lwz     r3,EDGE_NEXT(r3)
        lwz     r8,EDGE_U(r3)
        lwz     r9,EDGE_U_STEP(r3)
        add     r8,r8,r9
        stw     r8,EDGE_U(r3)
        cmpw    r8,r5
        blt     .SApushback
        mr      r5,r8
        lwz     r3,EDGE_NEXT(r3)
        lwz     r8,EDGE_U(r3)
        lwz     r9,EDGE_U_STEP(r3)
        add     r8,r8,r9
        stw     r8,EDGE_U(r3)
        cmpw    r8,r5
        blt     .SApushback
        mr      r5,r8
        lwz     r3,EDGE_NEXT(r3)
        lwz     r8,EDGE_U(r3)
        lwz     r9,EDGE_U_STEP(r3)
        add     r8,r8,r9
        stw     r8,EDGE_U(r3)
        cmpw    r8,r5
        blt     .SApushback
        mr      r5,r8
        lwz     r3,EDGE_NEXT(r3)
        b       .SAloop

#                if (pedge == &edge_aftertail)
#                        return;
#
#        // push it back to keep it sorted
#                pnext_edge = pedge->next;
#
#        // pull the edge out of the edge list
#                pedge->next->prev = pedge->prev;
#                pedge->prev->next = pedge->next;
#
#        // find out where the edge goes in the edge list
#                pwedge = pedge->prev->prev;
#
#                while (pwedge->u > pedge->u)
#                {
#                        pwedge = pwedge->prev;
#                }
#
#        // put the edge back into the edge list
#                pedge->next = pwedge->next;
#                pedge->prev = pwedge;
#                pedge->next->prev = pedge;
#                pwedge->next = pedge;
#
#                pedge = pnext_edge;
#                if (pedge == &edge_tail)
#                        return;

.SApushback:
        cmpw    r6,r3
        beq     .SAend
        lwz     r11,EDGE_NEXT(r3)
        lwz     r9,EDGE_PREV(r3)
        stw     r9,EDGE_PREV(r11)
        stw     r11,EDGE_NEXT(r9)
        lwz     r9,EDGE_PREV(r9)
.SAloop2:
        lwz     r0,EDGE_U(r9)
        cmpw    r8,r0
        bgt     .SAcont
        lwz     r9,EDGE_PREV(r9)
        b       .SAloop2
.SAcont:
        lwz     r4,EDGE_NEXT(r9)
        stw     r4,EDGE_NEXT(r3)
        stw     r9,EDGE_PREV(r3)
        stw     r3,EDGE_PREV(r4)
        stw     r3,EDGE_NEXT(r9)
        mr      r3,r11
        cmpw    r3,r7
        bne     .SAloop
.SAend:
        blr

	funcend	R_StepActiveU




###########################################################################
#
#       void R_GenerateSpans (void)
#
#       R_TrailingEdge and R_LeadingEdge are inlined
#
#       notes:
#       Increment and Decrement of _r_bmodelactive removed, because it's
#       obsolete here
#
###########################################################################

	funcdef	R_GenerateSpans

	init	0,16,8,0
	stmw	r24,gb(r1)

	li	r0,0
	sw	r0,r_bmodelactive
        lw      r3,current_iv
        lw      r4,surfaces
        la      r5,SURF_SIZEOF(r4)
        stw     r5,SURF_NEXT(r5)
        stw     r5,SURF_PREV(r5)
        lw      r6,edge_head_u_shift20
        stw     r6,SURF_LAST_U(r5)
        lxa     r7,edge_head
        lwz     r8,EDGE_NEXT(r7)
        lxa     r24,edge_tail
        lw      r9,span_p
        lis     r26,0xfffff@h
        ori     r26,r26,0xfffff@l
	lf      f1,INT2DBL_0            #for int2dbl_setup, r25=tmp
	stfd	f1,local(r1)
        ls      f3,ca
        ls      f4,fv
        ls      f7,c0_99
        ls      f8,c1_01
        b       .GStry
.GSloop:
        lwz     r10,EDGE_U(r8)          #r10 = edge->u
        srawi   r11,r10,20              #r11 = edge->u >> 20
        lwz     r12,EDGE_SURFS(r8)
        rlwinm. r0,r12,22,10,25         #implies SURF_SIZEOF_EXP
        beq     .GScont
        add     r31,r4,r0               #r31 = surf

######  R_TrailingEdge (inlined)

#        if (--surf->spanstate == 0)
#        {
#                if (surf->insubmodel)
#                        r_bmodelactive--;
#
#                if (surf == surfaces[1].next)
#                {
#                // emit a span (current top going away)
#                        iu = edge->u >> 20;
#                        if (iu > surf->last_u)
#                        {
#                                span = span_p++;
#                                span->u = surf->last_u;
#                                span->count = iu - span->u;
#                                span->v = current_iv;
#                                span->pnext = surf->spans;
#                                surf->spans = span;
#                        }
#
#                // set last_u on the surface below
#                        surf->next->last_u = iu;
#                }
#
#                surf->prev->next = surf->next;
#                surf->next->prev = surf->prev;
#        }

        lwz     r30,SURF_SPANSTATE(r31)
        subic.  r30,r30,1
        stw     r30,SURF_SPANSTATE(r31)
        bne     .GScont
        lwz     r30,SURF_NEXT(r31)
        lwz     r0,SURF_SIZEOF+SURF_NEXT(r4)
        cmpw    r0,r31
        bne     .GSte_cont2
        lwz     r0,SURF_LAST_U(r31)
        stw     r11,SURF_LAST_U(r30)
        subf.   r29,r0,r11
        ble     .GSte_cont2
        lwz     r28,SURF_SPANS(r31)
        stw     r9,SURF_SPANS(r31)
        stw     r0,0(r9)
        stw     r3,4(r9)
        stw     r29,8(r9)
        stw     r28,12(r9)
        addi    r9,r9,16
.GSte_cont2:
        lwz     r28,SURF_PREV(r31)
        stw     r30,SURF_NEXT(r28)
        stw     r28,SURF_PREV(r30)

######  end of R_TrailingEdge

.GScont:

######  R_LeadingEdge (inlined)

#        if (edge->surfs[1])
#        {
#        // it's adding a new surface in, so find the correct place
#                surf = &surfaces[edge->surfs[1]];
#

        rlwinm. r0,r12,SURF_SIZEOF_EXP,10,25
        beq     .GSnext
        add     r30,r4,r0               #r30 = surf

#
#        // don't start a span if this is an inverted span, with the end
#        // edge preceding the start edge (that is, we've already seen the
#        // end edge)
#                if (++surf->spanstate == 1)
#                {
#                        if (surf->insubmodel)
#                                r_bmodelactive++;
#
#                        surf2 = surfaces[1].next;
#
#                        if (surf->key < surf2->key)
#                                goto newtop;
#                // if it's two surfaces on the same plane, the one that's already
#                // active is in front, so keep going unless it's a bmodel
#                        if (surf->insubmodel && (surf->key == surf2->key))
#                        {

        lwz     r28,SURF_SPANSTATE(r30)
        mr.     r28,r28
        beq     .GSle_zero
        addi    r28,r28,1
        stw     r28,SURF_SPANSTATE(r30)
        b       .GSnext
.GSle_zero:
        addi    r28,r28,1
        stw     r28,SURF_SPANSTATE(r30)
        lwz     r7,SURF_INSUBMODEL(r30)
        lwz     r31,SURF_SIZEOF+SURF_NEXT(r4)
        lwz     r6,SURF_KEY(r30)
        li      r27,0
        lwz     r0,SURF_KEY(r31)
        cmpw    r6,r0
        blt     .GSle_newtop
        bgt     .GSle_search
        mr.     r7,r7
        beq     .GSle_search
        li      r27,-1
        subf    r0,r26,r10
	int2dbl f2,r0,r25,local,f1
        fmuls   f2,f2,f3
        lfs     f5,SURF_D_ZIORIGIN(r30)
        lfs     f6,SURF_D_ZISTEPV(r30)
        fmadds  f5,f4,f6,f5
        lfs     f6,SURF_D_ZISTEPU(r30)
        fmadds  f5,f2,f6,f5
        fmuls   f11,f7,f5               #f11 = newzibottom
        fmuls   f5,f8,f5                #f5 = newzitop

#                        // must be two bmodels in the same leaf; sort on 1/z
#                                fu = (float)(edge->u - 0xFFFFF) * (1.0 / 0x100000);
#                                newzi = surf->d_ziorigin + fv*surf->d_zistepv +
#                                                fu*surf->d_zistepu;
#                                newzibottom = newzi * 0.99;
#
#                                testzi = surf2->d_ziorigin + fv*surf2->d_zistepv +
#                                                fu*surf2->d_zistepu;
#
#                                if (newzibottom >= testzi)
#                                {
#                                        goto newtop;
#                                }
#
#                                newzitop = newzi * 1.01;
#                                if (newzitop >= testzi)
#                                {
#                                        if (surf->d_zistepu >= surf2->d_zistepu)
#                                        {
#                                                goto newtop;
#                                        }
#                                }

        lfs     f9,SURF_D_ZIORIGIN(r31)
        lfs     f10,SURF_D_ZISTEPV(r31)
        fmadds  f9,f4,f10,f9
        lfs     f10,SURF_D_ZISTEPU(r31)
        fmadds  f9,f2,f10,f9
        fcmpo   cr0,f11,f9
        bge     .GSle_newtop
        fcmpo   cr0,f5,f9
        blt     .GSle_search
        fcmpo   cr0,f6,f10
        bge     .GSle_newtop

#                        do
#                        {
#                                surf2 = surf2->next;
#                        } while (surf->key > surf2->key);
#


.GSle_search:
        lwz     r31,SURF_NEXT(r31)
        lwz     r0,SURF_KEY(r31)
        cmpw    r6,r0
        bgt     .GSle_search

#                        if (surf->key == surf2->key)
#                        {
#                        // if it's two surfaces on the same plane, the one that's already
#                        // active is in front, so keep going unless it's a bmodel
#                                if (!surf->insubmodel)
#                                        goto continue_search;

        bne     .GSle_gotposition
        mr.     r7,r7
        beq     .GSle_search
        mr.     r27,r27
        bne     .GSle_precalc_done
        li      r27,-1
        subf    r0,r26,r10
	int2dbl	f2,r0,r25,local,f1
        fmuls   f2,f2,f3
        lfs     f5,SURF_D_ZIORIGIN(r30)
        lfs     f6,SURF_D_ZISTEPV(r30)
        fmadds  f5,f4,f6,f5
        lfs     f6,SURF_D_ZISTEPU(r30)
        fmadds  f5,f2,f6,f5
        fmuls   f11,f7,f5               #f11 = newzibottom
        fmuls   f5,f8,f5                #f5 = newzitop
.GSle_precalc_done:

#                        // must be two bmodels in the same leaf; sort on 1/z
#                                fu = (float)(edge->u - 0xFFFFF) * (1.0 / 0x100000);
#                                newzi = surf->d_ziorigin + fv*surf->d_zistepv +
#                                                fu*surf->d_zistepu;
#                                newzibottom = newzi * 0.99;
#
#                                testzi = surf2->d_ziorigin + fv*surf2->d_zistepv +
#                                                fu*surf2->d_zistepu;
#
#                                if (newzibottom >= testzi)
#                                {
#                                        goto gotposition;
#                                }
#
#                                newzitop = newzi * 1.01;
#                                if (newzitop >= testzi)
#                                {
#                                        if (surf->d_zistepu >= surf2->d_zistepu)
#                                        {
#                                                goto gotposition;
#                                        }
#                                }
#
#                                goto continue_search;
#                        }
#
#                        goto gotposition;

        lfs     f9,SURF_D_ZIORIGIN(r31)
        lfs     f10,SURF_D_ZISTEPV(r31)
        fmadds  f9,f4,f10,f9
        lfs     f10,SURF_D_ZISTEPU(r31)
        fmadds  f9,f2,f10,f9
        fcmpo   cr0,f11,f9
        bge     .GSle_gotposition
        fcmpo   cr0,f5,f9
        blt     .GSle_search
        fcmpo   cr0,f6,f10
        bge     .GSle_gotposition
        b       .GSle_search

#                // emit a span (obscures current top)
#                        iu = edge->u >> 20;
#
#                        if (iu > surf2->last_u)
#                        {
#                                span = span_p++;
#                                span->u = surf2->last_u;
#                                span->count = iu - span->u;
#                                span->v = current_iv;
#                                span->pnext = surf2->spans;
#                                surf2->spans = span;
#                        }
#
#                        // set last_u on the new span
#                        surf->last_u = iu;

.GSle_newtop:
        lwz     r0,SURF_LAST_U(r31)
        stw     r11,SURF_LAST_U(r30)
        subf.   r29,r0,r11
        ble     .GSle_cont2
        lwz     r28,SURF_SPANS(r31)
        stw     r9,SURF_SPANS(r31)
        stw     r0,0(r9)
        stw     r3,4(r9)
        stw     r29,8(r9)
        stw     r28,12(r9)
        addi    r9,r9,16
.GSle_cont2:

#                // insert before surf2
#                        surf->next = surf2;
#                        surf->prev = surf2->prev;
#                        surf2->prev->next = surf;
#                        surf2->prev = surf;

.GSle_gotposition:
        lwz     r28,SURF_PREV(r31)
        stw     r28,SURF_PREV(r30)
        stw     r31,SURF_NEXT(r30)
        stw     r30,SURF_NEXT(r28)
        stw     r30,SURF_PREV(r31)

######  end of R_LeadingEdge

.GSnext:
        lwz     r8,EDGE_NEXT(r8)
.GStry:
        cmpw    r24,r8
        bne     .GSloop
        sw      r9,span_p
	call	R_CleanupSpan

	lmw	r24,gb(r1)
	exit

	funcend	R_GenerateSpans





.ifdef	WOS
	.tocd
.else
	.data
.endif

lab ca
	.float	9.53674e-07
lab c0_99
	.float	0.99
lab c1_01
	.float	1.01
