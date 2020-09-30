// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
D3D12ConstantBuffer.cpp: D3D Constant buffer RHI implementation.
=============================================================================*/

#include "D3D12RHIPrivate.h"

DEFINE_STAT(STAT_D3D12GlobalConstantBufferUpdateTime);

// New circular buffer system for faster constant uploads.  Avoids CopyResource and speeds things up considerably
FD3D12ConstantBuffer::FD3D12ConstantBuffer(FD3D12Device* InParent, FD3D12FastConstantAllocator& InAllocator) :
	CurrentUpdateSize(0),
	TotalUpdateSize(0),
	Allocator(InAllocator),
	bIsDirty(false),
	FD3D12DeviceChild(InParent)
{
	FMemory::Memset(ShadowData, 0);
}

FD3D12ConstantBuffer::~FD3D12ConstantBuffer()
{}

bool FD3D12ConstantBuffer::Version(FD3D12ResourceLocation& BufferOut, bool bDiscardSharedConstants)
{
	// If nothing has changed there is no need to alloc a new buffer.
	if (CurrentUpdateSize == 0)
	{
		return false;
	}

	SCOPE_CYCLE_COUNTER(STAT_D3D12GlobalConstantBufferUpdateTime);

	if (bDiscardSharedConstants)
	{
		// If we're discarding shared constants, just use constants that have been updated since the last Commit.
		TotalUpdateSize = CurrentUpdateSize;
	}
	else
	{
		// If we're re-using shared constants, use all constants.
		TotalUpdateSize = FMath::Max(CurrentUpdateSize, TotalUpdateSize);
	}

	// Get the next constant buffer
	void* Data = Allocator.Allocate(TotalUpdateSize, BufferOut);

	check(TotalUpdateSize <= sizeof(ShadowData));
	FMemory::Memcpy(Data, ShadowData, TotalUpdateSize);

	bIsDirty = false;
	return true;
}
