// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#define LOCTEXT_NAMESPACE "IntegralCurveKeyEditor"

/**
 * A widget for editing a curve representing integer keys.
 */
template<typename IntegralType>
class SIntegralCurveKeyEditor : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SIntegralCurveKeyEditor) { }

		/** The sequencer which is controlling this key editor. */
		SLATE_ARGUMENT(ISequencer*, Sequencer)

		/** The section that owns the data edited by this key editor. */
		SLATE_ARGUMENT(UMovieSceneSection*, OwningSection)

		/** The curve being edited by this curve editor. */
		SLATE_ARGUMENT(FIntegralCurve*, Curve)

		/** Allows the value displayed and edited by this key editor to be supplied from an external source.  This
		is useful for curves on property tracks who's property value can change without changing the animation. */
		SLATE_ATTRIBUTE(TOptional<IntegralType>, ExternalValue)

	SLATE_END_ARGS();

	void Construct(const FArguments& InArgs)
	{
		Sequencer = InArgs._Sequencer;
		OwningSection = InArgs._OwningSection;
		Curve = InArgs._Curve;
		ExternalValue = InArgs._ExternalValue;

		ChildSlot
		[
			SNew(SSpinBox<IntegralType>)
			.Style(&FEditorStyle::GetWidgetStyle<FSpinBoxStyle>("Sequencer.HyperlinkSpinBox"))
			.Font(FEditorStyle::GetFontStyle("Sequencer.AnimationOutliner.RegularFont"))
			.MinValue(TOptional<IntegralType>())
			.MaxValue(TOptional<IntegralType>())
			.MaxSliderValue(TOptional<IntegralType>())
			.MinSliderValue(TOptional<IntegralType>())
			.Delta(1)
			.Value(this, &SIntegralCurveKeyEditor::OnGetKeyValue)
			.OnValueChanged(this, &SIntegralCurveKeyEditor::OnValueChanged)
			.OnValueCommitted(this, &SIntegralCurveKeyEditor::OnValueCommitted)
			.OnBeginSliderMovement(this, &SIntegralCurveKeyEditor::OnBeginSliderMovement)
			.OnEndSliderMovement(this, &SIntegralCurveKeyEditor::OnEndSliderMovement)
			.ClearKeyboardFocusOnCommit(true)
		];
	}

private:
	void OnBeginSliderMovement()
	{
		GEditor->BeginTransaction(LOCTEXT("SetIntegralKey", "Set Integral Key Value"));
		OwningSection->SetFlags(RF_Transactional);
		OwningSection->TryModify();
	}

	void OnEndSliderMovement(IntegralType Value)
	{
		if (GEditor->IsTransactionActive())
		{
			GEditor->EndTransaction();
		}
	}

	IntegralType OnGetKeyValue() const
	{
		if (ExternalValue.IsSet() && ExternalValue.Get().IsSet())
		{
			return ExternalValue.Get().GetValue();
		}

		float CurrentTime = Sequencer->GetCurrentLocalTime(*Sequencer->GetFocusedMovieSceneSequence());
		return Curve->Evaluate(CurrentTime);
	}

	void OnValueChanged(IntegralType Value)
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
					Curve->AddKey(CurrentTime, Value,  CurrentKeyHandle);
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

	void OnValueCommitted(IntegralType Value, ETextCommit::Type CommitInfo)
	{
		if (CommitInfo == ETextCommit::OnEnter || CommitInfo == ETextCommit::OnUserMovedFocus)
		{
			const FScopedTransaction Transaction(LOCTEXT("SetIntegralKey", "Set Integral Key Value"));

			OnValueChanged(Value);
		}
	}

private:

	ISequencer* Sequencer;
	UMovieSceneSection* OwningSection;
	FIntegralCurve* Curve;
	TAttribute<TOptional<IntegralType>> ExternalValue;
};

#undef LOCTEXT_NAMESPACE