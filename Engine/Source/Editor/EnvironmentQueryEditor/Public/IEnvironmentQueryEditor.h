// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Toolkits/IToolkit.h"
#include "Toolkits/AssetEditorToolkit.h"


/** EnvQuery Editor public interface */
class IEnvironmentQueryEditor : public FAssetEditorToolkit
{

public:

	virtual uint32 GetSelectedNodesCount() const = 0;
};


