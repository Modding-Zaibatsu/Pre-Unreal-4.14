// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#pragma once

#if !UE_ENABLE_ICU

class FLegacyInternationalization
{
public:
	FLegacyInternationalization(FInternationalization* const I18N);

	bool Initialize();
	void Terminate();

	void LoadAllCultureData();

	bool IsCultureRemapped(const FString& Name, FString* OutMappedCulture);
	bool IsCultureDisabled(const FString& Name);

	bool SetCurrentCulture(const FString& Name);
	void GetCultureNames(TArray<FString>& CultureNames) const;
	TArray<FString> GetPrioritizedCultureNames(const FString& Name);
	FCulturePtr GetCulture(const FString& Name);

private:
	FInternationalization* const I18N;
};

#endif