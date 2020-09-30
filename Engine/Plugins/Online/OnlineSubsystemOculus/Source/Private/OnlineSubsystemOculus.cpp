// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "OnlineSubsystemOculusPrivatePCH.h"
#include "OnlineSubsystemOculus.h"

#include "OnlineAchievementsInterfaceOculus.h"
#include "OnlineFriendsInterfaceOculus.h"
#include "OnlineIdentityOculus.h"
#include "OnlineLeaderboardInterfaceOculus.h"
#include "OnlineSessionInterfaceOculus.h"
#include "OnlineUserCloudOculus.h"

#if PLATFORM_ANDROID
#include "AndroidApplication.h"
#endif

#if !OVRPL_DISABLED && WITH_EDITOR
OVRPL_PUBLIC_FUNCTION(void) ovr_ResetInitAndContext();
#endif

IOnlineSessionPtr FOnlineSubsystemOculus::GetSessionInterface() const
{
	return SessionInterface;
}

IOnlineGroupsPtr FOnlineSubsystemOculus::GetGroupsInterface() const
{
	return nullptr;
}

IOnlineFriendsPtr FOnlineSubsystemOculus::GetFriendsInterface() const
{
	return FriendsInterface;
}

IOnlineSharedCloudPtr FOnlineSubsystemOculus::GetSharedCloudInterface() const
{
	return nullptr;
}

IOnlineUserCloudPtr FOnlineSubsystemOculus::GetUserCloudInterface() const
{
	return UserCloudInterface;
}

IOnlineEntitlementsPtr FOnlineSubsystemOculus::GetEntitlementsInterface() const
{
	return nullptr;
};

IOnlineLeaderboardsPtr FOnlineSubsystemOculus::GetLeaderboardsInterface() const
{
	return LeaderboardsInterface;
}

IOnlineVoicePtr FOnlineSubsystemOculus::GetVoiceInterface() const
{
	return nullptr;
}

IOnlineExternalUIPtr FOnlineSubsystemOculus::GetExternalUIInterface() const
{
	return nullptr;
}

IOnlineTimePtr FOnlineSubsystemOculus::GetTimeInterface() const
{
	return nullptr;
}

IOnlineIdentityPtr FOnlineSubsystemOculus::GetIdentityInterface() const
{
	return IdentityInterface;
}

IOnlinePartyPtr FOnlineSubsystemOculus::GetPartyInterface() const
{
	return nullptr;
}

IOnlineTitleFilePtr FOnlineSubsystemOculus::GetTitleFileInterface() const
{
	return nullptr;
}

IOnlineStorePtr FOnlineSubsystemOculus::GetStoreInterface() const
{
	return nullptr;
}

IOnlineStoreV2Ptr FOnlineSubsystemOculus::GetStoreV2Interface() const
{
	return nullptr;
}

IOnlinePurchasePtr FOnlineSubsystemOculus::GetPurchaseInterface() const
{
	return nullptr;
}

IOnlineEventsPtr FOnlineSubsystemOculus::GetEventsInterface() const
{
	return nullptr;
}

IOnlineAchievementsPtr FOnlineSubsystemOculus::GetAchievementsInterface() const
{
	return AchievementsInterface;
}

IOnlineSharingPtr FOnlineSubsystemOculus::GetSharingInterface() const
{
	return nullptr;
}

IOnlineUserPtr FOnlineSubsystemOculus::GetUserInterface() const
{
	return nullptr;
}

IOnlineMessagePtr FOnlineSubsystemOculus::GetMessageInterface() const
{
	return nullptr;
}

IOnlinePresencePtr FOnlineSubsystemOculus::GetPresenceInterface() const
{
	return nullptr;
}

IOnlineChatPtr FOnlineSubsystemOculus::GetChatInterface() const
{
	return nullptr;
}

IOnlineTurnBasedPtr FOnlineSubsystemOculus::GetTurnBasedInterface() const
{
	return nullptr;
}

bool FOnlineSubsystemOculus::Tick(float DeltaTime)
{
	if (!FOnlineSubsystemImpl::Tick(DeltaTime))
	{
		return false;
	}

	if (SessionInterface.IsValid())
	{
		SessionInterface->TickPendingInvites(DeltaTime);
	}

	if (MessageTaskManager.IsValid())
	{
		if (!MessageTaskManager->Tick(DeltaTime))
		{
			UE_LOG_ONLINE(Error, TEXT("An error occured when processing the message queue"));
		}
	}

	return true;
}

void FOnlineSubsystemOculus::AddRequestDelegate(ovrRequest RequestId, FOculusMessageOnCompleteDelegate&& Delegate)
{
	check(MessageTaskManager);
	MessageTaskManager->AddRequestDelegate(RequestId, MoveTemp(Delegate));
}

FOculusMulticastMessageOnCompleteDelegate& FOnlineSubsystemOculus::GetNotifDelegate(ovrMessageType MessageType)
{
	check(MessageTaskManager);
	return MessageTaskManager->GetNotifDelegate(MessageType);
}

void FOnlineSubsystemOculus::RemoveNotifDelegate(ovrMessageType MessageType, const FDelegateHandle& Delegate)
{
	check(MessageTaskManager);
	return MessageTaskManager->RemoveNotifDelegate(MessageType, Delegate);
}

bool FOnlineSubsystemOculus::Init()
{
	bool bOculusInit = false;
#if PLATFORM_WINDOWS
	bOculusInit = InitWithWindowsPlatform();
#elif PLATFORM_ANDROID
	bOculusInit = InitWithAndroidPlatform();
#endif
	if (bOculusInit)
	{
		MessageTaskManager = MakeUnique<FOnlineMessageTaskManagerOculus>();
		check(MessageTaskManager);

		IdentityInterface = MakeShareable(new FOnlineIdentityOculus(*this));
		AchievementsInterface = MakeShareable(new FOnlineAchievementsOculus(*this));
		FriendsInterface = MakeShareable(new FOnlineFriendsOculus(*this));
		SessionInterface = MakeShareable(new FOnlineSessionOculus(*this));
		LeaderboardsInterface = MakeShareable(new FOnlineLeaderboardOculus(*this));
		UserCloudInterface = MakeShareable(new FOnlineUserCloudOculus(*this));
	}
	else
	{
		// Only do the parent shutdown since nothing else is setup and we don't want to do
		// any LibOVRPlatform calls against an invalid or missing dll
		FOnlineSubsystemImpl::Shutdown();
	}

	return bOculusInit;
}

#if PLATFORM_WINDOWS
bool FOnlineSubsystemOculus::InitWithWindowsPlatform()
{
	UE_LOG_ONLINE(Display, TEXT("FOnlineSubsystemOculus::InitWithWindowsPlatform()"));
	auto OculusAppId = GetAppId();
	if (OculusAppId.IsEmpty())
	{
		UE_LOG_ONLINE(Error, TEXT("Missing OculusAppId key in OnlineSubsystemOculus of DefaultEngine.ini"));
		return false;
	}

	auto InitResult = ovr_PlatformInitializeWindows(TCHAR_TO_ANSI(*OculusAppId));
	if (InitResult != ovrPlatformInitialize_Success)
	{
		UE_LOG_ONLINE(Warning, TEXT("Failed to initialize the Oculus Platform SDK! Error code: %d"), (int)InitResult);
		return false;
	}
	return true;
}
#elif PLATFORM_ANDROID
bool FOnlineSubsystemOculus::InitWithAndroidPlatform()
{
	UE_LOG_ONLINE(Display, TEXT("FOnlineSubsystemOculus::InitWithAndroidPlatform()"));
	auto OculusAppId = GetAppId();
	if (OculusAppId.IsEmpty())
	{
		UE_LOG_ONLINE(Error, TEXT("Missing OculusAppId key in OnlineSubsystemOculus of DefaultEngine.ini"));
		return false;
	}

	JNIEnv* Env = FAndroidApplication::GetJavaEnv();

	if (Env == nullptr)
	{
		UE_LOG_ONLINE(Error, TEXT("Missing JNIEnv"));
		return false;
	}

	auto InitResult = ovr_PlatformInitializeAndroid(TCHAR_TO_ANSI(*OculusAppId), FAndroidApplication::GetGameActivityThis(), Env);
	if (InitResult != ovrPlatformInitialize_Success)
	{
		UE_LOG_ONLINE(Error, TEXT("Failed to initialize the Oculus Platform SDK! Error code: %d"), (int)InitResult);
		return false;
	}

	return true;
}
#endif

bool FOnlineSubsystemOculus::Shutdown()
{
	UE_LOG_ONLINE(Display, TEXT("FOnlineSubsystemOculus::Shutdown()"));

	FOnlineSubsystemImpl::Shutdown();

	AchievementsInterface.Reset();
	FriendsInterface.Reset();
	IdentityInterface.Reset();
	SessionInterface.Reset();
	LeaderboardsInterface.Reset();
	UserCloudInterface.Reset();

	if (MessageTaskManager.IsValid())
	{
		MessageTaskManager.Release();
	}

#if !OVRPL_DISABLED && WITH_EDITOR
	// If we are playing in the editor,
	// Destroy the context and reset the init status
	ovr_ResetInitAndContext();
#endif

	return true;
}

FString FOnlineSubsystemOculus::GetAppId() const
{
	return GConfig->GetStr(TEXT("OnlineSubsystemOculus"), TEXT("OculusAppId"), GEngineIni);
}

bool FOnlineSubsystemOculus::Exec(UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar)
{
	return false;
}

bool FOnlineSubsystemOculus::IsEnabled()
{
	bool bEnableOculus = true;
	GConfig->GetBool(TEXT("OnlineSubsystemOculus"), TEXT("bEnabled"), bEnableOculus, GEngineIni);
	return bEnableOculus;
}
