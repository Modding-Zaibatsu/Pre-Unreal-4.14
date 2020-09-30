// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "CorePrivatePCH.h"
#include "AndroidApplication.h"
#include "AndroidInputInterface.h"
#include "AndroidWindow.h"
#include "IInputDeviceModule.h"

DEFINE_LOG_CATEGORY_STATIC(LogAndroidApplication, Log, All);

bool FAndroidApplication::bWindowSizeChanged = false;

FAndroidApplication* FAndroidApplication::CreateAndroidApplication()
{
	return new FAndroidApplication();
}

FAndroidApplication::FAndroidApplication()
	: GenericApplication( NULL )
	, InputInterface( FAndroidInputInterface::Create( MessageHandler ) )
	, bHasLoadedInputPlugins(false)
{
}

void FAndroidApplication::SetMessageHandler( const TSharedRef< FGenericApplicationMessageHandler >& InMessageHandler )
{
	GenericApplication::SetMessageHandler(InMessageHandler);
	InputInterface->SetMessageHandler( MessageHandler );
}

void FAndroidApplication::AddExternalInputDevice(TSharedPtr<IInputDevice> InputDevice)
{
	if (InputDevice.IsValid())
	{
		InputInterface->AddExternalInputDevice(InputDevice);
	}
}

void FAndroidApplication::PollGameDeviceState( const float TimeDelta )
{
	// initialize any externally-implemented input devices (we delay load initialize the array so any plugins have had time to load)
	if (!bHasLoadedInputPlugins)
	{
		TArray<IInputDeviceModule*> PluginImplementations = IModularFeatures::Get().GetModularFeatureImplementations<IInputDeviceModule>(IInputDeviceModule::GetModularFeatureName());
		for (auto InputPluginIt = PluginImplementations.CreateIterator(); InputPluginIt; ++InputPluginIt)
		{
			TSharedPtr<IInputDevice> Device = (*InputPluginIt)->CreateInputDevice(MessageHandler);
			AddExternalInputDevice(Device);
		}

		bHasLoadedInputPlugins = true;
	}

	// Poll game device state and send new events
	InputInterface->Tick( TimeDelta );
	InputInterface->SendControllerEvents();
	
	if (bWindowSizeChanged && 
		Windows.Num() > 0 && 
		FPlatformMisc::GetHardwareWindow() != nullptr)
	{
		int32 WindowX,WindowY, WindowWidth,WindowHeight;
		Windows[0]->GetFullScreenInfo(WindowX, WindowY, WindowWidth, WindowHeight);

		GenericApplication::GetMessageHandler()->OnSizeChanged(Windows[0],WindowWidth,WindowHeight, false);
		GenericApplication::GetMessageHandler()->OnResizingWindow(Windows[0]);
		
		FDisplayMetrics DisplayMetrics;
		FDisplayMetrics::GetDisplayMetrics(DisplayMetrics);
		BroadcastDisplayMetricsChanged(DisplayMetrics);
		bWindowSizeChanged = false;
	}
}

FPlatformRect FAndroidApplication::GetWorkArea( const FPlatformRect& CurrentWindow ) const
{
	return FAndroidWindow::GetScreenRect();
}

IForceFeedbackSystem *FAndroidApplication::GetForceFeedbackSystem()
{
	// NOTE: This does not increase the reference count, so don't cache the result
	return InputInterface.Get();
}

IForceFeedbackSystem *FAndroidApplication::DEPRECATED_GetForceFeedbackSystem()
{
	// NOTE: This does not increase the reference count, so don't cache the result
	return InputInterface.Get();
}

IInputInterface* FAndroidApplication::GetInputInterface()
{
	// NOTE: This does not increase the reference count, so don't cache the result
	return InputInterface.Get();
}

bool FAndroidApplication::IsGamepadAttached() const
{
	FAndroidInputInterface* AndroidInputInterface = (FAndroidInputInterface*)InputInterface.Get();

	if (AndroidInputInterface)
	{
		return AndroidInputInterface->IsGamepadAttached();
	}

	return false;
}

void FDisplayMetrics::GetDisplayMetrics( FDisplayMetrics& OutDisplayMetrics )
{
	// Get screen rect
	OutDisplayMetrics.PrimaryDisplayWorkAreaRect = FAndroidWindow::GetScreenRect();
	OutDisplayMetrics.VirtualDisplayRect = OutDisplayMetrics.PrimaryDisplayWorkAreaRect;

	// Total screen size of the primary monitor
	OutDisplayMetrics.PrimaryDisplayWidth = OutDisplayMetrics.PrimaryDisplayWorkAreaRect.Right - OutDisplayMetrics.PrimaryDisplayWorkAreaRect.Left;
	OutDisplayMetrics.PrimaryDisplayHeight = OutDisplayMetrics.PrimaryDisplayWorkAreaRect.Bottom - OutDisplayMetrics.PrimaryDisplayWorkAreaRect.Top;

	// Apply the debug safe zones
	OutDisplayMetrics.ApplyDefaultSafeZones();
}

TSharedRef< FGenericWindow > FAndroidApplication::MakeWindow()
{
	return FAndroidWindow::Make();
}

void FAndroidApplication::InitializeWindow( const TSharedRef< FGenericWindow >& InWindow, const TSharedRef< FGenericWindowDefinition >& InDefinition, const TSharedPtr< FGenericWindow >& InParent, const bool bShowImmediately )
{
	const TSharedRef< FAndroidWindow > Window = StaticCastSharedRef< FAndroidWindow >( InWindow );
	const TSharedPtr< FAndroidWindow > ParentWindow = StaticCastSharedPtr< FAndroidWindow >( InParent );

	Windows.Add( Window );
	Window->Initialize( this, InDefinition, ParentWindow, bShowImmediately );
}

void FAndroidApplication::OnWindowSizeChanged()
{
	bWindowSizeChanged = true;
}

//////////////////////////////////////////////////////////////////////////
// FJNIHelper
static JavaVM* CurrentJavaVM = nullptr;
static jint CurrentJavaVersion;
static jobject GlobalObjectRef;
static jobject ClassLoader;
static jmethodID FindClassMethod;


// Caches access to the environment, attached to the current thread
class FJNIHelper : public TThreadSingleton<FJNIHelper>
{
public:
	static JNIEnv* GetEnvironment()
	{
		return Get().CachedEnv;
	}

private:
	JNIEnv* CachedEnv = NULL;

private:
	friend class TThreadSingleton<FJNIHelper>;

	FJNIHelper()
		: CachedEnv(nullptr)
	{
		check(CurrentJavaVM);
		CurrentJavaVM->GetEnv((void **)&CachedEnv, CurrentJavaVersion);

		const jint AttachResult = CurrentJavaVM->AttachCurrentThread(&CachedEnv, nullptr);
		if (AttachResult == JNI_ERR)
		{
			FPlatformMisc::LowLevelOutputDebugString(TEXT("FJNIHelper failed to attach thread to Java VM!"));
			check(false);
		}
	}

	~FJNIHelper()
	{
		check(CurrentJavaVM);
		const jint DetachResult = CurrentJavaVM->DetachCurrentThread();
		if (DetachResult == JNI_ERR)
		{
			FPlatformMisc::LowLevelOutputDebugString(TEXT("FJNIHelper failed to detach thread from Java VM!"));
			check(false);
		}

		CachedEnv = nullptr;
	}
};

void FAndroidApplication::InitializeJavaEnv( JavaVM* VM, jint Version, jobject GlobalThis )
{
	if (CurrentJavaVM == nullptr)
	{
		CurrentJavaVM = VM;
		CurrentJavaVersion = Version;

		JNIEnv* Env = GetJavaEnv(false);
		jclass MainClass = Env->FindClass("com/epicgames/ue4/GameActivity");
		jclass classClass = Env->FindClass("java/lang/Class");
		jclass classLoaderClass = Env->FindClass("java/lang/ClassLoader");
		jmethodID getClassLoaderMethod = Env->GetMethodID(classClass, "getClassLoader", "()Ljava/lang/ClassLoader;");
		jobject classLoader = Env->CallObjectMethod(MainClass, getClassLoaderMethod);
		ClassLoader = Env->NewGlobalRef(classLoader);
		FindClassMethod = Env->GetMethodID(classLoaderClass, "findClass", "(Ljava/lang/String;)Ljava/lang/Class;");
	}
	GlobalObjectRef = GlobalThis;
}

jobject FAndroidApplication::GetGameActivityThis()
{
	return GlobalObjectRef;
}

static void JavaEnvDestructor(void*)
{
	FPlatformMisc::LowLevelOutputDebugStringf(TEXT("*** JavaEnvDestructor: %d"), FPlatformTLS::GetCurrentThreadId());
	FAndroidApplication::DetachJavaEnv();
}

JNIEnv* FAndroidApplication::GetJavaEnv( bool bRequireGlobalThis /*= true*/ )
{
	//@TODO: ANDROID: Remove the other version if the helper works well
#if 0
	if (!bRequireGlobalThis || (GlobalObjectRef != nullptr))
	{
		return FJNIHelper::GetEnvironment();
	}
	else
	{
		return nullptr;
	}
#endif
#if 0
	// not reliable at the moment.. revisit later

	// Magic static - *should* be thread safe
	//Android & pthread specific, bind a destructor for thread exit
	static uint32 TlsSlot = 0;
	if (TlsSlot == 0)
	{
		pthread_key_create((pthread_key_t*)&TlsSlot, &JavaEnvDestructor);
	}
	JNIEnv* Env = (JNIEnv*)FPlatformTLS::GetTlsValue(TlsSlot);
	if (Env == nullptr)
	{
		CurrentJavaVM->GetEnv((void **)&Env, CurrentJavaVersion);

		jint AttachResult = CurrentJavaVM->AttachCurrentThread(&Env, NULL);
		if (AttachResult == JNI_ERR)
		{
			FPlatformMisc::LowLevelOutputDebugString(L"UNIT TEST -- Failed to get the JNI environment!");
			check(false);
			return nullptr;
		}
		FPlatformTLS::SetTlsValue(TlsSlot, (void*)Env);
	}

	return (!bRequireGlobalThis || (GlobalObjectRef != nullptr)) ? Env : nullptr;
#else
	// register a destructor to detach this thread
	static uint32 TlsSlot = 0;
	if (TlsSlot == 0)
	{
		pthread_key_create((pthread_key_t*)&TlsSlot, &JavaEnvDestructor);
	}

	JNIEnv* Env = nullptr;
	jint GetEnvResult = CurrentJavaVM->GetEnv((void **)&Env, CurrentJavaVersion);
	if (GetEnvResult == JNI_EDETACHED)
	{
		// attach to this thread
		jint AttachResult = CurrentJavaVM->AttachCurrentThread(&Env, NULL);
		if (AttachResult == JNI_ERR)
		{
			FPlatformMisc::LowLevelOutputDebugString(L"UNIT TEST -- Failed to attach thread to get the JNI environment!");
			check(false);
			return nullptr;
		}
		FPlatformTLS::SetTlsValue(TlsSlot, (void*)Env);
	}
	else if (GetEnvResult != JNI_OK)
	{
		FPlatformMisc::LowLevelOutputDebugStringf(L"UNIT TEST -- Failed to get the JNI environment! Result = %d", GetEnvResult);
		check(false);
		return nullptr;

	}
	return Env;
#endif
}

jclass FAndroidApplication::FindJavaClass( const char* name )
{
	JNIEnv* Env = GetJavaEnv();
	if (!Env)
	{
		return nullptr;
	}
	jstring ClassNameObj = Env->NewStringUTF(name);
	jclass FoundClass = static_cast<jclass>(Env->CallObjectMethod(ClassLoader, FindClassMethod, ClassNameObj));
	CheckJavaException();
	Env->DeleteLocalRef(ClassNameObj);
	return FoundClass;
}

void FAndroidApplication::DetachJavaEnv()
{
	CurrentJavaVM->DetachCurrentThread();
}

bool FAndroidApplication::CheckJavaException()
{
	JNIEnv* Env = GetJavaEnv();
	if (!Env)
	{
		return true;
	}
	if (Env->ExceptionCheck())
	{
		Env->ExceptionDescribe();
		Env->ExceptionClear();
		verify(false && "Java JNI call failed with an exception.");
		return true;
	}
	return false;
}
