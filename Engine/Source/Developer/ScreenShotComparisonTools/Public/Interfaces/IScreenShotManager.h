// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	IScreenShotComparisonModule.h: Declares the IScreenShotComparisonModule interface.
=============================================================================*/

#pragma once

#include "Async/Async.h"
#include "IScreenShotData.h"
#include "../ImageComparer.h"

/**
 * Type definition for shared pointers to instances of IScreenShotManager.
 */
typedef TSharedPtr<class IScreenShotManager> IScreenShotManagerPtr;

/**
 * Type definition for shared references to instances of IScreenShotManager.
 */
typedef TSharedRef<class IScreenShotManager> IScreenShotManagerRef;

struct FScreenShotDataItem;



/**
 * Delegate type for session filter changing.
 */
DECLARE_DELEGATE(FOnScreenFilterChanged)


/**
 * Interface for screen manager module.
 */
class IScreenShotManager
{
public:
	virtual ~IScreenShotManager(){ }

	/**
	 * Generate the screen shot data
	 */
	virtual void GenerateLists( ) = 0;

	/**
	 * Get the screen shot list
	 *
	 * @return the array of screen shot data
	 */
	virtual TArray<IScreenShotDataPtr>& GetLists( ) = 0;

	/**
	 */
	virtual TArray< TSharedPtr<FScreenShotDataItem> >& GetScreenshotResults() = 0;

	/**
	 * Get the list of active platforms
	 *
	 * @return the array of platforms
	 */
	virtual TArray< TSharedPtr<FString> >& GetCachedPlatfomList( ) = 0;

	/**
	 * Register for screen shot updates
	 *
	 * @param InDelegate - Delegate register
	 */
	virtual void RegisterScreenShotUpdate( const FOnScreenFilterChanged& InDelegate ) = 0;

	/**
	 * Set the filter
	 *
	 * @param InFilter - The screen shot filter
	 */
	virtual void SetFilter(TSharedPtr<ScreenShotFilterCollection> InFilter ) = 0;

	/**
	 * Only display every Nth screen shot
	 *
	 * @param NewNth - The new N.
	 */
	virtual void SetDisplayEveryNthScreenshot(int32 NewNth) = 0;

	/**
	 * Compares screenshots.
	 */
	virtual TFuture< void > CompareScreensotsAsync() = 0;

	/**
	 * Exports the screenshots to the export location specified
	 */
	virtual TFuture< void > ExportScreensotsAsync(FString ExportPath) = 0;

	/**
	 * Imports screenshot comparison data from a given path.
	 */
	virtual TSharedPtr<FComparisonResults> ImportScreensots(FString ImportPath) = 0;
};
