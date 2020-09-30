// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "MovieSceneCapturePCH.h"
#include "MovieSceneCapture.h"
#include "MovieSceneCaptureModule.h"
#include "MovieSceneCaptureEnvironment.h"

int32 UMovieSceneCaptureEnvironment::GetCaptureFrameNumber()
{
	UMovieSceneCapture* Capture = static_cast<UMovieSceneCapture*>(IMovieSceneCaptureModule::Get().GetFirstActiveMovieSceneCapture());
	return Capture ? Capture->GetMetrics().Frame : 0;
}

float UMovieSceneCaptureEnvironment::GetCaptureElapsedTime()
{
	UMovieSceneCapture* Capture = static_cast<UMovieSceneCapture*>(IMovieSceneCaptureModule::Get().GetFirstActiveMovieSceneCapture());
	return Capture ? Capture->GetMetrics().ElapsedSeconds : 0.f;
}