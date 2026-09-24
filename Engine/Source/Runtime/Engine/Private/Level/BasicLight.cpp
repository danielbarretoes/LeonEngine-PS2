#include "Level/BasicLight.h"
#include "Engine/Level.h"


// Class-name parsers live in Content/LevelClassNames.cpp (shared with ContentValidator / cook).

FBasicLight FBasicLight::directional(glm::vec3 rotationDegrees, glm::vec3 lightColor,
                                   float intensity) {
    FBasicLight light;
    light.type = EBasicLight::Directional;
    light.transform.RotationDegrees = rotationDegrees;
    light.lightColor = lightColor;
    light.intensity = intensity;
    light.castShadows = true;
    return light;
}

FBasicLight FBasicLight::point(glm::vec3 position, glm::vec3 lightColor, float intensity,
                             float range) {
    FBasicLight light;
    light.type = EBasicLight::Point;
    light.transform.Position = position;
    light.lightColor = lightColor;
    light.intensity = intensity;
    light.range = range;
    light.castShadows = false;
    return light;
}

FDirectionalLight FBasicLight::asDirectional() const {
    FDirectionalLight light;
    light.transform = transform;
    light.lightColor = lightColor;
    light.intensity = intensity;
    light.castShadows = castShadows;
    light.sourceAngle = sourceAngle;
    return light;
}

FPointLight FBasicLight::asPoint() const {
    FPointLight light;
    light.transform = transform;
    light.lightColor = lightColor;
    light.intensity = intensity;
    light.range = range;
    light.castShadows = castShadows;
    return light;
}

void FBasicLight::addTo(ULevel& level) const {
    switch (type) {
    case EBasicLight::Directional:
        level.DirectionalLights().push_back(asDirectional());
        break;
    case EBasicLight::Point:
        level.PointLights().push_back(asPoint());
        break;
    }
}

