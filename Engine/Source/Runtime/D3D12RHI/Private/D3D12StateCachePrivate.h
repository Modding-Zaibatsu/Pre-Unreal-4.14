// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

// Implementation of Device Context State Caching to improve draw
//	thread performance by removing redundant device context calls.

#pragma once
#include "Queue.h"
#include "D3D12DirectCommandListManager.h"

//-----------------------------------------------------------------------------
//	Configuration
//-----------------------------------------------------------------------------

// If set, includes a runtime toggle console command for debugging D3D12  state caching.
// ("TOGGLESTATECACHE")
#define D3D12_STATE_CACHE_RUNTIME_TOGGLE 0

// If set, includes a cache state verification check.
// After each state set call, the cached state is compared against the actual state.
// This is *very slow* and should only be enabled to debug the state caching system.
#ifndef D3D12_STATE_CACHE_DEBUG
#define D3D12_STATE_CACHE_DEBUG 0
#endif

// Uncomment only for debugging of the descriptor heap management; this is very noisy
//#define VERBOSE_DESCRIPTOR_HEAP_DEBUG 1

// The number of view descriptors available per (online) descriptor heap, depending on hardware tier
#define NUM_SAMPLER_DESCRIPTORS D3D12_MAX_SHADER_VISIBLE_SAMPLER_HEAP_SIZE
#define DESCRIPTOR_HEAP_BLOCK_SIZE 10000

#define NUM_VIEW_DESCRIPTORS_TIER_1 D3D12_MAX_SHADER_VISIBLE_DESCRIPTOR_HEAP_SIZE_TIER_1
#define NUM_VIEW_DESCRIPTORS_TIER_2 D3D12_MAX_SHADER_VISIBLE_DESCRIPTOR_HEAP_SIZE_TIER_2
// Only some tier 3 hardware can use > 1 million descriptors in a heap, the only way to tell if hardware can
// is to try and create a heap and check for failure. Unless we really want > 1 million Descriptors we'll cap
// out at 1M for now.
#define NUM_VIEW_DESCRIPTORS_TIER_3 D3D12_MAX_SHADER_VISIBLE_DESCRIPTOR_HEAP_SIZE_TIER_2

// This value defines how many descriptors will be in the device global view heap which
// is shared across contexts to allow the driver to eliminate redundant descriptor heap sets.
// This should be tweaked for each title as heaps require VRAM. The default value of 512k takes up ~16MB
#define GLOBAL_VIEW_HEAP_SIZE (1024 * 512)

// Heap for updating UAV counter values.
#define COUNTER_HEAP_SIZE 1024 * 64

// Keep set state functions inline to reduce call overhead
#define D3D12_STATE_CACHE_INLINE FORCEINLINE

#if D3D12_STATE_CACHE_RUNTIME_TOGGLE
extern bool GD3D12SkipStateCaching;
#else
static const bool GD3D12SkipStateCaching = false;
#endif

struct FD3D12VertexBufferCache
{
	FD3D12VertexBufferCache() : MaxBoundVertexBufferIndex(INDEX_NONE), BoundVBMask(0)
	{
		FMemory::Memzero(CurrentVertexBufferViews, sizeof(CurrentVertexBufferViews));
		FMemory::Memzero(ResidencyHandles, sizeof(ResidencyHandles));
		FMemory::Memzero(CurrentVertexBufferResources);
	};

	D3D12_VERTEX_BUFFER_VIEW CurrentVertexBufferViews[D3D12_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT];
	FD3D12ResourceLocation* CurrentVertexBufferResources[D3D12_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT];
	FD3D12ResidencyHandle* ResidencyHandles[D3D12_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT];
	int32 MaxBoundVertexBufferIndex;
	uint32 BoundVBMask;
};

struct FD3D12IndexBufferCache
{
	FD3D12IndexBufferCache() : ResidencyHandle(nullptr), CurrentIndexBufferLocation(nullptr)
	{
		FMemory::Memzero(&CurrentIndexBufferView, sizeof(CurrentIndexBufferView));
	}

	D3D12_INDEX_BUFFER_VIEW CurrentIndexBufferView;
	FD3D12ResourceLocation* CurrentIndexBufferLocation;
	FD3D12ResidencyHandle* ResidencyHandle;
};

struct FD3D12ConstantBufferCache
{
	FD3D12ConstantBufferCache()
	{
		Clear();
	}

	inline void Clear()
	{
		FMemory::Memzero(DirtySlotMask, sizeof(DirtySlotMask));
		FMemory::Memzero(CurrentGPUVirtualAddress, sizeof(CurrentGPUVirtualAddress));
		FMemory::Memzero(ResidencyHandles, sizeof(ResidencyHandles));
	}

	// Mark a specific shader stage as dirty.
	inline void Dirty(EShaderFrequency ShaderFrequency, const CBVSlotMask& SlotMask = -1)
	{
		DirtySlotMask[ShaderFrequency] |= SlotMask;
	}

	// Mark specified bind slots, on all graphics stages, as dirty.
	inline void DirtyGraphics(const CBVSlotMask& SlotMask = -1)
	{
		for (uint32 i = 0; i < SF_Compute; i++)
		{
			DirtySlotMask[i] |= SlotMask;
		}
	}

	// Mark specified bind slots on compute as dirty.
	inline void DirtyCompute(const CBVSlotMask& SlotMask = -1)
	{
		Dirty(SF_Compute, SlotMask);
	}

	CBVSlotMask DirtySlotMask[SF_NumFrequencies];
	D3D12_GPU_VIRTUAL_ADDRESS CurrentGPUVirtualAddress[SF_NumFrequencies][MAX_CBS];
	FD3D12ResidencyHandle* ResidencyHandles[SF_NumFrequencies][MAX_CBS];
};

struct FD3D12ShaderResourceViewCache
{
	FD3D12ShaderResourceViewCache() : NumViewsIntersectWithDepthCount(0)
	{
		Clear();
	}

	inline void Clear()
	{
		NumViewsIntersectWithDepthCount = 0;
		FMemory::Memzero(ResidencyHandles);
		FMemory::Memzero(ViewsIntersectWithDepthRT);
		FMemory::Memzero(BoundMask);
		FMemory::Memzero(Views);

		for (int32& index : MaxBoundIndex)
		{
			index = INDEX_NONE;
		}
	}

	FD3D12ShaderResourceView* Views[SF_NumFrequencies][MAX_SRVS];
	FD3D12ResidencyHandle* ResidencyHandles[SF_NumFrequencies][MAX_SRVS];

	bool ViewsIntersectWithDepthRT[SF_NumFrequencies][MAX_SRVS];
	uint32 NumViewsIntersectWithDepthCount;

	uint32 BoundMask[SF_NumFrequencies];
	int32 MaxBoundIndex[SF_NumFrequencies];
};

//-----------------------------------------------------------------------------
//	FD3D12StateCache Class Definition
//-----------------------------------------------------------------------------
class FD3D12StateCacheBase : public FD3D12DeviceChild , public FD3D12SingleNodeGPUObject
{
	friend class FD3D12DynamicRHI;

public:
	enum ESRV_Type
	{
		SRV_Unknown,
		SRV_Dynamic,
		SRV_Static,
	};
protected:
	FD3D12CommandContext* CmdContext;

	bool bNeedSetVB;
	bool bNeedSetIB;
	bool bNeedSetUAVsPerShaderStage[SF_NumFrequencies];
	bool bNeedSetRTs;
	bool bNeedSetSOs;
	bool bNeedSetSamplersPerShaderStage[SF_NumFrequencies];
	bool bNeedSetSamplers;
	bool bNeedSetSRVsPerShaderStage[SF_NumFrequencies];
	bool bSRVSCleared;
	bool bNeedSetSRVs;
	bool bNeedSetViewports;
	bool bNeedSetScissorRects;
	bool bNeedSetPrimitiveTopology;
	bool bNeedSetBlendFactor;
	bool bNeedSetStencilRef;
	bool bAutoFlushComputeShaderCache;
	D3D12_RESOURCE_BINDING_TIER ResourceBindingTier;

	struct
	{
		struct
		{
			// Cache
			ID3D12PipelineState* CurrentPipelineStateObject;
			bool bNeedRebuildPSO;

			// Note: Current root signature is part of the bound shader state
			bool bNeedSetRootSignature;

			// Full high level PSO desc
			FD3D12HighLevelGraphicsPipelineStateDesc HighLevelDesc;

			// Depth Stencil State Cache
			uint32 CurrentReferenceStencil;

			// Blend State Cache
			float CurrentBlendFactor[4];

			// Viewport
			uint32	CurrentNumberOfViewports;
			D3D12_VIEWPORT CurrentViewport[D3D12_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE];

			// Vertex Buffer State
			__declspec(align(16)) FD3D12VertexBufferCache VBCache;

			// SO
			uint32			CurrentNumberOfStreamOutTargets;
			FD3D12Resource* CurrentStreamOutTargets[D3D12_SO_STREAM_COUNT];
			uint32			CurrentSOOffsets[D3D12_SO_STREAM_COUNT];

			// Index Buffer State
			__declspec(align(16)) FD3D12IndexBufferCache IBCache;

			// Primitive Topology State
			D3D_PRIMITIVE_TOPOLOGY CurrentPrimitiveTopology;

			// Input Layout State
			D3D12_RECT CurrentScissorRects[D3D12_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE];
			D3D12_RECT CurrentViewportScissorRects[D3D12_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE];
			uint32 CurrentNumberOfScissorRects;

			FD3D12RenderTargetView* RenderTargetArray[D3D12_SIMULTANEOUS_RENDER_TARGET_COUNT];

			FD3D12DepthStencilView* CurrentDepthStencilTarget;
		} Graphics;

		struct
		{
			// Cache
			ID3D12PipelineState* CurrentPipelineStateObject;
			bool bNeedRebuildPSO;

			// Note: Current root signature is part of the bound compute shader
			bool bNeedSetRootSignature;

			// Compute
			FD3D12ComputeShader* CurrentComputeShader;
		} Compute;

		struct
		{
			// UAVs
			TRefCountPtr<FD3D12UnorderedAccessView> UnorderedAccessViewArray[SF_NumFrequencies][D3D12_PS_CS_UAV_REGISTER_COUNT];
			uint32 CurrentUAVStartSlot[SF_NumFrequencies];

			// Shader Resource Views Cache
			FD3D12ShaderResourceViewCache SRVCache;

			// Sampler State
			FD3D12SamplerState* CurrentSamplerStates[SF_NumFrequencies][D3D12_COMMONSHADER_SAMPLER_SLOT_COUNT];

			// PSO
			ID3D12PipelineState* CurrentPipelineStateObject;
			bool bNeedSetPSO;

			uint32 CurrentShaderSamplerCounts[SF_NumFrequencies];
			uint32 CurrentShaderSRVCounts[SF_NumFrequencies];
			uint32 CurrentShaderCBCounts[SF_NumFrequencies];
			uint32 CurrentShaderUAVCounts[SF_NumFrequencies];

			FD3D12Resource* CurrentConstantBuffers[SF_NumFrequencies][MAX_CBS];
			
			__declspec(align(16)) FD3D12ConstantBufferCache CBVCache;

		} Common;
	} PipelineState;

	FD3D12DescriptorCache DescriptorCache;
	bool bAlwaysSetIndexBuffers;

	void InternalSetIndexBuffer(FD3D12ResourceLocation *IndexBufferLocation, DXGI_FORMAT Format, uint32 Offset);

	void InternalSetShaderResourceView(FD3D12ShaderResourceView*& SRV, uint32 ResourceIndex, EShaderFrequency ShaderFrequency);

	void InternalSetStreamSource(FD3D12ResourceLocation* VertexBufferLocation, uint32 StreamIndex, uint32 Stride, uint32 Offset);

	typedef void(*TSetSamplerStateAlternate)(FD3D12StateCacheBase* StateCache, FD3D12SamplerState* SamplerState, uint32 SamplerIndex);
	template <EShaderFrequency ShaderFrequency>
	D3D12_STATE_CACHE_INLINE void InternalSetSamplerState(FD3D12SamplerState* SamplerState, uint32 SamplerIndex, TSetSamplerStateAlternate AlternatePathFunction)
	{
		check(SamplerIndex < ARRAYSIZE(PipelineState.Common.CurrentSamplerStates[ShaderFrequency]));;
		if ((PipelineState.Common.CurrentSamplerStates[ShaderFrequency][SamplerIndex] != SamplerState) || GD3D12SkipStateCaching)
		{
			PipelineState.Common.CurrentSamplerStates[ShaderFrequency][SamplerIndex] = SamplerState;
			bNeedSetSamplersPerShaderStage[ShaderFrequency] = true;
			bNeedSetSamplers = true;
			if (AlternatePathFunction != nullptr)
			{
				(*AlternatePathFunction)(this, SamplerState, SamplerIndex);
			}
		}
	}

	// Shorthand for typing/reading convenience
	D3D12_STATE_CACHE_INLINE FD3D12BoundShaderState* BSS()
	{
		return PipelineState.Graphics.HighLevelDesc.BoundShaderState;
	}

	template <typename TShader> struct StateCacheShaderTraits;
#define DECLARE_SHADER_TRAITS(Name) \
	template <> struct StateCacheShaderTraits<FD3D12##Name##Shader> \
	{ \
		static const EShaderFrequency Frequency = SF_##Name; \
		static FD3D12##Name##Shader* GetShader(FD3D12BoundShaderState* BSS) { return BSS ? BSS->Get##Name##Shader() : nullptr; } \
	}
	DECLARE_SHADER_TRAITS(Vertex);
	DECLARE_SHADER_TRAITS(Pixel);
	DECLARE_SHADER_TRAITS(Domain);
	DECLARE_SHADER_TRAITS(Hull);
	DECLARE_SHADER_TRAITS(Geometry);
#undef DECLARE_SHADER_TRAITS

	template <typename TShader> D3D12_STATE_CACHE_INLINE void SetShader(TShader* Shader)
	{
		typedef StateCacheShaderTraits<TShader> Traits;
		TShader* OldShader = Traits::GetShader(BSS());
		if (OldShader != Shader)
		{
			PipelineState.Common.CurrentShaderSamplerCounts[Traits::Frequency] = (Shader) ? Shader->ResourceCounts.NumSamplers : 0;
			PipelineState.Common.CurrentShaderSRVCounts[Traits::Frequency]     = (Shader) ? Shader->ResourceCounts.NumSRVs     : 0;
			PipelineState.Common.CurrentShaderCBCounts[Traits::Frequency]      = (Shader) ? Shader->ResourceCounts.NumCBs      : 0;
			PipelineState.Common.CurrentShaderUAVCounts[Traits::Frequency]     = (Shader) ? Shader->ResourceCounts.NumUAVs     : 0;
		}
	}

	template <typename TShader> D3D12_STATE_CACHE_INLINE void GetShader(TShader** Shader)
	{
		*Shader = StateCacheShaderTraits<TShader>::GetShader(BSS());
	}

public:

	void InheritState(const FD3D12StateCacheBase& AncestralCache)
	{
		FMemory::Memcpy(&PipelineState, &AncestralCache.PipelineState, sizeof(PipelineState));
		RestoreState();
	}

	FD3D12DescriptorCache* GetDescriptorCache()
	{
		return &DescriptorCache;
	}

	ID3D12PipelineState* GetPipelineStateObject()
	{
		return PipelineState.Common.CurrentPipelineStateObject;
	}

	const FD3D12RootSignature* GetGraphicsRootSignature()
	{
		return PipelineState.Graphics.HighLevelDesc.BoundShaderState ?
			PipelineState.Graphics.HighLevelDesc.BoundShaderState->pRootSignature : nullptr;
	}

	const FD3D12RootSignature* GetComputeRootSignature()
	{
		return PipelineState.Compute.CurrentComputeShader ?
			PipelineState.Compute.CurrentComputeShader->pRootSignature : nullptr;
	}

	void ClearSamplers();
	void ClearSRVs();

	template <EShaderFrequency ShaderFrequency>
	void ClearShaderResourceViews(FD3D12ResourceLocation*& ResourceLocation)
	{
		auto& CurrentShaderResourceViews = PipelineState.Common.SRVCache.Views[ShaderFrequency];

		if (PipelineState.Common.SRVCache.MaxBoundIndex[ShaderFrequency] < 0)
		{
			return;
		}

		for (int32 i = 0; i <= PipelineState.Common.SRVCache.MaxBoundIndex[ShaderFrequency]; ++i)
		{
			if (CurrentShaderResourceViews[i] && CurrentShaderResourceViews[i]->GetResourceLocation() == ResourceLocation)
			{
				SetShaderResourceView<ShaderFrequency>(nullptr, i);
			}
		}
	}

	template <EShaderFrequency ShaderFrequency>
	D3D12_STATE_CACHE_INLINE void SetShaderResourceView(FD3D12ShaderResourceView* SRV, uint32 ResourceIndex, ESRV_Type SrvType = SRV_Unknown)
	{
		InternalSetShaderResourceView(SRV, ResourceIndex, ShaderFrequency);
	}

	template <EShaderFrequency ShaderFrequency>
	D3D12_STATE_CACHE_INLINE void GetShaderResourceViews(uint32 StartResourceIndex, uint32& NumResources, FD3D12ShaderResourceView** SRV)
	{
		{
			uint32 NumLoops = D3D12_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - StartResourceIndex;
			NumResources = 0;
			for (uint32 ResourceLoop = 0; ResourceLoop < NumLoops; ResourceLoop++)
			{
				SRV[ResourceLoop] = PipelineState.Common.CurrentShaderResourceViews[ShaderFrequency][ResourceLoop + StartResourceIndex];
				if (SRV[ResourceLoop])
				{
					SRV[ResourceLoop]->AddRef();
					NumResources = ResourceLoop;
				}
			}
		}
	}

	void UpdateViewportScissorRects();
	void SetScissorRects(uint32 Count, D3D12_RECT* ScissorRects);
	void SetScissorRect(D3D12_RECT ScissorRect);

	D3D12_STATE_CACHE_INLINE void GetScissorRect(D3D12_RECT *ScissorRect) const
	{
		check(ScissorRect);
		FMemory::Memcpy(ScissorRect, &PipelineState.Graphics.CurrentScissorRects, sizeof(D3D12_RECT));
	}


	void SetViewport(D3D12_VIEWPORT Viewport);
	void SetViewports(uint32 Count, D3D12_VIEWPORT* Viewports);

	D3D12_STATE_CACHE_INLINE void GetViewport(D3D12_VIEWPORT *Viewport) const
	{
		check(Viewport);
		FMemory::Memcpy(Viewport, &PipelineState.Graphics.CurrentViewport, sizeof(D3D12_VIEWPORT));
	}

	D3D12_STATE_CACHE_INLINE void GetViewports(uint32* Count, D3D12_VIEWPORT *Viewports) const
	{
		check(*Count);
		if (Viewports) //NULL is legal if you just want count
		{
			//as per d3d spec
			int32 StorageSizeCount = (int32)(*Count);
			int32 CopyCount = FMath::Min(FMath::Min(StorageSizeCount, (int32)PipelineState.Graphics.CurrentNumberOfViewports), D3D12_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE);
			if (CopyCount > 0)
			{
				FMemory::Memcpy(Viewports, &PipelineState.Graphics.CurrentViewport[0], sizeof(D3D12_VIEWPORT) * CopyCount);
			}
			//remaining viewports in supplied array must be set to zero
			if (StorageSizeCount > CopyCount)
			{
				FMemory::Memset(&Viewports[CopyCount], 0, sizeof(D3D12_VIEWPORT) * (StorageSizeCount - CopyCount));
			}
		}
		*Count = PipelineState.Graphics.CurrentNumberOfViewports;
	}

	template <EShaderFrequency ShaderFrequency>
	D3D12_STATE_CACHE_INLINE void SetSamplerState(FD3D12SamplerState* SamplerState, uint32 SamplerIndex)
	{
		InternalSetSamplerState<ShaderFrequency>(SamplerState, SamplerIndex, nullptr);
	}

	template <EShaderFrequency ShaderFrequency>
	D3D12_STATE_CACHE_INLINE void GetSamplerState(uint32 StartSamplerIndex, uint32 NumSamplerIndexes, FD3D12SamplerState** SamplerStates) const
	{
		check(StartSamplerIndex + NumSamplerIndexes <= D3D12_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT);
		for (uint32 StateLoop = 0; StateLoop < NumSamplerIndexes; StateLoop++)
		{
			SamplerStates[StateLoop] = CurrentShaderResourceViews[ShaderFrequency][StateLoop + StartSamplerIndex];
			if (SamplerStates[StateLoop])
			{
				SamplerStates[StateLoop]->AddRef();
			}
		}
	}

	template <EShaderFrequency ShaderFrequency>
	void D3D12_STATE_CACHE_INLINE SetConstantsFromUniformBuffer(uint32 SlotIndex, FD3D12UniformBuffer* UniformBuffer)
	{
		check(SlotIndex < MAX_CBS);
		FD3D12ConstantBufferCache& CBVCache = PipelineState.Common.CBVCache;
		D3D12_GPU_VIRTUAL_ADDRESS& CurrentGPUVirtualAddress = CBVCache.CurrentGPUVirtualAddress[ShaderFrequency][SlotIndex];

		if (UniformBuffer && UniformBuffer->ResourceLocation.GetGPUVirtualAddress())
		{
			const FD3D12ResourceLocation& ResourceLocation = UniformBuffer->ResourceLocation;
			// Only update the constant buffer if it has changed.
			if (ResourceLocation.GetGPUVirtualAddress() != CurrentGPUVirtualAddress)
			{
				CurrentGPUVirtualAddress = ResourceLocation.GetGPUVirtualAddress();
				CBVCache.ResidencyHandles[ShaderFrequency][SlotIndex] = ResourceLocation.GetResource()->GetResidencyHandle();

				// Mark the CB as dirty.
				CBVCache.DirtySlotMask[ShaderFrequency] |= (1 << SlotIndex);
			}
		}
		else if(CurrentGPUVirtualAddress != 0)
		{
			CurrentGPUVirtualAddress = 0;
			CBVCache.ResidencyHandles[ShaderFrequency][SlotIndex] = nullptr;

			// Mark the CB as dirty.
			CBVCache.DirtySlotMask[ShaderFrequency] |= (1 << SlotIndex);
		}
	}

	template <EShaderFrequency ShaderFrequency>
	void D3D12_STATE_CACHE_INLINE SetConstantBuffer(FD3D12ConstantBuffer& Buffer, bool bDiscardSharedConstants)
	{
		FD3D12ResourceLocation Location(GetParentDevice());

		if (Buffer.Version(Location, bDiscardSharedConstants))
		{
			// Note: Code assumes the slot index is always 0.
			const uint32 SlotIndex = 0;

			FD3D12ConstantBufferCache& CBVCache = PipelineState.Common.CBVCache;
			D3D12_GPU_VIRTUAL_ADDRESS& CurrentGPUVirtualAddress = CBVCache.CurrentGPUVirtualAddress[ShaderFrequency][SlotIndex];
			check(Location.GetGPUVirtualAddress() != CurrentGPUVirtualAddress);
			CurrentGPUVirtualAddress = Location.GetGPUVirtualAddress();
			CBVCache.ResidencyHandles[ShaderFrequency][SlotIndex] = Location.GetResource()->GetResidencyHandle();

			// Mark the CB as dirty.
			CBVCache.DirtySlotMask[ShaderFrequency] |= (1 << SlotIndex);
		}
	}

	D3D12_STATE_CACHE_INLINE void SetRasterizerState(D3D12_RASTERIZER_DESC* State)
	{
		if (PipelineState.Graphics.HighLevelDesc.RasterizerState != State || GD3D12SkipStateCaching)
		{
			PipelineState.Graphics.HighLevelDesc.RasterizerState = State;
			PipelineState.Graphics.bNeedRebuildPSO = true;
		}
	}

	D3D12_STATE_CACHE_INLINE void GetRasterizerState(D3D12_RASTERIZER_DESC** RasterizerState) const
	{
		*RasterizerState = PipelineState.Graphics.HighLevelDesc.RasterizerState;
	}

	void SetBlendState(D3D12_BLEND_DESC* State, const float BlendFactor[4], uint32 SampleMask);

	D3D12_STATE_CACHE_INLINE void GetBlendState(D3D12_BLEND_DESC** BlendState, float BlendFactor[4], uint32* SampleMask) const
	{
		*BlendState = PipelineState.Graphics.HighLevelDesc.BlendState;
		*SampleMask = PipelineState.Graphics.HighLevelDesc.SampleMask;
		FMemory::Memcpy(BlendFactor, PipelineState.Graphics.CurrentBlendFactor, sizeof(PipelineState.Graphics.CurrentBlendFactor));
	}

	void SetDepthStencilState(D3D12_DEPTH_STENCIL_DESC* State, uint32 RefStencil);

	D3D12_STATE_CACHE_INLINE void GetDepthStencilState(D3D12_DEPTH_STENCIL_DESC** DepthStencilState, uint32* StencilRef) const
	{
		*DepthStencilState = PipelineState.Graphics.HighLevelDesc.DepthStencilState;
		*StencilRef = PipelineState.Graphics.CurrentReferenceStencil;
	}

	D3D12_STATE_CACHE_INLINE void GetVertexShader(FD3D12VertexShader** Shader)
	{
		GetShader(Shader);
	}

	D3D12_STATE_CACHE_INLINE void GetHullShader(FD3D12HullShader** Shader)
	{
		GetShader(Shader);
	}

	D3D12_STATE_CACHE_INLINE void GetDomainShader(FD3D12DomainShader** Shader)
	{
		GetShader(Shader);
	}

	D3D12_STATE_CACHE_INLINE void GetGeometryShader(FD3D12GeometryShader** Shader)
	{
		GetShader(Shader);
	}

	D3D12_STATE_CACHE_INLINE void GetPixelShader(FD3D12PixelShader** Shader)
	{
		GetShader(Shader);
	}

	D3D12_STATE_CACHE_INLINE void SetBoundShaderState(FD3D12BoundShaderState* BoundShaderState)
	{
		if (BoundShaderState)
		{
			SetShader(BoundShaderState->GetVertexShader());
			SetShader(BoundShaderState->GetPixelShader());
			SetShader(BoundShaderState->GetDomainShader());
			SetShader(BoundShaderState->GetHullShader());
			SetShader(BoundShaderState->GetGeometryShader());
		}
		else
		{
			SetShader<FD3D12VertexShader>(nullptr);
			SetShader<FD3D12PixelShader>(nullptr);
			SetShader<FD3D12HullShader>(nullptr);
			SetShader<FD3D12DomainShader>(nullptr);
			SetShader<FD3D12GeometryShader>(nullptr);
		}

		FD3D12BoundShaderState*& CurrentBSS = PipelineState.Graphics.HighLevelDesc.BoundShaderState;
		if (CurrentBSS != BoundShaderState)
		{
			const FD3D12RootSignature* const pCurrentRootSignature = CurrentBSS ? CurrentBSS->pRootSignature : nullptr;
			const FD3D12RootSignature* const pNewRootSignature = BoundShaderState ? BoundShaderState->pRootSignature : nullptr;
			if (pCurrentRootSignature != pNewRootSignature)
			{
				PipelineState.Graphics.bNeedSetRootSignature = true;
			}

			CurrentBSS = BoundShaderState;
			PipelineState.Graphics.bNeedRebuildPSO = true;
		}
	}

	D3D12_STATE_CACHE_INLINE void GetBoundShaderState(FD3D12BoundShaderState** BoundShaderState) const
	{
		*BoundShaderState = PipelineState.Graphics.HighLevelDesc.BoundShaderState;
	}

	D3D12_STATE_CACHE_INLINE void SetComputeShader(FD3D12ComputeShader* Shader)
	{
		if (PipelineState.Compute.CurrentComputeShader != Shader)
		{
			// See if we need to change the root signature
			const FD3D12RootSignature* const pCurrentRootSignature = PipelineState.Compute.CurrentComputeShader ? PipelineState.Compute.CurrentComputeShader->pRootSignature : nullptr;
			const FD3D12RootSignature* const pNewRootSignature = Shader ? Shader->pRootSignature : nullptr;
			if (pCurrentRootSignature != pNewRootSignature)
			{
				PipelineState.Compute.bNeedSetRootSignature = true;
			}

			PipelineState.Compute.CurrentComputeShader                  = Shader;
			PipelineState.Compute.bNeedRebuildPSO = true;
			PipelineState.Common.CurrentShaderSamplerCounts[SF_Compute] = (Shader) ? Shader->ResourceCounts.NumSamplers : 0;
			PipelineState.Common.CurrentShaderSRVCounts[SF_Compute]     = (Shader) ? Shader->ResourceCounts.NumSRVs     : 0;
			PipelineState.Common.CurrentShaderCBCounts[SF_Compute]      = (Shader) ? Shader->ResourceCounts.NumCBs      : 0;
			PipelineState.Common.CurrentShaderUAVCounts[SF_Compute]     = (Shader) ? Shader->ResourceCounts.NumUAVs     : 0;
		}
	}

	D3D12_STATE_CACHE_INLINE void GetComputeShader(FD3D12ComputeShader** ComputeShader)
	{
		*ComputeShader = PipelineState.Compute.CurrentComputeShader;
	}

	D3D12_STATE_CACHE_INLINE void GetInputLayout(D3D12_INPUT_LAYOUT_DESC* InputLayout) const
	{
		*InputLayout = PipelineState.Graphics.HighLevelDesc.BoundShaderState->InputLayout;
	}

	D3D12_STATE_CACHE_INLINE void SetStreamSource(FD3D12ResourceLocation* VertexBufferLocation, uint32 StreamIndex, uint32 Stride, uint32 Offset)
	{
		InternalSetStreamSource(VertexBufferLocation, StreamIndex, Stride, Offset);
	}

	D3D12_STATE_CACHE_INLINE bool IsShaderResource(const FD3D12ResourceLocation* VertexBufferLocation) const
	{
		for (int i = 0; i < SF_NumFrequencies; i++)
		{
			if (PipelineState.Common.SRVCache.MaxBoundIndex[i] < 0)
			{
				continue;
			}

			for (int32 j = 0; j < PipelineState.Common.SRVCache.MaxBoundIndex[i]; ++j)
			{
				if (PipelineState.Common.SRVCache.Views[i][j] && PipelineState.Common.SRVCache.Views[i][j]->GetResourceLocation())
				{
					if (PipelineState.Common.SRVCache.Views[i][j]->GetResourceLocation() == VertexBufferLocation)
					{
						return true;
					}
				}
			}
		}

		return false;
	}

	D3D12_STATE_CACHE_INLINE bool IsStreamSource(const FD3D12ResourceLocation* VertexBufferLocation) const
	{
		for (int32 index = 0; index <= PipelineState.Graphics.VBCache.MaxBoundVertexBufferIndex; ++index)
		{
			if (PipelineState.Graphics.VBCache.CurrentVertexBufferResources[index] == VertexBufferLocation)
			{
				return true;
			}
		}

		return false;
	}

public:

	D3D12_STATE_CACHE_INLINE void SetIndexBuffer(FD3D12ResourceLocation* IndexBufferLocation, DXGI_FORMAT Format, uint32 Offset)
	{
		InternalSetIndexBuffer(IndexBufferLocation, Format, Offset);
	}

	D3D12_STATE_CACHE_INLINE bool IsIndexBuffer(const FD3D12ResourceLocation *ResourceLocation) const
	{
		return PipelineState.Graphics.IBCache.CurrentIndexBufferLocation == ResourceLocation;
	}

	void SetPrimitiveTopology(D3D12_PRIMITIVE_TOPOLOGY PrimitiveTopology);


	D3D12_STATE_CACHE_INLINE void GetPrimitiveTopology(D3D12_PRIMITIVE_TOPOLOGY* PrimitiveTopology) const
	{
		*PrimitiveTopology = PipelineState.Graphics.CurrentPrimitiveTopology;
	}

	FD3D12StateCacheBase(GPUNodeMask Node);

	void Init(FD3D12Device* InParent, FD3D12CommandContext* InCmdContext, const FD3D12StateCacheBase* AncestralState, FD3D12SubAllocatedOnlineHeap::SubAllocationDesc& SubHeapDesc, bool bInAlwaysSetIndexBuffers = false);

	~FD3D12StateCacheBase()
	{
	}

	template <bool IsCompute = false> void ApplyState();
	void ApplySamplers(const FD3D12RootSignature* const pRootSignature, uint32 StartStage, uint32 EndStage);
	void RestoreState();
	void DirtyViewDescriptorTables();
	void DirtySamplerDescriptorTables();
	bool AssertResourceStates(const bool IsCompute);


	void SetRenderTargets(uint32 NumSimultaneousRenderTargets, FD3D12RenderTargetView** RTArray, FD3D12DepthStencilView* DSTarget);
	D3D12_STATE_CACHE_INLINE void GetRenderTargets(FD3D12RenderTargetView **RTArray, uint32* NumSimultaneousRTs, FD3D12DepthStencilView** DepthStencilTarget)
	{
		if (RTArray) //NULL is legal
		{
			FMemory::Memcpy(RTArray, PipelineState.Graphics.RenderTargetArray, sizeof(FD3D12RenderTargetView*)* D3D12_SIMULTANEOUS_RENDER_TARGET_COUNT);
			*NumSimultaneousRTs = PipelineState.Graphics.HighLevelDesc.NumRenderTargets;
		}

		if (DepthStencilTarget)
		{
			*DepthStencilTarget = PipelineState.Graphics.CurrentDepthStencilTarget;
		}
	}

	void SetStreamOutTargets(uint32 NumSimultaneousStreamOutTargets, FD3D12Resource** SOArray, const uint32* SOOffsets);

	void SetUAVs(EShaderFrequency ShaderStage, uint32 UAVStartSlot, uint32 NumSimultaneousUAVs, FD3D12UnorderedAccessView** UAVArray, uint32 *UAVInitialCountArray);

	D3D12_STATE_CACHE_INLINE void AutoFlushComputeShaderCache(bool bEnable)
	{
		bAutoFlushComputeShaderCache = bEnable;
	}

	void FlushComputeShaderCache(bool bForce = false);

	/**
	 * Clears all D3D12 State, setting all input/output resource slots, shaders, input layouts,
	 * predications, scissor rectangles, depth-stencil state, rasterizer state, blend state,
	 * sampler state, and viewports to NULL
	 */
	virtual void ClearState();

	/**
	 * Releases any object references held by the state cache
	 */
	void Clear();

	void ForceRebuildGraphicsPSO() { PipelineState.Graphics.bNeedRebuildPSO = true; }
	void ForceRebuildComputePSO() { PipelineState.Compute.bNeedRebuildPSO = true; }
	void ForceSetGraphicsRootSignature() { PipelineState.Graphics.bNeedSetRootSignature = true; }
	void ForceSetComputeRootSignature() { PipelineState.Compute.bNeedSetRootSignature = true; }
	void ForceSetVB() { bNeedSetVB = true; }
	void ForceSetIB() { bNeedSetIB = true; }
	void ForceSetRTs() { bNeedSetRTs = true; }
	void ForceSetSOs() { bNeedSetSOs = true; }
	void ForceSetSamplersPerShaderStage(uint32 Frequency) { bNeedSetSamplersPerShaderStage[Frequency] = true; }
	void ForceSetSamplers() { bNeedSetSamplers = true; }
	void ForceSetSRVsPerShaderStage(uint32 Frequency) { bNeedSetSRVsPerShaderStage[Frequency] = true; }
	void ForceSetSRVs() { bNeedSetSRVs = true; }
	void ForceSetViewports() { bNeedSetViewports = true; }
	void ForceSetScissorRects() { bNeedSetScissorRects = true; }
	void ForceSetPrimitiveTopology() { bNeedSetPrimitiveTopology = true; }
	void ForceSetBlendFactor() { bNeedSetBlendFactor = true; }
	void ForceSetStencilRef() { bNeedSetStencilRef = true; }

	bool GetForceRebuildGraphicsPSO() const { return PipelineState.Graphics.bNeedRebuildPSO; }
	bool GetForceRebuildComputePSO() const { return PipelineState.Compute.bNeedRebuildPSO; }
	bool GetForceSetVB() const { return bNeedSetVB; }
	bool GetForceSetIB() const { return bNeedSetIB; }
	bool GetForceSetRTs() const { return bNeedSetRTs; }
	bool GetForceSetSOs() const { return bNeedSetSOs; }
	bool GetForceSetSamplersPerShaderStage(uint32 Frequency) const { return bNeedSetSamplersPerShaderStage[Frequency]; }
	bool GetForceSetSamplers() const { return bNeedSetSamplers; }
	bool GetForceSetSRVsPerShaderStage(uint32 Frequency) const { return bNeedSetSRVsPerShaderStage[Frequency]; }
	bool GetForceSetSRVs() const { return bNeedSetSRVs; }
	bool GetForceSetViewports() const { return bNeedSetViewports; }
	bool GetForceSetScissorRects() const { return bNeedSetScissorRects; }
	bool GetForceSetPrimitiveTopology() const { return bNeedSetPrimitiveTopology; }
	bool GetForceSetBlendFactor() const { return bNeedSetBlendFactor; }
	bool GetForceSetStencilRef() const { return bNeedSetStencilRef; }


#if D3D12_STATE_CACHE_DEBUG
protected:
	// Debug helper methods to verify cached state integrity.
	template <EShaderFrequency ShaderFrequency>
	void VerifySamplerStates();

	template <EShaderFrequency ShaderFrequency>
	void VerifyConstantBuffers();

	template <EShaderFrequency ShaderFrequency>
	void VerifyShaderResourceViews();
#endif
};
