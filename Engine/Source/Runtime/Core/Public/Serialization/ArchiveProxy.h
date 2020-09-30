// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Archive.h"

/**
 * Base class for archive proxies.
 *
 * Archive proxies are archive types that modify the behavior of another archive type.
 */
class FArchiveProxy : public FArchive
{
public:
	/**
	 * Creates and initializes a new instance of the archive proxy.
	 *
	 * @param InInnerArchive The inner archive to proxy.
	 */
	CORE_API FArchiveProxy(FArchive& InInnerArchive);

	virtual FArchive& operator<<(class FName& Value) override
	{
		InnerArchive << Value;
		return *this;
	}

	virtual FArchive& operator<<(class FText& Value) override
	{
		InnerArchive << Value;
		return *this;
	}

	virtual FArchive& operator<<(class UObject*& Value) override
	{
		InnerArchive << Value;
		return *this;
	}

	virtual FArchive& operator<<(struct FStringAssetReference& Value) override
	{
		InnerArchive << Value;
		return *this;
	}

	virtual void Serialize(void* V, int64 Length) override
	{
		InnerArchive.Serialize(V, Length);
	}

	virtual void SerializeBits(void* Bits, int64 LengthBits) override
	{
		InnerArchive.SerializeBits(Bits, LengthBits);
	}

	virtual void SerializeInt(uint32& Value, uint32 Max) override
	{
		InnerArchive.SerializeInt(Value, Max);
	}

	virtual void Preload(UObject* Object) override
	{
		InnerArchive.Preload(Object);
	}

	virtual void CountBytes(SIZE_T InNum, SIZE_T InMax) override
	{
		InnerArchive.CountBytes(InNum, InMax);
	}

	CORE_API virtual FString GetArchiveName() const override;

	virtual class FLinker* GetLinker() override
	{
		return InnerArchive.GetLinker();
	}

#if USE_STABLE_LOCALIZATION_KEYS
	CORE_API virtual void SetLocalizationNamespace(const FString& InLocalizationNamespace) override;
	CORE_API virtual FString GetLocalizationNamespace() const override;
#endif // USE_STABLE_LOCALIZATION_KEYS

	virtual int64 Tell() override
	{
		return InnerArchive.Tell();
	}

	virtual int64 TotalSize() override
	{
		return InnerArchive.TotalSize();
	}

	virtual bool AtEnd() override
	{
		return InnerArchive.AtEnd();
	}

	virtual void Seek(int64 InPos) override
	{
		InnerArchive.Seek(InPos);
	}

	virtual void AttachBulkData(UObject* Owner, FUntypedBulkData* BulkData) override
	{
		InnerArchive.AttachBulkData(Owner, BulkData);
	}

	virtual void DetachBulkData(FUntypedBulkData* BulkData, bool bEnsureBulkDataIsLoaded) override
	{
		InnerArchive.DetachBulkData(BulkData, bEnsureBulkDataIsLoaded);
	}

	virtual bool Precache(int64 PrecacheOffset, int64 PrecacheSize) override
	{
		return InnerArchive.Precache(PrecacheOffset, PrecacheSize);
	}

	virtual bool SetCompressionMap(TArray<struct FCompressedChunk>* CompressedChunks, ECompressionFlags CompressionFlags) override
	{
		return InnerArchive.SetCompressionMap(CompressedChunks, CompressionFlags);
	}

	virtual void Flush() override
	{
		InnerArchive.Flush();
	}

	virtual bool Close() override
	{
		return InnerArchive.Close();
	}

	virtual bool GetError() override
	{
		return InnerArchive.GetError();
	}

	virtual void MarkScriptSerializationStart(const UObject* Obj) override
	{
		InnerArchive.MarkScriptSerializationStart(Obj);
	}

	virtual void MarkScriptSerializationEnd(const UObject* Obj) override
	{
		InnerArchive.MarkScriptSerializationEnd(Obj);
	}

	virtual bool IsCloseComplete(bool& bHasError) override
	{
		return InnerArchive.IsCloseComplete(bHasError);
	}
#if WITH_EDITOR
	virtual void PushDebugDataString(const FName& DebugData) override
	{
		InnerArchive.PushDebugDataString(DebugData);
	}
	virtual void PopDebugDataString() override
	{
		InnerArchive.PopDebugDataString();
	}
#endif
protected:

	/** Holds the archive that this archive is a proxy to. */
	FArchive& InnerArchive;
};
