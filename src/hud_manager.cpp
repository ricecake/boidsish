#include "hud_manager.h"
#include "logger.h"
#include <GL/glew.h>
#include <algorithm>
#include <stdexcept>
#include "stb_image.h"
#include "logger.h"

namespace Boidsish {

HudManager::HudManager() {}

HudManager::~HudManager() {
    for (auto const& [key, val] : m_texture_cache) {
        glDeleteTextures(1, &val);
    }
}

// Icon management
void HudManager::AddIcon(const HudIcon& icon) {
    m_icons.push_back(icon);
}

void HudManager::UpdateIcon(int id, const HudIcon& icon) {
    auto it = std::find_if(m_icons.begin(), m_icons.end(), [id](const HudIcon& i) { return i.id == id; });
    if (it != m_icons.end()) {
        *it = icon;
    }
}

void HudManager::RemoveIcon(int id) {
    m_icons.erase(std::remove_if(m_icons.begin(), m_icons.end(), [id](const HudIcon& i) { return i.id == id; }), m_icons.end());
}

const std::vector<HudIcon>& HudManager::GetIcons() const {
    return m_icons;
}

unsigned int HudManager::GetTextureId(const std::string& path) {
    logger::LOG("HudManager::GetTextureId called for path: " + path);
    if (m_texture_cache.find(path) != m_texture_cache.end()) {
        logger::LOG("Texture found in cache.");
        return m_texture_cache[path];
    }
    logger::LOG("Texture not in cache. Loading...");
    unsigned int textureId = LoadTexture(path);
    logger::LOG("LoadTexture returned: " + std::to_string(textureId));
    if (textureId != 0) {
        m_texture_cache[path] = textureId;
    }
    return textureId;
}

// Number management
void HudManager::AddNumber(const HudNumber& number) {
    m_numbers.push_back(number);
}

void HudManager::UpdateNumber(int id, const HudNumber& number) {
    auto it = std::find_if(m_numbers.begin(), m_numbers.end(), [id](const HudNumber& n) { return n.id == id; });
    if (it != m_numbers.end()) {
        *it = number;
    }
}

void HudManager::RemoveNumber(int id) {
    m_numbers.erase(std::remove_if(m_numbers.begin(), m_numbers.end(), [id](const HudNumber& n) { return n.id == id; }), m_numbers.end());
}

const std::vector<HudNumber>& HudManager::GetNumbers() const {
    return m_numbers;
}

// Gauge management
void HudManager::AddGauge(const HudGauge& gauge) {
    m_gauges.push_back(gauge);
}

void HudManager::UpdateGauge(int id, const HudGauge& gauge) {
    auto it = std::find_if(m_gauges.begin(), m_gauges.end(), [id](const HudGauge& g) { return g.id == id; });
    if (it != m_gauges.end()) {
        *it = gauge;
    }
}

void HudManager::RemoveGauge(int id) {
    m_gauges.erase(std::remove_if(m_gauges.begin(), m_gauges.end(), [id](const HudGauge& g) { return g.id == id; }), m_gauges.end());
}

const std::vector<HudGauge>& HudManager::GetGauges() const {
    return m_gauges;
}

unsigned int HudManager::LoadTexture(const std::string& path) {
    unsigned int textureID;
    glGenTextures(1, &textureID);

    int width, height, nrComponents;
    unsigned char* data = stbi_load(path.c_str(), &width, &height, &nrComponents, 0);
    if (data) {
        GLenum format;
        if (nrComponents == 1)
            format = GL_RED;
        else if (nrComponents == 3)
            format = GL_RGB;
        else if (nrComponents == 4)
            format = GL_RGBA;
        else {
             logger::ERROR("Unsupported number of components in texture: " + std::to_string(nrComponents));
             stbi_image_free(data);
             return 0;
        }

        glBindTexture(GL_TEXTURE_2D, textureID);
        glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);
        glGenerateMipmap(GL_TEXTURE_2D);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        stbi_image_free(data);
    } else {
        logger::ERROR("Texture failed to load", path, stbi_failure_reason());
        return 0;
    }

    return textureID;
}

} // namespace Boidsish
