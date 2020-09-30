// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "WebBrowserPrivatePCH.h"
#include "AndroidJSScripting.h"
#include "AndroidJSStructSerializerBackend.h"

void FAndroidJSStructSerializerBackend::WriteProperty(const FStructSerializerState& State, int32 ArrayIndex)
{
	// The parent class serialzes UObjects as NULLs
	if (State.ValueType == UObjectProperty::StaticClass())
	{
		WriteUObject(State, Cast<UObjectProperty>(State.ValueProperty)->GetPropertyValue_InContainer(State.ValueData, ArrayIndex));
	}
	// basic property type (json serializable)
	else
	{
		FJsonStructSerializerBackend::WriteProperty(State, ArrayIndex);
	}
}

void FAndroidJSStructSerializerBackend::WriteUObject(const FStructSerializerState& State, UObject* Value)
{
	// Note this function uses WriteRawJSONValue to append non-json data to the output stream.
	FString RawValue = Scripting->ConvertObject(Value);
	if ((State.ValueProperty == nullptr) || (State.ValueProperty->ArrayDim > 1) || (State.ValueProperty->GetOuter()->GetClass() == UArrayProperty::StaticClass()))
	{
		GetWriter()->WriteRawJSONValue(RawValue);
	}
	else if (State.KeyProperty != nullptr)
	{
		FString KeyString;
		State.KeyProperty->ExportTextItem(KeyString, State.KeyData, nullptr, nullptr, PPF_None);
		GetWriter()->WriteRawJSONValue(KeyString, RawValue);
	}
	else
	{
		GetWriter()->WriteRawJSONValue(State.ValueProperty->GetName(), RawValue);
	}
}

FString FAndroidJSStructSerializerBackend::ToString()
{
	ReturnBuffer.Add(0);
	ReturnBuffer.Add(0);
	auto CastObject = StringCast<TCHAR>((UCS2CHAR*)ReturnBuffer.GetData());
	return FString(CastObject.Get(), CastObject.Length());
}

FAndroidJSStructSerializerBackend::FAndroidJSStructSerializerBackend(TSharedRef<class FAndroidJSScripting> InScripting)
	: Scripting(InScripting)
	, ReturnBuffer()
	, Writer(ReturnBuffer)
	, FJsonStructSerializerBackend(Writer)
{
}
