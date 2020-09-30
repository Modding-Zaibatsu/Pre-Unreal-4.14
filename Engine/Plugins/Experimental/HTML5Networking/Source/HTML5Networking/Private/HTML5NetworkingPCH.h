// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Core.h"
#include "Engine.h"
#include "Sockets.h"
#include "SocketSubsystem.h"
#include "ModuleManager.h"

class FWebSocket; 
class FWebSocketServer; 

typedef struct lws_context WebSocketInternalContext;
typedef struct lws WebSocketInternal;
typedef struct lws_protocols WebSocketInternalProtocol;

DECLARE_DELEGATE_TwoParams(FWebsocketPacketRecievedCallBack, void* /*Data*/, int32 /*Data Size*/);
DECLARE_DELEGATE_OneParam(FWebsocketClientConnectedCallBack, FWebSocket* /*Socket*/);
DECLARE_DELEGATE(FWebsocketInfoCallBack); 

DECLARE_LOG_CATEGORY_EXTERN(LogHTML5Networking, Warning, All);
