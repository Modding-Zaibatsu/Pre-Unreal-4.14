// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

// Actions that can be invoked in the Size Map
class FSizeMapActions : public TCommands<FSizeMapActions>
{
public:
	FSizeMapActions() : TCommands<FSizeMapActions>
	(
		"SizeMap",
		NSLOCTEXT("Contexts", "SizeMap", "Size Map"),
		"MainFrame", FEditorStyle::GetStyleSetName()
	)
	{
	}

	// TCommands<> interface
	virtual void RegisterCommands() override;
	// End of TCommands<> interface
public:

	// @todo sizemap: Register any commands we have here
	// TSharedPtr<FUICommandInfo> DoSomething;
	
};
