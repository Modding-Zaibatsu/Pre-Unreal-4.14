// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "MovieSceneToolsPrivatePCH.h"
#include "SFloatCurveKeyEditor.h"

#define LOCTEXT_NAMESPACE "FloatCurveKeyEditor"


template<typename T>
struct SNonThrottledSpinBox : SSpinBox<T>
{
	virtual FReply OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override
	{
		FReply Reply = SSpinBox<T>::OnMouseButtonDown(MyGeometry, MouseEvent);
		if (Reply.IsEventHandled())
		{
			Reply.PreventThrottling();
		}
		return Reply;
	}
};

void SFloatCurveKeyEditor::Construct(const FArguments& InArgs)
{
	Sequencer = InArgs._Sequencer;
	OwningSection = InArgs._OwningSection;
	Curve = InArgs._Curve;
	ExternalValue = InArgs._ExternalValue;

	ChildSlot
	[
		SNew(SNonThrottledSpinBox<float>)
		.Style(&FEditorStyle::GetWidgetStyle<FSpinBoxStyle>("Sequencer.HyperlinkSpinBox"))
		.Font(FEditorStyle::GetFontStyle("Sequencer.AnimationOutliner.RegularFont"))
		.MinValue(TOptional<float>())
		.MaxValue(TOptional<float>())
		.MaxSliderValue(TOptional<float>())
		.MinSliderValue(TOptional<float>())
		.Delta(0.001f)
		.Value(this, &SFloatCurveKeyEditor::OnGetKeyValue)
		.OnValueChanged(this, &SFloatCurveKeyEditor::OnValueChanged)
		.OnValueCommitted(this, &SFloatCurveKeyEditor::OnValueCommitted)
		.OnBeginSliderMovement(this, &SFloatCurveKeyEditor::OnBeginSliderMovement)
		.OnEndSliderMovement(this, &SFloatCurveKeyEditor::OnEndSliderMovement)
		.ClearKeyboardFocusOnCommit(true)
	];
}

void SFloatCurveKeyEditor::OnBeginSliderMovement()
{
	GEditor->BeginTransaction(LOCTEXT("SetFloatKey", "Set Float Key Value"));
	OwningSection->SetFlags(RF_Transactional);
}

void SFloatCurveKeyEditor::OnEndSliderMovement(float Value)
{
	if (GEditor->IsTransactionActive())
	{
		GEditor->EndTransaction();
	}
}

float SFloatCurveKeyEditor::OnGetKeyValue() const
{
	if (ExternalValue.IsSet() && ExternalValue.Get().IsSet())
	{
		return ExternalValue.Get().GetValue();
	}

	float CurrentTime = Sequencer->GetCurrentLocalTime(*Sequencer->GetFocusedMovieSceneSequence());
	return Curve->Eval(CurrentTime);
}

void SFloatCurveKeyEditor::OnValueChanged(float Value)
{
	if (OwningSection->TryModify())
	{
		float CurrentTime = Sequencer->GetCurrentLocalTime(*Sequencer->GetFocusedMovieSceneSequence());
		bool bAutoSetTrackDefaults = Sequencer->GetAutoSetTrackDefaults();
		
		FKeyHandle CurrentKeyHandle = Curve->FindKey(CurrentTime);
		if (Curve->IsKeyHandleValid(CurrentKeyHandle))
		{
			Curve->SetKeyValue(CurrentKeyHandle, Value);
		}
		else
		{
			if (Curve->GetNumKeys() != 0 || bAutoSetTrackDefaults == false)
			{
				// When auto setting track defaults are disabled, add a key even when it's empty so that the changed
				// value is saved and is propagated to the property.
				Curve->AddKey(CurrentTime, Value, false, CurrentKeyHandle);
				MovieSceneHelpers::SetKeyInterpolation(*Curve, CurrentKeyHandle, Sequencer->GetKeyInterpolation());
			}

			if (OwningSection->GetStartTime() > CurrentTime)
			{
				OwningSection->SetStartTime(CurrentTime);
			}
			if (OwningSection->GetEndTime() < CurrentTime)
			{
				OwningSection->SetEndTime(CurrentTime);
			}
		}

		// Always update the default value when auto-set default values is enabled so that the last changes
		// are always saved to the track.
		if (bAutoSetTrackDefaults)
		{
			Curve->SetDefaultValue(Value);
		}

		Sequencer->NotifyMovieSceneDataChanged(EMovieSceneDataChangeType::TrackValueChangedRefreshImmediately);
	}
}

void SFloatCurveKeyEditor::OnValueCommitted(float Value, ETextCommit::Type CommitInfo)
{
	if (CommitInfo == ETextCommit::OnEnter || CommitInfo == ETextCommit::OnUserMovedFocus)
	{
		const FScopedTransaction Transaction( LOCTEXT("SetFloatKey", "Set Float Key Value") );
		OwningSection->SetFlags(RF_Transactional);
		OnValueChanged(Value);
	}
}

#undef LOCTEXT_NAMESPACE