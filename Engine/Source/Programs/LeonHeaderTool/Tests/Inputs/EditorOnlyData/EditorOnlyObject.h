// Properties inside #if WITH_EDITORONLY_DATA get CPF_EditorOnly and #if blocks in the generated tables.
#pragma once

#include "CoreMinimal.h"
#include "EditorOnlyObject.generated.h"

UCLASS()
class UEditorOnlyObject : public UObject
{
	GENERATED_BODY()

public:
	UPROPERTY()
	int32 RuntimeValue;

#if WITH_EDITORONLY_DATA
	UPROPERTY(EditAnywhere)
	FString EditorNote;

	UPROPERTY()
	TArray<FName> EditorTags;
#endif // WITH_EDITORONLY_DATA

	UPROPERTY()
	float AfterEditor;
};

USTRUCT()
struct FEditorOnlyStruct
{
	GENERATED_BODY()

#if WITH_EDITORONLY_DATA
	UPROPERTY()
	int32 OnlyInEditor;

	UPROPERTY()
	bool bEditorFlag;
#endif
};
