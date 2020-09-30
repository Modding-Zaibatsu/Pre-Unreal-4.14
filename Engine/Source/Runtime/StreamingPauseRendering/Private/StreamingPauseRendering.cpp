// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "Engine.h"
#include "StreamingPauseRendering.h"
#include "SceneViewport.h"
#include "MoviePlayer.h"
#include "SThrobber.h"


IMPLEMENT_MODULE(FStreamingPauseRenderingModule, StreamingPauseRendering);

class FBackgroundView : public ISlateViewport
{
public:
	FBackgroundView(FTexture2DRHIRef RenderTargetTexture, FIntPoint InSize)
		: RenderTarget( new FSlateRenderTargetRHI(RenderTargetTexture, InSize.X, InSize.Y))
		, Size(InSize)
	{
		BeginInitResource(RenderTarget);
	}

	~FBackgroundView()
	{
		ReleaseResourceAndFlush(RenderTarget);
		delete RenderTarget;
	}

	/* ISlateViewport interface. */
	virtual FIntPoint GetSize() const override
	{
		return Size;
	}

	virtual FSlateShaderResource* GetViewportRenderTargetTexture() const override
	{
		return RenderTarget;
	}

	virtual bool RequiresVsync() const override
	{
		return false;
	}

private:
	FSlateRenderTargetRHI* RenderTarget;
	FIntPoint Size;
};

static TAutoConsoleVariable<int32> CVarRenderLastFrameInStreamingPause(
	TEXT("r.RenderLastFrameInStreamingPause"),
	1,
	TEXT("If 1 the previous frame is displayed during streaming pause. If zero the screen is left black."),
	ECVF_RenderThreadSafe);


FStreamingPauseRenderingModule::FStreamingPauseRenderingModule()
	: SceneViewport(nullptr)
	, ViewportWidget(nullptr)
	, BackgroundView(nullptr)
{ }


void FStreamingPauseRenderingModule::StartupModule()
{
	BeginDelegate.BindRaw(this, &FStreamingPauseRenderingModule::BeginStreamingPause);
	EndDelegate.BindRaw(this, &FStreamingPauseRenderingModule::EndStreamingPause);
	check(GEngine);
	GEngine->RegisterBeginStreamingPauseRenderingDelegate(&BeginDelegate);
	GEngine->RegisterEndStreamingPauseRenderingDelegate(&EndDelegate);
}


void FStreamingPauseRenderingModule::ShutdownModule()
{
	BeginDelegate.Unbind();
	EndDelegate.Unbind();
	if( GEngine )
	{
		GEngine->RegisterBeginStreamingPauseRenderingDelegate(nullptr);
		GEngine->RegisterEndStreamingPauseRenderingDelegate(nullptr);
	}
}


void FStreamingPauseRenderingModule::BeginStreamingPause( FViewport* GameViewport )
{
	check(GameViewport);

	//Create the viewport widget and add a throbber.
	ViewportWidget = SNew( SViewport )
		.EnableGammaCorrection(false);

	ViewportWidget->SetContent
	(
		SNew(SVerticalBox)
		+SVerticalBox::Slot()
		.VAlign(VAlign_Bottom)
		.HAlign(HAlign_Right)
		.Padding(FMargin(10.0f))
		[
			SNew(SThrobber)
		]
	);

	//Render the current scene to a new viewport.
 	FIntPoint SizeXY = GameViewport->GetSizeXY();
 	bool bRenderLastFrame = CVarRenderLastFrameInStreamingPause.GetValueOnGameThread() != 0;
 	bRenderLastFrame = bRenderLastFrame && !( GEngine->StereoRenderingDevice.IsValid() && GameViewport->IsStereoRenderingAllowed() );
 
 	if( SizeXY.X > 0 && SizeXY.Y > 0 && bRenderLastFrame )
	{
		FViewportClient* ViewportClient = GameViewport->GetClient();

		SceneViewport = MakeShareable(new FSceneViewport(ViewportClient, ViewportWidget));

		SceneViewport->UpdateViewportRHI(false,SizeXY.X,SizeXY.Y, EWindowMode::Fullscreen);

		SceneViewport->EnqueueBeginRenderFrame();

		FCanvas Canvas(SceneViewport.Get(), nullptr, ViewportClient->GetWorld(), ViewportClient->GetWorld()->FeatureLevel);
		{
			ViewportClient->Draw(SceneViewport.Get(), &Canvas);
		}
		Canvas.Flush_GameThread();


		ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER(
			EndDrawingCommand,
			FViewport*,Viewport, SceneViewport.Get(),
		{
			Viewport->EndRenderFrame(RHICmdList, false, false);
		});

		BackgroundView = MakeShareable(new FBackgroundView(SceneViewport->GetRenderTargetTexture(), SizeXY));
		ViewportWidget->SetViewportInterface(BackgroundView.ToSharedRef());

	}

	//Create the loading screen and begin rendering the widget.
	FLoadingScreenAttributes LoadingScreen;
	LoadingScreen.bAutoCompleteWhenLoadingCompletes = true; 
	LoadingScreen.WidgetLoadingScreen = ViewportWidget; // SViewport from above
	GetMoviePlayer()->SetupLoadingScreen(LoadingScreen);			
	GetMoviePlayer()->PlayMovie();
}


void FStreamingPauseRenderingModule::EndStreamingPause()
{
	//Stop rendering the loading screen and resume
	GetMoviePlayer()->WaitForMovieToFinish();

	ViewportWidget = nullptr;
	SceneViewport = nullptr;
	BackgroundView = nullptr;
	
	FlushRenderingCommands();
}