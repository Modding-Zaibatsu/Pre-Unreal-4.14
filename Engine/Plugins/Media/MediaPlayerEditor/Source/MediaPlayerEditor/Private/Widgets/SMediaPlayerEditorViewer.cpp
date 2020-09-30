// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "MediaPlayerEditorPCH.h"
#include "MediaPlayerEditorSettings.h"
#include "SMediaPlayerEditorOutput.h"
#include "SMediaPlayerEditorOverlay.h"
#include "SMediaPlayerEditorViewer.h"


#define LOCTEXT_NAMESPACE "SMediaPlayerEditorViewer"


/* SMediaPlayerEditorPlayer structors
 *****************************************************************************/

SMediaPlayerEditorViewer::SMediaPlayerEditorViewer()
	: DragOver(false)
	, DragValid(false)
	, MediaPlayer(nullptr)
{ }


SMediaPlayerEditorViewer::~SMediaPlayerEditorViewer()
{
	if (MediaPlayer != nullptr)
	{
		MediaPlayer->OnMediaEvent().RemoveAll(this);
	}
}


/* SMediaPlayerEditorPlayer interface
 *****************************************************************************/

void SMediaPlayerEditorViewer::Construct(const FArguments& InArgs, UMediaPlayer& InMediaPlayer, const TSharedRef<ISlateStyle>& InStyle)
{
	MediaPlayer = &InMediaPlayer;
	Style = InStyle;

	// initialize media player asset
	MediaPlayer->OnMediaEvent().AddSP(this, &SMediaPlayerEditorViewer::HandleMediaPlayerMediaEvent);
	MediaPlayer->DesiredPlayerName = NAME_None;

	FName DesiredPlayerName = GetDefault<UMediaPlayerEditorSettings>()->DesiredPlayerName;

	if (DesiredPlayerName != NAME_None)
	{
		IMediaModule* MediaModule = FModuleManager::LoadModulePtr<IMediaModule>("Media");

		if ((MediaModule != nullptr) && (MediaModule->GetPlayerFactory(DesiredPlayerName) != nullptr))
		{
			MediaPlayer->DesiredPlayerName = DesiredPlayerName;
		}
	}

	// initialize settings menu
	FMenuBuilder SettingsMenuBuilder(true, nullptr);
	{
		SettingsMenuBuilder.BeginSection("PlayerSection", LOCTEXT("PlayerSection", "Player"));
		{
			SettingsMenuBuilder.AddSubMenu(
				LOCTEXT("DecoderMenuLabel", "Decoder"),
				LOCTEXT("DecoderMenuTooltip", "Select the desired media decoder"),
				FNewMenuDelegate::CreateRaw(this, &SMediaPlayerEditorViewer::HandlePlayerMenuNewMenu),
				false,
				FSlateIcon()
			);
		}
		SettingsMenuBuilder.EndSection();

		SettingsMenuBuilder.BeginSection("TracksSection", LOCTEXT("TracksSection", "Tracks"));
		{
			SettingsMenuBuilder.AddSubMenu(
				LOCTEXT("AudioTrackMenuLabel", "Audio"),
				LOCTEXT("AudioTrackMenuTooltip", "Select the active audio track"),
				FNewMenuDelegate::CreateRaw(this, &SMediaPlayerEditorViewer::HandleTrackMenuNewMenu, EMediaPlayerTrack::Audio),
				false,
				FSlateIcon()
			);

			SettingsMenuBuilder.AddSubMenu(
				LOCTEXT("CaptionTrackMenuLabel", "Captions"),
				LOCTEXT("CaptionTrackMenuTooltip", "Select the active closed caption track"),
				FNewMenuDelegate::CreateRaw(this, &SMediaPlayerEditorViewer::HandleTrackMenuNewMenu, EMediaPlayerTrack::Caption),
				false,
				FSlateIcon()
			);

			SettingsMenuBuilder.AddSubMenu(
				LOCTEXT("SubtitleTrackMenuLabel", "Subtitles"),
				LOCTEXT("SubtitleTrackMenuTooltip", "Select the active subtitle track"),
				FNewMenuDelegate::CreateRaw(this, &SMediaPlayerEditorViewer::HandleTrackMenuNewMenu, EMediaPlayerTrack::Subtitle),
				false,
				FSlateIcon()
			);

			SettingsMenuBuilder.AddSubMenu(
				LOCTEXT("TextTrackMenuLabel", "Text"),
				LOCTEXT("TextTrackMenuTooltip", "Select the active generic text track"),
				FNewMenuDelegate::CreateRaw(this, &SMediaPlayerEditorViewer::HandleTrackMenuNewMenu, EMediaPlayerTrack::Text),
				false,
				FSlateIcon()
			);

			SettingsMenuBuilder.AddSubMenu(
				LOCTEXT("VideoTrackMenuLabel", "Video"),
				LOCTEXT("VideoTrackMenuTooltip", "Select the active video track"),
				FNewMenuDelegate::CreateRaw(this, &SMediaPlayerEditorViewer::HandleTrackMenuNewMenu, EMediaPlayerTrack::Video),
				false,
				FSlateIcon()
			);
		}
		SettingsMenuBuilder.EndSection();

		SettingsMenuBuilder.BeginSection("ViewSection", LOCTEXT("ViewSection", "View"));
		{
			SettingsMenuBuilder.AddSubMenu(
				LOCTEXT("ScaleMenuLabel", "Scale"),
				LOCTEXT("ScaleMenuTooltip", "Select the video viewport's scaling mode"),
				FNewMenuDelegate::CreateRaw(this, &SMediaPlayerEditorViewer::HandleScaleMenuNewMenu),
				false,
				FSlateIcon()
			);

			SettingsMenuBuilder.AddMenuEntry(
				LOCTEXT("ShowTextOverlaysMenuLabel", "Show Text Overlays"),
				LOCTEXT("ShowTextOverlaysMenuTooltip", "Show caption and subtitle text overlays"),
				FSlateIcon(),
				FUIAction(
					FExecuteAction::CreateLambda([]{
						auto Settings = GetMutableDefault<UMediaPlayerEditorSettings>();
						Settings->ShowTextOverlays = !Settings->ShowTextOverlays;
						Settings->SaveConfig();
					}),
					FCanExecuteAction(),
					FIsActionChecked::CreateLambda([]{ return GetDefault<UMediaPlayerEditorSettings>()->ShowTextOverlays; })
				),
				NAME_None,
				EUserInterfaceActionType::ToggleButton
			);
		}
		SettingsMenuBuilder.EndSection();
	}

	ChildSlot
	[
		SNew(SOverlay)

		+ SOverlay::Slot()
			[
				SNew(SVerticalBox)

				+ SVerticalBox::Slot()
					.AutoHeight()
					[
						SNew(SHorizontalBox)

						+ SHorizontalBox::Slot()
							.FillWidth(1.0f)
							[
								// url box
								SAssignNew(UrlTextBox, SEditableTextBox)
									.ClearKeyboardFocusOnCommit(true)
									.HintText(LOCTEXT("UrlTextBoxHint", "Media URL"))
									.ToolTipText(LOCTEXT("UrlTextBoxToolTip", "Enter the URL of a media source"))
									.OnKeyDownHandler(this, &SMediaPlayerEditorViewer::HandleUrlBoxKeyDown)
									.OnTextCommitted_Lambda([this](const FText& InText, ETextCommit::Type InCommitType){
										if (InCommitType == ETextCommit::OnEnter)
										{
											OpenUrlTextBoxUrl();
										}
									})
							]

						+ SHorizontalBox::Slot()
							.AutoWidth()
							.Padding(4.0f, 0.0f, 0.0f, 0.0f)
							[
								// go button
								SNew(SButton)
									.ToolTipText_Lambda([this]() {
										return ((UrlTextBox->GetText().ToString() == MediaPlayer->GetUrl()) && !MediaPlayer->GetUrl().IsEmpty())
											? LOCTEXT("ReloadButtonToolTip", "Reload the current media URL")
											: LOCTEXT("GoButtonToolTip", "Open the specified media URL");
									})
									.IsEnabled_Lambda([this]{
										return !UrlTextBox->GetText().IsEmpty();
									})
									.OnClicked_Lambda([this]{
										OpenUrlTextBoxUrl();
										return FReply::Handled();
									})
									[
										SNew(SImage)
											.Image_Lambda([this]() {
												return ((UrlTextBox->GetText().ToString() == MediaPlayer->GetUrl()) && !MediaPlayer->GetUrl().IsEmpty())
													? Style->GetBrush("MediaPlayerEditor.ReloadButton")
													: Style->GetBrush("MediaPlayerEditor.GoButton");
											})
									]
							]
					]

				+ SVerticalBox::Slot()
					.FillHeight(1.0f)
					.Padding(0.0f, 4.0f, 0.0f, 0.0f)
					[
						SNew(SBorder)
							.BorderImage(FCoreStyle::Get().GetBrush("BlackBrush"))
							.Padding(0.0f)
							.OnMouseButtonUp(this, &SMediaPlayerEditorViewer::HandleTextureMouseButtonUp)
							[
								SNew(SOverlay)

								+ SOverlay::Slot()
									[
										SNew(SScaleBox)
											.Stretch_Lambda([]() -> EStretch::Type {
												auto Settings = GetDefault<UMediaPlayerEditorSettings>();
												if (Settings->ViewportScale == EMediaPlayerEditorScale::Fill)
												{
													return EStretch::Fill;
												}
												if (Settings->ViewportScale == EMediaPlayerEditorScale::Fit)
												{
													return EStretch::ScaleToFit;
												}
												return EStretch::None;
											})
											[
												SNew(SOverlay)

												+ SOverlay::Slot()
													[
														// movie texture
														SNew(SMediaPlayerEditorOutput, InMediaPlayer)
													]

												+ SOverlay::Slot()
													[
														// text overlays
														SNew(SMediaPlayerEditorOverlay, InMediaPlayer)
															.Visibility_Lambda([]{ return GetDefault<UMediaPlayerEditorSettings>()->ShowTextOverlays ? EVisibility::Visible : EVisibility::Collapsed; })
													]
											]
									]

								+ SOverlay::Slot()
									.Padding(FMargin(12.0f, 8.0f))
									[
										SNew(SHorizontalBox)

										+ SHorizontalBox::Slot()
											.HAlign(HAlign_Left)
											.VAlign(VAlign_Top)
											[
												// playback state
												SNew(STextBlock)
													.ColorAndOpacity(FSlateColor::UseSubduedForeground())
													.Font(FSlateFontInfo(FPaths::EngineContentDir() / TEXT("Slate/Fonts/Roboto-Regular.ttf"), 18))
													.ShadowOffset(FVector2D(1.f, 1.f))
													.Text(this, &SMediaPlayerEditorViewer::HandleOverlayStateText)
											]

										+ SHorizontalBox::Slot()
											.HAlign(HAlign_Right)
											.VAlign(VAlign_Top)
											[
												// player name
												SNew(STextBlock)
													.ColorAndOpacity(FSlateColor::UseSubduedForeground())
													.Font(FSlateFontInfo(FPaths::EngineContentDir() / TEXT("Slate/Fonts/Roboto-Regular.ttf"), 18))
													.ShadowOffset(FVector2D(1.f, 1.f))
													.Text(this, &SMediaPlayerEditorViewer::HandleOverlayPlayerNameText)
											]
									]
							]
					]

				+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(0.0f, 2.0f, 0.0f, 0.0f)
					.VAlign(VAlign_Center)
					[
						SNew(SBorder)
							.BorderImage(FEditorStyle::GetBrush("ToolPanel.GroupBorder"))
							.ForegroundColor(FLinearColor::Gray)
							.Padding(6.0f)
							[
								SNew(SVerticalBox)

								+ SVerticalBox::Slot()
									.AutoHeight()
									[
										SNew(SOverlay)

										+ SOverlay::Slot()
											[
												// scrubber
												SAssignNew(ScrubberSlider, SSlider)
													.IsEnabled(this, &SMediaPlayerEditorViewer::HandlePositionSliderIsEnabled)
													.OnMouseCaptureBegin(this, &SMediaPlayerEditorViewer::HandlePositionSliderMouseCaptureBegin)
													.OnMouseCaptureEnd(this, &SMediaPlayerEditorViewer::HandlePositionSliderMouseCaptureEnd)
													.OnValueChanged(this, &SMediaPlayerEditorViewer::HandlePositionSliderValueChanged)
													.Orientation(Orient_Horizontal)
													.Value(this, &SMediaPlayerEditorViewer::HandlePositionSliderValue)
											]

										+ SOverlay::Slot()
											[
												SNew(SProgressBar)
													.ToolTipText(LOCTEXT("BufferingTooltip", "Buffering..."))
													.Visibility_Lambda([this]() -> EVisibility { return MediaPlayer->IsPreparing() ? EVisibility::Visible : EVisibility::Hidden; })
											]
									]

								+ SVerticalBox::Slot()
									.AutoHeight()
									.Padding(0.0f, 2.0f, 0.0f, 0.0f)
									[
										SNew(SHorizontalBox)

										// timer
										+ SHorizontalBox::Slot()
											.AutoWidth()
											.Padding(0.0f, 0.0f, 4.0f, 0.0f)
											.VAlign(VAlign_Center)
											[
												SNew(STextBlock)
													.Text(this, &SMediaPlayerEditorViewer::HandleTimerTextBlockText)
													.ToolTipText(this, &SMediaPlayerEditorViewer::HandleTimerTextBlockToolTipText)
											]

										// transport controls
										+ SHorizontalBox::Slot()
											.FillWidth(1.0f)
											.Padding(8.0f, 0.0f)
											.VAlign(VAlign_Center)
											[
												SNew(SSpacer)
												//SNew(STransportControl)
											]

										// settings
										+ SHorizontalBox::Slot()
											.AutoWidth()
											.Padding(4.0f, 0.0f, 0.0f, 0.0f)
											.VAlign(VAlign_Center)
											[
												SNew(SComboButton)
													.ContentPadding(0.0f)
													.ButtonContent()
													[
														SNew(SHorizontalBox)

														+ SHorizontalBox::Slot()
															.AutoWidth()
															.VAlign(VAlign_Center)
															[
																SNew(SImage)
																	.Image(InStyle->GetBrush("MediaPlayerEditor.SettingsButton"))
															]

														+ SHorizontalBox::Slot()
															.AutoWidth()
															.Padding(3.0f, 0.0f, 0.0f, 0.0f)
															.VAlign(VAlign_Center)
															[
																SNew(STextBlock)
																	.Text(LOCTEXT("OptionsButton", "Playback Options"))
															]
													]
													.ButtonStyle(FEditorStyle::Get(), "ToggleButton")
													.ForegroundColor(FSlateColor::UseForeground())
													.MenuContent()
													[
														SettingsMenuBuilder.MakeWidget()
													]
											]
									]
							]
					]
			]

		+ SOverlay::Slot()
			[
				SNew(SBorder)
					.BorderImage(FEditorStyle::GetBrush("WhiteBrush"))
					.BorderBackgroundColor_Lambda([this]() -> FLinearColor {
						return DragValid
							? FLinearColor(0.0f, 1.0f, 0.0f, 0.15f)
							: FLinearColor(1.0f, 0.0f, 0.0f, 0.15f);
					})
					.Visibility_Lambda([this]() -> EVisibility {
						return DragOver && FSlateApplication::Get().IsDragDropping()
							? EVisibility::HitTestInvisible
							: EVisibility::Hidden;
					})
			]
	];

	RegisterActiveTimer(0.0f, FWidgetActiveTimerDelegate::CreateSP(this, &SMediaPlayerEditorViewer::HandleActiveTimer));
}


/* SWidget interface
 *****************************************************************************/

void SMediaPlayerEditorViewer::OnDragEnter(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent)
{
	DragOver = true;

	TSharedPtr<FExternalDragOperation> DragDropOp = DragDropEvent.GetOperationAs<FExternalDragOperation>();
	
	if (DragDropOp.IsValid() && DragDropOp->HasFiles())
	{
		const TArray<FString>& Files = DragDropOp->GetFiles();
		
		if (Files.Num() == 1)
		{
			const FString FilePath = FPaths::ConvertRelativePathToFull(Files[0]);
			DragValid = MediaPlayer->CanPlayUrl(FString(TEXT("file://")) + FilePath);

			return;
		}
	}

	DragValid = false;
}


void SMediaPlayerEditorViewer::OnDragLeave(const FDragDropEvent& DragDropEvent)
{
	DragOver = false;
}


FReply SMediaPlayerEditorViewer::OnDragOver(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent)
{
	if (DragValid)
	{
		return FReply::Handled();
	}

	return SCompoundWidget::OnDragOver(MyGeometry,DragDropEvent);
}


FReply SMediaPlayerEditorViewer::OnDrop(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent)
{
	if (DragValid)
	{
		TSharedPtr<FExternalDragOperation> DragDropOp = DragDropEvent.GetOperationAs<FExternalDragOperation>();

		if (DragDropOp.IsValid() && DragDropOp->HasFiles())
		{
			const TArray<FString>& Files = DragDropOp->GetFiles();

			if (Files.Num() == 1)
			{
				const FString FilePath = FPaths::ConvertRelativePathToFull(Files[0]);
				MediaPlayer->OpenUrl(FString(TEXT("file://")) + FilePath);

				return FReply::Handled();
			}
		}
	}

	return SCompoundWidget::OnDrop(MyGeometry,DragDropEvent);
}


/* SMediaPlayerEditorPlayer implementation
 *****************************************************************************/

void SMediaPlayerEditorViewer::OpenUrlTextBoxUrl()
{
	FString Url = UrlTextBox->GetText().ToString();
	{
		Url.Trim();
		Url.TrimTrailing();
	}

	if (!Url.Contains(TEXT("://"), ESearchCase::CaseSensitive))
	{
		Url.InsertAt(0, TEXT("file://"));
	}

	MediaPlayer->OpenUrl(Url);
}


void SMediaPlayerEditorViewer::SetDesiredPlayerName(FName PlayerName)
{
	if (PlayerName != MediaPlayer->DesiredPlayerName)
	{
		MediaPlayer->DesiredPlayerName = PlayerName;

		if (PlayerName != NAME_None)
		{
			MediaPlayer->Reopen();
		}
	}

	auto Settings = GetMutableDefault<UMediaPlayerEditorSettings>();
	{
		Settings->DesiredPlayerName = PlayerName;
		Settings->SaveConfig();
	}
}


/* SMediaPlayerEditorPlayer callbacks
 *****************************************************************************/

EActiveTimerReturnType SMediaPlayerEditorViewer::HandleActiveTimer(double InCurrentTime, float InDeltaTime)
{
	return EActiveTimerReturnType::Continue;
}


void SMediaPlayerEditorViewer::HandleMediaPlayerMediaEvent(EMediaEvent Event)
{
	switch (Event)
	{
	case EMediaEvent::MediaClosed:
	case EMediaEvent::MediaOpened:
	case EMediaEvent::MediaOpenFailed:
		UrlTextBox->SetText(FText::FromString(MediaPlayer->GetUrl()));
		break;
	}

	if (Event == EMediaEvent::MediaOpenFailed)
	{
		UrlTextBox->SetBorderBackgroundColor(FLinearColor::Red);

		FNotificationInfo NotificationInfo(LOCTEXT("MediaOpenFailedError", "The media failed to open. Check Output Log for details!"));
		{
			NotificationInfo.ExpireDuration = 2.0f;
		}

		FSlateNotificationManager::Get().AddNotification(NotificationInfo)->SetCompletionState(SNotificationItem::CS_Fail);
	}
	else if (Event == EMediaEvent::MediaClosed)
	{
		UrlTextBox->SetBorderBackgroundColor(FLinearColor::White);
	}
}


FText SMediaPlayerEditorViewer::HandleOverlayPlayerNameText() const
{
	if (MediaPlayer->IsPlaying())
	{
		return FText::GetEmpty();
	}

	FName PlayerName = MediaPlayer->GetPlayerName();

	if (PlayerName == NAME_None)
	{
		if (MediaPlayer->DesiredPlayerName == NAME_None)
		{
			return LOCTEXT("AutoPlayerName", "Auto");
		}

		return FText::FromName(MediaPlayer->DesiredPlayerName);
	}

	return FText::FromName(PlayerName);
}


FText SMediaPlayerEditorViewer::HandleOverlayStateText() const
{
	if (MediaPlayer->GetUrl().IsEmpty())
	{
		return LOCTEXT("StateOverlayNoMedia", "No Media");
	}

	if (!MediaPlayer->IsReady())
	{
		return LOCTEXT("StateOverlayStopped", "Not Ready");
	}

	if (MediaPlayer->IsPaused())
	{
		return LOCTEXT("StateOverlayPaused", "Paused");
	}

	if (!MediaPlayer->IsPlaying())
	{
		return FText::FromString(FPaths::GetBaseFilename(MediaPlayer->GetUrl()));
	}

	float Rate = MediaPlayer->GetRate();

	if (FMath::IsNearlyZero(Rate) || (Rate == 1.0f))
	{
		return FText::GetEmpty();
	}

	if (Rate < 0.0f)
	{
		return FText::Format(LOCTEXT("StateOverlayReverseFormat", "Reverse ({0}x)"), FText::AsNumber(-Rate));
	}

	return FText::Format(LOCTEXT("StateOverlayForwardFormat", "Forward ({0}x)"), FText::AsNumber(Rate));
}


TOptional<float> SMediaPlayerEditorViewer::HandlePlaybackRateBoxMaxValue() const
{
	FFloatRange Rates = MediaPlayer->GetForwardRates(false);

	return Rates.IsEmpty() ? 0.0f : Rates.GetUpperBoundValue();
}


TOptional<float> SMediaPlayerEditorViewer::HandlePlaybackRateBoxMinValue() const
{
	FFloatRange Rates = MediaPlayer->GetReverseRates(false);

	return Rates.IsEmpty() ? 0.0f : Rates.GetLowerBoundValue();
}


TOptional<float> SMediaPlayerEditorViewer::HandlePlaybackRateSpinBoxValue() const
{
	return MediaPlayer->GetRate();
}


void SMediaPlayerEditorViewer::HandlePlaybackRateBoxValueChanged( float NewValue )
{
	MediaPlayer->SetRate(NewValue);
}


void SMediaPlayerEditorViewer::HandlePlayerMenuNewMenu(FMenuBuilder& MenuBuilder)
{
	// automatic player option
	MenuBuilder.AddMenuEntry(
		LOCTEXT("AutoPlayer", "Automatic"),
		LOCTEXT("AutoPlayerTooltip", "Select a player automatically based on the media source"),
		FSlateIcon(),
		FUIAction(
			FExecuteAction::CreateLambda([this] { SetDesiredPlayerName(NAME_None); }),
			FCanExecuteAction(),
			FIsActionChecked::CreateLambda([this] { return MediaPlayer->DesiredPlayerName == NAME_None; })
		),
		NAME_None,
		EUserInterfaceActionType::RadioButton
	);

	MenuBuilder.AddMenuSeparator();

	// get registered player plug-ins
	auto MediaModule = FModuleManager::LoadModulePtr<IMediaModule>("Media");

	if (MediaModule == nullptr)
	{
		return;
	}

	const TArray<IMediaPlayerFactory*>& PlayerFactories = MediaModule->GetPlayerFactories();

	if (PlayerFactories.Num() == 0)
	{
		TSharedRef<SWidget> NoPlayersAvailableWidget = SNew(STextBlock)
			.ColorAndOpacity(FSlateColor::UseSubduedForeground())
			.Text(LOCTEXT("NoPlayersAvailableLabel", "No players available"));

		MenuBuilder.AddWidget(NoPlayersAvailableWidget, FText::GetEmpty(), true, false);

		return;
	}

	// add option for each player
	const FString PlatformName(FPlatformProperties::IniPlatformName());

	for (IMediaPlayerFactory* Factory : PlayerFactories)
	{
		const bool SupportsRunningPlatform = Factory->GetSupportedPlatforms().Contains(PlatformName);
		const FName PlayerName = Factory->GetName();

		MenuBuilder.AddMenuEntry(
			FText::Format(LOCTEXT("PlayerNameFormat", "{0} ({1})"), Factory->GetDisplayName(), FText::FromName(PlayerName)),
			FText::FromString(FString::Join(Factory->GetSupportedPlatforms(), TEXT(", "))),
			FSlateIcon(),
			FUIAction(
				FExecuteAction::CreateLambda([=]{ SetDesiredPlayerName(PlayerName); }),
				FCanExecuteAction::CreateLambda([=]{ return SupportsRunningPlatform; }),
				FIsActionChecked::CreateLambda([=]{ return MediaPlayer->DesiredPlayerName == PlayerName; })
			),
			NAME_None,
			EUserInterfaceActionType::RadioButton
		);
	}
}


bool SMediaPlayerEditorViewer::HandlePositionSliderIsEnabled() const
{
	return MediaPlayer->SupportsSeeking();
}


void SMediaPlayerEditorViewer::HandlePositionSliderMouseCaptureBegin()
{
	if (MediaPlayer->SupportsScrubbing())
	{
		PreScrubRate = MediaPlayer->GetRate();
		MediaPlayer->SetRate(0.0f);
	}
}


void SMediaPlayerEditorViewer::HandlePositionSliderMouseCaptureEnd()
{
	if (MediaPlayer->SupportsScrubbing())
	{
		MediaPlayer->SetRate(PreScrubRate);
	}
}


float SMediaPlayerEditorViewer::HandlePositionSliderValue() const
{
	if (MediaPlayer->GetDuration() > FTimespan::Zero())
	{
		return (float)MediaPlayer->GetTime().GetTicks() / (float)MediaPlayer->GetDuration().GetTicks();
	}

	return 0.0;
}


void SMediaPlayerEditorViewer::HandlePositionSliderValueChanged( float NewValue )
{
	if (!ScrubberSlider->HasMouseCapture() || MediaPlayer->SupportsScrubbing())
	{
		MediaPlayer->Seek(MediaPlayer->GetDuration() * NewValue);
	}
}


void SMediaPlayerEditorViewer::HandleScaleMenuNewMenu(FMenuBuilder& MenuBuilder)
{
	MenuBuilder.AddMenuEntry(
		LOCTEXT("ScaleFitMenuLabel", "Fit"),
		LOCTEXT("ScaleFitMenuTooltip", "Scale the video to fit the viewport, but maintain the aspect ratio"),
		FSlateIcon(),
		FUIAction(
			FExecuteAction::CreateLambda([]{
				auto Settings = GetMutableDefault<UMediaPlayerEditorSettings>();
				Settings->ViewportScale = EMediaPlayerEditorScale::Fit;
				Settings->SaveConfig();
			}),
			FCanExecuteAction(),
			FIsActionChecked::CreateLambda([]{ return (GetDefault<UMediaPlayerEditorSettings>()->ViewportScale == EMediaPlayerEditorScale::Fit); })
		),
		NAME_None,
		EUserInterfaceActionType::RadioButton
	);

	MenuBuilder.AddMenuEntry(
		LOCTEXT("ScaleFillMenuLabel", "Fill"),
		LOCTEXT("ScaleFillMenuTooltip", "Scale the video non-uniformly to fill the entire viewport"),
		FSlateIcon(),
		FUIAction(
			FExecuteAction::CreateLambda([]{
				auto Settings = GetMutableDefault<UMediaPlayerEditorSettings>();
				Settings->ViewportScale = EMediaPlayerEditorScale::Fill;
				Settings->SaveConfig();
			}),
			FCanExecuteAction(),
			FIsActionChecked::CreateLambda([]{ return (GetDefault<UMediaPlayerEditorSettings>()->ViewportScale == EMediaPlayerEditorScale::Fill); })
		),
		NAME_None,
		EUserInterfaceActionType::RadioButton
	);

	MenuBuilder.AddMenuEntry(
		LOCTEXT("ScaleOriginalMenuLabel", "Original Size"),
		LOCTEXT("ScaleOriginalMenuTooltip", "Do not scale or stretch the video"),
		FSlateIcon(),
		FUIAction(
			FExecuteAction::CreateLambda([]{
				auto Settings = GetMutableDefault<UMediaPlayerEditorSettings>();
				Settings->ViewportScale = EMediaPlayerEditorScale::Original;
				Settings->SaveConfig();
			}),
			FCanExecuteAction(),
			FIsActionChecked::CreateLambda([]{ return (GetDefault<UMediaPlayerEditorSettings>()->ViewportScale == EMediaPlayerEditorScale::Original); })
		),
		NAME_None,
		EUserInterfaceActionType::RadioButton
	);
}


FReply SMediaPlayerEditorViewer::HandleTextureMouseButtonUp(const FGeometry& /*Geometry*/, const FPointerEvent& MouseEvent)
{
	if (MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		if (MediaPlayer->IsPlaying())
		{
			MediaPlayer->Pause();
		}
		else
		{
			MediaPlayer->Play();
		}

		return FReply::Handled();
	}

	return FReply::Unhandled();
}


FText SMediaPlayerEditorViewer::HandleTimerTextBlockText() const
{
	if (!MediaPlayer->IsReady())
	{
		return FText::GetEmpty();
	}

	FTimespan Time = MediaPlayer->GetTime();

	if (Time < FTimespan::Zero())
	{
		return FText::GetEmpty();
	}

	FTimespan Duration = MediaPlayer->GetDuration();

	if (Duration <= FTimespan::Zero())
	{
		return FText::AsTimespan(Time);
	}

	return FText::Format(LOCTEXT("TimerTextBlockFormat", "{0} / {1}"), FText::AsTimespan(Time), FText::AsTimespan(Duration));
}


FText SMediaPlayerEditorViewer::HandleTimerTextBlockToolTipText() const
{
	if (!MediaPlayer->IsReady())
	{
		return FText::GetEmpty();
	}

	FTimespan Remaining = MediaPlayer->GetDuration() - MediaPlayer->GetTime();

	if (Remaining <= FTimespan::Zero())
	{
		return LOCTEXT("UnknownTimeRemainingTooltip", "Unknown time remaining");
	}

	return FText::Format(LOCTEXT("TimeRemainingTooltipFormat", "{0} remaining"), FText::AsTimespan(Remaining));
}


void SMediaPlayerEditorViewer::HandleTrackMenuNewMenu(FMenuBuilder& MenuBuilder, EMediaPlayerTrack TrackType)
{
	const int32 NumTracks = MediaPlayer->GetNumTracks(TrackType);

	if (NumTracks > 0)
	{
		MenuBuilder.AddMenuEntry(
			LOCTEXT("DisabledTrackMenuName", "Disabled"),
			FText::GetEmpty(),
			FSlateIcon(),
			FUIAction(
				FExecuteAction::CreateLambda([=] { MediaPlayer->SelectTrack(TrackType, INDEX_NONE); }),
				FCanExecuteAction(),
				FIsActionChecked::CreateLambda([=] { return (MediaPlayer->GetSelectedTrack(TrackType) == INDEX_NONE); })
			),
			NAME_None,
			EUserInterfaceActionType::RadioButton
		);

		FInternationalization& I18n = FInternationalization::Get();

		for (int32 TrackIndex = 0; TrackIndex < NumTracks; ++TrackIndex)
		{
			const FText DisplayName = MediaPlayer->GetTrackDisplayName(TrackType, TrackIndex);
			const FString Language = MediaPlayer->GetTrackLanguage(TrackType, TrackIndex);
			const FCulturePtr Culture = I18n.GetCulture(Language);
			const FString LanguageDisplayName = Culture.IsValid() ? Culture->GetDisplayName() : FString();
			const FString LanguageNativeName = Culture.IsValid() ? Culture->GetNativeName() : FString();

			MenuBuilder.AddMenuEntry(
				LanguageNativeName.IsEmpty()
					? DisplayName
					: FText::Format(LOCTEXT("TrackNameFormat", "{0} ({1})"), DisplayName, FText::FromString(LanguageNativeName)),
				FText::FromString(LanguageDisplayName),
				FSlateIcon(),
				FUIAction(
					FExecuteAction::CreateLambda([=]{ MediaPlayer->SelectTrack(TrackType, TrackIndex); }),
					FCanExecuteAction(),
					FIsActionChecked::CreateLambda([=]{ return (MediaPlayer->GetSelectedTrack(TrackType) == TrackIndex); })
				),
				NAME_None,
				EUserInterfaceActionType::RadioButton
			);
		}

		return;
	}

	TSharedRef<SWidget> NoTracksAvailableWidget = SNew(SBox)
		.Padding(FMargin(2.0f, 2.0f, 2.0f, 0.0f))
		[
			SNew(STextBlock)
				.ColorAndOpacity(FSlateColor::UseSubduedForeground())
				.Text(LOCTEXT("NoTracksAvailableLabel", "No tracks available"))
		];

	MenuBuilder.AddWidget(NoTracksAvailableWidget, FText::GetEmpty(), true, false);
}


FReply SMediaPlayerEditorViewer::HandleUrlBoxKeyDown(const FGeometry&, const FKeyEvent& KeyEvent)
{
	if (KeyEvent.GetKey() == EKeys::Escape)
	{
		UrlTextBox->SetText(FText::FromString(MediaPlayer->GetUrl()));

		return FReply::Handled().ClearUserFocus(true);
	}

	return FReply::Unhandled();
}


#undef LOCTEXT_NAMESPACE
