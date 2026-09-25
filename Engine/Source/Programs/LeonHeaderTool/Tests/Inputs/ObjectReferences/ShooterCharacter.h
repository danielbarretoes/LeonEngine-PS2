// Object references of every kind and types from another reflected module (Engine).
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ShooterCharacter.generated.h"

class UStaticMesh;

UCLASS(Config = Game)
class GAME_API AShooterCharacter : public AActor
{
	GENERATED_BODY()

public:
	AShooterCharacter();

	UPROPERTY(EditDefaultsOnly, Instanced)
	class UStaticMesh* Mesh;

	UPROPERTY()
	const UStaticMesh* ConstMesh = nullptr;

	UPROPERTY(EditDefaultsOnly)
	TSubclassOf<AActor> ProjectileClass;

	UPROPERTY()
	TSoftObjectPtr<UStaticMesh> LazyMesh;

	UPROPERTY()
	TSoftClassPtr<AActor> LazyClass;

	UPROPERTY()
	TWeakObjectPtr<AActor> Target;

	UPROPERTY()
	FHitResult LastHit;

	UPROPERTY()
	TEnumAsByte<ECollisionChannel> Channel;

	UPROPERTY()
	EMovementMode Movement;

	UPROPERTY()
	UObject* Anything;

	UPROPERTY()
	TArray<AActor*> Followers;

	UFUNCTION(Exec)
	void Teleport(const FHitResult& Where, AActor* Other);

	UFUNCTION()
	AActor* FindTarget(TSubclassOf<AActor> Filter, EMovementMode Mode) const;
};
