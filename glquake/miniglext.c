/*

MiniGL extensions - direct vertexbuffer construction with quake polys and interface for fake multitexturing

*/

#pragma amiga-align

#include <Warp3D/Warp3D.h>

#pragma default-align

#include "quakedef.h"

#include <stddef.h>
#include "miniglclient.h"

#include "miniglext.h"


/******************

Fake multitexturing

*******************/


void mglMtexTV23fv(GLcontext context, GLsizei stride, glpoly_t *p)
{
	int count;
	GLfloat *v;
#ifdef MINIGL_DISPATCH_CLIENT

	(void)context;
	v = p->verts[0];
	count = p->numverts;

	while (count--)
	{
		glMultiTexCoord2fARB(GL_TEXTURE0_ARB, v[3], v[4]);
		glMultiTexCoord2fARB(GL_TEXTURE1_ARB, v[5], v[6]);
		glVertex3f(v[0], v[1], v[2]);
		v += stride;
	}
#else
	MGLVertex *vert;

	v = p->verts[0];
	count = p->numverts;

	vert = &context->VertexBuffer[0];
	context->VertexBufferPointer = count;

	do
	{
	vert->bx = (GLfloat)v[0];
	vert->by = (GLfloat)v[1];
	vert->bz = (GLfloat)v[2];
#if 0
	vert->s0 = (GLfloat)v[3];
	vert->t0 = (GLfloat)v[4];
	vert->s1 = (GLfloat)v[5];
	vert->t1 = (GLfloat)v[6];
#else
	vert->v.u = (GLfloat)v[3];
	vert->v.v = (GLfloat)v[4];
	vert->tcoord.s = (GLfloat)v[5];
	vert->tcoord.t = (GLfloat)v[6];
#endif
	v+=stride; vert++;
	} while (--count);
#endif
}

/**********************************************************

These function allows an array of texcoords and verts to be
built directly in the vertexbuffer in 1 go

***********************************************************/

void mglTV23fv(GLcontext context, GLsizei stride, glpoly_t *p)
{
	int count;
	GLfloat *v;
#ifdef MINIGL_DISPATCH_CLIENT

	(void)context;
	v = p->verts[0];
	count = p->numverts;

	while (count--)
	{
		glTexCoord2f(v[3], v[4]);
		glVertex3f(v[0], v[1], v[2]);
		v += stride;
	}
#else
	MGLVertex *vert;

	v = p->verts[0];
	count = p->numverts;

	vert = &context->VertexBuffer[0];
	context->VertexBufferPointer = count;

	do
	{
	vert->bx = (GLfloat)v[0];
	vert->by = (GLfloat)v[1];
	vert->bz = (GLfloat)v[2];

	vert->v.u = (W3D_Float)v[3];
	vert->v.v = (W3D_Float)v[4];

	v+=stride; vert++;
	} while (--count);
#endif
}

/*******************************

Same function, but for lightmaps

********************************/

void mglTV23fvLM(GLcontext context, GLsizei stride, glpoly_t *p)
{
	int count;
	GLfloat *v;
#ifdef MINIGL_DISPATCH_CLIENT

	(void)context;
	v = p->verts[0];
	count = p->numverts;

	while (count--)
	{
		glTexCoord2f(v[5], v[6]);
		glVertex3f(v[0], v[1], v[2]);
		v += stride;
	}
#else
	MGLVertex *vert;

	v = p->verts[0];
	count = p->numverts;

	vert = &context->VertexBuffer[0];
	context->VertexBufferPointer = count;

	do
	{
	vert->bx = (GLfloat)v[0];
	vert->by = (GLfloat)v[1];
	vert->bz = (GLfloat)v[2];

	vert->v.u = (W3D_Float)v[5];
	vert->v.v = (W3D_Float)v[6];

	v+=stride; vert++;
	} while (--count);
#endif
}
