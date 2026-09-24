#include "Misc/Ascii.h"
#include "Level/BasicLight.h"
#include "Level/BasicShape.h"


bool tryParseBasicShapeName(std::string_view name, EBasicShape& out) {
    const std::string key = AsciiToLower(name);
    if (key == "cube") {
        out = EBasicShape::Cube;
        return true;
    }
    if (key == "sphere") {
        out = EBasicShape::Sphere;
        return true;
    }
    if (key == "plane") {
        out = EBasicShape::Plane;
        return true;
    }
    return false;
}

bool isBlockingVolumeName(std::string_view name) {
    return AsciiToLower(name) == "blockingvolume";
}

bool isPlayerStartName(std::string_view name) {
    return AsciiToLower(name) == "playerstart";
}

bool tryParseBasicLightName(std::string_view name, EBasicLight& out) {
    const std::string key = AsciiToLower(name);
    if (key == "directionallight" || key == "directional" || key == "dirlight") {
        out = EBasicLight::Directional;
        return true;
    }
    if (key == "pointlight" || key == "point") {
        out = EBasicLight::Point;
        return true;
    }
    return false;
}

