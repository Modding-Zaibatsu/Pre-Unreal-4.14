// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Matinee/InterpTrackToggle.h"
#include "MovieSceneParticleSection.h"

class UInterpTrackLinearColorProp;
class UInterpTrackColorProp;
class UInterpTrackBoolProp;
class UInterpTrackFloatBase;
class UInterpTrackMove;
class UInterpTrackToggle;
class UInterpTrackAnimControl;
class UInterpTrackSound;
class UInterpTrackFade;
class UInterpTrackDirector;
class UInterpTrackEvent;
class UInterpTrackVisibility;

class UMovieSceneColorTrack;
class UMovieSceneBoolTrack;
class UMovieSceneFloatTrack;
class UMovieScene3DTransformTrack;
class UMovieSceneParticleTrack;
class UMovieSceneSkeletalAnimationTrack;
class UMovieSceneAudioTrack;
class UMovieSceneFadeTrack;
class UMovieSceneCameraCutTrack;
class UMovieSceneEventTrack;
class UMovieSceneVisibilityTrack;

class UMovieSceneSequence;

class MOVIESCENETOOLS_API FMatineeImportTools
{
public:

	/** Converts a matinee interpolation mode to its equivalent rich curve interpolation mode. */
	static ERichCurveInterpMode MatineeInterpolationToRichCurveInterpolation( EInterpCurveMode CurveMode );

	/** Converts a matinee interpolation mode to its equivalent rich curve tangent mode. */
	static ERichCurveTangentMode MatineeInterpolationToRichCurveTangent( EInterpCurveMode CurveMode );

	/** Tries to convert a matinee toggle to a particle key. */
	static bool TryConvertMatineeToggleToOutParticleKey( ETrackToggleAction ToggleAction, EParticleKey::Type& OutParticleKey );

	/** Adds a key to a rich curve based on matinee curve key data. */
	static void SetOrAddKey( FRichCurve& Curve, float Time, float Value, float ArriveTangent, float LeaveTangent, EInterpCurveMode MatineeInterpMode );

	/** Copies keys from a matinee bool track to a sequencer bool track. */
	static bool CopyInterpBoolTrack( UInterpTrackBoolProp* MatineeBoolTrack, UMovieSceneBoolTrack* BoolTrack );

	/** Copies keys from a matinee float track to a sequencer float track. */
	static bool CopyInterpFloatTrack( UInterpTrackFloatBase* MatineeFloatTrack, UMovieSceneFloatTrack* FloatTrack );

	/** Copies keys from a matinee move track to a sequencer transform track. */
	static bool CopyInterpMoveTrack( UInterpTrackMove* MoveTrack, UMovieScene3DTransformTrack* TransformTrack );

	/** Copies keys from a matinee color track to a sequencer color track. */
	static bool CopyInterpColorTrack( UInterpTrackColorProp* ColorPropTrack, UMovieSceneColorTrack* ColorTrack );

	/** Copies keys from a matinee linear color track to a sequencer color track. */
	static bool CopyInterpLinearColorTrack( UInterpTrackLinearColorProp* LinearColorPropTrack, UMovieSceneColorTrack* ColorTrack );

	/** Copies keys from a matinee toggle track to a sequencer particle track. */
	static bool CopyInterpParticleTrack( UInterpTrackToggle* MatineeToggleTrack, UMovieSceneParticleTrack* ParticleTrack );

	/** Copies keys from a matinee anim control track to a sequencer skeletal animation track. */
	static bool CopyInterpAnimControlTrack( UInterpTrackAnimControl* MatineeAnimControlTrack, UMovieSceneSkeletalAnimationTrack* SkeletalAnimationTrack, float EndPlaybackRange );

	/** Copies keys from a matinee sound track to a sequencer audio track. */
	static bool CopyInterpSoundTrack( UInterpTrackSound* MatineeSoundTrack, UMovieSceneAudioTrack* AudioTrack );

	/** Copies keys from a matinee fade track to a sequencer fade track. */
	static bool CopyInterpFadeTrack( UInterpTrackFade* MatineeFadeTrack, UMovieSceneFadeTrack* FadeTrack );

	/** Copies keys from a matinee director track to a sequencer camera cut track. */
	static bool CopyInterpDirectorTrack( UInterpTrackDirector* DirectorTrack, UMovieSceneCameraCutTrack* CameraCutTrack, AMatineeActor* MatineeActor, UMovieSceneSequence* MovieSceneSequence );

	/** Copies keys from a matinee event track to a sequencer event track. */
	static bool CopyInterpEventTrack( UInterpTrackEvent* MatineeEventTrack, UMovieSceneEventTrack* EventTrack );

	/** Copies keys from a matinee visibility track to a sequencer visibility track. */
	static bool CopyInterpVisibilityTrack( UInterpTrackVisibility* MatineeVisibilityTrack, UMovieSceneVisibilityTrack* VisibilityTrack );
};