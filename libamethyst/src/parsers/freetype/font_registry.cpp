#include "parsers/freetype/font_registry.h"

#include "logging/log.h"

namespace Amethyst {

static FontRegistry s_instance;

// A file that declares no style, or declares itself Regular, is named by its family alone.
static std::string s_declaredName(const FontDescription &description)
{
    if (description.style.empty() || description.style == "Regular") {
        return description.family;
    }
    return description.family + " " + description.style;
}

FontRegistry &FontRegistry::instance()
{
    return s_instance;
}

void FontRegistry::shutdown()
{
    m_fontFaces.clear();
    m_default = FontId{};
}

FontId FontRegistry::loadFont(const std::string &path)
{
    auto loader = std::make_unique<FontLoader>();
    if (!loader->loadFont(path)) {
        return FontId{};
    }

    std::string name = s_declaredName(loader->getDescription());
    if (name.empty()) {
        AM_LOG_ERROR("Font at {} declares no family name, load it under an explicit one", path);
        return FontId{};
    }

    return addFontFace(std::move(name), std::move(loader));
}

FontId FontRegistry::loadFont(std::string_view name, const std::string &path)
{
    FontId existing = findFont(name);
    if (existing.isValid()) {
        return existing;
    }

    auto loader = std::make_unique<FontLoader>();
    if (!loader->loadFont(path)) {
        return FontId{};
    }

    return addFontFace(std::string(name), std::move(loader));
}

FontId FontRegistry::addFontFace(std::string name, std::unique_ptr<FontLoader> loader)
{
    FontId existing = findFont(name);
    if (existing.isValid()) {
        return existing;
    }

    m_fontFaces.push_back({std::move(name), std::move(loader)});

    FontId id{static_cast<uint16_t>(m_fontFaces.size() - 1)};
    if (!m_default.isValid()) {
        m_default = id;
    }
    return id;
}

FontId FontRegistry::findFont(std::string_view name) const
{
    for (size_t i = 0; i < m_fontFaces.size(); i++) {
        if (m_fontFaces[i].name == name) {
            return FontId{static_cast<uint16_t>(i)};
        }
    }
    return FontId{};
}

void FontRegistry::setDefaultFont(FontId id)
{
    if (id.index < m_fontFaces.size()) {
        m_default = id;
    }
}

FontLoader *FontRegistry::getLoader(FontId id)
{
    FontId resolved = resolveFont(id);
    if (!resolved.isValid()) {
        return nullptr;
    }
    return m_fontFaces[resolved.index].loader.get();
}

FontId FontRegistry::resolveFont(FontId id) const
{
    if (id.index < m_fontFaces.size()) {
        return id;
    }
    if (m_default.index < m_fontFaces.size()) {
        return m_default;
    }
    return FontId{};
}

} // namespace Amethyst
