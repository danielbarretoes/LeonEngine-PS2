#include "Level/BasicLight.h"
#include "Engine/Level.h"


// Class-name parsers live in Content/LevelClassNames.cpp (shared with ContentValidator / cook).

FBasicLight FBasicLight::Directional(glm::vec3 RotationDegrees, glm::vec3 InLightColor,
                                   float InIntensity) {
    FBasicLight Light;
    Light.Type = EBasicLight::Directional;
    Light.Transform.RotationDegrees = RotationDegrees;
    Light.LightColor = InLightColor;
    Light.Intensity = InIntensity;
    Light.bCastShadows = true;
    return Light;
}

FBasicLight FBasicLight::Point(glm::vec3 Position, glm::vec3 InLightColor, float InIntensity,
                             float InRange) {
    FBasicLight Light;
    Light.Type = EBasicLight::Point;
    Light.Transform.Position = Position;
    Light.LightColor = InLightColor;
    Light.Intensity = InIntensity;
    Light.Range = InRange;
    Light.bCastShadows = false;
    return Light;
}

FDirectionalLight FBasicLight::AsDirectional() const {
    FDirectionalLight Light;
    Light.Transform = Transform;
    Light.LightColor = LightColor;
    Light.Intensity = Intensity;
    Light.bCastShadows = bCastShadows;
    Light.SourceAngle = SourceAngle;
    return Light;
}

FPointLight FBasicLight::AsPoint() const {
    FPointLight Light;
    Light.Transform = Transform;
    Light.LightColor = LightColor;
    Light.Intensity = Intensity;
    Light.Range = Range;
    Light.bCastShadows = bCastShadows;
    return Light;
}

void FBasicLight::AddTo(ULevel& Level) const {
    switch (Type) {
    case EBasicLight::Directional:
        Level.GetDirectionalLights().push_back(AsDirectional());
        break;
    case EBasicLight::Point:
        Level.GetPointLights().push_back(AsPoint());
        break;
    }
}

