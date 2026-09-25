#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "CoreMinimal.h"
#include "Engine/DirectionalLight.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "Misc/AutomationTest.h"
#include "PrimitiveSceneProxy.h"
#include "Primitives.h"
#include "SceneInterface.h"
#include "Tests/ScopedTestWorld.h"
#include "UObject/GarbageCollection.h"
#include "UObject/WeakObjectPtrTemplates.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FComponentsShapesReportTheirCollisionShapeTest,
	"System.Engine.Components.ShapesReportTheirCollisionShape",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FComponentsShapesReportTheirCollisionShapeTest::RunTest(const FString& Parameters)
{
	// Capsule, box and sphere components describe their FCollisionShape, scaled by the smallest component scale.
	UCapsuleComponent& Capsule = *NewObject<UCapsuleComponent>();
	Capsule.SetCapsuleSize(30.0f, 90.0f);
	const FCollisionShape CapsuleShape = Capsule.GetCollisionShape();
	TestTrue("Capsule shape", CapsuleShape.IsCapsule());
	TestEqual("Capsule radius", CapsuleShape.GetCapsuleRadius(), 30.0f);
	TestEqual("Capsule half height", CapsuleShape.GetCapsuleHalfHeight(), 90.0f);
	Capsule.SetCapsuleSize(50.0f, 20.0f);
	TestEqual("Half height at least the radius", Capsule.GetUnscaledCapsuleHalfHeight(), 50.0f);
	Capsule.RelativeScale3D = FVector(2.0f, 3.0f, 4.0f);
	TestEqual("Scaled radius", Capsule.GetScaledCapsuleRadius(), 100.0f);

	UBoxComponent& Box = *NewObject<UBoxComponent>();
	Box.SetBoxExtent(FVector(10.0f, 20.0f, 30.0f));
	Box.RelativeScale3D = FVector(2.0f, -1.0f, 1.0f);
	const FCollisionShape BoxShape = Box.GetCollisionShape(1.0f);
	TestTrue("Box shape", BoxShape.IsBox());
	TestTrue("Scaled and inflated extent", BoxShape.GetBox().Equals(FVector(21.0f, 21.0f, 31.0f), 0.0f));

	USphereComponent& Sphere = *NewObject<USphereComponent>();
	Sphere.SetSphereRadius(25.0f);
	TestTrue("Sphere shape", Sphere.GetCollisionShape().IsSphere());
	TestEqual("Sphere radius", Sphere.GetCollisionShape().GetSphereRadius(), 25.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FComponentsPrimitivesRegisterWithTheWorldTest,
	"System.Engine.Components.PrimitivesRegisterWithTheWorld",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FComponentsPrimitivesRegisterWithTheWorldTest::RunTest(const FString& Parameters)
{
	// A primitive registered in a world gets its render state (its place in the world's scene) and loses it when it is
	// destroyed; a hidden owner is not rendered.
	FScopedTestWorld TestWorld;
	UWorld& World = *TestWorld;
	AActor* Actor = World.SpawnActor<AActor>();
	UStaticMeshComponent* MeshComponent = NewObject<UStaticMeshComponent>(Actor);
	TestFalse("Not registered yet", MeshComponent->IsRenderStateCreated());
	MeshComponent->SetupAttachment(Actor->GetRootComponent());
	MeshComponent->RegisterComponent();
	TestTrue("Registered", MeshComponent->IsRegistered());
	TestTrue("In the world's primitives", MeshComponent->IsRenderStateCreated());
	// A component created after the spawn is kept by its actor's OwnedComponents (AActor::AddReferencedObjects).
	TWeakObjectPtr<UStaticMeshComponent> WeakMeshComponent = MeshComponent;
	CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS);
	TestTrue("Kept by its owner", WeakMeshComponent.IsValid());
	TestTrue("Attached at registration", Actor->GetRootComponent()->GetAttachChildren().Contains(MeshComponent));
	TestFalse("No mesh yet", MeshComponent->HasValidMesh());
	TestTrue("Renders while visible", MeshComponent->ShouldRender());
	Actor->SetActorHiddenInGame(true);
	TestFalse("Hidden with its owner", MeshComponent->ShouldRender());

	FMaterial Override;
	Override.Roughness = 0.25f;
	MeshComponent->SetMaterial(1, Override);
	TestTrue("Slot 1 overridden", MeshComponent->HasOverrideMaterial(1));
	TestFalse("Slot 0 not overridden", MeshComponent->HasOverrideMaterial(0));
	TestEqual("Override material", MeshComponent->GetMaterial(1).Roughness, 0.25f);

	MeshComponent->DestroyComponent();
	TestFalse("Left the world's primitives", MeshComponent->IsRenderStateCreated());
	TestFalse("Not an owned component", Actor->GetComponents().Contains(MeshComponent));

	// Every character registers its capsule and its mesh.
	ACharacter* Character = World.SpawnActor<ACharacter>();
	TestTrue("Capsule primitive", Character->GetCapsuleComponent()->IsRenderStateCreated());
	TestTrue("Mesh primitive", Character->GetMesh().IsRenderStateCreated());
	Character->Destroy();
	TestFalse("Capsule gone", Character->GetCapsuleComponent()->IsRenderStateCreated());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FComponentsSceneProxiesFollowTheComponentsTest,
	"System.Engine.Components.SceneProxiesFollowTheComponents",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FComponentsSceneProxiesFollowTheComponentsTest::RunTest(const FString& Parameters)
{
	// A mesh component with a mesh and a visible light add their proxies to the world's scene; the proxy follows a
	// hidden owner and the moved transform, and leaves with the component. A world that cannot render has no scene.
	FScopedTestWorld TestWorld;
	UWorld& World = *TestWorld;
	if (!TestNotNull("The world has a scene", World.Scene))
	{
		return false;
	}
	FSceneInterface& Scene = *World.Scene;
	TestTrue("The scene's world", Scene.GetWorld() == &World);

	AStaticMeshActor* MeshActor = World.SpawnActor<AStaticMeshActor>();
	UStaticMeshComponent* MeshComponent = MeshActor->GetStaticMeshComponent();
	TestEqual("No proxy without a mesh", Scene.GetNumPrimitives(), 0);
	(void)MeshComponent->SetStaticMesh(MakeShared<UStaticMesh>(UStaticMesh::CreateCpu(MakeCube())));
	TestEqual("A proxy with a mesh", Scene.GetNumPrimitives(), 1);
	if (!TestNotNull("The component knows its proxy", MeshComponent->SceneProxy))
	{
		return false;
	}
	TestTrue("Shown", MeshComponent->SceneProxy->IsShown());
	MeshActor->SetActorHiddenInGame(true);
	TestFalse("Hidden with its owner", MeshComponent->SceneProxy->IsShown());
	MeshActor->SetActorLocation(FVector(100.0f, 0.0f, 0.0f));
	World.SendAllEndOfFrameUpdates();
	TestEqual("Moved with the component", MeshComponent->SceneProxy->GetLocalToWorld().GetOrigin().X, 100.0f);

	ADirectionalLight* Light = World.SpawnActor<ADirectionalLight>();
	TestEqual("A light", Scene.GetNumLights(), 1);
	Light->SetActorHiddenInGame(true);
	TestEqual("A hidden light leaves the scene", Scene.GetNumLights(), 0);

	MeshActor->Destroy();
	TestEqual("The proxy leaves with the component", Scene.GetNumPrimitives(), 0);
	TestNull("The component forgets it", MeshComponent->SceneProxy);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FComponentsAttachmentRulesAndSocketsTest,
	"System.Engine.Components.AttachmentRulesAndSockets",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FComponentsAttachmentRulesAndSocketsTest::RunTest(const FString& Parameters)
{
	// Attachment rules keep the relative or the world transform or snap; a socket the parent does not have is its
	// component transform; detaching keeps the world transform on request.
	FScopedTestWorld TestWorld;
	UWorld& World = *TestWorld;
	AActor* Actor = World.SpawnActor<AActor>(FVector(1000.0f, 0.0f, 0.0f), FRotator(0.0f, 90.0f, 0.0f));
	USceneComponent* Root = Actor->GetRootComponent();

	USceneComponent* KeepRelative = NewObject<USceneComponent>(Actor);
	KeepRelative->RelativeLocation = FVector(100.0f, 0.0f, 0.0f);
	TestTrue("Attach keeping relative",
		KeepRelative->AttachToComponent(Root, FAttachmentTransformRules::KeepRelativeTransform));
	// Yaw 90 turns the relative +X offset to +Y.
	TestTrue("Relative offset turned with the parent",
		KeepRelative->GetComponentLocation().Equals(FVector(1000.0f, 100.0f, 0.0f), 1.0e-3f));

	USceneComponent* KeepWorld = NewObject<USceneComponent>(Actor);
	KeepWorld->RelativeLocation = FVector(0.0f, 500.0f, 0.0f);
	TestTrue("Attach keeping world", KeepWorld->AttachToComponent(Root, FAttachmentTransformRules::KeepWorldTransform));
	TestTrue("World location kept", KeepWorld->GetComponentLocation().Equals(FVector(0.0f, 500.0f, 0.0f), 1.0e-3f));

	USceneComponent* Snapped = NewObject<USceneComponent>(Actor);
	Snapped->RelativeLocation = FVector(7.0f, 8.0f, 9.0f);
	TestTrue("Attach snapping",
		Snapped->AttachToComponent(
			KeepRelative, FAttachmentTransformRules::SnapToTargetIncludingScale, TEXT("Muzzle")));
	TestEqual("Socket name kept", Snapped->GetAttachSocketName(), FName(TEXT("Muzzle")));
	TestFalse("Scene components have no sockets", KeepRelative->DoesSocketExist(TEXT("Muzzle")));
	TestTrue("Snapped onto the parent",
		Snapped->GetComponentLocation().Equals(KeepRelative->GetComponentLocation(), 1.0e-3f));

	Snapped->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);
	TestNull("Detached", Snapped->GetAttachParent());
	TestTrue("World location kept after detaching",
		Snapped->GetComponentLocation().Equals(FVector(1000.0f, 100.0f, 0.0f), 1.0e-3f));

	// A skeletal mesh without a mesh has no bones: its sockets are its own transform.
	USkeletalMeshComponent* Skeletal = NewObject<USkeletalMeshComponent>(Actor);
	TestTrue("Attach the skeletal mesh",
		Skeletal->AttachToComponent(Root, FAttachmentTransformRules::KeepRelativeTransform));
	TestFalse("No bone socket", Skeletal->DoesSocketExist(TEXT("hand_r")));
	TestTrue("Missing bone falls back to the component",
		Skeletal->GetSocketTransform(TEXT("hand_r")).GetLocation().Equals(Skeletal->GetComponentLocation(), 1.0e-3f));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FComponentsCharacterDefaultSubobjectsTest,
	"System.Engine.Components.CharacterDefaultSubobjects",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FComponentsCharacterDefaultSubobjectsTest::RunTest(const FString& Parameters)
{
	// A character's root is its capsule (35 x 92.5 cm), its movement component moves the capsule and knows its
	// character, and its mesh hangs from the capsule; each instance builds its own subobjects.
	FScopedTestWorld TestWorld;
	UWorld& World = *TestWorld;
	ACharacter* Character = World.SpawnActor<ACharacter>();
	ACharacter* Other = World.SpawnActor<ACharacter>();
	UCapsuleComponent* Capsule = Character->GetCapsuleComponent();
	if (!TestNotNull("Capsule", Capsule))
	{
		return false;
	}
	TestTrue("Capsule is the root", Character->GetRootComponent() == Capsule);
	TestEqual("Capsule name", Capsule->GetFName(), ACharacter::CapsuleComponentName);
	TestEqual("Radius", Character->GetCapsule().GetCapsuleRadius(), 35.0f);
	TestEqual("Half height", Character->GetCapsule().GetCapsuleHalfHeight(), 92.5f);
	TestTrue("Movement moves the capsule", Character->GetCharacterMovement().UpdatedComponent == Capsule);
	TestTrue("Movement owner", Character->GetCharacterMovement().GetCharacterOwner() == Character);
	TestTrue("Movement pawn owner", Character->GetCharacterMovement().GetPawnOwner() == Character);
	TestTrue("Mesh under the capsule", Character->GetMesh().GetAttachParent() == Capsule);
	TestTrue("Own subobjects", Other->GetCapsuleComponent() != Capsule && &Other->GetMesh() != &Character->GetMesh());
	TestNull("No default scene root", Character->FindComponentByClass<USphereComponent>());
	TestEqual("Capsule, movement and mesh", Character->GetComponents().Num(), 3);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
