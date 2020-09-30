// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ApplicationMode.h"
#include "WorkflowCentricApplication.h"
#include "PersonaModule.h"
#include "IDocumentation.h"

class FSkeletalMeshEditorMode : public FApplicationMode
{
public:
	FSkeletalMeshEditorMode(TSharedRef<class FWorkflowCentricApplication> InHostingApp, TSharedRef<class ISkeletonTree> InSkeletonTree);

	/** FApplicationMode interface */
	virtual void RegisterTabFactories(TSharedPtr<FTabManager> InTabManager) override;

protected:
	/** The hosting app */
	TWeakPtr<class FWorkflowCentricApplication> HostingAppPtr;

	/** The tab factories we support */
	FWorkflowAllowedTabSet TabFactories;
};

struct FMeshPropertiesSummoner : public FWorkflowTabFactory
{
	FMeshPropertiesSummoner(TSharedPtr<class FAssetEditorToolkit> InHostingApp, FOnGetAsset InOnGetAsset);

	// FWorkflowTabFactory interface
	virtual TSharedRef<SWidget> CreateTabBody(const FWorkflowTabSpawnInfo& Info) const override;

	// Create a tooltip widget for the tab
	virtual TSharedPtr<SToolTip> CreateTabToolTipWidget(const FWorkflowTabSpawnInfo& Info) const override
	{
		return  IDocumentation::Get()->CreateToolTip(NSLOCTEXT("MeshProperties", "MeshDetailsTooltip", "The Mesh Details tab lets you edit properties (materials etc) of the current Skeletal Mesh."), NULL, TEXT("Shared/Editors/Persona"), TEXT("MeshDetail_Window"));
	}
	// FWorkflowTabFactory interface

private:
	FOnGetAsset OnGetAsset;
};