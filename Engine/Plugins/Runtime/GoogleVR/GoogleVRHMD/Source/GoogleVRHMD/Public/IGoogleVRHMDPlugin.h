/* Copyright 2016 Google Inc. All rights reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

#include "ModuleManager.h"
#include "IHeadMountedDisplayModule.h"

#define GOOGLEVRHMD_SUPPORTED_ANDROID_PLATFORMS (PLATFORM_ANDROID)
#define GOOGLEVRHMD_SUPPORTED_IOS_PLATFORMS (PLATFORM_IOS)
#define GOOGLEVRHMD_SUPPORTED_PLATFORMS (GOOGLEVRHMD_SUPPORTED_ANDROID_PLATFORMS || GOOGLEVRHMD_SUPPORTED_IOS_PLATFORMS)

/**
 * The public interface to this module.
 */
class IGoogleVRHMDPlugin : public IHeadMountedDisplayModule
{
public:

	/**
	 * Singleton-like access to this module's interface.  This is just for convenience!
	 * Beware of calling this during the shutdown phase, though.  Your module might have been unloaded already.
	 *
	 * @return Returns singleton instance, loading the module on demand if needed
	 */
	static inline IGoogleVRHMDPlugin& Get()
	{
		return FModuleManager::LoadModuleChecked< IGoogleVRHMDPlugin >( "GoogleVRHMD" );
	}

	/**
	 * Checks to see if this module is loaded and ready.  It is only valid to call Get() if IsAvailable() returns true.
	 *
	 * @return True if the module is loaded and ready to use
	 */
	static inline bool IsAvailable()
	{
		return FModuleManager::Get().IsModuleLoaded( "GoogleVRHMD" );
	}
};
