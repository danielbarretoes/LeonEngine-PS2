#include "Level/BasicLight.h"

#include "Engine/DirectionalLight.h"
#include "Engine/PointLight.h"
#include "Engine/World.h"

// Class-name parsers live in Content/LevelClassNames.cpp (shared with the level format / cook).

FBasicLight FBasicLight::Directional(const FQuat& Rotation, const FVector& InLightColor, float InIntensity)
{
	FBasicLight Light;
	Light.Type = EBasicLight::Directional;
	Light.Transform.SetRotation(Rotation);
	Light.LightColor = InLightColor;
	Light.Intensity = InIntensity;
	Light.bCastShadows = true;
	return Light;
}

FBasicLight FBasicLight::Point(const FVector& Location, const FVector& InLightColor, float InIntensity, float InRange)
{
	FBasicLight Light;
	Light.Type = EBasicLight::Point;
	Light.Transform.SetLocation(Location);
	Light.LightColor = InLightColor;
	Light.Intensity = InIntensity;
	Light.Range = InRange;
	Light.bCastShadows = false;
	return Light;
}

FDirectionalLight FBasicLight::AsDirectional() const
{
	FDirectionalLight Light;
	Light.Transform = Transform;
	Light.LightColor = LightColor;
	Light.Intensity = Intensity;
	Light.bCastShadows = bCastShadows;
	Light.SourceAngle = SourceAngle;
	return Light;
}

FPointLight FBasicLight::AsPoint() const
{
	FPointLight Light;
	Light.Transform = Transform;
	Light.LightColor = LightColor;
	Light.Intensity = Intensity;
	Light.Range = Range;
	Light.bCastShadows = bCastShadows;
	return Light;
}

ALight* FBasicLight::SpawnIn(UWorld& World) const
{
	if (Type == EBasicLight::Directional)
	{
		ADirectionalLight* Light = World.SpawnActor<ADirectionalLight>(ADirectionalLight::StaticClass(), Transform);
		if (Light != nullptr)
		{
			UDirectionalLightComponent* Component = Light->GetDirectionalLightComponent();
			Component->SetLightColor(FLinearColor(LightColor.X, LightColor.Y, LightColor.Z));
			Component->SetIntensity(Intensity);
			Component->SetCastShadows(bCastShadows);
			Component->LightSourceAngle = SourceAngle;
		}
		return Light;
	}
	APointLight* Light = World.SpawnActor<APointLight>(APointLight::StaticClass(), Transform);
	if (Light != nullptr)
	{
		UPointLightComponent* Component = Light->GetPointLightComponent();
		Component->SetLightColor(FLinearColor(LightColor.X, LightColor.Y, LightColor.Z));
		Component->SetIntensity(Intensity);
		Component->SetCastShadows(bCastShadows);
		Component->SetAttenuationRadius(Range);
	}
	return Light;
}
