// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
// ..

#include "VulkanShaderFormat.h"
#include "Core.h"
#include "ShaderPreprocessor.h"
#include "ShaderCompilerCommon.h"
#include "hlslcc.h"
#include "VulkanBackend.h"
#include "VulkanShaderResources.h"


DEFINE_LOG_CATEGORY_STATIC(LogVulkanShaderCompiler, Log, All); 

//static int32 GUseExternalShaderCompiler = 0;
//static FAutoConsoleVariableRef CVarVulkanUseExternalShaderCompiler(
//	TEXT("r.Vulkan.UseExternalShaderCompiler"),
//	GUseExternalShaderCompiler,
//	TEXT("Whether to use the internal shader compiling library or the external glslang tool.\n")
//	TEXT(" 0: Internal compiler\n")
//	TEXT(" 1: External compiler)"),
//	ECVF_Default
//	);

extern bool GenerateSpirv(const ANSICHAR* Source, FCompilerInfo& CompilerInfo, FString& OutErrors, const FString& DumpDebugInfoPath, TArray<uint8>& OutSpirv);


static TArray<ANSICHAR> ParseIdentifierANSI(const FString& Str)
{
	TArray<ANSICHAR> Result;
	Result.Reserve(Str.Len());
	for (int32 Index = 0; Index < Str.Len(); ++Index)
	{
		Result.Add(FChar::ToLower((ANSICHAR)Str[Index]));
	}
	Result.Add('\0');

	return Result;
}


inline const ANSICHAR * CStringEndOfLine(const ANSICHAR * Text)
{
	const ANSICHAR * LineEnd = FCStringAnsi::Strchr(Text, '\n');
	if (nullptr == LineEnd)
	{
		LineEnd = Text + FCStringAnsi::Strlen(Text);
	}
	return LineEnd;
}

inline bool CStringIsBlankLine(const ANSICHAR * Text)
{
	while (!FCharAnsi::IsLinebreak(*Text))
	{
		if (!FCharAnsi::IsWhitespace(*Text))
		{
			return false;
		}
		++Text;
	}
	return true;
}

static FString ParseIdentifier(const ANSICHAR* &Str)
{
	FString Result;

	while ((*Str >= 'A' && *Str <= 'Z')
		|| (*Str >= 'a' && *Str <= 'z')
		|| (*Str >= '0' && *Str <= '9')
		|| *Str == '_')
	{
		Result += *Str;
		++Str;
	}

	return Result;
}

inline void AppendCString(TArray<ANSICHAR> & Dest, const ANSICHAR * Source)
{
	if (Dest.Num() > 0)
	{
		Dest.Insert(Source, FCStringAnsi::Strlen(Source), Dest.Num() - 1);;
	}
	else
	{
		Dest.Append(Source, FCStringAnsi::Strlen(Source) + 1);
	}
}

inline bool MoveHashLines(TArray<ANSICHAR> & Dest, TArray<ANSICHAR> & Source)
{
	// Walk through the lines to find the first non-# line...
	const ANSICHAR * LineStart = Source.GetData();
	for (bool FoundNonHashLine = false; !FoundNonHashLine;)
	{
		const ANSICHAR * LineEnd = CStringEndOfLine(LineStart);
		if (LineStart[0] != '#' && !CStringIsBlankLine(LineStart))
		{
			FoundNonHashLine = true;
		}
		else if (LineEnd[0] == '\n')
		{
			LineStart = LineEnd + 1;
		}
		else
		{
			LineStart = LineEnd;
		}
	}
	// Copy the hash lines over, if we found any. And delete from
	// the source.
	if (LineStart > Source.GetData())
	{
		int32 LineLength = LineStart - Source.GetData();
		if (Dest.Num() > 0)
		{
			Dest.Insert(Source.GetData(), LineLength, Dest.Num() - 1);
		}
		else
		{
			Dest.Append(Source.GetData(), LineLength);
			Dest.Append("", 1);
		}
		if (Dest.Last(1) != '\n')
		{
			Dest.Insert("\n", 1, Dest.Num() - 1);
		}
		Source.RemoveAt(0, LineStart - Source.GetData());
		return true;
	}
	return false;
}

static bool Match(const ANSICHAR* &Str, ANSICHAR Char)
{
	if (*Str == Char)
	{
		++Str;
		return true;
	}

	return false;
}

template <typename T>
uint32 ParseNumber(const T* Str)
{
	check(Str);

	uint32 Num = 0;

	int32 Len = 0;
	// Find terminating character
	for(int32 Index=0; Index<128; Index++)
	{
		if(Str[Index] == 0)
		{
			Len = Index;
			break;
		}
	}

	check(Len > 0);

	// Find offset to integer type
	int32 Offset = -1;
	for(int32 Index=0; Index<Len; Index++)
	{
		if (*(Str + Index) >= '0' && *(Str + Index) <= '9')
		{
			Offset = Index;
			break;
		}
	}

	// Check if we found a number
	check(Offset >= 0);

	Str += Offset;

	while (*(Str) && *Str >= '0' && *Str <= '9')
	{
		Num = Num * 10 + *Str++ - '0';
	}

	return Num;
}

static inline FString GetExtension(EHlslShaderFrequency Frequency, bool bAddDot = true)
{
	const TCHAR* Name = nullptr;
	switch (Frequency)
	{
	default:
		check(0);
		// fallthrough...

	case HSF_PixelShader:		Name = TEXT(".frag"); break;
	case HSF_VertexShader:		Name = TEXT(".vert"); break;
	case HSF_ComputeShader:		Name = TEXT(".comp"); break;
	case HSF_GeometryShader:	Name = TEXT(".geom"); break;
	case HSF_HullShader:		Name = TEXT(".tesc"); break;
	case HSF_DomainShader:		Name = TEXT(".tese"); break;
	}

	if (!bAddDot)
	{
		++Name;
	}
	return FString(Name);
}

static uint32 GetTypeComponents(const FString& Type)
{
	static const FString TypePrefix[] = { "f", "i", "u" };
	uint32 Components = 0;
	int32 PrefixLength = 0;
	for (uint32 i = 0; i<ARRAY_COUNT(TypePrefix); i++)
	{
		const FString& Prefix = TypePrefix[i];
		const int32 CmpLength = Type.Contains(Prefix, ESearchCase::CaseSensitive, ESearchDir::FromStart);
		if (CmpLength == Prefix.Len())
		{
			PrefixLength = CmpLength;
			break;
		}
	}

	check(PrefixLength > 0);
	Components = ParseNumber(*Type + PrefixLength);

	check(Components > 0);
	return Components;
}


static void GenerateBindingTable(const FVulkanShaderSerializedBindings& SerializedBindings, FVulkanShaderBindingTable& OutBindingTable)
{
	int32 NumCombinedSamplers = 0;
	int32 NumSamplerBuffers = 0;
	int32 NumUniformBuffers = 0;

	auto& Layouts = SerializedBindings.Bindings;

	//#todo-rco: FIX! SamplerBuffers share numbering with Samplers
	NumCombinedSamplers = Layouts[FVulkanShaderSerializedBindings::TYPE_COMBINED_IMAGE_SAMPLER].Num() + Layouts[FVulkanShaderSerializedBindings::TYPE_SAMPLER_BUFFER].Num();
	NumSamplerBuffers = Layouts[FVulkanShaderSerializedBindings::TYPE_COMBINED_IMAGE_SAMPLER].Num() + Layouts[FVulkanShaderSerializedBindings::TYPE_SAMPLER_BUFFER].Num();
	NumUniformBuffers = Layouts[FVulkanShaderSerializedBindings::TYPE_PACKED_UNIFORM_BUFFER].Num() + Layouts[FVulkanShaderSerializedBindings::TYPE_UNIFORM_BUFFER].Num();

	for (int32 Index = 0; Index < CrossCompiler::PACKED_TYPEINDEX_MAX; ++Index)
	{
		OutBindingTable.PackedGlobalUBsIndices[Index] = -1;
	}

	OutBindingTable.CombinedSamplerBindingIndices.AddUninitialized(NumCombinedSamplers);
	//#todo-rco: FIX! SamplerBuffers share numbering with Samplers
	OutBindingTable.SamplerBufferBindingIndices.AddUninitialized(NumSamplerBuffers);
	OutBindingTable.UniformBufferBindingIndices.AddUninitialized(NumUniformBuffers);

	for (int32 Index = 0; Index < Layouts[FVulkanShaderSerializedBindings::TYPE_COMBINED_IMAGE_SAMPLER].Num(); ++Index)
	{
		auto& Mapping = Layouts[FVulkanShaderSerializedBindings::TYPE_COMBINED_IMAGE_SAMPLER][Index];
		OutBindingTable.CombinedSamplerBindingIndices[Mapping.EngineBindingIndex] = Mapping.VulkanBindingIndex;
		//#todo-rco: FIX! SamplerBuffers share numbering with Samplers
		OutBindingTable.SamplerBufferBindingIndices[Mapping.EngineBindingIndex] = Mapping.VulkanBindingIndex;
	}

	for (int32 Index = 0; Index < Layouts[FVulkanShaderSerializedBindings::TYPE_SAMPLER_BUFFER].Num(); ++Index)
	{
		auto& Mapping = Layouts[FVulkanShaderSerializedBindings::TYPE_SAMPLER_BUFFER][Index];
		OutBindingTable.CombinedSamplerBindingIndices[Mapping.EngineBindingIndex] = Mapping.VulkanBindingIndex;
		//#todo-rco: FIX! SamplerBuffers share numbering with Samplers
		OutBindingTable.SamplerBufferBindingIndices[Mapping.EngineBindingIndex] = Mapping.VulkanBindingIndex;
	}

	for (int32 Index = 0; Index < Layouts[FVulkanShaderSerializedBindings::TYPE_UNIFORM_BUFFER].Num(); ++Index)
	{
		auto& Mapping = Layouts[FVulkanShaderSerializedBindings::TYPE_UNIFORM_BUFFER][Index];
		OutBindingTable.UniformBufferBindingIndices[Mapping.EngineBindingIndex] = Mapping.VulkanBindingIndex;
	}

	for (int32 Index = 0; Index < Layouts[FVulkanShaderSerializedBindings::TYPE_PACKED_UNIFORM_BUFFER].Num(); ++Index)
	{
		auto& Mapping = Layouts[FVulkanShaderSerializedBindings::TYPE_PACKED_UNIFORM_BUFFER][Index];
		OutBindingTable.UniformBufferBindingIndices[Mapping.EngineBindingIndex] = Mapping.VulkanBindingIndex;
		uint8 PackedIndex = SerializedBindings.PackedUBTypeIndex[Index];
		check(PackedIndex != (uint8)-1);
		OutBindingTable.PackedGlobalUBsIndices[PackedIndex] = Mapping.EngineBindingIndex;
	}

	// Do not share numbers here
	OutBindingTable.NumDescriptorsWithoutPackedUniformBuffers = Layouts[FVulkanShaderSerializedBindings::TYPE_COMBINED_IMAGE_SAMPLER].Num() + Layouts[FVulkanShaderSerializedBindings::TYPE_SAMPLER_BUFFER].Num() + Layouts[FVulkanShaderSerializedBindings::TYPE_UNIFORM_BUFFER].Num();
	OutBindingTable.NumDescriptors = OutBindingTable.NumDescriptorsWithoutPackedUniformBuffers + Layouts[FVulkanShaderSerializedBindings::TYPE_PACKED_UNIFORM_BUFFER].Num();
}


static void BuildShaderOutput(
	FShaderCompilerOutput& ShaderOutput,
	const FShaderCompilerInput& ShaderInput,
	const ANSICHAR* InShaderSource,
	int32 SourceLen,
	const FVulkanBindingTable& BindingTable,
	const ANSICHAR* InShaderSourceES,
	int32 SourceLenES,
	TArray<uint8>& Spirv,
	const FString& DebugName
	)
{
	const ANSICHAR* USFSource = InShaderSource;
	CrossCompiler::FHlslccHeader CCHeader;
	if (!CCHeader.Read(USFSource, SourceLen))
	{
		UE_LOG(LogVulkanShaderCompiler, Error, TEXT("Bad hlslcc header found"));
		return;
	}

	if (*USFSource != '#')
	{
		UE_LOG(LogVulkanShaderCompiler, Error, TEXT("Bad hlslcc header found! Missing '#'!"));
		return;
	}

	FVulkanCodeHeader Header;

	FShaderParameterMap& ParameterMap = ShaderOutput.ParameterMap;
	EShaderFrequency Frequency = (EShaderFrequency)ShaderOutput.Target.Frequency;

	TBitArray<> UsedUniformBufferSlots;
	UsedUniformBufferSlots.Init(false, 32);


	static const FString AttributePrefix = TEXT("in_ATTRIBUTE");
	static const FString GL_Prefix = TEXT("gl_");
	for (auto& Input : CCHeader.Inputs)
	{
		// Only process attributes for vertex shaders.
		if (Frequency == SF_Vertex && Input.Name.StartsWith(AttributePrefix))
		{
			int32 AttributeIndex = ParseNumber(*Input.Name + AttributePrefix.Len());
			Header.SerializedBindings.InOutMask |= (1 << AttributeIndex);
		}
#if 0
		// Record user-defined input varyings
		else if (!Input.Name.StartsWith(GL_Prefix))
		{
			FVulkanShaderVarying Var;
			Var.Location = Input.Index;
			Var.Varying = ParseIdentifierANSI(Input.Name);
			Var.Components = GetTypeComponents(Input.Type);
			Header.SerializedBindings.InputVaryings.Add(Var);
		}
#endif
	}

	static const FString TargetPrefix = "out_Target";
	static const FString GL_FragDepth = "gl_FragDepth";
	for (auto& Output : CCHeader.Outputs)
	{
		// Only targets for pixel shaders must be tracked.
		if (Frequency == SF_Pixel && Output.Name.StartsWith(TargetPrefix))
		{
			uint8 TargetIndex = ParseNumber(*Output.Name + TargetPrefix.Len());
			Header.SerializedBindings.InOutMask |= (1 << TargetIndex);
		}
		// Only depth writes for pixel shaders must be tracked.
		else if (Frequency == SF_Pixel && Output.Name.Equals(GL_FragDepth))
		{
			Header.SerializedBindings.InOutMask |= 0x8000;
		}
#if 0
		// Record user-defined output varyings
		else if (!Output.Name.StartsWith(GL_Prefix))
		{
			FVulkanShaderVarying Var;
			Var.Location = Output.Index;
			Var.Varying = ParseIdentifierANSI(Output.Name);
			Var.Components = GetTypeComponents(Output.Type);
			Header.SerializedBindings.OutputVaryings.Add(Var);
		}
#endif
	}

	TMap<uint8, uint32> PackedGlobalUBs;
	// Then 'normal' uniform buffers.
	static const FString CBPrefix = "HLSLCC_CB";
	for (auto& UniformBlock : CCHeader.UniformBlocks)
	{
		uint16 UBIndex = UniformBlock.Index;
		UsedUniformBufferSlots[UBIndex] = true;
		if (UniformBlock.Name.StartsWith(CBPrefix))
		{
			// This is a uniform buffer that holds the array for packed global uniforms
			ANSICHAR Type = (ANSICHAR)UniformBlock.Name[CBPrefix.Len()];
			PackedGlobalUBs.Add(Type) = UBIndex;
		}
		else
		{
			// Regular UB
			ParameterMap.AddParameterAllocation(*UniformBlock.Name, Header.SerializedBindings.NumUniformBuffers++, 0, 0);
		}
	}

	// Parse binding table; categorize by type
	FMemory::Memset(Header.SerializedBindings.PackedUBTypeIndex, (uint8)-1);
	for (const auto& CurrBinding : BindingTable.GetBindings())
	{
		FVulkanShaderSerializedBindings::FBindMap NewBinding;
		NewBinding.VulkanBindingIndex = CurrBinding.Index;
		auto NewBindingName = ParseIdentifierANSI(CurrBinding.Name);
		NewBinding.EngineBindingIndex = ParseNumber(NewBindingName.GetData());

		auto Type = FVulkanShaderSerializedBindings::TYPE_MAX;
		switch (CurrBinding.Type)
		{
		//case FVulkanBindingTable::TYPE_SAMPLER:
		//	Type = FVulkanShaderSerializedBindings::TYPE_SAMPLER;
		//	break;
		case FVulkanBindingTable::TYPE_COMBINED_IMAGE_SAMPLER:
			Type = FVulkanShaderSerializedBindings::TYPE_COMBINED_IMAGE_SAMPLER;
			break;
		case FVulkanBindingTable::TYPE_SAMPLER_BUFFER:
			Type = FVulkanShaderSerializedBindings::TYPE_SAMPLER_BUFFER;
			break;
		case FVulkanBindingTable::TYPE_UNIFORM_BUFFER:
			Type = FVulkanShaderSerializedBindings::TYPE_UNIFORM_BUFFER;
			break;
		case FVulkanBindingTable::TYPE_PACKED_UNIFORM_BUFFER:
			Type = FVulkanShaderSerializedBindings::TYPE_PACKED_UNIFORM_BUFFER;
			check(Header.SerializedBindings.Bindings[Type].Num() < CrossCompiler::PACKED_TYPEINDEX_MAX);
			Header.SerializedBindings.PackedUBTypeIndex[Header.SerializedBindings.Bindings[Type].Num()] = CrossCompiler::PackedTypeNameToTypeIndex(CurrBinding.SubType);
			break;
		default:
			checkf(0, TEXT("Binding Type %d not found"), (int32)CurrBinding.Type);
			break;
		}

		Header.SerializedBindings.Bindings[Type].Add(NewBinding);
	}

	// Fix up packed global layouts so they are in packed name type index order
	{
		// Simple bubble sort of 5 elements
		bool bChanged = false;
		do
		{
			bChanged = false;

			auto& Bindings = Header.SerializedBindings.Bindings[FVulkanShaderSerializedBindings::TYPE_PACKED_UNIFORM_BUFFER];
			auto& PackedTypes = Header.SerializedBindings.PackedUBTypeIndex;
			for (int32 Index = 0; Index < Bindings.Num() - 1; ++Index)
			{
				if (PackedTypes[Index] > PackedTypes[Index + 1])
				{
					Swap(PackedTypes[Index], PackedTypes[Index + 1]);
					Swap(Bindings[Index], Bindings[Index + 1]);
					bChanged = true;
				}
			}
		}
		while (bChanged);
	}

	const uint16 BytesPerComponent = 4;

	// Packed global uniforms
	TMap<ANSICHAR, uint16> PackedGlobalArraySize;
	for (auto& PackedGlobal : CCHeader.PackedGlobals)
	{
		ParameterMap.AddParameterAllocation(
			*PackedGlobal.Name,
			PackedGlobal.PackedType,
			PackedGlobal.Offset * BytesPerComponent,
			PackedGlobal.Count * BytesPerComponent
			);

		uint16& Size = PackedGlobalArraySize.FindOrAdd(PackedGlobal.PackedType);
		Size = FMath::Max<uint16>(BytesPerComponent * (PackedGlobal.Offset + PackedGlobal.Count), Size);
	}

	// Packed Uniform Buffers
	TMap<int, TMap<ANSICHAR, uint16> > PackedUniformBuffersSize;
	for (auto& PackedUB : CCHeader.PackedUBs)
	{
		check(PackedUB.Attribute.Index == Header.SerializedBindings.NumUniformBuffers);
		UsedUniformBufferSlots[PackedUB.Attribute.Index] = true;
		ParameterMap.AddParameterAllocation(*PackedUB.Attribute.Name, Header.SerializedBindings.NumUniformBuffers++, 0, 0);

		// Nothing else...
		//for (auto& Member : PackedUB.Members)
		//{
		//}
	}

	// Packed Uniform Buffers copy lists & setup sizes for each UB/Precision entry
	enum EFlattenUBState
	{
		Unknown,
		GroupedUBs,
		FlattenedUBs,
	};

	EFlattenUBState UBState = Unknown;

	for (auto& PackedUBCopy : CCHeader.PackedUBCopies)
	{
		CrossCompiler::FUniformBufferCopyInfo CopyInfo;
		CopyInfo.SourceUBIndex = PackedUBCopy.SourceUB;
		CopyInfo.SourceOffsetInFloats = PackedUBCopy.SourceOffset;
		CopyInfo.DestUBIndex = PackedUBCopy.DestUB;
		CopyInfo.DestUBTypeName = PackedUBCopy.DestPackedType;
		CopyInfo.DestUBTypeIndex = CrossCompiler::PackedTypeNameToTypeIndex(CopyInfo.DestUBTypeName);
		CopyInfo.DestOffsetInFloats = PackedUBCopy.DestOffset;
		CopyInfo.SizeInFloats = PackedUBCopy.Count;

		Header.UniformBuffersCopyInfo.Add(CopyInfo);

		auto& UniformBufferSize = PackedUniformBuffersSize.FindOrAdd(CopyInfo.DestUBIndex);
		uint16& Size = UniformBufferSize.FindOrAdd(CopyInfo.DestUBTypeName);
		Size = FMath::Max<uint16>(BytesPerComponent * (CopyInfo.DestOffsetInFloats + CopyInfo.SizeInFloats), Size);

		check(UBState == Unknown || UBState == GroupedUBs);
		UBState = GroupedUBs;
	}

	for (auto& PackedUBCopy : CCHeader.PackedUBGlobalCopies)
	{
		CrossCompiler::FUniformBufferCopyInfo CopyInfo;
		CopyInfo.SourceUBIndex = PackedUBCopy.SourceUB;
		CopyInfo.SourceOffsetInFloats = PackedUBCopy.SourceOffset;
		CopyInfo.DestUBIndex = PackedUBCopy.DestUB;
		CopyInfo.DestUBTypeName = PackedUBCopy.DestPackedType;
		CopyInfo.DestUBTypeIndex = CrossCompiler::PackedTypeNameToTypeIndex(CopyInfo.DestUBTypeName);
		CopyInfo.DestOffsetInFloats = PackedUBCopy.DestOffset;
		CopyInfo.SizeInFloats = PackedUBCopy.Count;

		Header.UniformBuffersCopyInfo.Add(CopyInfo);

		uint16& Size = PackedGlobalArraySize.FindOrAdd(CopyInfo.DestUBTypeName);
		Size = FMath::Max<uint16>(BytesPerComponent * (CopyInfo.DestOffsetInFloats + CopyInfo.SizeInFloats), Size);

		check(UBState == Unknown || UBState == FlattenedUBs);
		UBState = FlattenedUBs;
	}

	Header.SerializedBindings.bFlattenUB = (UBState == FlattenedUBs);

	// Setup Packed Array info
	Header.SerializedBindings.PackedGlobalArrays.Reserve(PackedGlobalArraySize.Num());
	for (auto Iterator = PackedGlobalArraySize.CreateIterator(); Iterator; ++Iterator)
	{
		ANSICHAR TypeName = Iterator.Key();
		uint16 Size = Iterator.Value();
		Size = (Size + 0xf) & (~0xf);
		CrossCompiler::FPackedArrayInfo Info;
		Info.Size = Size;
		Info.TypeName = TypeName;
		Info.TypeIndex = CrossCompiler::PackedTypeNameToTypeIndex(TypeName);
		Header.SerializedBindings.PackedGlobalArrays.Add(Info);
	}

	// Setup Packed Uniform Buffers info
	Header.SerializedBindings.PackedUniformBuffers.Reserve(PackedUniformBuffersSize.Num());
	for (auto Iterator = PackedUniformBuffersSize.CreateIterator(); Iterator; ++Iterator)
	{
		int BufferIndex = Iterator.Key();
		auto& ArraySizes = Iterator.Value();
		TArray<CrossCompiler::FPackedArrayInfo> InfoArray;
		InfoArray.Reserve(ArraySizes.Num());
		for (auto IterSizes = ArraySizes.CreateIterator(); IterSizes; ++IterSizes)
		{
			ANSICHAR TypeName = IterSizes.Key();
			uint16 Size = IterSizes.Value();
			Size = (Size + 0xf) & (~0xf);
			CrossCompiler::FPackedArrayInfo Info;
			Info.Size = Size;
			Info.TypeName = TypeName;
			Info.TypeIndex = CrossCompiler::PackedTypeNameToTypeIndex(TypeName);
			InfoArray.Add(Info);
		}

		Header.SerializedBindings.PackedUniformBuffers.Add(InfoArray);
	}

	// Then samplers.
	for (auto& Sampler : CCHeader.Samplers)
	{
		ParameterMap.AddParameterAllocation(
			*Sampler.Name,
			0,
			Sampler.Offset,
			Sampler.Count
			);

		Header.SerializedBindings.NumSamplers = FMath::Max<uint8>(
			Header.SerializedBindings.NumSamplers,
			Sampler.Offset + Sampler.Count
			);

		for (auto& SamplerState : Sampler.SamplerStates)
		{
			ParameterMap.AddParameterAllocation(
				*SamplerState,
				0,
				Sampler.Offset,
				Sampler.Count
				);
		}
	}

	// Then UAVs (images in GLSL)
	for (auto& UAV : CCHeader.UAVs)
	{
		ParameterMap.AddParameterAllocation(
			*UAV.Name,
			0,
			UAV.Offset,
			UAV.Count
			);

		Header.SerializedBindings.NumUAVs = FMath::Max<uint8>(
			Header.SerializedBindings.NumSamplers,
			UAV.Offset + UAV.Count
			);
	}

	// Lats make sure that there is some type of name visible
	Header.ShaderName = CCHeader.Name.Len() > 0 ? CCHeader.Name : DebugName;

	FSHA1::HashBuffer(USFSource, FCStringAnsi::Strlen(USFSource), (uint8*)&Header.SourceHash);

	// Build the SRT for this shader.
	{
		// Build the generic SRT for this shader.
		FShaderCompilerResourceTable GenericSRT;
		BuildResourceTableMapping(ShaderInput.Environment.ResourceTableMap, ShaderInput.Environment.ResourceTableLayoutHashes, UsedUniformBufferSlots, ShaderOutput.ParameterMap, /*MaxBoundResourceTable, */GenericSRT);

		// Copy over the bits indicating which resource tables are active.
		Header.SerializedBindings.ShaderResourceTable.ResourceTableBits = GenericSRT.ResourceTableBits;

		Header.SerializedBindings.ShaderResourceTable.ResourceTableLayoutHashes = GenericSRT.ResourceTableLayoutHashes;

		// Now build our token streams.
		BuildResourceTableTokenStream(GenericSRT.TextureMap, GenericSRT.MaxBoundResourceTable, Header.SerializedBindings.ShaderResourceTable.TextureMap);
		BuildResourceTableTokenStream(GenericSRT.ShaderResourceViewMap, GenericSRT.MaxBoundResourceTable, Header.SerializedBindings.ShaderResourceTable.ShaderResourceViewMap);
		BuildResourceTableTokenStream(GenericSRT.SamplerMap, GenericSRT.MaxBoundResourceTable, Header.SerializedBindings.ShaderResourceTable.SamplerMap);
		BuildResourceTableTokenStream(GenericSRT.UnorderedAccessViewMap, GenericSRT.MaxBoundResourceTable, Header.SerializedBindings.ShaderResourceTable.UnorderedAccessViewMap);
	}

	// Write out the header and shader source code.
	FMemoryWriter Ar(ShaderOutput.ShaderCode.GetWriteAccess(), true);
	Ar << Header;

	FVulkanShaderBindingTable ShaderBindingTable;
	GenerateBindingTable(Header.SerializedBindings, ShaderBindingTable);

	Ar << ShaderBindingTable;

	TArray<ANSICHAR> DebugNameArray;
	AppendCString(DebugNameArray, TCHAR_TO_ANSI(*DebugName));
	Ar << DebugNameArray;

	Ar << Spirv;

	TArray<ANSICHAR> GlslSourceArray;
	AppendCString(GlslSourceArray, InShaderSource);
	Ar << GlslSourceArray;

	// store data we can pickup later with ShaderCode.FindOptionalData('n'), could be removed for shipping
	// Daniel L: This GenerateShaderName does not generate a deterministic output among shaders as the shader code can be shared. 
	//			uncommenting this will cause the project to have non deterministic materials and will hurt patch sizes
	// ShaderOutput.ShaderCode.AddOptionalData('n', TCHAR_TO_UTF8(*ShaderInput.GenerateShaderName()));

	ShaderOutput.NumInstructions = 0;
	ShaderOutput.NumTextureSamplers = Header.SerializedBindings.NumSamplers;
	ShaderOutput.bSucceeded = true;
}

static void BuildShaderOutput(
	FShaderCompilerOutput& ShaderOutput,
	const FShaderCompilerInput& ShaderInput,
	const ANSICHAR* InShaderSource,
	int32 SourceLen,
	const FVulkanBindingTable& BindingTable,
	const ANSICHAR* InShaderSourceES,
	int32 SourceLenES,
	const FString& SPVFile,
	const FString& DebugName
	)
{
	TArray<uint8> Spirv;
	FFileHelper::LoadFileToArray(Spirv, *SPVFile);

	BuildShaderOutput(
		ShaderOutput,
		ShaderInput,
		InShaderSource,
		SourceLen,
		BindingTable,
		InShaderSourceES,
		SourceLenES,
		Spirv,
		DebugName
		);
}

static bool StringToFile(const FString& Filepath, const char* str)
{
	int32 StrLength = str ? FCStringAnsi::Strlen(str) : 0;

	if(StrLength == 0)
	{
		return false;
	}

	FArchive* FileWriter = IFileManager::Get().CreateFileWriter(*Filepath);
	if (FileWriter)
	{
		// const cast...
		FileWriter->Serialize((void*)str, StrLength+1);
		FileWriter->Close();
		delete FileWriter;
	}

	return true;
}

static char* PatchGLSLVersionPosition(const char* InSourceGLSL)
{
	if(!InSourceGLSL)
	{
		return nullptr;
	}

	const int32 InSrcLength = FCStringAnsi::Strlen(InSourceGLSL);
	if(InSrcLength <= 0)
	{
		return nullptr;
	}

	char* GlslSource = (char*)malloc(InSrcLength+1);
	check(GlslSource);
	memcpy(GlslSource, InSourceGLSL, InSrcLength+1);

	// Find begin of "#version" line
	char* VersionBegin = strstr(GlslSource, "#version");

	// Find end of "#version line"
	char* VersionEnd = VersionBegin ? strstr(VersionBegin, "\n") : nullptr;

	if(VersionEnd)
	{
		// Add '\n' character
		VersionEnd++;

		const int32 VersionLineLength = VersionEnd - VersionBegin - 1;

		// Copy version line into a temporary buffer (+1 for term-char).
		const int32 TmpStrBytes = (VersionEnd - VersionBegin) + 1;
		char* TmpVersionLine = (char*)malloc(TmpStrBytes);
		check(TmpVersionLine);
		memset(TmpVersionLine, 0, TmpStrBytes);
		memcpy(TmpVersionLine, VersionBegin, VersionEnd - VersionBegin);

		// Erase current version number, just replace it with spaces...
		for(char* str=VersionBegin; str<(VersionEnd-1); str++)
		{
			*str=' ';
		}

		// Allocate new source buffer to place version string on the first line.
		char* NewSource = (char*)malloc(InSrcLength + TmpStrBytes);
		check(NewSource);

		// Copy version line
		memcpy(NewSource, TmpVersionLine, TmpStrBytes);

		// Copy original source after the source line
		// -1 to offset back from the term-char
		memcpy(NewSource + TmpStrBytes - 1, GlslSource, InSrcLength + 1);

		free(TmpVersionLine);
		TmpVersionLine = nullptr;
							
		// Update string pointer
		free(GlslSource);
		GlslSource = NewSource;
	}

	return GlslSource;
}

static void PatchForToWhileLoop(char** InOutSourceGLSL)
{
	//checkf(InOutSourceGLSL, TEXT("Attempting to patch an invalid glsl source-string"));

	char* srcGlsl = *InOutSourceGLSL;
	//checkf(srcGlsl, TEXT("Attempting to patch an invalid glsl source-string"));

	const size_t InSrcLength = strlen(srcGlsl);
	//checkf(InSrcLength > 0, TEXT("Attempting to patch an empty glsl source-string."));

	// This is what we are relacing
	const char* srcPatchable = "for (;;)";
	const size_t srcPatchableLength = strlen(srcPatchable);

	// This is where we are replacing with
	const char* dstPatchable = "while(true)";
	const size_t dstPatchableLength = strlen(dstPatchable);
	
	// Find number of occurances
	int numNumberOfOccurances = 0;
	for(char* dstReplacePos = strstr(srcGlsl, srcPatchable);
		dstReplacePos != NULL;
		dstReplacePos = strstr(dstReplacePos+srcPatchableLength, srcPatchable))
	{
		numNumberOfOccurances++;
	}

	// No patching needed
	if(numNumberOfOccurances == 0)
	{
		return;
	}

	// Calc new required string-length
	const size_t newLength = InSrcLength + (dstPatchableLength-srcPatchableLength)*numNumberOfOccurances;

	// Allocate destination buffer + 1 char for terminating character
	char* GlslSource = (char*)malloc(newLength+1);
	check(GlslSource)
	memset(GlslSource, 0, sizeof(char)*(newLength+1));
	memcpy(GlslSource, srcGlsl, InSrcLength);

	// Scan and replace
	char* dstReplacePos = strstr(GlslSource, srcPatchable);
	char* srcReplacePos = strstr(srcGlsl, srcPatchable);
	int bytesRemaining = (int)newLength;
	while(dstReplacePos != NULL && srcReplacePos != NULL)
	{
		// Replace the string
		bytesRemaining = (int)newLength - (int)(dstReplacePos - GlslSource);
		memcpy(dstReplacePos, dstPatchable, dstPatchableLength);
		
		// Increment positions
		dstReplacePos+=dstPatchableLength;
		srcReplacePos+=srcPatchableLength;

		// Append remaining code
		int bytesToCopy = InSrcLength - (int)(srcReplacePos - srcGlsl);
		memcpy(dstReplacePos, srcReplacePos, bytesToCopy);

		dstReplacePos = strstr(dstReplacePos, srcPatchable);
		srcReplacePos = strstr(srcReplacePos, srcPatchable);
	}

	free(*InOutSourceGLSL);

	*InOutSourceGLSL = GlslSource;
}

static FString CreateShaderCompileCommandLine(FCompilerInfo& CompilerInfo, EHlslCompileTarget Target)
{
	//const FString OutputFileNoExt = FPaths::GetBaseFilename(OutputFile);
	FString CmdLine;
	FString GLSLFile = CompilerInfo.Input.DumpDebugInfoPath / (TEXT("Output") + GetExtension(CompilerInfo.Frequency));
	FString SPVFile = CompilerInfo.Input.DumpDebugInfoPath / TEXT("Output.spv");
	FString SPVDisasmFile = CompilerInfo.Input.DumpDebugInfoPath / TEXT("Output.spvasm");

	FString DumpedUSFFile = CompilerInfo.Input.DumpDebugInfoPath / (CompilerInfo.BaseSourceFilename + TEXT(".usf"));
	const TCHAR* VersionSwitch = TEXT("-esdeferred");
	switch (Target)
	{
	case HCT_FeatureLevelES3_1Ext:
	case HCT_FeatureLevelES3_1:
		VersionSwitch = TEXT("-vulkan");
		break;
	case HCT_FeatureLevelSM4:
		VersionSwitch = TEXT("-vulkansm4");
		break;
	case HCT_FeatureLevelSM5:
		VersionSwitch = TEXT("-vulkansm5");
		break;
	default:
		check(0);
	}
	CmdLine += CrossCompiler::CreateBatchFileContents(DumpedUSFFile, GLSLFile, CompilerInfo.Frequency, CompilerInfo.Input.EntryPointName, VersionSwitch, CompilerInfo.CCFlags);

	CmdLine += TEXT("\n\"");
	CmdLine += *(FPaths::RootDir() / TEXT("Engine/Binaries/ThirdParty/glslang/glslangValidator.exe"));
	CmdLine += TEXT("\"");
	CmdLine += TEXT(" -V -H -r -o \"") + SPVFile + TEXT("\" \"") + GLSLFile + TEXT("\" > \"" + SPVDisasmFile + "\"");
	CmdLine += TEXT("\npause\n");

	return CmdLine;
}


FCompilerInfo::FCompilerInfo(const FShaderCompilerInput& InInput, const FString& InWorkingDirectory, EHlslShaderFrequency InFrequency) :
	Input(InInput),
	WorkingDirectory(InWorkingDirectory),
	CCFlags(0),
	Frequency(InFrequency),
	bDebugDump(false)
{
	bDebugDump = Input.DumpDebugInfoPath != TEXT("") && IFileManager::Get().DirectoryExists(*Input.DumpDebugInfoPath);
	BaseSourceFilename = FPaths::GetBaseFilename(Input.SourceFilename);
}


/**
 * Compile a shader using the external shader compiler
 */
static void CompileUsingExternal(const struct FShaderCompilerInput& Input, struct FShaderCompilerOutput& Output, const class FString& WorkingDirectory, EVulkanShaderVersion Version)
{
	FString PreprocessedShader;
	FShaderCompilerDefinitions AdditionalDefines;
	EHlslCompileTarget HlslCompilerTarget = HCT_FeatureLevelES3_1Ext;
	EHlslCompileTarget HlslCompilerTargetES = HCT_FeatureLevelES3_1Ext;
	AdditionalDefines.SetDefine(TEXT("COMPILER_HLSLCC"), 1);
	if (Version == EVulkanShaderVersion::ES3_1 || Version == EVulkanShaderVersion::ES3_1_ANDROID || Version == EVulkanShaderVersion::ES3_1_UB)
	{
		HlslCompilerTarget = HCT_FeatureLevelES3_1Ext;
		HlslCompilerTargetES = HCT_FeatureLevelES3_1Ext;
		AdditionalDefines.SetDefine(TEXT("USE_LOWER_PRECISION"), 1);
		AdditionalDefines.SetDefine(TEXT("ES2_PROFILE"), 1);
	}
	else if (Version == EVulkanShaderVersion::SM4)
	{
		HlslCompilerTarget = HCT_FeatureLevelSM4;
		HlslCompilerTargetES = HCT_FeatureLevelSM4;
	}
	else if (Version == EVulkanShaderVersion::SM5)
	{
		HlslCompilerTarget = HCT_FeatureLevelSM5;
		HlslCompilerTargetES = HCT_FeatureLevelSM5;
	}
	AdditionalDefines.SetDefine(TEXT("row_major"), TEXT(""));
	AdditionalDefines.SetDefine(TEXT("VULKAN_PROFILE"), 1);

	FString DebugName = Input.DumpDebugInfoPath.Right(Input.DumpDebugInfoPath.Len() - Input.DumpDebugInfoRootPath.Len());

	const bool bDumpDebugInfo = (Input.DumpDebugInfoPath != TEXT("") && IFileManager::Get().DirectoryExists(*Input.DumpDebugInfoPath));

	AdditionalDefines.SetDefine(TEXT("COMPILER_SUPPORTS_ATTRIBUTES"), (uint32)1);

	const bool bUseFullPrecisionInPS = Input.Environment.CompilerFlags.Contains(CFLAG_UseFullPrecisionInPS);
	if (bUseFullPrecisionInPS)
	{
		AdditionalDefines.SetDefine(TEXT("FORCE_FLOATS"), (uint32)1);
	}

	auto DoPreprocess = [&]() -> bool
	{
		if (Input.bSkipPreprocessedCache)
		{
			return FFileHelper::LoadFileToString(PreprocessedShader, *Input.SourceFilename);
		}
		else
		{
			return PreprocessShader(PreprocessedShader, Output, Input, AdditionalDefines);
		}
	};

	if (DoPreprocess())
	{
		char* GlslShaderSource = NULL;
		char* ESShaderSource = NULL;
		char* ErrorLog = NULL;

		const bool bIsSM5 = (Version == EVulkanShaderVersion::SM5);

		const EHlslShaderFrequency FrequencyTable[] =
		{
			HSF_VertexShader,
			bIsSM5 ? HSF_HullShader : HSF_InvalidFrequency,
			bIsSM5 ? HSF_DomainShader : HSF_InvalidFrequency,
			HSF_PixelShader,
			bIsSM5 ? HSF_GeometryShader : HSF_InvalidFrequency,
			bIsSM5 ? HSF_ComputeShader : HSF_InvalidFrequency
		};

		const EHlslShaderFrequency Frequency = FrequencyTable[Input.Target.Frequency];
		if (Frequency == HSF_InvalidFrequency)
		{
			Output.bSucceeded = false;
			FShaderCompilerError* NewError = new(Output.Errors) FShaderCompilerError();
			NewError->StrippedErrorMessage = FString::Printf(
				TEXT("%s shaders not supported for use in Vulkan."),
				CrossCompiler::GetFrequencyName((EShaderFrequency)Input.Target.Frequency));
			return;
		}

		// This requires removing the HLSLCC_NoPreprocess flag later on!
		if (!RemoveUniformBuffersFromSource(PreprocessedShader))
		{
			return;
		}

		// Write out the preprocessed file and a batch file to compile it if requested (DumpDebugInfoPath is valid)
		if (bDumpDebugInfo && !Input.bSkipPreprocessedCache)
		{
			FArchive* FileWriter = IFileManager::Get().CreateFileWriter(*(Input.DumpDebugInfoPath / Input.SourceFilename + TEXT(".usf")));
			if (FileWriter)
			{
				auto AnsiSourceFile = StringCast<ANSICHAR>(*PreprocessedShader);
				FileWriter->Serialize((ANSICHAR*)AnsiSourceFile.Get(), AnsiSourceFile.Length());
				FileWriter->Close();
				delete FileWriter;
			}
		}

		uint32 CCFlags = 0;
		CCFlags |= HLSLCC_NoPreprocess;
		//if (!bIsSM5)
		{
			CCFlags |= HLSLCC_PackUniformsIntoUniformBuffers;
			CCFlags |= HLSLCC_FlattenUniformBuffers;
			CCFlags |= HLSLCC_SeparateShaderObjects;
		}
		//CCFlags |= HLSLCC_DX11ClipSpace;

		if (bUseFullPrecisionInPS)
		{
			CCFlags |= HLSLCC_UseFullPrecisionInPS;
		}

		// Required as we added the RemoveUniformBuffersFromSource() function (the cross-compiler won't be able to interpret comments w/o a preprocessor)
		CCFlags &= ~HLSLCC_NoPreprocess;

		// ES SL doesn't support origin layout
		uint32 CCFlagsES = CCFlags | HLSLCC_DX11ClipSpace;

		FVulkanBindingTable BindingTableES(Frequency);

		FVulkanCodeBackend VulkanBackendES(CCFlagsES, BindingTableES, HlslCompilerTargetES);
		FVulkanLanguageSpec VulkanLanguageSpec(false);

		int32 Result = 0;
		if (!bIsSM5)
		{
			FHlslCrossCompilerContext CrossCompilerContextES(CCFlagsES, Frequency, HlslCompilerTargetES);
			if (CrossCompilerContextES.Init(TCHAR_TO_ANSI(*Input.SourceFilename), &VulkanLanguageSpec))
			{
				Result = CrossCompilerContextES.Run(
					TCHAR_TO_ANSI(*PreprocessedShader),
					TCHAR_TO_ANSI(*Input.EntryPointName),
					&VulkanBackendES,
					&ESShaderSource,
					&ErrorLog
					) ? 1 : 0;
			}

			if (Result != 0)
			{
				if (bDumpDebugInfo)
				{
					int32 ESSourceLen = ESShaderSource ? FCStringAnsi::Strlen(ESShaderSource) : 0;
					const FString ESFile = (Input.DumpDebugInfoPath / TEXT("Output_ES") + GetExtension(Frequency));

					StringToFile(ESFile, ESShaderSource);
				}
			}
		}

		FVulkanBindingTable BindingTable(Frequency);
		FVulkanCodeBackend VulkanBackend(CCFlags, BindingTable, HlslCompilerTarget);

		FHlslCrossCompilerContext CrossCompilerContext(CCFlags, Frequency, HlslCompilerTarget);
		if (CrossCompilerContext.Init(TCHAR_TO_ANSI(*Input.SourceFilename), &VulkanLanguageSpec))
		{
			Result = CrossCompilerContext.Run(
				TCHAR_TO_ANSI(*PreprocessedShader),
				TCHAR_TO_ANSI(*Input.EntryPointName),
				&VulkanBackend,
				&GlslShaderSource,
				&ErrorLog
				) ? 1 : 0;
		}

		if (Result != 0)
		{
			int32 GlslSourceLen = GlslShaderSource ? FCStringAnsi::Strlen(GlslShaderSource) : 0;

			// If no GLSL file is generated, we cannot generate SPIR-V, there for we have to generate an error
			if (GlslSourceLen > 0)
			{
				FString GLSLFile;
				if (bDumpDebugInfo)
				{
					GLSLFile = (Input.DumpDebugInfoPath / TEXT("Output") + GetExtension(Frequency));

					// Store unchanged GLSL source
					StringToFile(GLSLFile, GlslShaderSource);
				}

				// Convert GLSL to SPIRV
				{
					// In order to convert GLSL to SPIRV, we need to bypass the signature and set the version on the first line.
					// This is probably glsl to spriv converter bug, since it only occurs some versions of glsl.

					// Patch GLSL source
					char* PatchedGlslSource = PatchGLSLVersionPosition(GlslShaderSource);
					check(PatchedGlslSource);
					PatchForToWhileLoop(&PatchedGlslSource);

					FString SPVFile;
					if (bDumpDebugInfo)
					{
						// Change output file name to patched filename and store
						GLSLFile = (Input.DumpDebugInfoPath / TEXT("patched") + GetExtension(Frequency));
						if (GLSLFile.Len() >= MAX_PATH)
						{
							FShaderCompilerError* Error = new(Output.Errors) FShaderCompilerError();
							Error->ErrorLineString = FString::Printf(TEXT("Filepath exeeding %d characters: "), MAX_PATH, *GLSLFile);
							Output.bSucceeded = false;

							if (PatchedGlslSource)
							{
								free(PatchedGlslSource);
							}
							if (ESShaderSource)
							{
								free(ESShaderSource);
							}
							if (GlslShaderSource)
							{
								free(GlslShaderSource);
							}
							if (ErrorLog)
							{
								free(ErrorLog);
							}
							return;
						}

						StringToFile(GLSLFile, PatchedGlslSource);
						free(PatchedGlslSource);

						SPVFile = Input.DumpDebugInfoPath / GetExtension(Frequency, false) + TEXT(".spv");
					}
					else
					{
						FString WorkDirectory = *(FPaths::Combine(*(FPaths::ConvertRelativePathToFull(*FPaths::GameSavedDir())), TEXT("VulkanShaderWork")));

						GLSLFile = FPaths::CreateTempFilename(*WorkDirectory, TEXT("VSW"), *(TEXT(".") + GetExtension(Frequency, false)));
						StringToFile(GLSLFile, PatchedGlslSource);
						free(PatchedGlslSource);

						SPVFile = *(FPaths::GetBaseFilename(GLSLFile, false) + TEXT(".spv"));
					}

					int32 ReturnCode = 0;
					FString Out, Err;
					const FString ConverterToolPath = *(FPaths::RootDir() / TEXT("Engine/Binaries/ThirdParty/glslang/glslangValidator.exe"));
					const FString InputArguments = TEXT(" -V -H -r -o \"") + SPVFile + TEXT("\" \"") + GLSLFile + TEXT("\"");
					bool bResult = FPlatformProcess::ExecProcess(*ConverterToolPath, *InputArguments, &ReturnCode, &Out, &Err);

					if (bResult && FPaths::FileExists(SPVFile))
					{
						if (bDumpDebugInfo)
						{
							FArchive* FileWriter = IFileManager::Get().CreateFileWriter(*(SPVFile + TEXT("asm")));	// So it becomes *.spvasm
							if (FileWriter)
							{
								FileWriter->Serialize(TCHAR_TO_ANSI(*Out), Out.Len() + 1);
								FileWriter->Close();
								delete FileWriter;
							}
						}

						Output.Target = Input.Target;
						BuildShaderOutput(Output, Input, 
							GlslShaderSource, GlslSourceLen, 
							BindingTable, 
							ESShaderSource, ESShaderSource ? FCStringAnsi::Strlen(ESShaderSource) : 0 , 
							SPVFile, DebugName);

						if (!bDumpDebugInfo)
						{
							IFileManager::Get().Delete(*SPVFile, false, true);
						}
					}
					else
					{
						FShaderCompilerError* Error = new(Output.Errors) FShaderCompilerError();
						Error->ErrorLineString = Out;
						Output.bSucceeded = false;
					}

					if (!bDumpDebugInfo)
					{
						IFileManager::Get().Delete(*GLSLFile, false, true);
					}
				}
			}
			else
			{
				FShaderCompilerError* Error = new(Output.Errors) FShaderCompilerError();
				if (bDumpDebugInfo)
				{
					Error->ErrorLineString = FString::Printf(TEXT("No GLSL code generated for SPIR-V conversion. Shader: '%s'"), *Input.DumpDebugInfoPath);
				}
				else
				{
					Error->ErrorLineString = FString::Printf(TEXT("No GLSL code generated for SPIR-V conversion. Shader: '%s'"), *DebugName);
				}
				Output.bSucceeded = false;
			}
		}
		else
		{
			FString Tmp = ANSI_TO_TCHAR(ErrorLog);
			TArray<FString> ErrorLines;
			Tmp.ParseIntoArray(ErrorLines, TEXT("\n"), true);
			for (int32 LineIndex = 0; LineIndex < ErrorLines.Num(); ++LineIndex)
			{
				const FString& Line = ErrorLines[LineIndex];
				CrossCompiler::ParseHlslccError(Output.Errors, Line);
			}
		}

		if (ESShaderSource)
		{
			free(ESShaderSource);
		}
		if (GlslShaderSource)
		{
			free(GlslShaderSource);
		}
		if (ErrorLog)
		{
			free(ErrorLog);
		}
	}
}

/**
 * Compile a shader using the internal shader compiling library
 */
static void CompileUsingInternal(FCompilerInfo& CompilerInfo, FVulkanBindingTable& BindingTable, TArray<ANSICHAR>& GlslSource, FString& EntryPointName, FShaderCompilerOutput& Output)
{
	FString Errors;
	TArray<uint8> Spirv;
	if (GenerateSpirv(GlslSource.GetData(), CompilerInfo, Errors, CompilerInfo.Input.DumpDebugInfoPath, Spirv))
	{
		FString DebugName = CompilerInfo.Input.DumpDebugInfoPath.Right(CompilerInfo.Input.DumpDebugInfoPath.Len() - CompilerInfo.Input.DumpDebugInfoRootPath.Len());

		Output.Target = CompilerInfo.Input.Target;
		BuildShaderOutput(Output, CompilerInfo.Input,
			GlslSource.GetData(), GlslSource.Num(),
			BindingTable, nullptr, 0, Spirv, DebugName);
	}
	else
	{
		if (Errors.Len() > 0)
		{
			FShaderCompilerError* Error = new(Output.Errors) FShaderCompilerError();
			Error->ErrorLineString = Errors;
		}
	}
}


static bool CallHlslcc(const FString& PreprocessedShader, FVulkanBindingTable& BindingTable, FCompilerInfo& CompilerInfo, FString& EntryPointName, EHlslCompileTarget HlslCompilerTarget, FShaderCompilerOutput& Output, TArray<ANSICHAR>& OutGlsl)
{
	char* GlslShaderSource = nullptr;
	char* ErrorLog = nullptr;

	auto InnerFunction = [&]()
	{
		// Call hlslcc
		FVulkanCodeBackend VulkanBackend(CompilerInfo.CCFlags, BindingTable, HlslCompilerTarget);
		FHlslCrossCompilerContext CrossCompilerContext(CompilerInfo.CCFlags, CompilerInfo.Frequency, HlslCompilerTarget);
		const bool bShareSamplers = false;
		FVulkanLanguageSpec VulkanLanguageSpec(bShareSamplers);
		int32 Result = 0;
		if (CrossCompilerContext.Init(TCHAR_TO_ANSI(*CompilerInfo.Input.SourceFilename), &VulkanLanguageSpec))
		{
			Result = CrossCompilerContext.Run(
				TCHAR_TO_ANSI(*PreprocessedShader),
				TCHAR_TO_ANSI(*EntryPointName),
				&VulkanBackend,
				&GlslShaderSource,
				&ErrorLog
				) ? 1 : 0;
		}

		if (Result == 0)
		{
			FString Tmp = ANSI_TO_TCHAR(ErrorLog);
			TArray<FString> ErrorLines;
			Tmp.ParseIntoArray(ErrorLines, TEXT("\n"), true);
			for (int32 LineIndex = 0; LineIndex < ErrorLines.Num(); ++LineIndex)
			{
				const FString& Line = ErrorLines[LineIndex];
				CrossCompiler::ParseHlslccError(Output.Errors, Line);
			}

			return false;
		}

		check(GlslShaderSource);

		// Patch GLSL source
		PatchForToWhileLoop(&GlslShaderSource);

		if (CompilerInfo.bDebugDump)
		{
			FString DumpedGlslFile = CompilerInfo.Input.DumpDebugInfoPath / (TEXT("Output") + GetExtension(CompilerInfo.Frequency));
			FArchive* FileWriter = IFileManager::Get().CreateFileWriter(*DumpedGlslFile);
			if (FileWriter)
			{
				FileWriter->Serialize(GlslShaderSource, FCStringAnsi::Strlen(GlslShaderSource));
				FileWriter->Close();
				delete FileWriter;
			}
		}

		int32 Length = FCStringAnsi::Strlen(GlslShaderSource);
		OutGlsl.AddUninitialized(Length + 1);
		FCStringAnsi::Strcpy(OutGlsl.GetData(), Length + 1, GlslShaderSource);

		return true;
	};

	bool bResult = InnerFunction();

	if (ErrorLog)
	{
		free(ErrorLog);
	}

	if (GlslShaderSource)
	{
		free(GlslShaderSource);
	}

	return bResult;
}


void CompileShader_Windows_Vulkan(const FShaderCompilerInput& Input, FShaderCompilerOutput& Output, const class FString& WorkingDirectory, EVulkanShaderVersion Version)
{
	check(IsVulkanPlatform((EShaderPlatform)Input.Target.Platform));

	//if (GUseExternalShaderCompiler)
	//{
	//	// Old path...
	//	CompileUsingExternal(Input, Output, WorkingDirectory, Version);
	//	return;
	//}

	const bool bIsSM5 = (Version == EVulkanShaderVersion::SM5);
	const bool bIsSM4 = (Version == EVulkanShaderVersion::SM4);

	const EHlslShaderFrequency FrequencyTable[] =
	{
		HSF_VertexShader,
		bIsSM5 ? HSF_HullShader : HSF_InvalidFrequency,
		bIsSM5 ? HSF_DomainShader : HSF_InvalidFrequency,
		HSF_PixelShader,
		(bIsSM4 || bIsSM5) ? HSF_GeometryShader : HSF_InvalidFrequency,
		bIsSM5 ? HSF_ComputeShader : HSF_InvalidFrequency
	};

	const EHlslShaderFrequency Frequency = FrequencyTable[Input.Target.Frequency];
	if (Frequency == HSF_InvalidFrequency)
	{
		Output.bSucceeded = false;
		FShaderCompilerError* NewError = new(Output.Errors) FShaderCompilerError();
		NewError->StrippedErrorMessage = FString::Printf(
			TEXT("%s shaders not supported for use in Vulkan."),
			CrossCompiler::GetFrequencyName((EShaderFrequency)Input.Target.Frequency));
		return;
	}

	FString PreprocessedShader;
	FShaderCompilerDefinitions AdditionalDefines;
	EHlslCompileTarget HlslCompilerTarget = HCT_FeatureLevelES3_1Ext;
	EHlslCompileTarget HlslCompilerTargetES = HCT_FeatureLevelES3_1Ext;
	AdditionalDefines.SetDefine(TEXT("COMPILER_HLSLCC"), 1);
	if (Version == EVulkanShaderVersion::ES3_1 || Version == EVulkanShaderVersion::ES3_1_ANDROID || Version == EVulkanShaderVersion::ES3_1_UB)
	{
		HlslCompilerTarget = HCT_FeatureLevelES3_1Ext;
		HlslCompilerTargetES = HCT_FeatureLevelES3_1Ext;
		AdditionalDefines.SetDefine(TEXT("USE_LOWER_PRECISION"), 1);
		AdditionalDefines.SetDefine(TEXT("ES2_PROFILE"), 1);
		AdditionalDefines.SetDefine(TEXT("VULKAN_PROFILE"), 1);
	}
	else if (Version == EVulkanShaderVersion::SM4)
	{
		HlslCompilerTarget = HCT_FeatureLevelSM4;
		HlslCompilerTargetES = HCT_FeatureLevelSM4;
		AdditionalDefines.SetDefine(TEXT("VULKAN_PROFILE_SM4"), 1);
	}
	else if (Version == EVulkanShaderVersion::SM5)
	{
		HlslCompilerTarget = HCT_FeatureLevelSM5;
		HlslCompilerTargetES = HCT_FeatureLevelSM5;
		AdditionalDefines.SetDefine(TEXT("VULKAN_PROFILE_SM5"), 1);
	}
	AdditionalDefines.SetDefine(TEXT("row_major"), TEXT(""));

	AdditionalDefines.SetDefine(TEXT("COMPILER_SUPPORTS_ATTRIBUTES"), (uint32)1);

	const bool bUseFullPrecisionInPS = Input.Environment.CompilerFlags.Contains(CFLAG_UseFullPrecisionInPS);
	if (bUseFullPrecisionInPS)
	{
		AdditionalDefines.SetDefine(TEXT("FORCE_FLOATS"), (uint32)1);
	}

	//#todo-rco: Glslang doesn't allow this yet
	AdditionalDefines.SetDefine(TEXT("noperspective"), TEXT(""));

	// Preprocess the shader.
	FString PreprocessedShaderSource;
	if (Input.bSkipPreprocessedCache)
	{
		if (!FFileHelper::LoadFileToString(PreprocessedShaderSource, *Input.SourceFilename))
		{
			return;
		}
	}
	else
	{
		if (!PreprocessShader(PreprocessedShaderSource, Output, Input, AdditionalDefines))
		{
			// The preprocessing stage will add any relevant errors.
			return;
		}

		// Disable instanced stereo until supported for Vulkan
		StripInstancedStereo(PreprocessedShaderSource);
	}

	FString EntryPointName = Input.EntryPointName;

	if (!RemoveUniformBuffersFromSource(PreprocessedShaderSource))
	{
		return;
	}

	{
		// Tiny helper when debugging issues on glslang
		static bool bRemoveHashLine = false;
		if (bRemoveHashLine)
		{
			PreprocessedShaderSource = PreprocessedShaderSource.Replace(TEXT("#line"), TEXT("///#line"), ESearchCase::CaseSensitive);
		}
	}

	FCompilerInfo CompilerInfo(Input, WorkingDirectory, Frequency);

	CompilerInfo.CCFlags |= HLSLCC_PackUniforms;
	CompilerInfo.CCFlags |= HLSLCC_PackUniformsIntoUniformBuffers;
	//#todo-rco: All version using packed currently
	//if (Version == EVulkanShaderVersion::ES3_1 || Version == EVulkanShaderVersion::ES3_1_ANDROID)
	{
		CompilerInfo.CCFlags |= HLSLCC_FlattenUniformBuffers;
	}

	if (bUseFullPrecisionInPS)
	{
		CompilerInfo.CCFlags |= HLSLCC_UseFullPrecisionInPS;
	}

	CompilerInfo.CCFlags |= HLSLCC_SeparateShaderObjects;

	// ES doesn't support origin layout
	CompilerInfo.CCFlags |= HLSLCC_DX11ClipSpace;

	// Required as we added the RemoveUniformBuffersFromSource() function (the cross-compiler won't be able to interpret comments w/o a preprocessor)
	CompilerInfo.CCFlags &= ~HLSLCC_NoPreprocess;

	// Write out the preprocessed file and a batch file to compile it if requested (DumpDebugInfoPath is valid)
	if (CompilerInfo.bDebugDump)
	{
		FString DumpedUSFFile = CompilerInfo.Input.DumpDebugInfoPath / (CompilerInfo.BaseSourceFilename + TEXT(".usf"));
		FArchive* FileWriter = IFileManager::Get().CreateFileWriter(*DumpedUSFFile);
		if (FileWriter)
		{
			auto AnsiSourceFile = StringCast<ANSICHAR>(*PreprocessedShaderSource);
			FileWriter->Serialize((ANSICHAR*)AnsiSourceFile.Get(), AnsiSourceFile.Length());
			FileWriter->Close();
			delete FileWriter;
		}

		const FString BatchFileContents = CreateShaderCompileCommandLine(CompilerInfo, HlslCompilerTarget);
		FFileHelper::SaveStringToFile(BatchFileContents, *(CompilerInfo.Input.DumpDebugInfoPath / TEXT("CompileSPIRV.bat")));

		if (Input.bGenerateDirectCompileFile)
		{
			FFileHelper::SaveStringToFile(CreateShaderCompilerWorkerDirectCommandLine(Input), *(Input.DumpDebugInfoPath / TEXT("DirectCompile.txt")));
		}
	}

	TArray<ANSICHAR> GeneratedGlslSource;
	FVulkanBindingTable BindingTable(CompilerInfo.Frequency);
	if (CallHlslcc(PreprocessedShaderSource, BindingTable, CompilerInfo, EntryPointName, HlslCompilerTarget, Output, GeneratedGlslSource))
	{
		//#todo-rco: Once it's all cleaned up...
		//if (GUseExternalShaderCompiler)
		//{
		//	CompileUsingExternal(CompilerInfo, BindingTable, GeneratedGlslSource, EntryPointName, Output);
		//}
		//else
		{
			// For debugging; if you hit an error from Glslang/Spirv, use the SourceNoHeader for line numbers
			auto* SourceWithHeader = GeneratedGlslSource.GetData();
			char* SourceNoHeader = strstr(SourceWithHeader, "#version");
			CompileUsingInternal(CompilerInfo, BindingTable, GeneratedGlslSource, EntryPointName, Output);
		}
	}
}
