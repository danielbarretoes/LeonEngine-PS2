/*===========================================================================
	Generated code exported from LeonHeaderTool.
	DO NOT modify this manually! Edit the corresponding .h files instead!
===========================================================================*/

#include "UObject/GeneratedCppIncludes.h"
#include "NoExportTypes.h"
PRAGMA_DISABLE_DEPRECATION_WARNINGS
// Cross Module References
	LHTTEST_API UScriptStruct* Z_Construct_UScriptStruct_FVector();
	LHTTEST_API UScriptStruct* Z_Construct_UScriptStruct_FQuat();
	LHTTEST_API UScriptStruct* Z_Construct_UScriptStruct_FPlane();
	LHTTEST_API UScriptStruct* Z_Construct_UScriptStruct_FTransform();
	LHTTEST_API UScriptStruct* Z_Construct_UScriptStruct_FUsesNoExport();
	UPackage* Z_Construct_UPackage__Script_LhtTest();
// End Cross Module References
	static class UScriptStruct* FVector_StaticStruct()
	{
		static class UScriptStruct* Singleton = nullptr;
		if (!Singleton)
		{
			Singleton = GetStaticStruct(Z_Construct_UScriptStruct_FVector, Z_Construct_UPackage__Script_LhtTest(), TEXT("Vector"), sizeof(FVector), 0);
		}
		return Singleton;
	}
	template<> LHTTEST_API UScriptStruct* StaticStruct<FVector>()
	{
		return FVector_StaticStruct();
	}
	struct Z_Construct_UScriptStruct_FVector_Statics
	{
		static void* NewStructOps();
		/** The NoExport declaration of FVector, checked against the C++ type. */
		struct FNoExportLayout
		{
			float X;
			float Y;
			float Z;
		};
		static_assert(sizeof(FNoExportLayout) == sizeof(FVector), "NoExport FVector does not match the size of the C++ type");
		static_assert(TIsSame<decltype(FVector::X), float>::Value, "NoExport FVector::X does not match the type of the C++ member");
		static_assert(STRUCT_OFFSET(FNoExportLayout, X) == STRUCT_OFFSET(FVector, X), "NoExport FVector::X does not match the offset of the C++ member");
		static_assert(TIsSame<decltype(FVector::Y), float>::Value, "NoExport FVector::Y does not match the type of the C++ member");
		static_assert(STRUCT_OFFSET(FNoExportLayout, Y) == STRUCT_OFFSET(FVector, Y), "NoExport FVector::Y does not match the offset of the C++ member");
		static_assert(TIsSame<decltype(FVector::Z), float>::Value, "NoExport FVector::Z does not match the type of the C++ member");
		static_assert(STRUCT_OFFSET(FNoExportLayout, Z) == STRUCT_OFFSET(FVector, Z), "NoExport FVector::Z does not match the offset of the C++ member");
		static const UE4CodeGen_Private::FFloatPropertyParams NewProp_X;
		static const UE4CodeGen_Private::FFloatPropertyParams NewProp_Y;
		static const UE4CodeGen_Private::FFloatPropertyParams NewProp_Z;
		static const UE4CodeGen_Private::FPropertyParamsBase* const PropPointers[];
		static const UE4CodeGen_Private::FStructParams ReturnStructParams;
	};
	void* Z_Construct_UScriptStruct_FVector_Statics::NewStructOps()
	{
		return (UScriptStruct::ICppStructOps*)new UScriptStruct::TCppStructOps<FVector>();
	}
	const UE4CodeGen_Private::FFloatPropertyParams Z_Construct_UScriptStruct_FVector_Statics::NewProp_X = { "X", nullptr, (EPropertyFlags)0x0010000001000001, UE4CodeGen_Private::EPropertyGenFlags::Float, RF_Public|RF_Transient|RF_MarkAsNative, 1, STRUCT_OFFSET(FVector, X) };
	const UE4CodeGen_Private::FFloatPropertyParams Z_Construct_UScriptStruct_FVector_Statics::NewProp_Y = { "Y", nullptr, (EPropertyFlags)0x0010000001000001, UE4CodeGen_Private::EPropertyGenFlags::Float, RF_Public|RF_Transient|RF_MarkAsNative, 1, STRUCT_OFFSET(FVector, Y) };
	const UE4CodeGen_Private::FFloatPropertyParams Z_Construct_UScriptStruct_FVector_Statics::NewProp_Z = { "Z", nullptr, (EPropertyFlags)0x0010000001000001, UE4CodeGen_Private::EPropertyGenFlags::Float, RF_Public|RF_Transient|RF_MarkAsNative, 1, STRUCT_OFFSET(FVector, Z) };
	const UE4CodeGen_Private::FPropertyParamsBase* const Z_Construct_UScriptStruct_FVector_Statics::PropPointers[] = {
		(const UE4CodeGen_Private::FPropertyParamsBase*)&Z_Construct_UScriptStruct_FVector_Statics::NewProp_X,
		(const UE4CodeGen_Private::FPropertyParamsBase*)&Z_Construct_UScriptStruct_FVector_Statics::NewProp_Y,
		(const UE4CodeGen_Private::FPropertyParamsBase*)&Z_Construct_UScriptStruct_FVector_Statics::NewProp_Z,
	};
	const UE4CodeGen_Private::FStructParams Z_Construct_UScriptStruct_FVector_Statics::ReturnStructParams = {
		(UObject* (*)())Z_Construct_UPackage__Script_LhtTest,
		nullptr,
		&NewStructOps,
		"Vector",
		sizeof(FVector),
		alignof(FVector),
		Z_Construct_UScriptStruct_FVector_Statics::PropPointers,
		UE_ARRAY_COUNT(Z_Construct_UScriptStruct_FVector_Statics::PropPointers),
		RF_Public|RF_Transient|RF_MarkAsNative,
		EStructFlags(0x00000039),
	};
	UScriptStruct* Z_Construct_UScriptStruct_FVector()
	{
		static UScriptStruct* ReturnStruct = nullptr;
		if (!ReturnStruct)
		{
			UE4CodeGen_Private::ConstructUScriptStruct(ReturnStruct, Z_Construct_UScriptStruct_FVector_Statics::ReturnStructParams);
		}
		return ReturnStruct;
	}
	static class UScriptStruct* FQuat_StaticStruct()
	{
		static class UScriptStruct* Singleton = nullptr;
		if (!Singleton)
		{
			Singleton = GetStaticStruct(Z_Construct_UScriptStruct_FQuat, Z_Construct_UPackage__Script_LhtTest(), TEXT("Quat"), sizeof(FQuat), 0);
		}
		return Singleton;
	}
	template<> LHTTEST_API UScriptStruct* StaticStruct<FQuat>()
	{
		return FQuat_StaticStruct();
	}
	struct Z_Construct_UScriptStruct_FQuat_Statics
	{
		static void* NewStructOps();
		/** The NoExport declaration of FQuat, checked against the C++ type. */
		struct FNoExportLayout
		{
			float X;
			float Y;
			float Z;
			float W;
		};
		static_assert(sizeof(FNoExportLayout) == sizeof(FQuat), "NoExport FQuat does not match the size of the C++ type");
		static_assert(TIsSame<decltype(FQuat::X), float>::Value, "NoExport FQuat::X does not match the type of the C++ member");
		static_assert(STRUCT_OFFSET(FNoExportLayout, X) == STRUCT_OFFSET(FQuat, X), "NoExport FQuat::X does not match the offset of the C++ member");
		static_assert(TIsSame<decltype(FQuat::Y), float>::Value, "NoExport FQuat::Y does not match the type of the C++ member");
		static_assert(STRUCT_OFFSET(FNoExportLayout, Y) == STRUCT_OFFSET(FQuat, Y), "NoExport FQuat::Y does not match the offset of the C++ member");
		static_assert(TIsSame<decltype(FQuat::Z), float>::Value, "NoExport FQuat::Z does not match the type of the C++ member");
		static_assert(STRUCT_OFFSET(FNoExportLayout, Z) == STRUCT_OFFSET(FQuat, Z), "NoExport FQuat::Z does not match the offset of the C++ member");
		static_assert(TIsSame<decltype(FQuat::W), float>::Value, "NoExport FQuat::W does not match the type of the C++ member");
		static_assert(STRUCT_OFFSET(FNoExportLayout, W) == STRUCT_OFFSET(FQuat, W), "NoExport FQuat::W does not match the offset of the C++ member");
		static const UE4CodeGen_Private::FFloatPropertyParams NewProp_X;
		static const UE4CodeGen_Private::FFloatPropertyParams NewProp_Y;
		static const UE4CodeGen_Private::FFloatPropertyParams NewProp_Z;
		static const UE4CodeGen_Private::FFloatPropertyParams NewProp_W;
		static const UE4CodeGen_Private::FPropertyParamsBase* const PropPointers[];
		static const UE4CodeGen_Private::FStructParams ReturnStructParams;
	};
	void* Z_Construct_UScriptStruct_FQuat_Statics::NewStructOps()
	{
		return (UScriptStruct::ICppStructOps*)new UScriptStruct::TCppStructOps<FQuat>();
	}
	const UE4CodeGen_Private::FFloatPropertyParams Z_Construct_UScriptStruct_FQuat_Statics::NewProp_X = { "X", nullptr, (EPropertyFlags)0x0010000000000000, UE4CodeGen_Private::EPropertyGenFlags::Float, RF_Public|RF_Transient|RF_MarkAsNative, 1, STRUCT_OFFSET(FQuat, X) };
	const UE4CodeGen_Private::FFloatPropertyParams Z_Construct_UScriptStruct_FQuat_Statics::NewProp_Y = { "Y", nullptr, (EPropertyFlags)0x0010000000000000, UE4CodeGen_Private::EPropertyGenFlags::Float, RF_Public|RF_Transient|RF_MarkAsNative, 1, STRUCT_OFFSET(FQuat, Y) };
	const UE4CodeGen_Private::FFloatPropertyParams Z_Construct_UScriptStruct_FQuat_Statics::NewProp_Z = { "Z", nullptr, (EPropertyFlags)0x0010000000000000, UE4CodeGen_Private::EPropertyGenFlags::Float, RF_Public|RF_Transient|RF_MarkAsNative, 1, STRUCT_OFFSET(FQuat, Z) };
	const UE4CodeGen_Private::FFloatPropertyParams Z_Construct_UScriptStruct_FQuat_Statics::NewProp_W = { "W", nullptr, (EPropertyFlags)0x0010000000000000, UE4CodeGen_Private::EPropertyGenFlags::Float, RF_Public|RF_Transient|RF_MarkAsNative, 1, STRUCT_OFFSET(FQuat, W) };
	const UE4CodeGen_Private::FPropertyParamsBase* const Z_Construct_UScriptStruct_FQuat_Statics::PropPointers[] = {
		(const UE4CodeGen_Private::FPropertyParamsBase*)&Z_Construct_UScriptStruct_FQuat_Statics::NewProp_X,
		(const UE4CodeGen_Private::FPropertyParamsBase*)&Z_Construct_UScriptStruct_FQuat_Statics::NewProp_Y,
		(const UE4CodeGen_Private::FPropertyParamsBase*)&Z_Construct_UScriptStruct_FQuat_Statics::NewProp_Z,
		(const UE4CodeGen_Private::FPropertyParamsBase*)&Z_Construct_UScriptStruct_FQuat_Statics::NewProp_W,
	};
	const UE4CodeGen_Private::FStructParams Z_Construct_UScriptStruct_FQuat_Statics::ReturnStructParams = {
		(UObject* (*)())Z_Construct_UPackage__Script_LhtTest,
		nullptr,
		&NewStructOps,
		"Quat",
		sizeof(FQuat),
		alignof(FQuat),
		Z_Construct_UScriptStruct_FQuat_Statics::PropPointers,
		UE_ARRAY_COUNT(Z_Construct_UScriptStruct_FQuat_Statics::PropPointers),
		RF_Public|RF_Transient|RF_MarkAsNative,
		EStructFlags(0x00000039),
	};
	UScriptStruct* Z_Construct_UScriptStruct_FQuat()
	{
		static UScriptStruct* ReturnStruct = nullptr;
		if (!ReturnStruct)
		{
			UE4CodeGen_Private::ConstructUScriptStruct(ReturnStruct, Z_Construct_UScriptStruct_FQuat_Statics::ReturnStructParams);
		}
		return ReturnStruct;
	}
	static class UScriptStruct* FPlane_StaticStruct()
	{
		static class UScriptStruct* Singleton = nullptr;
		if (!Singleton)
		{
			Singleton = GetStaticStruct(Z_Construct_UScriptStruct_FPlane, Z_Construct_UPackage__Script_LhtTest(), TEXT("Plane"), sizeof(FPlane), 0);
		}
		return Singleton;
	}
	template<> LHTTEST_API UScriptStruct* StaticStruct<FPlane>()
	{
		return FPlane_StaticStruct();
	}
	struct Z_Construct_UScriptStruct_FPlane_Statics
	{
		static void* NewStructOps();
		/** The NoExport declaration of FPlane, checked against the C++ type. */
		struct FNoExportLayout : FVector
		{
			float W;
		};
		static_assert(sizeof(FNoExportLayout) == sizeof(FPlane), "NoExport FPlane does not match the size of the C++ type");
		static_assert(TIsSame<decltype(FPlane::W), float>::Value, "NoExport FPlane::W does not match the type of the C++ member");
		static_assert(STRUCT_OFFSET(FNoExportLayout, W) == STRUCT_OFFSET(FPlane, W), "NoExport FPlane::W does not match the offset of the C++ member");
		static const UE4CodeGen_Private::FFloatPropertyParams NewProp_W;
		static const UE4CodeGen_Private::FPropertyParamsBase* const PropPointers[];
		static const UE4CodeGen_Private::FStructParams ReturnStructParams;
	};
	void* Z_Construct_UScriptStruct_FPlane_Statics::NewStructOps()
	{
		return (UScriptStruct::ICppStructOps*)new UScriptStruct::TCppStructOps<FPlane>();
	}
	const UE4CodeGen_Private::FFloatPropertyParams Z_Construct_UScriptStruct_FPlane_Statics::NewProp_W = { "W", nullptr, (EPropertyFlags)0x0010000000000000, UE4CodeGen_Private::EPropertyGenFlags::Float, RF_Public|RF_Transient|RF_MarkAsNative, 1, STRUCT_OFFSET(FPlane, W) };
	const UE4CodeGen_Private::FPropertyParamsBase* const Z_Construct_UScriptStruct_FPlane_Statics::PropPointers[] = {
		(const UE4CodeGen_Private::FPropertyParamsBase*)&Z_Construct_UScriptStruct_FPlane_Statics::NewProp_W,
	};
	const UE4CodeGen_Private::FStructParams Z_Construct_UScriptStruct_FPlane_Statics::ReturnStructParams = {
		(UObject* (*)())Z_Construct_UPackage__Script_LhtTest,
		Z_Construct_UScriptStruct_FVector,
		&NewStructOps,
		"Plane",
		sizeof(FPlane),
		alignof(FPlane),
		Z_Construct_UScriptStruct_FPlane_Statics::PropPointers,
		UE_ARRAY_COUNT(Z_Construct_UScriptStruct_FPlane_Statics::PropPointers),
		RF_Public|RF_Transient|RF_MarkAsNative,
		EStructFlags(0x00000039),
	};
	UScriptStruct* Z_Construct_UScriptStruct_FPlane()
	{
		static UScriptStruct* ReturnStruct = nullptr;
		if (!ReturnStruct)
		{
			UE4CodeGen_Private::ConstructUScriptStruct(ReturnStruct, Z_Construct_UScriptStruct_FPlane_Statics::ReturnStructParams);
		}
		return ReturnStruct;
	}
	static class UScriptStruct* FTransform_StaticStruct()
	{
		static class UScriptStruct* Singleton = nullptr;
		if (!Singleton)
		{
			Singleton = GetStaticStruct(Z_Construct_UScriptStruct_FTransform, Z_Construct_UPackage__Script_LhtTest(), TEXT("Transform"), sizeof(FTransform), 0);
		}
		return Singleton;
	}
	template<> LHTTEST_API UScriptStruct* StaticStruct<FTransform>()
	{
		return FTransform_StaticStruct();
	}
	struct Z_Construct_UScriptStruct_FTransform_Statics
	{
		static void* NewStructOps();
		/** The NoExport declaration of FTransform, checked against the C++ type. */
		struct FNoExportLayout
		{
			FQuat Rotation;
			FVector Translation;
			FVector Scale3D;
		};
		static_assert(sizeof(FNoExportLayout) == sizeof(FTransform), "NoExport FTransform does not match the size of the C++ type");
		static_assert(TIsSame<decltype(FTransform::Rotation), FQuat>::Value, "NoExport FTransform::Rotation does not match the type of the C++ member");
		static_assert(STRUCT_OFFSET(FNoExportLayout, Rotation) == STRUCT_OFFSET(FTransform, Rotation), "NoExport FTransform::Rotation does not match the offset of the C++ member");
		static_assert(TIsSame<decltype(FTransform::Translation), FVector>::Value, "NoExport FTransform::Translation does not match the type of the C++ member");
		static_assert(STRUCT_OFFSET(FNoExportLayout, Translation) == STRUCT_OFFSET(FTransform, Translation), "NoExport FTransform::Translation does not match the offset of the C++ member");
		static_assert(TIsSame<decltype(FTransform::Scale3D), FVector>::Value, "NoExport FTransform::Scale3D does not match the type of the C++ member");
		static_assert(STRUCT_OFFSET(FNoExportLayout, Scale3D) == STRUCT_OFFSET(FTransform, Scale3D), "NoExport FTransform::Scale3D does not match the offset of the C++ member");
		static const UE4CodeGen_Private::FStructPropertyParams NewProp_Rotation;
		static const UE4CodeGen_Private::FStructPropertyParams NewProp_Translation;
		static const UE4CodeGen_Private::FStructPropertyParams NewProp_Scale3D;
		static const UE4CodeGen_Private::FPropertyParamsBase* const PropPointers[];
		static const UE4CodeGen_Private::FStructParams ReturnStructParams;
	};
	void* Z_Construct_UScriptStruct_FTransform_Statics::NewStructOps()
	{
		return (UScriptStruct::ICppStructOps*)new UScriptStruct::TCppStructOps<FTransform>();
	}
	const UE4CodeGen_Private::FStructPropertyParams Z_Construct_UScriptStruct_FTransform_Statics::NewProp_Rotation = { "Rotation", nullptr, (EPropertyFlags)0x0010000000000000, UE4CodeGen_Private::EPropertyGenFlags::Struct, RF_Public|RF_Transient|RF_MarkAsNative, 1, STRUCT_OFFSET(FTransform, Rotation), Z_Construct_UScriptStruct_FQuat };
	const UE4CodeGen_Private::FStructPropertyParams Z_Construct_UScriptStruct_FTransform_Statics::NewProp_Translation = { "Translation", nullptr, (EPropertyFlags)0x0010000000000000, UE4CodeGen_Private::EPropertyGenFlags::Struct, RF_Public|RF_Transient|RF_MarkAsNative, 1, STRUCT_OFFSET(FTransform, Translation), Z_Construct_UScriptStruct_FVector };
	const UE4CodeGen_Private::FStructPropertyParams Z_Construct_UScriptStruct_FTransform_Statics::NewProp_Scale3D = { "Scale3D", nullptr, (EPropertyFlags)0x0010000000000000, UE4CodeGen_Private::EPropertyGenFlags::Struct, RF_Public|RF_Transient|RF_MarkAsNative, 1, STRUCT_OFFSET(FTransform, Scale3D), Z_Construct_UScriptStruct_FVector };
	const UE4CodeGen_Private::FPropertyParamsBase* const Z_Construct_UScriptStruct_FTransform_Statics::PropPointers[] = {
		(const UE4CodeGen_Private::FPropertyParamsBase*)&Z_Construct_UScriptStruct_FTransform_Statics::NewProp_Rotation,
		(const UE4CodeGen_Private::FPropertyParamsBase*)&Z_Construct_UScriptStruct_FTransform_Statics::NewProp_Translation,
		(const UE4CodeGen_Private::FPropertyParamsBase*)&Z_Construct_UScriptStruct_FTransform_Statics::NewProp_Scale3D,
	};
	const UE4CodeGen_Private::FStructParams Z_Construct_UScriptStruct_FTransform_Statics::ReturnStructParams = {
		(UObject* (*)())Z_Construct_UPackage__Script_LhtTest,
		nullptr,
		&NewStructOps,
		"Transform",
		sizeof(FTransform),
		alignof(FTransform),
		Z_Construct_UScriptStruct_FTransform_Statics::PropPointers,
		UE_ARRAY_COUNT(Z_Construct_UScriptStruct_FTransform_Statics::PropPointers),
		RF_Public|RF_Transient|RF_MarkAsNative,
		EStructFlags(0x00000039),
	};
	UScriptStruct* Z_Construct_UScriptStruct_FTransform()
	{
		static UScriptStruct* ReturnStruct = nullptr;
		if (!ReturnStruct)
		{
			UE4CodeGen_Private::ConstructUScriptStruct(ReturnStruct, Z_Construct_UScriptStruct_FTransform_Statics::ReturnStructParams);
		}
		return ReturnStruct;
	}
	class UScriptStruct* FUsesNoExport::StaticStruct()
	{
		static class UScriptStruct* Singleton = nullptr;
		if (!Singleton)
		{
			Singleton = GetStaticStruct(Z_Construct_UScriptStruct_FUsesNoExport, Z_Construct_UPackage__Script_LhtTest(), TEXT("UsesNoExport"), sizeof(FUsesNoExport), 0);
		}
		return Singleton;
	}
	template<> LHTTEST_API UScriptStruct* StaticStruct<FUsesNoExport>()
	{
		return FUsesNoExport::StaticStruct();
	}
	struct Z_Construct_UScriptStruct_FUsesNoExport_Statics
	{
		static void* NewStructOps();
		static const UE4CodeGen_Private::FStructPropertyParams NewProp_Location;
		static const UE4CodeGen_Private::FStructPropertyParams NewProp_Path_Inner;
		static const UE4CodeGen_Private::FArrayPropertyParams NewProp_Path;
		static const UE4CodeGen_Private::FPropertyParamsBase* const PropPointers[];
		static const UE4CodeGen_Private::FStructParams ReturnStructParams;
	};
	void* Z_Construct_UScriptStruct_FUsesNoExport_Statics::NewStructOps()
	{
		return (UScriptStruct::ICppStructOps*)new UScriptStruct::TCppStructOps<FUsesNoExport>();
	}
	const UE4CodeGen_Private::FStructPropertyParams Z_Construct_UScriptStruct_FUsesNoExport_Statics::NewProp_Location = { "Location", nullptr, (EPropertyFlags)0x0010000000000000, UE4CodeGen_Private::EPropertyGenFlags::Struct, RF_Public|RF_Transient|RF_MarkAsNative, 1, STRUCT_OFFSET(FUsesNoExport, Location), Z_Construct_UScriptStruct_FVector };
	const UE4CodeGen_Private::FStructPropertyParams Z_Construct_UScriptStruct_FUsesNoExport_Statics::NewProp_Path_Inner = { "Path", nullptr, (EPropertyFlags)0x0000000000000000, UE4CodeGen_Private::EPropertyGenFlags::Struct, RF_Public|RF_Transient|RF_MarkAsNative, 1, 0, Z_Construct_UScriptStruct_FTransform };
	const UE4CodeGen_Private::FArrayPropertyParams Z_Construct_UScriptStruct_FUsesNoExport_Statics::NewProp_Path = { "Path", nullptr, (EPropertyFlags)0x0010000000000000, UE4CodeGen_Private::EPropertyGenFlags::Array, RF_Public|RF_Transient|RF_MarkAsNative, 1, STRUCT_OFFSET(FUsesNoExport, Path), EArrayPropertyFlags::None };
	const UE4CodeGen_Private::FPropertyParamsBase* const Z_Construct_UScriptStruct_FUsesNoExport_Statics::PropPointers[] = {
		(const UE4CodeGen_Private::FPropertyParamsBase*)&Z_Construct_UScriptStruct_FUsesNoExport_Statics::NewProp_Location,
		(const UE4CodeGen_Private::FPropertyParamsBase*)&Z_Construct_UScriptStruct_FUsesNoExport_Statics::NewProp_Path_Inner,
		(const UE4CodeGen_Private::FPropertyParamsBase*)&Z_Construct_UScriptStruct_FUsesNoExport_Statics::NewProp_Path,
	};
	const UE4CodeGen_Private::FStructParams Z_Construct_UScriptStruct_FUsesNoExport_Statics::ReturnStructParams = {
		(UObject* (*)())Z_Construct_UPackage__Script_LhtTest,
		nullptr,
		&NewStructOps,
		"UsesNoExport",
		sizeof(FUsesNoExport),
		alignof(FUsesNoExport),
		Z_Construct_UScriptStruct_FUsesNoExport_Statics::PropPointers,
		UE_ARRAY_COUNT(Z_Construct_UScriptStruct_FUsesNoExport_Statics::PropPointers),
		RF_Public|RF_Transient|RF_MarkAsNative,
		EStructFlags(0x00000001),
	};
	UScriptStruct* Z_Construct_UScriptStruct_FUsesNoExport()
	{
		static UScriptStruct* ReturnStruct = nullptr;
		if (!ReturnStruct)
		{
			UE4CodeGen_Private::ConstructUScriptStruct(ReturnStruct, Z_Construct_UScriptStruct_FUsesNoExport_Statics::ReturnStructParams);
		}
		return ReturnStruct;
	}
PRAGMA_ENABLE_DEPRECATION_WARNINGS
