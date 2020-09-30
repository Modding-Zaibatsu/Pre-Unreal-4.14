// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

// These are the inverse of the hidden area meshes Valve provides us for the Vive.
// i.e. a mesh that covers all pixels which *are* inputs to the distortion pass.
static const uint32 ViveHiddenAreaMeshCrc = 2526529894;
static const uint32 VisibleAreaVertexCount = 69;
static const FVector2D Vive_LeftEyeVisibleAreaPositions[] =
{
	FVector2D(1.0f, 0.1999999881f),
	FVector2D(1.0f, 0.1949999332f),
	FVector2D(0.9099999552f, 0.1099999547f),
	FVector2D(0.9159999971f, 0.3639999628f),
	FVector2D(1.0f, 0.1999999881f),
	FVector2D(0.9099999552f, 0.1099999547f),
	FVector2D(0.9099999552f, 0.1099999547f),
	FVector2D(0.9159999971f, 0.3639999628f),
	FVector2D(0.8100000505f, 0.04499995708f),
	FVector2D(0.8100000505f, 0.04499995708f),
	FVector2D(0.9159999971f, 0.3639999628f),
	FVector2D(0.6799999361f, 0.0f),
	FVector2D(0.6799999361f, 0.0f),
	FVector2D(0.9159999971f, 0.3639999628f),
	FVector2D(0.3699999933f, 0.0f),
	FVector2D(0.8949999695f, 0.4949999452f),
	FVector2D(0.9159999971f, 0.3639999628f),
	FVector2D(0.3699999933f, 0.0f),
	FVector2D(0.3699999933f, 0.0f),
	FVector2D(0.8949999695f, 0.4949999452f),
	FVector2D(0.2399999981f, 0.04999995232f),
	FVector2D(0.2399999981f, 0.04999995232f),
	FVector2D(0.8949999695f, 0.4949999452f),
	FVector2D(0.1320000176f, 0.1229999661f),
	FVector2D(0.1320000176f, 0.1229999661f),
	FVector2D(0.8949999695f, 0.4949999452f),
	FVector2D(0.05999999094f, 0.1999999881f),
	FVector2D(0.05999999094f, 0.1999999881f),
	FVector2D(0.8949999695f, 0.4949999452f),
	FVector2D(0.0f, 0.2949999571f),
	FVector2D(0.0f, 0.2949999571f),
	FVector2D(0.8949999695f, 0.4949999452f),
	FVector2D(0.0f, 0.6999999285f),
	FVector2D(0.0f, 0.6999999285f),
	FVector2D(0.8949999695f, 0.4949999452f),
	FVector2D(0.04000001001f, 0.7680000067f),
	FVector2D(0.04000001001f, 0.7680000067f),
	FVector2D(0.8949999695f, 0.4949999452f),
	FVector2D(0.09000002193f, 0.8329999447f),
	FVector2D(0.09000002193f, 0.8329999447f),
	FVector2D(0.8949999695f, 0.4949999452f),
	FVector2D(0.1899999862f, 0.9149999619f),
	FVector2D(0.1899999862f, 0.9149999619f),
	FVector2D(0.8949999695f, 0.4949999452f),
	FVector2D(0.2710000162f, 0.9599999189f),
	FVector2D(0.2710000162f, 0.9599999189f),
	FVector2D(0.8949999695f, 0.4949999452f),
	FVector2D(0.388f, 1.0f),
	FVector2D(0.388f, 1.0f),
	FVector2D(0.8949999695f, 0.4950000048f),
	FVector2D(0.6560000067f, 1.0f),
	FVector2D(0.9129999762f, 0.628000021f),
	FVector2D(0.8949999695f, 0.4949999452f),
	FVector2D(0.6560000067f, 1.0f),
	FVector2D(0.6560000067f, 1.0f),
	FVector2D(0.9129999762f, 0.628000021f),
	FVector2D(0.7399999981f, 0.9749999046f),
	FVector2D(0.7399999981f, 0.9749999046f),
	FVector2D(0.9129999762f, 0.628000021f),
	FVector2D(0.8129999523f, 0.9449999332f),
	FVector2D(0.8129999523f, 0.9449999332f),
	FVector2D(0.9129999762f, 0.628000021f),
	FVector2D(0.9299999361f, 0.8679999113f),
	FVector2D(1.0f, 0.7999999523f),
	FVector2D(0.9299999361f, 0.8679999113f),
	FVector2D(1.0f, 0.7879999876f),
	FVector2D(1.0f, 0.7879999876f),
	FVector2D(0.9129999762f, 0.628000021f),
	FVector2D(0.9299999361f, 0.8679999113f),
};

static const FVector2D Vive_RightEyeVisibleAreaPositions[] =
{
	FVector2D(0.09000000358f, 0.1099999917f),
	FVector2D(0.0f, 0.195f),
	FVector2D(0.0f, 0.1999999952f),
	FVector2D(0.0f, 0.1999999952f),
	FVector2D(0.09000000358f, 0.1099999917f),
	FVector2D(0.08399999142f, 0.3639999998f),
	FVector2D(0.1899999976f, 0.04499999404f),
	FVector2D(0.09000000358f, 0.1099999917f),
	FVector2D(0.08399999142f, 0.3639999998f),
	FVector2D(0.3199999928f, 0.0f),
	FVector2D(0.1899999976f, 0.04499999404f),
	FVector2D(0.08399999142f, 0.3639999998f),
	FVector2D(0.6299999952f, 0.0f),
	FVector2D(0.3199999928f, 0.0f),
	FVector2D(0.08399999142f, 0.3639999998f),
	FVector2D(0.08399999142f, 0.3639999998f),
	FVector2D(0.6299999952f, 0.0f),
	FVector2D(0.1049999893f, 0.4949999821f),
	FVector2D(0.7599999905f, 0.04999998927f),
	FVector2D(0.6299999952f, 0.0f),
	FVector2D(0.1049999893f, 0.4949999821f),
	FVector2D(0.8680000305f, 0.1230000031f),
	FVector2D(0.7599999905f, 0.04999998927f),
	FVector2D(0.1049999893f, 0.4949999821f),
	FVector2D(0.9400000572f, 0.1999999952f),
	FVector2D(0.8680000305f, 0.1230000031f),
	FVector2D(0.1049999893f, 0.4949999821f),
	FVector2D(1.0f, 0.294999994f),
	FVector2D(0.9400000572f, 0.1999999952f),
	FVector2D(0.1049999893f, 0.4949999821f),
	FVector2D(1.0f, 0.700000025f),
	FVector2D(1.0f, 0.294999994f),
	FVector2D(0.1049999893f, 0.4949999821f),
	FVector2D(0.9600000381f, 0.767999984f),
	FVector2D(1.0f, 0.700000025f),
	FVector2D(0.1049999893f, 0.4949999821f),
	FVector2D(0.9099999666f, 0.8330000412f),
	FVector2D(0.9600000381f, 0.767999984f),
	FVector2D(0.1049999893f, 0.4949999821f),
	FVector2D(0.8099999428f, 0.9150000584f),
	FVector2D(0.9099999666f, 0.8330000412f),
	FVector2D(0.1049999893f, 0.4949999821f),
	FVector2D(0.7289999723f, 0.9600000155f),
	FVector2D(0.8099999428f, 0.9150000584f),
	FVector2D(0.1049999893f, 0.4949999821f),
	FVector2D(0.6119999886f, 1.0f),
	FVector2D(0.7289999723f, 0.9600000155f),
	FVector2D(0.1049999893f, 0.4949999821f),
	FVector2D(0.3439999819f, 1.0f),
	FVector2D(0.6119999886f, 1.0f),
	FVector2D(0.1049999893f, 0.4949999821f),
	FVector2D(0.1049999893f, 0.4949999821f),
	FVector2D(0.3439999819f, 1.0f),
	FVector2D(0.0870000124f, 0.6279999983f),
	FVector2D(0.2599999905f, 0.9750000012f),
	FVector2D(0.3439999819f, 1.0f),
	FVector2D(0.0870000124f, 0.6279999983f),
	FVector2D(0.1870000064f, 0.9450000298f),
	FVector2D(0.2599999905f, 0.9750000012f),
	FVector2D(0.0870000124f, 0.6279999983f),
	FVector2D(0.06999999285f, 0.8680000079f),
	FVector2D(0.1870000064f, 0.9450000298f),
	FVector2D(0.0870000124f, 0.6279999983f),
	FVector2D(0.0f, 0.8000000489f),
	FVector2D(0.0f, 0.787999965f),
	FVector2D(0.06999999285f, 0.8680000079f),
	FVector2D(0.0870000124f, 0.6279999983f),
	FVector2D(0.06999999285f, 0.8680000079f),
	FVector2D(0.0f, 0.787999965f),
};