// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	DrawingPolicy.cpp: Base drawing policy implementation.
=============================================================================*/

#include "RendererPrivate.h"
#include "ScenePrivate.h"
#include "SceneUtils.h"

int32 GEmitMeshDrawEvent = 0;
static FAutoConsoleVariableRef CVarEmitMeshDrawEvent(
	TEXT("r.EmitMeshDrawEvents"),
	GEmitMeshDrawEvent,
	TEXT("Emits a GPU event around each drawing policy draw call.  /n")
	TEXT("Useful for seeing stats about each draw call, however it greatly distorts total time and time per draw call."),
	ECVF_RenderThreadSafe
	);

FMeshDrawingPolicy::FMeshDrawingPolicy(
	const FVertexFactory* InVertexFactory,
	const FMaterialRenderProxy* InMaterialRenderProxy,
	const FMaterial& InMaterialResource,
	EDebugViewShaderMode InDebugViewShaderMode,
	bool bInTwoSidedOverride,
	bool bInDitheredLODTransitionOverride,
	bool bInWireframeOverride
	):
	VertexFactory(InVertexFactory),
	MaterialRenderProxy(InMaterialRenderProxy),
	MaterialResource(&InMaterialResource),
	bIsDitheredLODTransitionMaterial(InMaterialResource.IsDitheredLODTransition() || bInDitheredLODTransitionOverride),
	bIsWireframeMaterial(InMaterialResource.IsWireframe() || bInWireframeOverride),
	//convert from signed bool to unsigned uint32
	DebugViewShaderMode((uint32)InDebugViewShaderMode)
{
	// using this saves a virtual function call
	bool bMaterialResourceIsTwoSided = InMaterialResource.IsTwoSided();

	bIsTwoSidedMaterial = bMaterialResourceIsTwoSided || bInTwoSidedOverride;

	bNeedsBackfacePass = !bInTwoSidedOverride && bMaterialResourceIsTwoSided
		//@todo - hook up bTwoSidedSeparatePass here if we re-add it
		&& false;
	
	bUsePositionOnlyVS = false;
}

void FMeshDrawingPolicy::DrawMesh(FRHICommandList& RHICmdList, const FMeshBatch& Mesh, int32 BatchElementIndex, const bool bIsInstancedStereo) const
{
	INC_DWORD_STAT(STAT_MeshDrawCalls);
	SCOPED_CONDITIONAL_DRAW_EVENTF(RHICmdList, MeshEvent, GEmitMeshDrawEvent != 0, TEXT("Mesh Draw"));

	const FMeshBatchElement& BatchElement = Mesh.Elements[BatchElementIndex];

	if (Mesh.UseDynamicData)
	{
		check(Mesh.DynamicVertexData);

		if (BatchElement.DynamicIndexData)
		{
			DrawIndexedPrimitiveUP(
				RHICmdList,
				Mesh.Type,
				BatchElement.MinVertexIndex,
				BatchElement.MaxVertexIndex - BatchElement.MinVertexIndex + 1,
				BatchElement.NumPrimitives,
				BatchElement.DynamicIndexData,
				BatchElement.DynamicIndexStride,
				Mesh.DynamicVertexData,
				Mesh.DynamicVertexStride
				);
		}
		else
		{
			DrawPrimitiveUP(
				RHICmdList,
				Mesh.Type,
				BatchElement.NumPrimitives,
				Mesh.DynamicVertexData,
				Mesh.DynamicVertexStride
				);
		}
	}
	else
	{
		if(BatchElement.IndexBuffer)
		{
			check(BatchElement.IndexBuffer->IsInitialized());
			if (BatchElement.bIsInstanceRuns)
			{
				checkSlow(BatchElement.bIsInstanceRuns);
				if (!GRHISupportsFirstInstance)
				{
					if (bUsePositionOnlyVS)
					{
						for (uint32 Run = 0; Run < BatchElement.NumInstances; Run++)
						{
							VertexFactory->OffsetPositionInstanceStreams(RHICmdList, BatchElement.InstanceRuns[Run * 2]); 
							RHICmdList.DrawIndexedPrimitive(
								BatchElement.IndexBuffer->IndexBufferRHI,
								Mesh.Type,
								0,
								0,
								BatchElement.MaxVertexIndex - BatchElement.MinVertexIndex + 1,
								BatchElement.FirstIndex,
								BatchElement.NumPrimitives,
								1 + BatchElement.InstanceRuns[Run * 2 + 1] - BatchElement.InstanceRuns[Run * 2]
							);
						}
					}
					else
					{
						for (uint32 Run = 0; Run < BatchElement.NumInstances; Run++)
						{
							VertexFactory->OffsetInstanceStreams(RHICmdList, BatchElement.InstanceRuns[Run * 2]); 
							RHICmdList.DrawIndexedPrimitive(
								BatchElement.IndexBuffer->IndexBufferRHI,
								Mesh.Type,
								0,
								0,
								BatchElement.MaxVertexIndex - BatchElement.MinVertexIndex + 1,
								BatchElement.FirstIndex,
								BatchElement.NumPrimitives,
								1 + BatchElement.InstanceRuns[Run * 2 + 1] - BatchElement.InstanceRuns[Run * 2]
							);
						}
					}
				}
				else
				{
					for (uint32 Run = 0; Run < BatchElement.NumInstances; Run++)
					{
						RHICmdList.DrawIndexedPrimitive(
							BatchElement.IndexBuffer->IndexBufferRHI,
							Mesh.Type,
							0,
							BatchElement.InstanceRuns[Run * 2],
							BatchElement.MaxVertexIndex - BatchElement.MinVertexIndex + 1,
							BatchElement.FirstIndex,
							BatchElement.NumPrimitives,
							1 + BatchElement.InstanceRuns[Run * 2 + 1] - BatchElement.InstanceRuns[Run * 2]
						);
					}
				}
			}
			else
			{
				// Currently only supporting this path for instanced stereo.
				const uint32 InstanceCount = (bIsInstancedStereo && !BatchElement.bIsInstancedMesh) ? 2 : BatchElement.NumInstances;

				RHICmdList.DrawIndexedPrimitive(
					BatchElement.IndexBuffer->IndexBufferRHI,
					Mesh.Type,
					0,
					0,
					BatchElement.MaxVertexIndex - BatchElement.MinVertexIndex + 1,
					BatchElement.FirstIndex,
					BatchElement.NumPrimitives,
					InstanceCount
					);
			}
		}
		else
		{
			RHICmdList.DrawPrimitive(
					Mesh.Type,
					BatchElement.FirstIndex,
					BatchElement.NumPrimitives,
					BatchElement.NumInstances
					);
		}
	}
}

void FMeshDrawingPolicy::SetSharedState(FRHICommandList& RHICmdList, const FSceneView* View, const ContextDataType PolicyContext) const
{
	check(VertexFactory && VertexFactory->IsInitialized());
	VertexFactory->Set(RHICmdList);
}

/**
* Get the decl and stream strides for this mesh policy type and vertexfactory
* @param VertexDeclaration - output decl 
*/
const FVertexDeclarationRHIRef& FMeshDrawingPolicy::GetVertexDeclaration() const
{
	check(VertexFactory && VertexFactory->IsInitialized());
	const FVertexDeclarationRHIRef& VertexDeclaration = VertexFactory->GetDeclaration();
	check(IsValidRef(VertexDeclaration));
	return VertexDeclaration;
}
