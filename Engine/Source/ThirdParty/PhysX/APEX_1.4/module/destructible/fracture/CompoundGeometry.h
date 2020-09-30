/*
 * Copyright (c) 2008-2015, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited.
 */


#include "RTdef.h"
#if RT_COMPILE
#ifndef COMPOUND_GEOMETRY
#define COMPOUND_GEOMETRY

#include <PxVec3.h>
#include <PxPlane.h>
#include <PsArray.h>

#include "CompoundGeometryBase.h"

namespace nvidia
{
namespace fracture
{

class CompoundGeometry : public nvidia::fracture::base::CompoundGeometry
{
};

}
}

#endif
#endif