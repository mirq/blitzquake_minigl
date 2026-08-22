/*

MiniGL extensions - direct vertexbuffer construction with quake polys and interface for fake multitexturing

*/

#pragma amiga-align

#include <Warp3D/Warp3D.h>

#pragma default-align

#include <stddef.h>
#include <mgl/gl.h>

#include "quakedef.h"
#include "miniglext.h"


/******************

Fake multitexturing

*******************/


void mglMtexTV23fv(GLcontext context, GLsizei stride, glpoly_t *p)
{
	int count;
	GLfloat *v;
	MGLVertex *vert;

	v = p->verts[0];
	count = p->numverts;

	vert = &context->VertexBuffer[0];
	context->VertexBufferPointer = count;

	do
	{
	vert->bx = v[0];
	vert->by = v[1];
	vert->bz = v[2];
#if 1
	vert->v.u = (W3D_Float)v[3];
	vert->v.v = (W3D_Float)v[4];
	vert->tcoord.s = (W3D_Float)v[5];
	vert->tcoord.t = (W3D_Float)v[6];
#else
	vert->s0 = (W3D_Float)v[3];
	vert->t0 = (W3D_Float)v[4];
	vert->s1 = (W3D_Float)v[5];
	vert->t1 = (W3D_Float)v[6];
#endif
	v+= stride; vert++;
	} while (--count);
}

/**********************************************************

These function allows an array of texcoords and verts to be
built directly in the vertexbuffer in 1 go

***********************************************************/

void mglTV23fv(GLcontext context, GLsizei stride, glpoly_t *p)
{
	int count;
	GLfloat *v;
	MGLVertex *vert;

	v = p->verts[0];
	count = p->numverts;
	context->VertexBufferPointer = count;
	vert = &context->VertexBuffer[0];

	do
	{
	vert->bx = v[0];
	vert->by = v[1];
	vert->bz = v[2];

	vert->v.u = (W3D_Float)v[3];
	vert->v.v = (W3D_Float)v[4];

	v+= stride; vert++;
	} while (--count);
}

/*******************************

Same function, but for lightmaps

********************************/

void mglTV23fvLM(GLcontext context, GLsizei stride, glpoly_t *p)
{
	int count;
	GLfloat *v;
	MGLVertex *vert;

	v = p->verts[0];
	count = p->numverts;
	context->VertexBufferPointer = count;
	vert = &context->VertexBuffer[0];

	do
	{
	vert->bx = v[0];
	vert->by = v[1];
	vert->bz = v[2];

	vert->v.u = (W3D_Float)v[5];
	vert->v.v = (W3D_Float)v[6];

	v+= stride; vert++;
	} while (--count);
}

