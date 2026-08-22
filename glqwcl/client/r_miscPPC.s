##
## Quake for AMIGA
##
## d_miscPPC.s
##
## Define WOS for PowerOpen ABI, otherwise SVR4-ABI is used.
##

.set NOLR,1
.include        "macrosPPC.i"

#
# external references
#

	xrefa	vright
	xrefa	vup
	xrefa	vpn




###########################################################################
#
#       void TransformVector (vec3_t in, vec3_t out)
#
###########################################################################

	funcdef	TransformVector

	lxa     r5,vright
	lfs     f1,0(r3)
	lfs     f4,0(r5)
	fmuls   f0,f1,f4
	lfs     f2,4(r3)
	lfs     f5,4(r5)
	fmadds  f0,f2,f5,f0
	lfs     f3,8(r3)
	lfs     f6,8(r5)
	fmadds  f0,f3,f6,f0
	stfs    f0,0(r4)
	lxa     r5,vup
	lfs     f4,0(r5)
	fmuls   f0,f1,f4
	lfs     f5,4(r5)
	fmadds  f0,f2,f5,f0
	lfs     f6,8(r5)
	fmadds  f0,f3,f6,f0
	stfs    f0,4(r4)
	lxa     r5,vpn
	lfs     f4,0(r5)
	fmuls   f0,f1,f4
	lfs     f5,4(r5)
	fmadds  f0,f2,f5,f0
	lfs     f6,8(r5)
	fmadds  f0,f3,f6,f0
	stfs    f0,8(r4)
	blr

	funcend	TransformVector
