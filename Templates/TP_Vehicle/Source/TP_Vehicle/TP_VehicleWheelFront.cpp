// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "TP_Vehicle.h"
#include "TP_VehicleWheelFront.h"

UTP_VehicleWheelFront::UTP_VehicleWheelFront()
{
	ShapeRadius = 35.f;
	ShapeWidth = 10.0f;
	bAffectedByHandbrake = false;
	SteerAngle = 50.f;
}
