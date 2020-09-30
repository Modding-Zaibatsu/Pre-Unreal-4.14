// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "EnginePrivate.h"
#include "SlateBasics.h"
#include "AudioDevice.h"
#include "Scalability.h"
#include "GameFramework/GameUserSettings.h"
#include "Engine/GameEngine.h"
#include "Kismet/KismetSystemLibrary.h"

extern EWindowMode::Type GetWindowModeType(EWindowMode::Type WindowMode);

enum EGameUserSettingsVersion
{
	/** Version for user game settings. All settings will be wiped if the serialized version differs. */
	UE_GAMEUSERSETTINGS_VERSION = 5
};


UGameUserSettings::UGameUserSettings(const FObjectInitializer& ObjectInitializer)
:	Super(ObjectInitializer)
{
	SetToDefaults();
}

FIntPoint UGameUserSettings::GetScreenResolution() const
{
	return FIntPoint(ResolutionSizeX, ResolutionSizeY);
}

FIntPoint UGameUserSettings::GetLastConfirmedScreenResolution() const
{
	return FIntPoint(LastUserConfirmedResolutionSizeX, LastUserConfirmedResolutionSizeY);
}

FIntPoint UGameUserSettings::GetDesktopResolution() const
{
	FDisplayMetrics DisplayMetrics;
	if (FSlateApplication::IsInitialized())
	{
		FSlateApplication::Get().GetInitialDisplayMetrics(DisplayMetrics);
	}
	else
	{
		if (FApp::CanEverRender())
		{
			FDisplayMetrics::GetDisplayMetrics(DisplayMetrics);
		}
		else
		{
			// assume headless
			DisplayMetrics.PrimaryDisplayWidth = 0;
			DisplayMetrics.PrimaryDisplayHeight = 0;
		}
	}
	return FIntPoint(DisplayMetrics.PrimaryDisplayWidth, DisplayMetrics.PrimaryDisplayHeight);
}

void UGameUserSettings::SetScreenResolution(FIntPoint Resolution)
{
	if (ResolutionSizeX != Resolution.X || ResolutionSizeY != Resolution.Y)
	{
		ResolutionSizeX = Resolution.X;
		ResolutionSizeY = Resolution.Y;
		UpdateResolutionQuality();
	}
}

EWindowMode::Type UGameUserSettings::GetFullscreenMode() const
{
	return EWindowMode::ConvertIntToWindowMode(FullscreenMode);
}

EWindowMode::Type UGameUserSettings::GetLastConfirmedFullscreenMode() const
{
	return EWindowMode::ConvertIntToWindowMode(LastConfirmedFullscreenMode);
}

void UGameUserSettings::SetFullscreenMode( EWindowMode::Type InFullscreenMode )
{
	switch ( InFullscreenMode )
	{
		case EWindowMode::Fullscreen:
			FullscreenMode = 0;
			break;
		case EWindowMode::WindowedFullscreen:
			FullscreenMode = 1;
			break;
		case EWindowMode::Windowed:
		default:
			FullscreenMode = 2;
			break;
	}

	UpdateResolutionQuality();
}

EWindowMode::Type UGameUserSettings::GetPreferredFullscreenMode() const
{
	return PreferredFullscreenMode == 0 ? EWindowMode::Fullscreen : EWindowMode::WindowedFullscreen;
}

void UGameUserSettings::SetVSyncEnabled( bool bEnable )
{
	bUseVSync = bEnable;
}

bool UGameUserSettings::IsVSyncEnabled() const
{
	return bUseVSync;
}

bool UGameUserSettings::IsScreenResolutionDirty() const
{
	bool bIsDirty = false;
	if ( GEngine && GEngine->GameViewport && GEngine->GameViewport->ViewportFrame )
	{
		bIsDirty = (ResolutionSizeX != GSystemResolution.ResX || ResolutionSizeY != GSystemResolution.ResY) ? true : false;
	}
	return bIsDirty;
}

bool UGameUserSettings::IsFullscreenModeDirty() const
{
	bool bIsDirty = false;
	if ( GEngine && GEngine->GameViewport && GEngine->GameViewport->ViewportFrame )
	{
		EWindowMode::Type WindowMode = GEngine->GameViewport->IsFullScreenViewport() ? EWindowMode::Fullscreen : EWindowMode::Windowed;
		EWindowMode::Type CurrentFullscreenMode = GetWindowModeType(WindowMode);
		EWindowMode::Type NewFullscreenMode = GetFullscreenMode();
		bIsDirty = (CurrentFullscreenMode != NewFullscreenMode) ? true : false;
	}
	return bIsDirty;
}

bool UGameUserSettings::IsVSyncDirty() const
{
	bool bIsDirty = false;
	if ( GEngine && GEngine->GameViewport && GEngine->GameViewport->ViewportFrame )
	{
		static const auto CVar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.VSync"));
		bIsDirty = (bUseVSync != (CVar->GetValueOnGameThread() != 0));
	}
	return bIsDirty;
}

bool UGameUserSettings::IsDirty() const
{
	return IsScreenResolutionDirty() || IsFullscreenModeDirty() || IsVSyncDirty();
}

void UGameUserSettings::ConfirmVideoMode()
{
	LastConfirmedFullscreenMode = FullscreenMode;
	LastUserConfirmedResolutionSizeX = ResolutionSizeX;
	LastUserConfirmedResolutionSizeY = ResolutionSizeY;
}

void UGameUserSettings::RevertVideoMode()
{
	FullscreenMode = LastConfirmedFullscreenMode;
	ResolutionSizeX = LastUserConfirmedResolutionSizeX;
	ResolutionSizeY = LastUserConfirmedResolutionSizeY;
}

void UGameUserSettings::SetToDefaults()
{
	ResolutionSizeX = LastUserConfirmedResolutionSizeX = GetDefaultResolution().X;
	ResolutionSizeY = LastUserConfirmedResolutionSizeY = GetDefaultResolution().Y;
	WindowPosX = GetDefaultWindowPosition().X;
	WindowPosY = GetDefaultWindowPosition().Y;
	FullscreenMode = GetDefaultWindowMode();
	FrameRateLimit = 0.0f;
	MinResolutionScale = Scalability::MinResolutionScale;
	DesiredScreenWidth = 1280;
	DesiredScreenHeight = 720;
	LastCPUBenchmarkResult = -1.0f;
	LastGPUBenchmarkResult = -1.0f;
	LastCPUBenchmarkSteps.Empty();
	LastGPUBenchmarkSteps.Empty();
	LastGPUBenchmarkMultiplier = 1.0f;
	LastRecommendedScreenWidth = -1.0f;
	LastRecommendedScreenHeight = -1.0f;

	static const auto CVar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.FullScreenMode"));
	PreferredFullscreenMode = CVar->GetValueOnGameThread();

	ScalabilityQuality.SetDefaults();

	if (FApp::CanEverRender())
	{
		UpdateResolutionQuality();
	}
}

bool UGameUserSettings::IsVersionValid()
{
	return (Version == UE_GAMEUSERSETTINGS_VERSION);
}

void UGameUserSettings::UpdateVersion()
{
	Version = UE_GAMEUSERSETTINGS_VERSION;
}

void UGameUserSettings::UpdateResolutionQuality()
{
	const int32 MinHeight = UKismetSystemLibrary::GetMinYResolutionFor3DView();
	const int32 ScreenWidth = (FullscreenMode == EWindowMode::WindowedFullscreen) ? GetDesktopResolution().X : ResolutionSizeX;
	const int32 ScreenHeight = (FullscreenMode == EWindowMode::WindowedFullscreen) ? GetDesktopResolution().Y : ResolutionSizeY;
	MinResolutionScale = FMath::Max<float>(Scalability::MinResolutionScale, ((float)MinHeight / (float)ScreenHeight) * 100.0f);

	// Clamp the desired width to the actual window width
	if (ScreenWidth > 0 && DesiredScreenWidth > ScreenWidth)
	{
		DesiredScreenWidth = ScreenWidth;
	}

	// Clamp the desired height to the actual window height
	if (ScreenHeight > 0 && DesiredScreenHeight > ScreenHeight)
	{
		DesiredScreenHeight = ScreenHeight;
	}

	if (bUseDesiredScreenHeight)
	{
		ScalabilityQuality.ResolutionQuality = UGameUserSettings::GetDefaultResolutionScale();
	}
	else
	{
		ScalabilityQuality.ResolutionQuality = FMath::Max(ScalabilityQuality.ResolutionQuality, MinResolutionScale);
	}

	OnGameUserSettingsUINeedsUpdate.Broadcast();
}

float UGameUserSettings::GetDefaultResolutionScale()
{
	const float DesiredResQuality = FindResolutionQualityForScreenSize(DesiredScreenWidth, DesiredScreenHeight);

	return FMath::Max(DesiredResQuality, MinResolutionScale);
}

float UGameUserSettings::FindResolutionQualityForScreenSize(float Width, float Height)
{
	float ResolutionQuality = 100.0f;

	const FIntPoint ScreenSize = (FullscreenMode == EWindowMode::WindowedFullscreen) ? GetDesktopResolution() : FIntPoint(ResolutionSizeX, ResolutionSizeY);
	const float ScreenAspectRatio = (float)ScreenSize.X / (float)ScreenSize.Y;
	const float AspectRatio = Width / Height;

	// If the screen aspect ratio is different than the target resolution's aspect ratio, we need to adjust the target width and height
	if (!FMath::IsNearlyEqual(ScreenAspectRatio, AspectRatio))
	{
		if (ScreenAspectRatio < AspectRatio)
		{
			// For smaller aspect ratios we allow more vertical space so that the screen width matches the width of the original mode
			Height = (Height * AspectRatio) / ScreenAspectRatio;
		}
		else
		{
			// For wider screens we try to choose a screen size that'd have similar total number of pixels as the original mode
			Height = FMath::Sqrt((Height * Height * AspectRatio) / ScreenAspectRatio);
		}
		Width = Height * AspectRatio;
	}

	if (Height < ScreenSize.Y)
	{
		ResolutionQuality = ((float)Height / (float)ScreenSize.Y) * 100.0f;
	}

	return ResolutionQuality;
}

void UGameUserSettings::SetFrameRateLimitCVar(float InLimit)
{
	static IConsoleVariable* MaxFPSCVar = IConsoleManager::Get().FindConsoleVariable(TEXT("t.MaxFPS"));
	if (ensure(MaxFPSCVar))
	{
		MaxFPSCVar->Set(FMath::Max(InLimit, 0.0f), ECVF_SetByGameSetting);
	}
}

float UGameUserSettings::GetEffectiveFrameRateLimit()
{
	return FrameRateLimit;
}

void UGameUserSettings::SetPreferredFullscreenMode(int32 Mode)
{
	PreferredFullscreenMode = Mode;

	auto CVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.FullScreenMode"));
	if (CVar)
	{
		CVar->Set(Mode, ECVF_SetByGameSetting);
	}
}

void UGameUserSettings::ValidateSettings()
{
	// Should we wipe all user settings?
	if ( !IsVersionValid() )
	{
		// First try loading the settings, if they haven't been loaded before.
		LoadSettings(true);

		// If it still an old version, delete the user settings file and reload defaults.
		if ( !IsVersionValid() )
		{
			// Force reset if there aren't any default .ini settings.
			SetToDefaults();
			static const auto CVar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.VSync"));
			SetVSyncEnabled( CVar->GetValueOnGameThread() != 0 );

			IFileManager::Get().Delete( *GGameUserSettingsIni );
			LoadSettings(true);
		}
	}

	if ( ResolutionSizeX <= 0 || ResolutionSizeY <= 0 )
	{
		SetScreenResolution(FIntPoint(GSystemResolution.ResX, GSystemResolution.ResY));

		// Set last confirmed video settings
		LastConfirmedFullscreenMode = FullscreenMode;
		LastUserConfirmedResolutionSizeX = ResolutionSizeX;
		LastUserConfirmedResolutionSizeY = ResolutionSizeY;
	}

	// The user settings have now been validated for the current version.
	UpdateVersion();
}

void UGameUserSettings::ApplyNonResolutionSettings()
{
	QUICK_SCOPE_CYCLE_COUNTER(GameUserSettings_ApplyNonResolutionSettings);

	ValidateSettings();

	// Update vsync cvar
	{
		FString ConfigSection = TEXT("SystemSettings");
#if WITH_EDITOR
		if (GIsEditor)
		{
			ConfigSection = TEXT("SystemSettingsEditor");
		}
#endif
		int32 VSyncValue = 0;
		if (GConfig->GetInt(*ConfigSection, TEXT("r.Vsync"), VSyncValue, GEngineIni))
		{
			// VSync was already set by system settings. We are capable of setting it here.
		}
		else
		{
			static auto CVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.VSync"));
			CVar->Set(IsVSyncEnabled(), ECVF_SetByGameSetting);
		}
	}

	if (!IsRunningDedicatedServer())
	{
		SetFrameRateLimitCVar(GetEffectiveFrameRateLimit());
	}

	// in init those are loaded earlier, after that we apply consolevariables.ini
	if (GEngine->IsInitialized())
	{
		Scalability::SetQualityLevels(ScalabilityQuality);
	}

	IConsoleManager::Get().CallAllConsoleVariableSinks();
}

void UGameUserSettings::ApplyResolutionSettings(bool bCheckForCommandLineOverrides)
{
#if UE_SERVER
	return;
#endif
	QUICK_SCOPE_CYCLE_COUNTER(GameUserSettings_ApplyResolutionSettings);

	ValidateSettings();

	EWindowMode::Type NewFullscreenMode = GetFullscreenMode();

	// Request a resolution change
	RequestResolutionChange(ResolutionSizeX, ResolutionSizeY, NewFullscreenMode, bCheckForCommandLineOverrides);

	if (NewFullscreenMode == EWindowMode::Fullscreen || NewFullscreenMode == EWindowMode::WindowedFullscreen)
	{
		SetPreferredFullscreenMode(NewFullscreenMode == EWindowMode::Fullscreen ? 0 : 1);
	}

	IConsoleManager::Get().CallAllConsoleVariableSinks();
}

void UGameUserSettings::ApplySettings(bool bCheckForCommandLineOverrides)
{
	ApplyResolutionSettings(bCheckForCommandLineOverrides);
	ApplyNonResolutionSettings();

	SaveSettings();
	UE_LOG(LogConsoleResponse, Display, TEXT(""));
}

void UGameUserSettings::LoadSettings( bool bForceReload/*=false*/ )
{
	QUICK_SCOPE_CYCLE_COUNTER(GameUserSettings_LoadSettings);

	if ( bForceReload )
	{
		LoadConfigIni( bForceReload );
	}
	LoadConfig(GetClass(), *GGameUserSettingsIni);

	// Note: Scalability::LoadState() should not be needed as we already loaded the settings earlier (needed so the engine can startup with that before the game is initialized)
	ScalabilityQuality = Scalability::GetQualityLevels();

	// Allow override using command-line settings
	bool bDetectingResolution = ResolutionSizeX == 0 || ResolutionSizeY == 0;

	if (bDetectingResolution)
	{
		ConfirmVideoMode();
	}

	// Update r.FullScreenMode CVar
	SetPreferredFullscreenMode(PreferredFullscreenMode);
}

void UGameUserSettings::RequestResolutionChange(int32 InResolutionX, int32 InResolutionY, EWindowMode::Type InWindowMode, bool bInDoOverrides /* = true */)
{
	if (bInDoOverrides)
	{
		UGameEngine::ConditionallyOverrideSettings(InResolutionX, InResolutionY, InWindowMode);
	}

	FSystemResolution::RequestResolutionChange(InResolutionX, InResolutionY, InWindowMode);
}

void UGameUserSettings::SaveSettings()
{
	QUICK_SCOPE_CYCLE_COUNTER(GameUserSettings_SaveSettings);

	// Save the Scalability state to the same ini file as it was loaded from in FEngineLoop::Preinit
	Scalability::SaveState(GIsEditor ? GEditorSettingsIni : GGameUserSettingsIni);
	SaveConfig(CPF_Config, *GGameUserSettingsIni);
}

void UGameUserSettings::LoadConfigIni( bool bForceReload/*=false*/ )
{
	// Load .ini, allowing merging
	FConfigCacheIni::LoadGlobalIniFile(GGameUserSettingsIni, TEXT("GameUserSettings"), NULL, bForceReload);
}

void UGameUserSettings::PreloadResolutionSettings()
{
	// Note: This preloads resolution settings without loading the user settings object.  
	// When changing this code care must be taken to ensure the window starts at the same resolution as the in game resolution
	LoadConfigIni();

	FString ScriptEngineCategory = TEXT("/Script/Engine.Engine");
	FString GameUserSettingsCategory = TEXT("/Script/Engine.GameUserSettings");

	GConfig->GetString(*ScriptEngineCategory, TEXT("GameUserSettingsClassName"), GameUserSettingsCategory, GEngineIni);

	int32 ResolutionX = GetDefaultResolution().X; 
	int32 ResolutionY = GetDefaultResolution().Y;
	EWindowMode::Type WindowMode = GetDefaultWindowMode();
	bool bUseDesktopResolution = false;

	int32 Version=0;
	if( GConfig->GetInt(*GameUserSettingsCategory, TEXT("Version"), Version, GGameUserSettingsIni ) && Version == UE_GAMEUSERSETTINGS_VERSION )
	{
		GConfig->GetBool(*GameUserSettingsCategory, TEXT("bUseDesktopResolution"), bUseDesktopResolution, GGameUserSettingsIni );

		int32 WindowModeInt = (int32)WindowMode;
		GConfig->GetInt(*GameUserSettingsCategory, TEXT("FullscreenMode"), WindowModeInt, GGameUserSettingsIni);
		WindowMode = EWindowMode::ConvertIntToWindowMode(WindowModeInt);

		GConfig->GetInt(*GameUserSettingsCategory, TEXT("ResolutionSizeX"), ResolutionX, GGameUserSettingsIni);
		GConfig->GetInt(*GameUserSettingsCategory, TEXT("ResolutionSizeY"), ResolutionY, GGameUserSettingsIni);

#if PLATFORM_DESKTOP
		if (bUseDesktopResolution && ResolutionX == 0 && ResolutionY == 0 && WindowMode != EWindowMode::Windowed)
		{
			// Grab display metrics so we can get the primary display output size.
			FDisplayMetrics DisplayMetrics;
			FDisplayMetrics::GetDisplayMetrics(DisplayMetrics);

			ResolutionX = DisplayMetrics.PrimaryDisplayWidth;
			ResolutionY = DisplayMetrics.PrimaryDisplayHeight;
		}
#endif
	}

	RequestResolutionChange(ResolutionX, ResolutionY, WindowMode);

	IConsoleManager::Get().CallAllConsoleVariableSinks();
}

FIntPoint UGameUserSettings::GetDefaultResolution()
{
	return FIntPoint::ZeroValue;
}

FIntPoint UGameUserSettings::GetDefaultWindowPosition()
{
	return FIntPoint(-1,-1);
}

EWindowMode::Type UGameUserSettings::GetDefaultWindowMode()
{
	return EWindowMode::Windowed;
}

void UGameUserSettings::ResetToCurrentSettings()
{
	if ( GEngine && GEngine->GameViewport && GEngine->GameViewport->GetWindow().IsValid() )
	{
		//handle the fullscreen setting
		SetFullscreenMode(GetWindowModeType(GEngine->GameViewport->GetWindow()->GetWindowMode()));

		//set the current resolution
		SetScreenResolution(FIntPoint(GSystemResolution.ResX, GSystemResolution.ResY));

		// Set the current VSync state
		static const auto CVar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.VSync"));
		SetVSyncEnabled( CVar->GetValueOnGameThread() != 0 );

		// Reset to confirmed settings
		FullscreenMode = LastConfirmedFullscreenMode;
		ResolutionSizeX = LastUserConfirmedResolutionSizeX;
		ResolutionSizeY = LastUserConfirmedResolutionSizeY;

		// Reset the quality settings to the current levels
		ScalabilityQuality = Scalability::GetQualityLevels();

		UpdateResolutionQuality();
	}
}

void UGameUserSettings::SetWindowPosition(int32 WinX, int32 WinY)
{
	WindowPosX = WinX;
	WindowPosY = WinY;
}

FIntPoint UGameUserSettings::GetWindowPosition()
{
	return FIntPoint(WindowPosX, WindowPosY);
}

void UGameUserSettings::SetBenchmarkFallbackValues()
{
	ScalabilityQuality.SetBenchmarkFallback();
}

void UGameUserSettings::SetAudioQualityLevel(int32 QualityLevel)
{
	AudioQualityLevel = QualityLevel;
}

void UGameUserSettings::SetFrameRateLimit(float NewLimit)
{
	FrameRateLimit = NewLimit;
}

float UGameUserSettings::GetFrameRateLimit() const
{
	return FrameRateLimit;
}

void UGameUserSettings::SetOverallScalabilityLevel(int32 Value)
{
	ScalabilityQuality.SetFromSingleQualityLevel(Value);	
}

int32 UGameUserSettings::GetOverallScalabilityLevel() const
{
	return ScalabilityQuality.GetSingleQualityLevel();
}

void UGameUserSettings::GetResolutionScaleInformation(float& CurrentScaleNormalized, int32& CurrentScaleValue, int32& MinScaleValue, int32& MaxScaleValue) const
{
	CurrentScaleValue = ScalabilityQuality.ResolutionQuality;
	MinScaleValue = MinResolutionScale;
	MaxScaleValue = Scalability::MaxResolutionScale;
	CurrentScaleNormalized = ((float)CurrentScaleValue - (float)MinScaleValue) / (float)(MaxScaleValue - MinScaleValue);
}

void UGameUserSettings::GetResolutionScaleInformationEx(float& CurrentScaleNormalized, float& CurrentScaleValue, float& MinScaleValue, float& MaxScaleValue) const
{
	CurrentScaleValue = ScalabilityQuality.ResolutionQuality;
	MinScaleValue = MinResolutionScale;
	MaxScaleValue = Scalability::MaxResolutionScale;
	CurrentScaleNormalized = ((float)CurrentScaleValue - (float)MinScaleValue) / (float)(MaxScaleValue - MinScaleValue);
}

void UGameUserSettings::SetResolutionScaleValue(int32 NewScaleValue)
{
	SetResolutionScaleValueEx((float)NewScaleValue);
}

void UGameUserSettings::SetResolutionScaleValueEx(float NewScaleValue)
{
	ScalabilityQuality.ResolutionQuality = FMath::Clamp(NewScaleValue, MinResolutionScale, Scalability::MaxResolutionScale);
	const int32 ScreenWidth = (FullscreenMode == EWindowMode::WindowedFullscreen) ? GetDesktopResolution().X : ResolutionSizeX;
	const int32 ScreenHeight = (FullscreenMode == EWindowMode::WindowedFullscreen) ? GetDesktopResolution().Y : ResolutionSizeY;
	DesiredScreenWidth = ScreenWidth * ScalabilityQuality.ResolutionQuality / 100.0f;
	DesiredScreenHeight = ScreenHeight * ScalabilityQuality.ResolutionQuality / 100.0f;
}

void UGameUserSettings::SetResolutionScaleNormalized(float NewScaleNormalized)
{
	const float RemappedValue = FMath::Lerp((float)MinResolutionScale, (float)Scalability::MaxResolutionScale, NewScaleNormalized);
	SetResolutionScaleValueEx(RemappedValue);
}

void UGameUserSettings::SetViewDistanceQuality(int32 Value)
{
	ScalabilityQuality.ViewDistanceQuality = Value;
}

int32 UGameUserSettings::GetViewDistanceQuality() const
{
	return ScalabilityQuality.ViewDistanceQuality;
}

void UGameUserSettings::SetShadowQuality(int32 Value)
{
	ScalabilityQuality.ShadowQuality = Value;
}

int32 UGameUserSettings::GetShadowQuality() const
{
	return ScalabilityQuality.ShadowQuality;
}

void UGameUserSettings::SetAntiAliasingQuality(int32 Value)
{
	ScalabilityQuality.AntiAliasingQuality = Value;
}

int32 UGameUserSettings::GetAntiAliasingQuality() const
{
	return ScalabilityQuality.AntiAliasingQuality;
}

void UGameUserSettings::SetTextureQuality(int32 Value)
{
	ScalabilityQuality.TextureQuality = Value;
}

int32 UGameUserSettings::GetTextureQuality() const
{
	return ScalabilityQuality.TextureQuality;
}

void UGameUserSettings::SetVisualEffectQuality(int32 Value)
{
	ScalabilityQuality.EffectsQuality = Value;
}

int32 UGameUserSettings::GetVisualEffectQuality() const
{
	return ScalabilityQuality.EffectsQuality;
}

void UGameUserSettings::SetPostProcessingQuality(int32 Value)
{
	ScalabilityQuality.PostProcessQuality = Value;
}

int32 UGameUserSettings::GetPostProcessingQuality() const
{
	return ScalabilityQuality.PostProcessQuality;
}

void UGameUserSettings::SetFoliageQuality(int32 Value)
{
	ScalabilityQuality.FoliageQuality = FMath::Clamp(Value, 0, 3);
}

int32 UGameUserSettings::GetFoliageQuality() const
{
	return ScalabilityQuality.FoliageQuality;
}

UGameUserSettings* UGameUserSettings::GetGameUserSettings()
{
	return GEngine->GetGameUserSettings();
}

void UGameUserSettings::RunHardwareBenchmark(int32 WorkScale, float CPUMultiplier, float GPUMultiplier)
{
	ScalabilityQuality = Scalability::BenchmarkQualityLevels(WorkScale, CPUMultiplier, GPUMultiplier);
	LastCPUBenchmarkResult = ScalabilityQuality.CPUBenchmarkResults;
	LastGPUBenchmarkResult = ScalabilityQuality.GPUBenchmarkResults;
	LastCPUBenchmarkSteps = ScalabilityQuality.CPUBenchmarkSteps;
	LastGPUBenchmarkSteps = ScalabilityQuality.GPUBenchmarkSteps;
	LastGPUBenchmarkMultiplier = GPUMultiplier;
}

void UGameUserSettings::ApplyHardwareBenchmarkResults()
{
	// Apply the new settings and save them
	Scalability::SetQualityLevels(ScalabilityQuality);
	Scalability::SaveState(GGameUserSettingsIni);

	SaveSettings();
}
