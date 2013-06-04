// Copyright information can be found in the file named COPYING
// located in the root directory of this distribution.

//-----------------------------------------------------------------------------
// AABB-triangle overlap test code originally by Tomas Akenine-M�ller
//               Assisted by Pierre Terdiman and David Hunt
// http://www.cs.lth.se/home/Tomas_Akenine_Moller/code/
// Ported to TSE by BJG, 2005-4-14
// Modified to avoid a lot of copying by ASM, 2007-9-28
//-----------------------------------------------------------------------------

#ifndef _TRIBOXCHECK_H_
#define _TRIBOXCHECK_H_

#include "math/mPoint3.h"
#include "math/mBox.h"

bool triBoxOverlap(const Point3F &boxcenter, const Point3F &boxhalfsize, const Point3F triverts[3]);

/// Massage stuff into right format for triBoxOverlap test. This is really
/// just a helper function - use the other version if you want to be fast!
inline bool triBoxOverlap(Box3F box, Point3F a, Point3F b, Point3F c)
{
   Point3F halfSize(box.len_x() / 2.f, box.len_y() / 2.f, box.len_z() / 2.f);

   Point3F center;
   box.getCenter(&center);

   Point3F verts[3] = {a,b,c};

   return triBoxOverlap(center, halfSize, verts);
}

#endif