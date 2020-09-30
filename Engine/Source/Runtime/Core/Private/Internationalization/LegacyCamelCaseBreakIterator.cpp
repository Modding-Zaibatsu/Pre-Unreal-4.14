// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "CorePrivatePCH.h"
#include "BreakIterator.h"
#include "CamelCaseBreakIterator.h"

#if !UE_ENABLE_ICU

class FLegacyCamelCaseBreakIterator : public FCamelCaseBreakIterator
{
protected:
	virtual void TokenizeString(TArray<FToken>& OutTokens) override;
};

void FLegacyCamelCaseBreakIterator::TokenizeString(TArray<FToken>& OutTokens)
{
	OutTokens.Empty(String.Len());

	for(int32 CurrentCharIndex = 0; CurrentCharIndex < String.Len(); ++CurrentCharIndex)
	{
		const TCHAR CurrentChar = String[CurrentCharIndex];

		ETokenType TokenType = ETokenType::Other;
		if(FChar::IsLower(CurrentChar))
		{
			TokenType = ETokenType::Lowercase;
		}
		else if(FChar::IsUpper(CurrentChar))
		{
			TokenType = ETokenType::Uppercase;
		}
		else if(FChar::IsDigit(CurrentChar))
		{
			TokenType = ETokenType::Digit;
		}

		OutTokens.Emplace(FToken(TokenType, CurrentCharIndex));
	}

	OutTokens.Emplace(FToken(ETokenType::Null, String.Len()));

	// There should always be at least one token for the end of the string
	check(OutTokens.Num());
}

TSharedRef<IBreakIterator> FBreakIterator::CreateCamelCaseBreakIterator()
{
	return MakeShareable(new FLegacyCamelCaseBreakIterator());
}

#endif
