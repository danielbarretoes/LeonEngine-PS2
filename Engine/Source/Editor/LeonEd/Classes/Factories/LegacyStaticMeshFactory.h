#pragma once

#include "CoreMinimal.h"
#include "Factories/Factory.h"
#include "LegacyStaticMeshFactory.generated.h"

/**
 * Makes a UStaticMesh of a legacy cooked `.lmesh` (Leon, temporary: MigrateLegacyContent's importer, deleted with it).
 * Only version 2 files are read (the engine world, written since 0.14.0); a version 1 file (Y up, metres) is refused:
 * import its OBJ / FBX / glTF source instead. Each material slot gets the material the runtime gave it,
 * `M_<Mesh>_Slot<N>` next to the mesh: the default parameters and the slot's diffuse map (a `.png` / `.jpg` slot
 * string) imported as a `T_` texture. The `.lmesh` is not kept as a source: the mesh has no import data.
 */
UCLASS()
class LEONED_API ULegacyStaticMeshFactory : public UFactory
{
	GENERATED_BODY()

public:
	ULegacyStaticMeshFactory(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	UObject* FactoryCreateBinary(UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags, UObject* Context,
		const TCHAR* Type, const uint8*& Buffer, const uint8* BufferEnd, bool& bOutOperationCanceled) override;
};
