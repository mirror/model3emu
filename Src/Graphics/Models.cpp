/**
 ** Supermodel
 ** A Sega Model 3 Arcade Emulator.
 ** Copyright 2011 Bart Trzynadlowski, Nik Henson 
 **
 ** This file is part of Supermodel.
 **
 ** Supermodel is free software: you can redistribute it and/or modify it under
 ** the terms of the GNU General Public License as published by the Free 
 ** Software Foundation, either version 3 of the License, or (at your option)
 ** any later version.
 **
 ** Supermodel is distributed in the hope that it will be useful, but WITHOUT
 ** ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 ** FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 ** more details.
 **
 ** You should have received a copy of the GNU General Public License along
 ** with Supermodel.  If not, see <http://www.gnu.org/licenses/>.
 **/
 
/*
 * Models.cpp
 *
 * Model parsing, caching, and drawing.
 *
 * TO-DO List:
 * -----------
 * - If vertex normals aren't offset from polygon normals, would that improve
 *   specular lighting?
 * - Check to see if vertices in LA Machineguns and Dirt Devils contain color
 *   values rather than normals.
 * - More should be predecoded into the polygon structures, so that things like
 *   texture base coordinates are not re-decoded in two different places!
 */

#include <cmath>
#include <cstring>
#include "Supermodel.h"


/******************************************************************************
 Definitions and Constants
******************************************************************************/

/*
 * VBO Vertex Layout
 *
 * All vertex information is stored in an array of GLfloats. Offset and size
 * information is defined here for now.
 */
#define VBO_VERTEX_OFFSET_X						0	// vertex X
#define VBO_VERTEX_OFFSET_Y						1	// vertex Y
#define VBO_VERTEX_OFFSET_Z						2	// vertex Z
#define VBO_VERTEX_OFFSET_NX					3	// normal X
#define VBO_VERTEX_OFFSET_NY					4	// normal Y
#define VBO_VERTEX_OFFSET_NZ					5	// normal Z
#define VBO_VERTEX_OFFSET_R						6	// color (untextured polys) and material (textured polys) R
#define VBO_VERTEX_OFFSET_G						7	// color and material G		
#define VBO_VERTEX_OFFSET_B						8	// color and material B
#define VBO_VERTEX_OFFSET_TRANSLUCENCE			9	// translucence level (0.0 fully transparent, 1.0 opaque)
#define VBO_VERTEX_OFFSET_LIGHTENABLE			10	// lighting enabled (0.0 luminous, 1.0 light enabled)
#define VBO_VERTEX_OFFSET_SHININESS				11	// shininess (if negative, disables specular lighting)
#define VBO_VERTEX_OFFSET_FOGINTENSITY			12	// fog intensity (0.0 no fog applied, 1.0 all fog applied)
#define VBO_VERTEX_OFFSET_U						13	// texture U coordinate (in texels, relative to sub-texture)
#define VBO_VERTEX_OFFSET_V						14	// texture V coordinate
#define VBO_VERTEX_OFFSET_TEXTURE_X				15	// sub-texture parameters, X (position in overall texture map, in texels)
#define VBO_VERTEX_OFFSET_TEXTURE_Y				16	// "" Y ""
#define VBO_VERTEX_OFFSET_TEXTURE_W				17	// sub-texture parameters, width of texture in texels
#define VBO_VERTEX_OFFSET_TEXTURE_H				18	// "" height of texture in texels
#define VBO_VERTEX_OFFSET_TEXPARAMS_EN			19	// texture parameter: ==1 texturing enabled, ==0 disabled (per-polygon)
#define VBO_VERTEX_OFFSET_TEXPARAMS_TRANS		20	// texture parameter: >=0 use transparency bit, <0 no transparency (per-polygon)
#define VBO_VERTEX_OFFSET_TEXPARAMS_UWRAP		21	// texture parameters: U wrap mode: ==1 mirrored repeat, ==0 normal repeat
#define VBO_VERTEX_OFFSET_TEXPARAMS_VWRAP		22	// "" V wrap mode ""
#define VBO_VERTEX_OFFSET_TEXFORMAT				23	// texture format 0-7 (also ==0 indicates contour texture - see also texParams.trans)
#define VBO_VERTEX_OFFSET_TEXMAP                24  // texture map number
#define VBO_VERTEX_SIZE							25	// total size (may include padding for alignment)


/******************************************************************************
 Math Routines
******************************************************************************/

// Macro to generate column-major (OpenGL) index from y,x subscripts
#define CMINDEX(y,x)	(x*4+y)

static void CrossProd(GLfloat out[3], GLfloat a[3], GLfloat b[3])
{
	out[0] = a[1]*b[2]-a[2]*b[1];
	out[1] = a[2]*b[0]-a[0]*b[2];
	out[2] = a[0]*b[1]-a[1]*b[0];
}

// 3x3 matrix used (upper-left of m[])
static void MultMat3Vec3(GLfloat out[3], GLfloat m[4*4], GLfloat v[3])
{
	out[0] = m[CMINDEX(0,0)]*v[0]+m[CMINDEX(0,1)]*v[1]+m[CMINDEX(0,2)]*v[2];
	out[1] = m[CMINDEX(1,0)]*v[0]+m[CMINDEX(1,1)]*v[1]+m[CMINDEX(1,2)]*v[2];
	out[2] = m[CMINDEX(2,0)]*v[0]+m[CMINDEX(2,1)]*v[1]+m[CMINDEX(2,2)]*v[2];
}

static GLfloat Sign(GLfloat x)
{
	if (x > 0.0f)
		return 1.0f;
	else if (x < 0.0f)
		return -1.0f;
	return 0.0f;
}

// Inverts and transposes a 3x3 matrix (upper-left of the 4x4), returning a 
// 4x4 matrix with the extra components undefined (do not use them!)
static void InvertTransposeMat3(GLfloat out[4*4], GLfloat m[4*4])
{
	GLfloat	invDet;
	GLfloat	a00 = m[CMINDEX(0,0)], a01 = m[CMINDEX(0,1)], a02 = m[CMINDEX(0,2)];
	GLfloat	a10 = m[CMINDEX(1,0)], a11 = m[CMINDEX(1,1)], a12 = m[CMINDEX(1,2)];
	GLfloat	a20 = m[CMINDEX(2,0)], a21 = m[CMINDEX(2,1)], a22 = m[CMINDEX(2,2)];
	
	invDet = 1.0f/(a00*(a22*a11-a21*a12)-a10*(a22*a01-a21*a02)+a20*(a12*a01-a11*a02));
	out[CMINDEX(0,0)] = invDet*(a22*a11-a21*a12);		out[CMINDEX(1,0)] = invDet*(-(a22*a01-a21*a02));	out[CMINDEX(2,0)] = invDet*(a12*a01-a11*a02);
	out[CMINDEX(0,1)] = invDet*(-(a22*a10-a20*a12));	out[CMINDEX(1,1)] = invDet*(a22*a00-a20*a02);		out[CMINDEX(2,1)] = invDet*(-(a12*a00-a10*a02));
	out[CMINDEX(0,2)] = invDet*(a21*a10-a20*a11);		out[CMINDEX(1,2)] = invDet*(-(a21*a00-a20*a01));	out[CMINDEX(2,2)] = invDet*(a11*a00-a10*a01);
}

static void PrintMatrix(GLfloat m[4*4])
{
	for (int i = 0; i < 3; i++)
	{
		for (int j = 0; j < 3; j++)
			printf("%g\t", m[CMINDEX(i,j)]);
		printf("\n");
	}
}


/******************************************************************************
 Display Lists
 
 Every instance of a model encountered in the scene database during rendering
 is stored in the display list along with its current transformation matrices
 and other state information. Display lists are bound to model caches for 
 performance: only one VBO has to be bound for an entire display list.
 
 Binding display lists to model caches may cause priority problems among 
 alpha polygons. Therefore, it may be necessary in the future to decouple them.
******************************************************************************/		
		
// Draws the display list
void CRender3D::DrawDisplayList(ModelCache *Cache, POLY_STATE state)
{
	DisplayList	*D;
	
	// Bind and activate VBO (pointers activate currently bound VBO)
	glBindBuffer(GL_ARRAY_BUFFER, Cache->vboID);
	glVertexPointer(3, GL_FLOAT, VBO_VERTEX_SIZE*sizeof(GLfloat), (GLvoid *) (VBO_VERTEX_OFFSET_X*sizeof(GLfloat))); 
	glNormalPointer(GL_FLOAT, VBO_VERTEX_SIZE*sizeof(GLfloat), (GLvoid *) (VBO_VERTEX_OFFSET_NX*sizeof(GLfloat))); 
	glTexCoordPointer(2, GL_FLOAT, VBO_VERTEX_SIZE*sizeof(GLfloat), (GLvoid *) (VBO_VERTEX_OFFSET_U*sizeof(GLfloat)));
	glColorPointer(3, GL_FLOAT, VBO_VERTEX_SIZE*sizeof(GLfloat), (GLvoid *) (VBO_VERTEX_OFFSET_R*sizeof(GLfloat)));
	if (subTextureLoc != -1)   glVertexAttribPointer(subTextureLoc, 4, GL_FLOAT, GL_FALSE, VBO_VERTEX_SIZE*sizeof(GLfloat), (GLvoid *) (VBO_VERTEX_OFFSET_TEXTURE_X*sizeof(GLfloat)));
	if (texParamsLoc != -1)    glVertexAttribPointer(texParamsLoc, 4, GL_FLOAT, GL_FALSE, VBO_VERTEX_SIZE*sizeof(GLfloat), (GLvoid *) (VBO_VERTEX_OFFSET_TEXPARAMS_EN*sizeof(GLfloat)));
	if (texFormatLoc != -1)    glVertexAttribPointer(texFormatLoc, 1, GL_FLOAT, GL_FALSE, VBO_VERTEX_SIZE*sizeof(GLfloat), (GLvoid *) (VBO_VERTEX_OFFSET_TEXFORMAT*sizeof(GLfloat)));
	if (texMapLoc != -1)       glVertexAttribPointer(texMapLoc, 1, GL_FLOAT, GL_FALSE, VBO_VERTEX_SIZE*sizeof(GLfloat), (GLvoid *) (VBO_VERTEX_OFFSET_TEXMAP*sizeof(GLfloat)));
	if (transLevelLoc != -1)   glVertexAttribPointer(transLevelLoc, 1, GL_FLOAT, GL_FALSE, VBO_VERTEX_SIZE*sizeof(GLfloat), (GLvoid *) (VBO_VERTEX_OFFSET_TRANSLUCENCE*sizeof(GLfloat)));
	if (lightEnableLoc != -1)  glVertexAttribPointer(lightEnableLoc, 1, GL_FLOAT, GL_FALSE, VBO_VERTEX_SIZE*sizeof(GLfloat), (GLvoid *) (VBO_VERTEX_OFFSET_LIGHTENABLE*sizeof(GLfloat)));
	if (shininessLoc != -1)    glVertexAttribPointer(shininessLoc, 1, GL_FLOAT, GL_FALSE, VBO_VERTEX_SIZE*sizeof(GLfloat), (GLvoid *) (VBO_VERTEX_OFFSET_SHININESS*sizeof(GLfloat)));
	if (fogIntensityLoc != -1) glVertexAttribPointer(fogIntensityLoc, 1, GL_FLOAT, GL_FALSE, VBO_VERTEX_SIZE*sizeof(GLfloat), (GLvoid *) (VBO_VERTEX_OFFSET_FOGINTENSITY*sizeof(GLfloat)));
	
	// Set up state
	if (state == POLY_STATE_ALPHA)
	{
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	}
	else
	{
		glDisable(GL_BLEND);
	}
	
	// Draw if there are items in the list
	D = Cache->ListHead[state];
	while (D != NULL)
	{	
		if (D->isViewport)
		{
			if (D->next != NULL)	// if nothing follows, no point in doing this
			{
				if (!D->next->isViewport)
				{
					if (lightingLoc != -1)         glUniform3fv(lightingLoc, 2, D->Data.Viewport.lightingParams);
					if (projectionMatrixLoc != -1) glUniformMatrix4fv(projectionMatrixLoc, 1, GL_FALSE, D->Data.Viewport.projectionMatrix);
       				glFogf(GL_FOG_DENSITY, D->Data.Viewport.fogParams[3]);
       				glFogf(GL_FOG_START, D->Data.Viewport.fogParams[4]);
       				glFogfv(GL_FOG_COLOR, &(D->Data.Viewport.fogParams[0]));
       				if (spotEllipseLoc != -1)      glUniform4fv(spotEllipseLoc, 1, D->Data.Viewport.spotEllipse);
       				if (spotRangeLoc != -1)        glUniform2fv(spotRangeLoc, 1, D->Data.Viewport.spotRange);
       				if (spotColorLoc != -1)        glUniform3fv(spotColorLoc, 1, D->Data.Viewport.spotColor);
       				glViewport(D->Data.Viewport.x, D->Data.Viewport.y, D->Data.Viewport.width, D->Data.Viewport.height);
       			}
       		}
       	}
       	else
       	{
       		GLint	frontFace;
       		
       		if (D->Data.Model.frontFace == -GL_CW)	// no backface culling (all normals have lost their Z component)
       			glDisable(GL_CULL_FACE);
       		else									// use appropriate winding convention
       		{
       			glGetIntegerv(GL_FRONT_FACE, &frontFace);
       			if (frontFace != D->Data.Model.frontFace)
	       			glFrontFace(D->Data.Model.frontFace);
	       	}
       			
			if (modelViewMatrixLoc != -1)
				glUniformMatrix4fv(modelViewMatrixLoc, 1, GL_FALSE, D->Data.Model.modelViewMatrix);
			glDrawArrays(GL_TRIANGLES, D->Data.Model.index, D->Data.Model.numVerts);
			
			if (D->Data.Model.frontFace == -GL_CW)
       			glEnable(GL_CULL_FACE);
		}
		
		D = D->next;
	}
}

// Appends an instance of a model or viewport to the display list, copying over the required state information
bool CRender3D::AppendDisplayList(ModelCache *Cache, bool isViewport, const struct VBORef *Model)
{
	int	lm, i;
	
	if ((Cache->listSize+2) > Cache->maxListSize)	// a model may have 2 states (viewports are added to both display lists)
		return FAIL;
		//return ErrorLog("Display list is full.");
	
	// Insert states into the display list
	for (i = 0; i < 2; i++)
	{
		if (isViewport)
		{
			// Get index for new display list item and advance to next one
			lm = Cache->listSize++;
			
			// Viewport parameters
			Cache->List[lm].Data.Viewport.x = viewportX;
			Cache->List[lm].Data.Viewport.y = viewportY;
			Cache->List[lm].Data.Viewport.width = viewportWidth;
			Cache->List[lm].Data.Viewport.height = viewportHeight;
			
			// Copy over lighting and fog state
			memcpy(Cache->List[lm].Data.Viewport.lightingParams, lightingParams, sizeof(lightingParams));
			memcpy(Cache->List[lm].Data.Viewport.fogParams, fogParams, sizeof(fogParams));
			memcpy(Cache->List[lm].Data.Viewport.spotEllipse, spotEllipse, sizeof(spotEllipse));
			memcpy(Cache->List[lm].Data.Viewport.spotRange, spotRange, sizeof(spotRange));
			memcpy(Cache->List[lm].Data.Viewport.spotColor, spotColor, sizeof(spotColor));
			
			// Copy projection matrix
			glGetFloatv(GL_PROJECTION_MATRIX, Cache->List[lm].Data.Viewport.projectionMatrix);
		}
		else if (Model->numVerts[i] > 0)	// vertices exist for this state
		{	
			// Get index for new display list item and advance to next one
			lm = Cache->listSize++;
			
			// Point to VBO for current model and state
			Cache->List[lm].Data.Model.index = Model->index[i];
			Cache->List[lm].Data.Model.numVerts = Model->numVerts[i];
			
			// Copy modelview matrix
			glGetFloatv(GL_MODELVIEW_MATRIX, Cache->List[lm].Data.Model.modelViewMatrix);
			
			/*
			 * Determining if winding was reversed (but not polygon normal):
			 *
			 * Real3D performs backface culling in view space based on the
			 * polygon normal unlike OpenGL, which uses the computed normal
			 * from the edges (in screen space) of the polygon. Consequently,
			 * it is possible to create a matrix that mirrors an axis without
			 * rotating the normal, which in turn flips the polygon winding and
			 * makes it invisible in OpenGL but not on Real3D, because the 
			 * normal is still facing the right way.
			 *
			 * To detect such a situation, we create a fictitious polygon with
			 * edges X = [1 0 0] and Y = [0 1 0], with normal Z = [0 0 1]. We 
			 * rotate the edges by the matrix then compute a normal P, which is
			 * what OpenGL would use for culling. We transform the normal Z by
			 * the normal matrix (normals are special and must be multiplied by
			 * Transpose(Inverse(M)), not M). If the Z components of P and the
			 * transformed Z vector have opposite signs, the OpenGL winding 
			 * mode must be switched in order to draw correctly. The X axis may
			 * have been flipped, for example, changing the winding mode while
			 * leaving the polygon normal unaffected. OpenGL would erroneously
			 * discard these polygons, so we flip the winding convention, 
			 * ensuring they are drawn correctly.
			 *
			 * We have to adjust the Z vector (fictitious normal) by the sign
			 * of the Z axis specified by the coordinate system matrix (#0).
			 * This is described further in InsertPolygon(), where the vertices
			 * are ordered in clockwise fashion.
			 */
			GLfloat	x[3] = { 1.0f, 0.0f, 0.0f };
			GLfloat	y[3] = { 0.0f, 1.0f, 0.0f };
			GLfloat z[3] = { 0.0f, 0.0f, -1.0f*matrixBasePtr[0x5] };
			GLfloat	m[4*4];
			GLfloat	xT[3], yT[3], zT[3], pT[3];

			InvertTransposeMat3(m,Cache->List[lm].Data.Model.modelViewMatrix);
			MultMat3Vec3(xT,Cache->List[lm].Data.Model.modelViewMatrix,x);
			MultMat3Vec3(yT,Cache->List[lm].Data.Model.modelViewMatrix,y);
			MultMat3Vec3(zT,m,z);
			CrossProd(pT,xT,yT);
			
			float s = Sign(zT[2]*pT[2]);
			if (s < 0.0f)
				Cache->List[lm].Data.Model.frontFace = GL_CCW;
			else if (s > 0.0f)
				Cache->List[lm].Data.Model.frontFace = GL_CW;
			else
				Cache->List[lm].Data.Model.frontFace = -GL_CW;
		}
		else	// nothing to do, continue loop
			continue;
			
		// Update list pointers and set list node type
		Cache->List[lm].isViewport = isViewport;
		Cache->List[lm].next = NULL;	// current end of list
		if (Cache->ListHead[i] == NULL)
		{
			Cache->ListHead[i] = &(Cache->List[lm]);
			Cache->ListTail[i] = Cache->ListHead[i];
		}
		else
		{
			Cache->ListTail[i]->next = &(Cache->List[lm]);
			Cache->ListTail[i] = &(Cache->List[lm]);
		}
	}
		
	return OKAY;
}

// Clears the display list in preparation for a new frame
void CRender3D::ClearDisplayList(ModelCache *Cache)
{
	Cache->listSize = 0;
	for (int i = 0; i < 2; i++)
	{
		Cache->ListHead[i] = NULL;
		Cache->ListTail[i] = NULL;
	}
}


/******************************************************************************
 Model Caching
 
 Note that as vertices are inserted into the appropriate local vertex buffer
 (sorted by polygon state -- alpha and normal), the VBO index is advanced to 
 reserve space and does not correspond to the actual position of each vertex.
 Vertices are copied in batches sorted by state when the model is complete.
******************************************************************************/

// Inserts a vertex into the local vertex buffer, incrementing both the local and VBO pointers. The normal is scaled by normFlip.
void CRender3D::InsertVertex(ModelCache *Cache, const Vertex *V, const Poly *P, float normFlip)
{
	GLfloat		r, g, b;
	GLfloat		translucence, fogIntensity, texWidth, texHeight, texBaseX, texBaseY, contourProcessing;
	unsigned	baseIdx, texFormat, texEnable, lightEnable, modulate, colorIdx;
	TexSheet   *texSheet;
	int			s, texPage, shininess;
	
	// Texture selection
	texEnable	= P->header[6]&0x04000000;
	texFormat	= (P->header[6]>>7)&7;
	texWidth 	= (GLfloat) (32<<((P->header[3]>>3)&7));
    texHeight	= (GLfloat) (32<<((P->header[3]>>0)&7));
    texPage		= (P->header[4]&0x40) ? 1024 : 0;	// treat texture page as Y coordinate
    texSheet    = fmtToTexSheet[texFormat];         // get X&Y offset of texture sheet within texture map
	texBaseX 	= (GLfloat) (texSheet->xOffset + (((32*(((P->header[4]&0x1F)<<1)|((P->header[5]>>7)&1))) + (int)texOffsetXY[0])&2047));
	texBaseY 	= (GLfloat) (texSheet->yOffset + (((32*(P->header[5]&0x1F)+texPage) + (int)texOffsetXY[1])&2047));
		
	/*
	 * Lighting and Color Modulation:
	 *
	 * It appears that there is a modulate bit which causes the polygon color
	 * to be multiplied by texel colors. However, if polygons are luminous,
	 * this appears to be disabled (not quite correct yet, though).
	 */
	 
	lightEnable = !(P->header[6]&0x00010000);
	//modulate	= !(P->header[4]&0x80);
	modulate	= P->header[3]&0x80;	// seems to work better
	
	// Material color
	if ((P->header[1]&2) == 0)
	{
		colorIdx = ((P->header[4]>>20)&0x7FF) - 0;
		b = (GLfloat) (polyRAM[0x400+colorIdx]&0xFF) * (1.0f/255.0f);
		g = (GLfloat) ((polyRAM[0x400+colorIdx]>>8)&0xFF) * (1.0f/255.0f);
		r = (GLfloat) ((polyRAM[0x400+colorIdx]>>16)&0xFF) * (1.0f/255.0f);
	}
	else
	{
		// Colors are 8-bit	(almost certainly true, see Star Wars)
		r = (GLfloat) (P->header[4]>>24) * (1.0f/255.0f);
		g = (GLfloat) ((P->header[4]>>16)&0xFF) * (1.0f/255.0f);
		b = (GLfloat) ((P->header[4]>>8)&0xFF) * (1.0f/255.0f);
	}

	// Determine modulation settings
	if (texEnable)
	{
		//if (!lightEnable|| !modulate)
		if (!modulate)
			r = g = b = 1.0f;
	}
	
	// Specular shininess
	shininess = (P->header[0]>>26)&0x3F;
	//shininess = (P->header[0]>>28)&0xF;
	//if (shininess)
	//	printf("%X\n", shininess);
	if (!(P->header[0]&0x80) || (shininess == 0))	// bit 0x80 seems to enable specular lighting
		shininess = -1;	// disable

#if 0	
	if (texFormat==5)//texFormat==6||texFormat==2)
	{
		//printf("%03X\n", P->header[4]>>8);
		//texEnable=0;
		g=b=1.0;
		r=1.0f;
	}
#endif
#if 0
	int testWord = 0;
	int testBit = 7;
	//if ((P->header[testWord]&(1<<testBit)))
	if (((P->header[0]>>24) & 0x3) != 0)
	//if (!((P->header[0]>>26) & 0x3F) && (P->header[0]&0x80))
	{
		texEnable = 0;
		r=b=0;
		g=1.0f;
		g = ((P->header[0]>>26)&0x3F) * (1.0f/64.0f);
		//if (!lightEnable)
		//	b=1.0f;
		lightEnable=0;
	}
#endif

	// Determine whether polygon is translucent
	translucence = (GLfloat) ((P->header[6]>>18)&0x1F) * (1.0f/31.0f);
	if ((P->header[6]&0x00800000))	// if set, polygon is opaque
		translucence = 1.0f;
		
	// Fog intensity (for luminous polygons)
	fogIntensity = (GLfloat) ((P->header[6]>>11)&0x1F) * (1.0f/31.0f);
	if (!(P->header[6]&0x00010000))	// if not luminous, always use full fog intensity
		fogIntensity = 1.0f;
		
	/*
	 * Contour processing. Any alpha value sufficiently close to 0 seems to
	 * cause pixels to be discarded entirely on Model 3 (no modification of the
	 * depth buffer). Strictly speaking, only T1RGB5 format textures are
	 * "contour textures" (in Real3D lingo), we enable contour processing for
	 * alpha blended texture formats as well in order to discard fully
	 * transparent pixels.
	 */
	if ((P->header[6]&0x80000000) || (texFormat==7) ||	// contour processing enabled or RGBA4 texture
		((texFormat==1) && (P->header[6]&2)) ||			// A4L4 interleaved (these formats are not being interpreted correctly, see Scud Race clock tower)
		((texFormat==3) && (P->header[6]&4)))			// A4L4 interleaved
		contourProcessing = 1.0f;
	else
		contourProcessing = -1.0f;

	// Store to local vertex buffer
	s = P->state;
	baseIdx = Cache->curVertIdx[s]*VBO_VERTEX_SIZE;
	
	Cache->verts[s][baseIdx + VBO_VERTEX_OFFSET_X] = V->x;
	Cache->verts[s][baseIdx + VBO_VERTEX_OFFSET_Y] = V->y;
	Cache->verts[s][baseIdx + VBO_VERTEX_OFFSET_Z] = V->z;
	Cache->verts[s][baseIdx + VBO_VERTEX_OFFSET_R] = r;
	Cache->verts[s][baseIdx + VBO_VERTEX_OFFSET_G] = g;
	Cache->verts[s][baseIdx + VBO_VERTEX_OFFSET_B] = b;
	Cache->verts[s][baseIdx + VBO_VERTEX_OFFSET_TRANSLUCENCE] = translucence;
	Cache->verts[s][baseIdx + VBO_VERTEX_OFFSET_LIGHTENABLE] = lightEnable ? 1.0f : 0.0f;
	Cache->verts[s][baseIdx + VBO_VERTEX_OFFSET_SHININESS] = (GLfloat) shininess;
	Cache->verts[s][baseIdx + VBO_VERTEX_OFFSET_FOGINTENSITY] = fogIntensity;
	
	Cache->verts[s][baseIdx + VBO_VERTEX_OFFSET_NX] = V->n[0]*normFlip;
	Cache->verts[s][baseIdx + VBO_VERTEX_OFFSET_NY] = V->n[1]*normFlip;
	Cache->verts[s][baseIdx + VBO_VERTEX_OFFSET_NZ] = V->n[2]*normFlip;	
	
	Cache->verts[s][baseIdx + VBO_VERTEX_OFFSET_U] = V->u;
	Cache->verts[s][baseIdx + VBO_VERTEX_OFFSET_V] = V->v;
	Cache->verts[s][baseIdx + VBO_VERTEX_OFFSET_TEXTURE_X] = texBaseX;
	Cache->verts[s][baseIdx + VBO_VERTEX_OFFSET_TEXTURE_Y] = texBaseY;
	Cache->verts[s][baseIdx + VBO_VERTEX_OFFSET_TEXTURE_W] = texWidth;
	Cache->verts[s][baseIdx + VBO_VERTEX_OFFSET_TEXTURE_H] = texHeight;
	Cache->verts[s][baseIdx + VBO_VERTEX_OFFSET_TEXPARAMS_EN] = texEnable ? 1.0f : 0.0f;
	Cache->verts[s][baseIdx + VBO_VERTEX_OFFSET_TEXPARAMS_TRANS] = contourProcessing;
	Cache->verts[s][baseIdx + VBO_VERTEX_OFFSET_TEXPARAMS_UWRAP] = (P->header[2]&2) ? 1.0f : 0.0f;
	Cache->verts[s][baseIdx + VBO_VERTEX_OFFSET_TEXPARAMS_VWRAP] = (P->header[2]&1) ? 1.0f : 0.0f;
	Cache->verts[s][baseIdx + VBO_VERTEX_OFFSET_TEXFORMAT] = (float)texFormat;
	Cache->verts[s][baseIdx + VBO_VERTEX_OFFSET_TEXMAP] = (float)texSheet->mapNum;

	Cache->curVertIdx[s]++;
	Cache->vboCurOffset += VBO_VERTEX_SIZE*sizeof(GLfloat);
}

bool CRender3D::InsertPolygon(ModelCache *Cache, const Poly *P)
{
	GLfloat	n[3], v1[3], v2[3], normZFlip;
	int		i;
	bool	doubleSided;
	
	// Bounds testing: up to 12 triangles will be inserted (worst case: double sided quad is 6 triangles)
	if ((Cache->curVertIdx[P->state]+6*2) >= Cache->maxVertIdx)
		return ErrorLocalVertexOverflow();	// local buffers are not expected to overflow
	if ((Cache->vboCurOffset+6*2*VBO_VERTEX_SIZE*sizeof(GLfloat)) >= Cache->vboMaxOffset)
		return FAIL;	// this just indicates we may need to re-cache
		
	// Is the polygon double sided?
	doubleSided = (P->header[1]&0x10) ? true : false;
	
	/*
 	 * Determine polygon winding by taking cross product of vectors formed from
	 * 3 polygon vertices (the middle one being the origin). In reality, back-
	 * face culling is determined by the polygon normal and two-sided polygons
	 * exist. This is just a temporary hack.
	 *
	 * If the cross product points the same way as the normal, the winding is
	 * clockwise and can be kept, otherwise it must be reversed.
	 *
	 * NOTE: This assumes that the Model 3 base coordinate system's Z axis
	 * (into the screen) is -1, like OpenGL. For some games (eg., Lost World),
	 * this is not the case. Assuming games consistently use the same type of
	 * coordinate system matrix, it seems that inverting the whole dot product
	 * when Z is positive helps. I don't understand exactly why... but it has
	 * to do with using the correct Z convention to identify a vector pointing
	 * toward or away from the screen.
	 */
	v1[0] = P->Vert[0].x-P->Vert[1].x;
	v1[1] = P->Vert[0].y-P->Vert[1].y;
	v1[2] = P->Vert[0].z-P->Vert[1].z;
	v2[0] = P->Vert[2].x-P->Vert[1].x;
	v2[1] = P->Vert[2].y-P->Vert[1].y;
	v2[2] = P->Vert[2].z-P->Vert[1].z;
	CrossProd(n,v1,v2);
	
	normZFlip = -1.0f*matrixBasePtr[0x5];	// coordinate system m13 component
	
	if (normZFlip*(n[0]*P->n[0]+n[1]*P->n[1]+n[2]*P->n[2]) >= 0.0)	// clockwise winding confirmed
	{
		// Store the first triangle
		for (i = 0; i < 3; i++)
		{
			InsertVertex(Cache, &(P->Vert[i]), P, 1.0f);
		}
		
		if (doubleSided)	// store backside as counter-clockwise
		{
			for (i = 2; i >=0; i--)
			{
				InsertVertex(Cache, &(P->Vert[i]), P, -1.0f);
			}
		}
	
		// If quad, second triangle will just be vertices 1, 3, 4
		if (P->numVerts == 4)
		{
			InsertVertex(Cache, &(P->Vert[0]), P, 1.0f);
			InsertVertex(Cache, &(P->Vert[2]), P, 1.0f);
			InsertVertex(Cache, &(P->Vert[3]), P, 1.0f);
			
			if (doubleSided)
			{
				InsertVertex(Cache, &(P->Vert[0]), P, -1.0f);
				InsertVertex(Cache, &(P->Vert[3]), P, -1.0f);
				InsertVertex(Cache, &(P->Vert[2]), P, -1.0f);
			}
		}
	}
	else	// counterclockwise winding, reverse it
	{
		for (i = 2; i >=0; i--)
		{
			InsertVertex(Cache, &(P->Vert[i]), P, 1.0f);
		}
		
		if (doubleSided)	// store backside as clockwise
		{
			for (i = 0; i < 3; i++)
			{
				InsertVertex(Cache, &(P->Vert[i]), P, -1.0f);
			}
		}
		
		if (P->numVerts == 4)
		{
			InsertVertex(Cache, &(P->Vert[0]), P, 1.0f);
			InsertVertex(Cache, &(P->Vert[3]), P, 1.0f);
			InsertVertex(Cache, &(P->Vert[2]), P, 1.0f);
			
			if (doubleSided)
			{
				InsertVertex(Cache, &(P->Vert[0]), P, -1.0f);
				InsertVertex(Cache, &(P->Vert[2]), P, -1.0f);
				InsertVertex(Cache, &(P->Vert[3]), P, -1.0f);
			}
		}
	}
	
	return OKAY;
}

// Begins caching a new model by resetting to the start of the local vertex buffer
struct VBORef *CRender3D::BeginModel(ModelCache *Cache)
{
	struct VBORef	*Model;
	
	unsigned	m = Cache->numModels;
	
	// Determine whether we've exceeded the model cache limits (caller will have to recache)
	if (m >= Cache->maxModels)
	{
		//ErrorLog("Too many %s models.", Cache->dynamic?"dynamic":"static");
		return NULL;	
	}
	
	Model = &(Cache->Models[m]);
	
	// Reset to the beginning of the local vertex buffer
	for (int i = 0; i < 2; i++)
		Cache->curVertIdx[i] = 0;
	
	// Clear the VBO reference to 0 and clear texture references
	Model->Clear();
	
	// Record starting index of first opaque polygon in VBO (alpha poly index will be re-set in EndModel())
	Model->index[POLY_STATE_NORMAL] = Cache->vboCurOffset/(VBO_VERTEX_SIZE*sizeof(GLfloat));
	Model->index[POLY_STATE_ALPHA] = Model->index[POLY_STATE_NORMAL];
	
	return Model;
}

// Uploads all vertices from the local vertex buffer to the VBO, sets up the VBO reference, updates the LUT
void CRender3D::EndModel(ModelCache *Cache, struct VBORef *Model, int lutIdx, UINT16 texOffset)
{
	int	m = Cache->numModels++;

	// Record the number of vertices, completing the VBORef
	for (int i = 0; i < 2; i++)
		Model->numVerts[i] = Cache->curVertIdx[i];

	// First alpha polygon immediately follows the normal polygons
	Model->index[POLY_STATE_ALPHA] = Model->index[POLY_STATE_NORMAL] + Model->numVerts[POLY_STATE_NORMAL];

	// Upload from local vertex buffer to real VBO
	glBindBuffer(GL_ARRAY_BUFFER, Cache->vboID);
	if (Model->numVerts[POLY_STATE_NORMAL] > 0)
		glBufferSubData(GL_ARRAY_BUFFER, Model->index[POLY_STATE_NORMAL]*VBO_VERTEX_SIZE*sizeof(GLfloat), Cache->curVertIdx[POLY_STATE_NORMAL]*VBO_VERTEX_SIZE*sizeof(GLfloat), Cache->verts[POLY_STATE_NORMAL]);
	if (Model->numVerts[POLY_STATE_ALPHA] > 0)
		glBufferSubData(GL_ARRAY_BUFFER, Model->index[POLY_STATE_ALPHA]*VBO_VERTEX_SIZE*sizeof(GLfloat), Cache->curVertIdx[POLY_STATE_ALPHA]*VBO_VERTEX_SIZE*sizeof(GLfloat), Cache->verts[POLY_STATE_ALPHA]);
		
	// Record LUT index in the model VBORef
	Model->lutIdx = lutIdx;
	
	// Texture offset of this model state
	Model->texOffset = texOffset;
	
	// Update the LUT and link up to any existing model that already exists here
	if (Cache->lut[lutIdx] >= 0)	// another texture offset state already cached
		Model->nextTexOffset = &(Cache->Models[Cache->lut[lutIdx]]);
	Cache->lut[lutIdx] = m;
}

/*
 * CacheModel():
 *
 * Decodes and caches a complete model. Returns NULL if any sort of overflow in
 * the cache occurred. In this case, the model cache should be cleared before
 * being used again because an incomplete model will be stored, wasting vertex
 * buffer space.
 *
 * A pointer to the VBO reference for the cached model is returned when
 * successful.
 */
struct VBORef *CRender3D::CacheModel(ModelCache *Cache, int lutIdx, UINT16 texOffset, const UINT32 *data)
{
	Vertex			Prev[4];	// previous vertices
	int				numPolys = 0;
	bool			done = false;
	
	// Sega Rally 2 bad models
	//if (lutIdx == 0x27a1  || lutIdx == 0x21e0)
	//	return FAIL;
		
	if (data == NULL)
		return NULL;
		
	// Start constructing a new model
	struct VBORef *Model = BeginModel(Cache);
	if (NULL == Model)
		return NULL;	// too many models!
	
	// Cache all polygons
	while (!done)
	{
		Poly		P;		// current polygon
		GLfloat		mag;
		GLfloat		uvScale;
		int			texEnable, texFormat, texWidth, texHeight, texPage, texBaseX, texBaseY;
		unsigned	i, j, vmask;
		UINT32		ix, iy, iz, it;
		
		// Set current header pointer (header is 7 words)
		P.header = data;
		data += 7;	// data will now point to first vertex
		if (P.header[6]==0)// || P.header[0]==0)
			break;
		
		// Obtain basic polygon parameters
		done = P.header[1]&4;	// last polygon?
		P.numVerts = (P.header[0]&0x40)?4:3;
			
		// Texture data
		texEnable	= P.header[6]&0x04000000;
		texFormat 	= (P.header[6]>>7)&7;
		texWidth 	= (32<<((P.header[3]>>3)&7));
    	texHeight	= (32<<((P.header[3]>>0)&7));
    	texPage		= (P.header[4]&0x40) ? 1024 : 0;	// treat texture page as Y coordinate
    	texBaseX 	= (32*(((P.header[4]&0x1F)<<1)|((P.header[5]>>7)&1))) + (int)texOffsetXY[0];
		texBaseY 	= (32*(P.header[5]&0x1F)+texPage) + (int)texOffsetXY[1];
		texBaseX	&= 2047;
		texBaseY	&= 2047;
		uvScale		= (P.header[1]&0x40)?1.0f:(1.0f/8.0f);
		
		// Determine whether this is an alpha polygon (TODO: when testing textures, test if texturing enabled? Might not matter)
		if (((P.header[6]&0x00800000)==0) ||	// translucent polygon
			(texFormat==7) ||					// RGBA4 texture
			(texFormat==4))						// A4L4 texture
			P.state = POLY_STATE_ALPHA;
		else
			P.state = POLY_STATE_NORMAL;
		if (texFormat==1) 						// A4L4 interleaved
		{
			if ((P.header[6]&2))
				P.state = POLY_STATE_ALPHA;
			else
				P.state = POLY_STATE_NORMAL;
		}
		if (texFormat==3)						// A4L4 interleaved
		{
			if ((P.header[6]&4))
				P.state = POLY_STATE_ALPHA;
			else
				P.state = POLY_STATE_NORMAL;
		}	
			
		// Decode the texture
		if (texEnable)
		{
			// If model cache is static, record texture reference in model cache entry for later decoding.
			// If cache is dynamic, or if it's not possible to record the texture reference (due to lack of
			// memory) then decode the texture now.
			if (Cache->dynamic || !Model->texRefs.AddRef(texFormat, texBaseX, texBaseY, texWidth, texHeight))
				DecodeTexture(texFormat, texBaseX, texBaseY, texWidth, texHeight);
		}
		
		// Polygon normal is in upper 24 bits: sign + 1.22 fixed point
		P.n[0] = (GLfloat) (((INT32)P.header[1])>>8) * (1.0f/4194304.0f);
		P.n[1] = (GLfloat) (((INT32)P.header[2])>>8) * (1.0f/4194304.0f);
		P.n[2] = (GLfloat) (((INT32)P.header[3])>>8) * (1.0f/4194304.0f);
		
		// Fetch reused vertices according to bitfield, then new verts
		i = 0;
		j = 0;
		vmask = 1;
		for (i = 0; i < 4; i++)		// up to 4 reused vertices
		{
			if ((P.header[0x00]&vmask))
			{
				P.Vert[j] = Prev[i];
				++j;
			}	
			vmask <<= 1;
		}
		
		for (; j < P.numVerts; j++)	// remaining vertices are new and defined here
		{
			// Fetch vertices
			ix = data[0];
			iy = data[1];
			iz = data[2];
			it = data[3];
			
			/*
			// Check for bad vertices (Sega Rally 2)
			if (((ix>>28)==7) || ((iy>>28)==7) || ((iz>>28)==7))
			{
				//printf("%X ix=%08X, iy=%08X, iz=%08X\n", lutIdx, ix, iy, iz);
				goto StopDecoding;
			}
			*/
			
			// Decode vertices
			P.Vert[j].x = (GLfloat) (((INT32)ix)>>8) * vertexFactor;
			P.Vert[j].y = (GLfloat) (((INT32)iy)>>8) * vertexFactor;
			P.Vert[j].z = (GLfloat) (((INT32)iz)>>8) * vertexFactor;
			P.Vert[j].n[0] = P.n[0]+(GLfloat)(INT8)(ix&0xFF);	// vertex normals are offset from polygon normal
			P.Vert[j].n[1] = P.n[1]+(GLfloat)(INT8)(iy&0xFF);
			P.Vert[j].n[2] = P.n[2]+(GLfloat)(INT8)(iz&0xFF);
			P.Vert[j].u = (GLfloat) ((UINT16)(it>>16)) * uvScale;	// TO-DO: might these be signed?
			P.Vert[j].v = (GLfloat) ((UINT16)(it&0xFFFF)) * uvScale;
			data += 4;
			
			// Normalize the vertex normal
			mag = sqrt(P.Vert[j].n[0]*P.Vert[j].n[0]+P.Vert[j].n[1]*P.Vert[j].n[1]+P.Vert[j].n[2]*P.Vert[j].n[2]);
			P.Vert[j].n[0] /= mag;
			P.Vert[j].n[1] /= mag;
			P.Vert[j].n[2] /= mag;
		}
		
		// Copy current vertices into previous vertex array
		for (i = 0; i < 4; i++)
			Prev[i] = P.Vert[i];
			
		// Copy this polygon into the model buffer
		if (OKAY != InsertPolygon(Cache,&P))
			return NULL;
		
		++numPolys;
	}
	
	// Finish model and enter it into the LUT
	EndModel(Cache,Model,lutIdx,texOffset);
	return Model;
}


/******************************************************************************
 Cache Management
******************************************************************************/

/*
 * Look up a model. Use this to determine if a model needs to be cached
 * (returns NULL if so).
 */
struct VBORef *CRender3D::LookUpModel(ModelCache *Cache, int lutIdx, UINT16 texOffset)
{
	int	m = Cache->lut[lutIdx];
	
	// Has any state associated with this model LUT index been cached at all?
	if (m < 0)
		return NULL;
	
	// Has the specified texture offset been cached?
	for (struct VBORef *Model = &(Cache->Models[m]); Model != NULL; Model = Model->nextTexOffset)
	{
		if (Model->texOffset == texOffset)
			return Model;
	}
	
	return NULL;	// no match found, we must cache this new model state
}

// Discard all models in the cache and the display list
void CRender3D::ClearModelCache(ModelCache *Cache)
{
	Cache->vboCurOffset = 0;
	for (int i = 0; i < 2; i++)
		Cache->curVertIdx[i] = 0;
	for (int i = 0; i < Cache->numModels; i++)
		Cache->lut[Cache->Models[i].lutIdx] = -1;

	Cache->numModels = 0;
	ClearDisplayList(Cache);
}

bool CRender3D::CreateModelCache(ModelCache *Cache, unsigned vboMaxVerts, 
								 unsigned localMaxVerts, unsigned maxNumModels, unsigned numLUTEntries, 
								 unsigned displayListSize, bool isDynamic)
{
	unsigned	i;
	int			vboBytes, localBytes;
	bool		success;
	
	Cache->dynamic = isDynamic;
	
	/*
	 * VBO allocation:
	 *
	 * Progressively smaller VBOs, in steps of localMaxVerts are allocated
	 * until successful. If the size dips below localMaxVerts, localMaxVerts is
	 * attempted as the final try.
	 */
	 
	glGetError();	// clear error flag
	glGenBuffers(1, &(Cache->vboID));
	glBindBuffer(GL_ARRAY_BUFFER, Cache->vboID);
	
	vboBytes = vboMaxVerts*VBO_VERTEX_SIZE*sizeof(GLfloat);
	localBytes = localMaxVerts*VBO_VERTEX_SIZE*sizeof(GLfloat);
	
	// Try allocating until size is 
	success = false;
	while (vboBytes >= localBytes)
	{
		glBufferData(GL_ARRAY_BUFFER, vboBytes, 0, isDynamic?GL_STREAM_DRAW:GL_STATIC_DRAW);
		if (glGetError() == GL_NO_ERROR)
		{
			success = true;
			break;
		}
		
		vboBytes -= localBytes;
	}
	
	if (!success)
	{
		// Last ditch attempt: try the local buffer size
		vboBytes = localBytes;
		glBufferData(GL_ARRAY_BUFFER, vboBytes, 0, isDynamic?GL_STREAM_DRAW:GL_STATIC_DRAW);
		if (glGetError() != GL_NO_ERROR)
			return ErrorLog("OpenGL was unable to provide a %s vertex buffer.", isDynamic?"dynamic":"static");
	}
	
	DebugLog("%s vertex buffer size: %1.2f MB", isDynamic?"Dynamic":"Static", (float)vboBytes/(float)0x100000);
	InfoLog("%s vertex buffer size: %1.2f MB", isDynamic?"Dynamic":"Static", (float)vboBytes/(float)0x100000);
	
	// Set the VBO to the size we obtained
	Cache->vboMaxOffset = vboBytes;
	Cache->vboCurOffset = 0;
	
	// Attempt to allocate space for local VBO
	for (i = 0; i < 2; i++)
	{
		Cache->verts[i] = new(std::nothrow) GLfloat[localMaxVerts*VBO_VERTEX_SIZE];
		Cache->curVertIdx[i] = 0;
	}
	Cache->maxVertIdx = localMaxVerts;
	
	// ... model array
	Cache->Models = new(std::nothrow) VBORef[maxNumModels];
	Cache->maxModels = maxNumModels;
	Cache->numModels = 0;
	
	// ... LUT
	Cache->lut = new(std::nothrow) INT16[numLUTEntries];
	Cache->lutSize = numLUTEntries;
	
	// ... display list
	Cache->List = new(std::nothrow) DisplayList[displayListSize];
	ClearDisplayList(Cache);
	Cache->maxListSize = displayListSize;
	
	// Check if memory allocation succeeded
	if ((Cache->verts[0]==NULL) || (Cache->verts[1]==NULL) || (Cache->Models==NULL) || (Cache->lut==NULL) || (Cache->List==NULL))
	{
		DestroyModelCache(Cache);
		return ErrorLog("Insufficient memory for model cache.");
	}

	// Clear LUT (MUST be done here because ClearModelCache() won't do it for dynamic models)
	for (i = 0; i < numLUTEntries; i++)
		Cache->lut[i] = -1;
		
	// All good!
	return OKAY;
}

void CRender3D::DestroyModelCache(ModelCache *Cache)
{
	glDeleteBuffers(1, &(Cache->vboID));

	for (int i = 0; i < 2; i++)
	{
		if (Cache->verts[i] != NULL)
			delete [] Cache->verts[i];
	}
	if (Cache->Models != NULL)
		delete [] Cache->Models;
	if (Cache->lut != NULL)
		delete [] Cache->lut;
	if (Cache->List != NULL)
		delete [] Cache->List;
	
	memset(Cache, 0, sizeof(ModelCache));
}
