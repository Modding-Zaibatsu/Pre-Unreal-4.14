// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "SlateRemotePrivatePCH.h"


USlateRemoteSettings::USlateRemoteSettings( const FObjectInitializer& ObjectInitializer )
	: Super(ObjectInitializer)
	, EnableRemoteServer(false)
	, EditorServerEndpoint(SLATE_REMOTE_SERVER_DEFAULT_EDITOR_ENDPOINT.ToString())
	, GameServerEndpoint(SLATE_REMOTE_SERVER_DEFAULT_GAME_ENDPOINT.ToString())
{ }
