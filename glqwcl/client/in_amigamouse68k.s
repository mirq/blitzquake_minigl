**
** Input interrupt routine for mouse control
**
** used for M68k as well as for PowerPC
**

	include "exec/types.i"
	include "devices/inputevent.i"


	STRUCTURE       InputIntDat,0
; must be the same as struct InputIntDat in in_amigamouse.c
	ULONG           iid_LeftButtonDown
	ULONG           iid_MidButtonDown
	ULONG           iid_RightButtonDown
	ULONG           iid_LeftButtonUp
	ULONG           iid_MidButtonUp
	ULONG           iid_RightButtonUp
	ULONG           iid_MouseX
	ULONG           iid_MouseY
	ULONG		iid_MouseSpeed


	code

	xdef    _InputIntCode
	xdef    InputIntCode
_InputIntCode:
InputIntCode:
	move.l  a0,-(sp)
	move.l	iid_MouseSpeed(a1),d1
.loop:
	cmp.b   #IECLASS_RAWMOUSE,ie_Class(a0)
	bne.b   .next
	move.w  ie_Code(a0),d0
	btst    #7,d0
	bne.b   .up
	cmp.w   #IECODE_LBUTTON,d0
	beq.b   .lmb_down
	cmp.w   #IECODE_MBUTTON,d0
	beq.b   .mmb_down
	cmp.w   #IECODE_RBUTTON,d0
	beq.b   .rmb_down
	bra.b   .move
.up:
	and.w   #$007f,d0
	cmp.w   #IECODE_LBUTTON,d0
	beq.b   .lmb_up
	cmp.w   #IECODE_MBUTTON,d0
	beq.b   .mmb_up
	cmp.w   #IECODE_RBUTTON,d0
	beq.b   .rmb_up
	bra.b   .move
.lmb_down:
	move.l  #-1,iid_LeftButtonDown(a1)
	clr.l   iid_LeftButtonUp(a1)
	bra.b   .move
.mmb_down:
	move.l  #-1,iid_MidButtonDown(a1)
	clr.l   iid_MidButtonUp(a1)
	bra.b   .move
.rmb_down:
	move.l  #-1,iid_RightButtonDown(a1)
	clr.l   iid_RightButtonUp(a1)
	bra.b   .move
.lmb_up:
	move.l  #-1,iid_LeftButtonUp(a1)
	clr.l   iid_LeftButtonDown(a1)
	bra.b   .move
.mmb_up:
	move.l  #-1,iid_MidButtonUp(a1)
	clr.l   iid_MidButtonDown(a1)
	bra.b   .move
.rmb_up:
	move.l  #-1,iid_RightButtonUp(a1)
	clr.l   iid_RightButtonDown(a1)
.move:
	move.w  ie_X(a0),d0
	muls	d1,d0
	move.l  d0,iid_MouseX(a1)
	move.w  ie_Y(a0),d0
	muls	d1,d0
	move.l  d0,iid_MouseY(a1)
.next:
	move.l  ie_NextEvent(a0),d0
	move.l  d0,a0
	bne.b   .loop
	move.l  (sp)+,d0
	rts
