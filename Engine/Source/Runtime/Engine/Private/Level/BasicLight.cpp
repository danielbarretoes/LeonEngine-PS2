#include "Level/BasicLight.h"
#include "Engine/Level.h"


// Class-name parsers live in Content/LevelClassNames.cpp (shared with ContentValidator / cook).

BasicLight BasicLight::directional(glm::vec3 rotationDegrees, glm::vec3 lightColor,
                                   float intensity) {
    BasicLight light;
    light.type = EBasicLight::Directional;
    light.transform.rotationDegrees = rotationDegrees;
    light.lightColor = lightColor;
    light.intensity = intensity;
    light.castShadows = true;
    return light;
}

BasicLight BasicLight::point(glm::vec3 position, glm::vec3 lightColor, float intensity,
                             float range) {
    BasicLight light;
    light.type = EBasicLight::Point;
    light.transform.position = position;
    light.lightColor = lightColor;
    light.intensity = intensity;
    light.range = range;
    light.castShadows = false;
    return light;
}

DirectionalLight BasicLight::asDirectional() const {
    DirectionalLight light;
    light.transform = transform;
    light.lightColor = lightColor;
    light.intensity = intensity;
    light.castShadows = castShadows;
    light.sourceAngle = sourceAngle;
    return light;
}

PointLight BasicLight::asPoint() const {
    PointLight light;
    light.transform = transform;
    light.lightColor = lightColor;
    light.intensity = intensity;
    light.range = range;
    light.castShadows = castShadows;
    return light;
}

void BasicLight::addTo(Level& level) const {
    switch (type) {
    case EBasicLight::Directional:
        level.DirectionalLights().push_back(asDirectional());
        break;
    case EBasicLight::Point:
        level.PointLights().push_back(asPoint());
        break;
    }
}

