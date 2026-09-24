#include "LeonMaterialFormat.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <iostream>
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "ResourceCache.h"
#include <sstream>

namespace {

[[nodiscard]] std::string ExtLower(const std::string& path) {
    const auto pos = path.find_last_of('.');
    if (pos == std::string::npos) {
        return {};
    }
    std::string e = path.substr(pos);
    for (char& c : e) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return e;
}

[[nodiscard]] std::string Trim(std::string s) {
    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.front()))) {
        s.erase(s.begin());
    }
    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.back()))) {
        s.pop_back();
    }
    return s;
}

[[nodiscard]] std::string StripComment(std::string line) {
    const auto hash = line.find('#');
    if (hash != std::string::npos) {
        line = line.substr(0, hash);
    }
    const auto semi = line.find(';');
    if (semi != std::string::npos) {
        line = line.substr(0, semi);
    }
    return Trim(std::move(line));
}

[[nodiscard]] bool ParseBool(const std::string& v, bool fallback) {
    std::string s = v;
    for (char& c : s) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    if (s == "1" || s == "true" || s == "yes" || s == "on") {
        return true;
    }
    if (s == "0" || s == "false" || s == "no" || s == "off") {
        return false;
    }
    return fallback;
}

[[nodiscard]] bool ParseFloat(const std::string& v, float& out) {
    try {
        size_t idx = 0;
        out = std::stof(v, &idx);
        return idx > 0;
    } catch (...) {
        return false;
    }
}

[[nodiscard]] bool ParseVec3(const std::string& v, glm::vec3& out) {
    std::stringstream ss(v);
    char comma = 0;
    float a = 0.0f;
    float b = 0.0f;
    float c = 0.0f;
    if (!(ss >> a)) {
        return false;
    }
    if (ss >> comma && comma == ',') {
        if (!(ss >> b)) {
            return false;
        }
        if (!(ss >> comma) || comma != ',') {
            return false;
        }
        if (!(ss >> c)) {
            return false;
        }
        out = {a, b, c};
        return true;
    }
    // Single scalar → gray
    out = {a, a, a};
    return true;
}

[[nodiscard]] bool ParseVec2(const std::string& v, glm::vec2& out) {
    std::stringstream ss(v);
    char comma = 0;
    float a = 0.0f;
    float b = 0.0f;
    if (!(ss >> a)) {
        return false;
    }
    if (ss >> comma && comma == ',') {
        if (!(ss >> b)) {
            return false;
        }
        out = {a, b};
        return true;
    }
    out = {a, a};
    return true;
}

void ApplyTextureKey(ResourceCache& resources, Material& material, const std::string& key,
                     const std::string& value) {
    if (value.empty()) {
        return;
    }
    std::string k = key;
    for (char& c : k) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    if (k == "basecolormap" || k == "albedomap" || k == "diffusemap") {
        if (value == "checker") {
            material.albedoMap = resources.CheckerTexture(64);
        } else {
            material.albedoMap = resources.LoadTexture(FPaths::ResolveAssetPath(value));
        }
    } else if (k == "normalmap") {
        if (value == "bump") {
            material.normalMap = resources.BumpNormalTexture(256);
        } else {
            material.normalMap = resources.LoadTexture(FPaths::ResolveAssetPath(value));
        }
    }
}

} // namespace

bool IsLeonMaterialPath(const std::string& path) {
    return ExtLower(path) == ".lmat";
}

bool LoadLeonMaterialDocument(const std::string& path, LeonMaterialDocument& out) {
    std::ifstream in(path);
    if (!in.is_open()) {
        std::cerr << "LeonMaterial: cannot open " << path << '\n';
        return false;
    }

    LeonMaterialDocument doc{};
    doc.material.shading = EShadingModel::BlinnPhong;
    bool hasRoughness = false;
    bool hasShininess = false;
    std::string section;

    std::string line;
    while (std::getline(in, line)) {
        line = StripComment(std::move(line));
        if (line.empty()) {
            continue;
        }
        if (line.front() == '[' && line.back() == ']') {
            section = line.substr(1, line.size() - 2);
            for (char& c : section) {
                c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            }
            continue;
        }
        const auto eq = line.find('=');
        if (eq == std::string::npos) {
            std::cerr << "LeonMaterial: ignoring line without '=': " << path << '\n';
            continue;
        }
        std::string key = Trim(line.substr(0, eq));
        std::string value = Trim(line.substr(eq + 1));
        std::string keyLower = key;
        for (char& c : keyLower) {
            c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        }

        if (section == "info") {
            if (keyLower == "name") {
                doc.name = value;
            } else if (keyLower == "shadingmodel" || keyLower == "shading") {
                std::string v = value;
                for (char& c : v) {
                    c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
                }
                doc.material.shading =
                    (v == "unlit") ? EShadingModel::Unlit : EShadingModel::BlinnPhong;
            } else {
                std::cerr << "LeonMaterial: unknown [Info] key '" << key << "' in " << path << '\n';
            }
            continue;
        }

        if (section == "textures" || keyLower.find("map") != std::string::npos) {
            if (keyLower == "basecolormap" || keyLower == "albedomap" || keyLower == "diffusemap") {
                doc.baseColorMapPath = value;
            } else if (keyLower == "normalmap") {
                doc.normalMapPath = value;
            } else if (section == "textures") {
                std::cerr << "LeonMaterial: unknown [Textures] key '" << key << "' in " << path
                          << '\n';
            }
            continue;
        }

        if (keyLower == "basecolor" || keyLower == "albedo") {
            if (!ParseVec3(value, doc.material.albedo)) {
                std::cerr << "LeonMaterial: bad BaseColor '" << value << "' in " << path << '\n';
            }
        } else if (keyLower == "specular") {
            if (!ParseVec3(value, doc.material.specular)) {
                std::cerr << "LeonMaterial: bad Specular '" << value << "' in " << path << '\n';
            }
        } else if (keyLower == "metallic") {
            float f = doc.material.metallic;
            if (ParseFloat(value, f)) {
                doc.material.metallic = std::clamp(f, 0.0f, 1.0f);
            } else {
                std::cerr << "LeonMaterial: bad Metallic '" << value << "' in " << path << '\n';
            }
        } else if (keyLower == "roughness") {
            float f = doc.material.roughness;
            if (ParseFloat(value, f)) {
                doc.material.roughness = std::clamp(f, 0.04f, 1.0f);
                hasRoughness = true;
            } else {
                std::cerr << "LeonMaterial: bad Roughness '" << value << "' in " << path << '\n';
            }
        } else if (keyLower == "opacity" || keyLower == "alpha") {
            float f = doc.material.alpha;
            if (ParseFloat(value, f)) {
                doc.material.alpha = std::clamp(f, 0.0f, 1.0f);
            } else {
                std::cerr << "LeonMaterial: bad Opacity '" << value << "' in " << path << '\n';
            }
        } else if (keyLower == "shininess") {
            float f = doc.material.shininess;
            if (ParseFloat(value, f)) {
                doc.material.shininess = f;
                hasShininess = true;
            } else {
                std::cerr << "LeonMaterial: bad Shininess '" << value << "' in " << path << '\n';
            }
        } else if (keyLower == "uvscale" || keyLower == "tiling") {
            if (!ParseVec2(value, doc.material.uvScale)) {
                std::cerr << "LeonMaterial: bad UVScale '" << value << "' in " << path << '\n';
            }
        } else if (keyLower == "castsshadows") {
            doc.material.castsShadows = ParseBool(value, doc.material.castsShadows);
        } else if (keyLower == "planarmirror") {
            doc.material.planarMirror = ParseBool(value, doc.material.planarMirror);
        } else if (keyLower == "unlit") {
            if (ParseBool(value, false)) {
                doc.material.shading = EShadingModel::Unlit;
            }
        } else {
            std::cerr << "LeonMaterial: unknown key '" << key << "' in " << path << '\n';
        }
    }

    if (!hasRoughness) {
        doc.material.syncRoughnessFromShininess();
    }
    if (doc.name.empty()) {
        doc.name = "Material";
    }
    out = std::move(doc);
    return true;
}

bool LoadLeonMaterialFile(ResourceCache& resources, const std::string& path, Material& out) {
    LeonMaterialDocument doc;
    if (!LoadLeonMaterialDocument(path, doc)) {
        return false;
    }
    if (!doc.baseColorMapPath.empty()) {
        ApplyTextureKey(resources, doc.material, "basecolormap", doc.baseColorMapPath);
    }
    if (!doc.normalMapPath.empty()) {
        ApplyTextureKey(resources, doc.material, "normalmap", doc.normalMapPath);
    }
    out = std::move(doc.material);
    return true;
}

bool SaveLeonMaterialFile(const std::string& path, const std::string& name,
                          const Material& material, const std::string& baseColorMapPath,
                          const std::string& normalMapPath) {
    std::ostringstream out;
    out << "# Leon Material (.lmat) — Unreal Material Instance–like parameters\n";
    out << "# version 1\n\n";
    out << "[Info]\n";
    out << "Name=" << (name.empty() ? "Material" : name) << '\n';
    out << "ShadingModel="
        << (material.shading == EShadingModel::Unlit ? "Unlit" : "DefaultLit") << "\n\n";
    out << "[Parameters]\n";
    out << "BaseColor=" << material.albedo.x << ',' << material.albedo.y << ',' << material.albedo.z
        << '\n';
    out << "Specular=" << material.specular.x << ',' << material.specular.y << ','
        << material.specular.z << '\n';
    out << "Metallic=" << material.metallic << '\n';
    out << "Roughness=" << material.roughness << '\n';
    out << "Opacity=" << material.alpha << '\n';
    out << "Shininess=" << material.shininess << '\n';
    out << "UVScale=" << material.uvScale.x << ',' << material.uvScale.y << '\n';
    out << "CastsShadows=" << (material.castsShadows ? "true" : "false") << '\n';
    out << "PlanarMirror=" << (material.planarMirror ? "true" : "false") << "\n\n";
    out << "[Textures]\n";
    out << "BaseColorMap=" << baseColorMapPath << '\n';
    out << "NormalMap=" << normalMapPath << '\n';
    if (!FFileHelper::WriteTextFileAtomic(path, out.str())) {
        std::cerr << "LeonMaterial: cannot write " << path << '\n';
        return false;
    }
    return true;
}

std::string MakeDefaultLeonMaterialText(const std::string& name, const glm::vec3& baseColor,
                                        float metallic, float roughness) {
    Material m{};
    m.albedo = baseColor;
    m.metallic = metallic;
    m.roughness = std::clamp(roughness, 0.04f, 1.0f);
    m.shininess = 32.0f;
    std::ostringstream oss;
    // Reuse writer via temp logic inline
    oss << "# Leon Material (.lmat) — Unreal Material Instance–like parameters\n";
    oss << "# version 1\n\n";
    oss << "[Info]\nName=" << (name.empty() ? "M_New" : name) << "\n";
    oss << "ShadingModel=DefaultLit\n\n";
    oss << "[Parameters]\n";
    oss << "BaseColor=" << baseColor.x << ',' << baseColor.y << ',' << baseColor.z << '\n';
    oss << "Specular=0.04,0.04,0.04\n";
    oss << "Metallic=" << metallic << '\n';
    oss << "Roughness=" << m.roughness << '\n';
    oss << "Opacity=1\n";
    oss << "UVScale=1,1\n";
    oss << "CastsShadows=true\n\n";
    oss << "[Textures]\n";
    oss << "BaseColorMap=\n";
    oss << "NormalMap=\n";
    return oss.str();
}

