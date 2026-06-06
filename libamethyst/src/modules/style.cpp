#include "modules/style.h"
#include "parsers/am_theme/am_theme_parser.h"
#include "utils/am_assert.h"

#include <algorithm>
#include <array>
#include <utility>

namespace Amethyst {

static Style s_instance;

static void applySparse(DenseSet &d, const SparseSet &s)
{
    for (auto &[p, v] : s) {
        d[static_cast<size_t>(p)] = v;
    }
}

Style::Style()
{
    m_fontNames.push_back("default");
    m_fontIndex["default"] = 0;
    buildDefaults();
}

Style &Style::instance()
{
    return s_instance;
}

bool Style::load(const std::filesystem::path &path)
{
    auto result = AmThemeParser::parseFile(path);
    if (result) {
        s_instance = std::move(*result);
        return true;
    }
    return false;
}

void Style::buildDefaults()
{
#define X(PROP, key, Type, dflt) m_defaults[static_cast<size_t>(StyleProperty::PROP)] = StyleValue(dflt);
    AM_STYLE_PROPS(X)
#undef X
}

uint32_t Style::internFont(std::string_view name)
{
    auto it = m_fontIndex.find(std::string(name));
    if (it != m_fontIndex.end()) {
        return it->second;
    }
    uint32_t id = static_cast<uint32_t>(m_fontNames.size());
    m_fontNames.emplace_back(name);
    m_fontIndex[std::string(name)] = id;
    return id;
}

const std::string &Style::fontName(FontHandle handle) const
{
    if (handle.id < m_fontNames.size()) {
        return m_fontNames[handle.id];
    }
    return m_fontNames[0];
}

StyleKey Style::classToken(std::string_view name)
{
    StyleKey h = 2166136261u;
    for (char c : name) {
        h ^= static_cast<uint8_t>(c);
        h *= 16777619u;
    }
    return h;
}

void Style::registerClassName(StyleKey token, std::string_view name)
{
    auto [it, inserted] = m_classNames.try_emplace(token, name);
    if (!inserted) {
        AM_ASSERT(it->second == name, "style class hash collision");
    }
}

uint64_t Style::typeClassKey(ComponentType type, StyleKey classToken)
{
    return (static_cast<uint64_t>(type) << 32) | classToken;
}

void Style::addTypeValue(ComponentType type, StyleProperty prop, const StyleValue &value)
{
    m_rawType[type].push_back({prop, value});
}

void Style::addClassValue(StyleKey classToken, uint32_t order, StyleProperty prop, const StyleValue &value)
{
    m_classSets[classToken].push_back({prop, value});
    m_classOrder[classToken] = order;
}

void Style::addTypeClassValue(ComponentType type, StyleKey classToken, uint32_t order, StyleProperty prop, const StyleValue &value)
{
    uint64_t key = typeClassKey(type, classToken);
    m_typeClassSets[key].push_back({prop, value});
    m_typeClassOrder[key] = order;
}

void Style::clearResolved()
{
    m_typeResolved.clear();
    m_lru.clear();
    m_cacheIndex.clear();
}

const DenseSet &Style::bakedFor(ComponentType type)
{
    auto found = m_typeResolved.find(type);
    if (found != m_typeResolved.end()) {
        return found->second;
    }

    DenseSet d = m_defaults;
    std::span<const ComponentType> hierarchy = getTypeHierarchy(type);
    for (auto it = hierarchy.rbegin(); it != hierarchy.rend(); ++it) {
        auto raw = m_rawType.find(*it);
        if (raw != m_rawType.end()) {
            applySparse(d, raw->second);
        }
    }

    auto [inserted, _] = m_typeResolved.emplace(type, std::move(d));
    return inserted->second;
}

const DenseSet &Style::resolveSet(ComponentType type, std::span<const StyleKey> classes)
{
    if (classes.empty()) {
        return bakedFor(type);
    }

    CacheKey key;
    key.type = type;
    key.classes.assign(classes.begin(), classes.end());
    std::sort(key.classes.begin(), key.classes.end());

    auto cached = m_cacheIndex.find(key);
    if (cached != m_cacheIndex.end()) {
        m_lru.splice(m_lru.begin(), m_lru, cached->second);
        return m_lru.front().second;
    }

    DenseSet d = bakedFor(type);

    struct Contributor {
        int tier;
        int depth;
        uint32_t order;
        const SparseSet *set;
    };
    std::vector<Contributor> contributors;

    for (StyleKey c : classes) {
        auto it = m_classSets.find(c);
        if (it != m_classSets.end()) {
            contributors.push_back({1, 0, m_classOrder[c], &it->second});
        }
    }

    std::span<const ComponentType> hierarchy = getTypeHierarchy(type);
    for (StyleKey c : classes) {
        for (size_t i = 0; i < hierarchy.size(); ++i) {
            uint64_t tcKey = typeClassKey(hierarchy[i], c);
            auto it = m_typeClassSets.find(tcKey);
            if (it != m_typeClassSets.end()) {
                int depth = static_cast<int>(hierarchy.size() - 1 - i);
                contributors.push_back({2, depth, m_typeClassOrder[tcKey], &it->second});
            }
        }
    }

    std::sort(contributors.begin(), contributors.end(), [](const Contributor &a, const Contributor &b) {
        if (a.tier != b.tier) {
            return a.tier < b.tier;
        }
        if (a.depth != b.depth) {
            return a.depth < b.depth;
        }
        return a.order < b.order;
    });

    for (const Contributor &c : contributors) {
        applySparse(d, *c.set);
    }

    m_lru.push_front({key, std::move(d)});
    m_cacheIndex[key] = m_lru.begin();
    if (m_lru.size() > CACHE_CAP) {
        m_cacheIndex.erase(m_lru.back().first);
        m_lru.pop_back();
    }
    return m_lru.front().second;
}

size_t Style::CacheKeyHash::operator()(const CacheKey &k) const
{
    size_t h = std::hash<size_t>{}(static_cast<size_t>(k.type));
    for (StyleKey c : k.classes) {
        h = h * 1099511628211ull ^ c;
    }
    return h;
}

BaseStyleProperties Style::getBaseStyle(ComponentType type, std::span<const StyleKey> classes)
{
    const DenseSet &d = resolveSet(type, classes);
    BaseStyleProperties r;
#define X(PROP, field, Type) r.field = std::get<Type>(d[static_cast<size_t>(StyleProperty::PROP)]);
    AM_BASE_STYLE_FIELDS(X)
#undef X
    return r;
}

TextStyleProperties Style::getTextStyle(ComponentType type, std::span<const StyleKey> classes)
{
    const DenseSet &d = resolveSet(type, classes);
    TextStyleProperties r;
#define X(PROP, field, Type) r.field = std::get<Type>(d[static_cast<size_t>(StyleProperty::PROP)]);
    AM_TEXT_STYLE_FIELDS(X)
#undef X
    r.fontFamily = fontName(std::get<FontHandle>(d[static_cast<size_t>(StyleProperty::FONT_FAMILY)]));
    return r;
}

ScrollingFrameStyleProperties Style::getScrollingFrameStyle(ComponentType type, std::span<const StyleKey> classes)
{
    const DenseSet &d = resolveSet(type, classes);
    ScrollingFrameStyleProperties r;
#define X(PROP, field, Type) r.field = std::get<Type>(d[static_cast<size_t>(StyleProperty::PROP)]);
    AM_SCROLLING_FRAME_STYLE_FIELDS(X)
#undef X
    return r;
}

SliderStyleProperties Style::getSliderStyle(ComponentType type, std::span<const StyleKey> classes)
{
    const DenseSet &d = resolveSet(type, classes);
    SliderStyleProperties r;
#define X(PROP, field, Type) r.field = std::get<Type>(d[static_cast<size_t>(StyleProperty::PROP)]);
    AM_SLIDER_STYLE_FIELDS(X)
#undef X
    return r;
}

TabBarStyleProperties Style::getTabBarStyle(ComponentType type, std::span<const StyleKey> classes)
{
    const DenseSet &d = resolveSet(type, classes);
    TabBarStyleProperties r;
#define X(PROP, field, Type) r.field = std::get<Type>(d[static_cast<size_t>(StyleProperty::PROP)]);
    AM_TAB_BAR_STYLE_FIELDS(X)
#undef X
    return r;
}

TableStyleProperties Style::getTableStyle(ComponentType type, std::span<const StyleKey> classes)
{
    const DenseSet &d = resolveSet(type, classes);
    TableStyleProperties r;
#define X(PROP, field, Type) r.field = std::get<Type>(d[static_cast<size_t>(StyleProperty::PROP)]);
    AM_TABLE_STYLE_FIELDS(X)
#undef X
    return r;
}

TreeViewStyleProperties Style::getTreeViewStyle(ComponentType type, std::span<const StyleKey> classes)
{
    const DenseSet &d = resolveSet(type, classes);
    TreeViewStyleProperties r;
#define X(PROP, field, Type) r.field = std::get<Type>(d[static_cast<size_t>(StyleProperty::PROP)]);
    AM_TREE_VIEW_STYLE_FIELDS(X)
#undef X
    return r;
}

CheckboxStyleProperties Style::getCheckboxStyle(ComponentType type, std::span<const StyleKey> classes)
{
    const DenseSet &d = resolveSet(type, classes);
    CheckboxStyleProperties r;
#define X(PROP, field, Type) r.field = std::get<Type>(d[static_cast<size_t>(StyleProperty::PROP)]);
    AM_CHECKBOX_STYLE_FIELDS(X)
#undef X
    return r;
}

CollapsibleHeaderStyleProperties Style::getCollapsibleHeaderStyle(ComponentType type, std::span<const StyleKey> classes)
{
    const DenseSet &d = resolveSet(type, classes);
    CollapsibleHeaderStyleProperties r;
#define X(PROP, field, Type) r.field = std::get<Type>(d[static_cast<size_t>(StyleProperty::PROP)]);
    AM_COLLAPSIBLE_HEADER_STYLE_FIELDS(X)
#undef X
    return r;
}

std::span<const ComponentType> Style::getTypeHierarchy(ComponentType type)
{
    static const std::array<ComponentType, 1> uiObject = {ComponentType::UI_OBJECT};
    static const std::array<ComponentType, 2> uiButton = {ComponentType::UI_BUTTON, ComponentType::UI_OBJECT};
    static const std::array<ComponentType, 2> uiLabel = {ComponentType::UI_LABEL, ComponentType::UI_OBJECT};

    static const std::array<ComponentType, 2> frame = {ComponentType::FRAME, ComponentType::UI_OBJECT};
    static const std::array<ComponentType, 2> scrollingFrame = {ComponentType::SCROLLING_FRAME, ComponentType::UI_OBJECT};
    static const std::array<ComponentType, 2> table = {ComponentType::TABLE, ComponentType::UI_OBJECT};
    static const std::array<ComponentType, 2> treeView = {ComponentType::TREE_VIEW, ComponentType::UI_OBJECT};
    static const std::array<ComponentType, 2> canvas = {ComponentType::CANVAS, ComponentType::UI_OBJECT};

    static const std::array<ComponentType, 3> textButton = {ComponentType::TEXT_BUTTON, ComponentType::UI_BUTTON,
                                                            ComponentType::UI_OBJECT};
    static const std::array<ComponentType, 3> imageButton = {ComponentType::IMAGE_BUTTON, ComponentType::UI_BUTTON,
                                                             ComponentType::UI_OBJECT};

    static const std::array<ComponentType, 3> textLabel = {ComponentType::TEXT_LABEL, ComponentType::UI_LABEL,
                                                           ComponentType::UI_OBJECT};
    static const std::array<ComponentType, 3> imageLabel = {ComponentType::IMAGE_LABEL, ComponentType::UI_LABEL,
                                                            ComponentType::UI_OBJECT};

    static const std::array<ComponentType, 3> checkbox = {ComponentType::CHECKBOX, ComponentType::UI_BUTTON,
                                                          ComponentType::UI_OBJECT};
    static const std::array<ComponentType, 3> dropdown = {ComponentType::DROPDOWN, ComponentType::UI_BUTTON,
                                                          ComponentType::UI_OBJECT};
    static const std::array<ComponentType, 2> tabBar = {ComponentType::TAB_BAR, ComponentType::UI_OBJECT};
    static const std::array<ComponentType, 2> slider = {ComponentType::SLIDER, ComponentType::UI_OBJECT};
    static const std::array<ComponentType, 3> radioButton = {ComponentType::RADIO_BUTTON, ComponentType::UI_BUTTON,
                                                             ComponentType::UI_OBJECT};
    static const std::array<ComponentType, 2> collapsibleHeader = {ComponentType::COLLAPSIBLE_HEADER, ComponentType::UI_OBJECT};

    switch (type) {
    case ComponentType::UI_OBJECT:
        return uiObject;
    case ComponentType::UI_BUTTON:
        return uiButton;
    case ComponentType::UI_LABEL:
        return uiLabel;
    case ComponentType::FRAME:
        return frame;
    case ComponentType::SCROLLING_FRAME:
        return scrollingFrame;
    case ComponentType::TABLE:
        return table;
    case ComponentType::TREE_VIEW:
        return treeView;
    case ComponentType::TEXT_BUTTON:
        return textButton;
    case ComponentType::IMAGE_BUTTON:
        return imageButton;
    case ComponentType::TEXT_LABEL:
        return textLabel;
    case ComponentType::IMAGE_LABEL:
        return imageLabel;
    case ComponentType::CANVAS:
        return canvas;
    case ComponentType::CHECKBOX:
        return checkbox;
    case ComponentType::DROPDOWN:
        return dropdown;
    case ComponentType::TAB_BAR:
        return tabBar;
    case ComponentType::SLIDER:
        return slider;
    case ComponentType::RADIO_BUTTON:
        return radioButton;
    case ComponentType::COLLAPSIBLE_HEADER:
        return collapsibleHeader;
    }
    return uiObject;
}

const std::unordered_map<std::string, ComponentType> &Style::getComponentTypeNames()
{
    static const std::unordered_map<std::string, ComponentType> names = {
        {"general", ComponentType::UI_OBJECT},
        {"buttons", ComponentType::UI_BUTTON},
        {"labels", ComponentType::UI_LABEL},
        {"frames", ComponentType::FRAME},
        {"scrollingFrames", ComponentType::SCROLLING_FRAME},
        {"tables", ComponentType::TABLE},
        {"treeViews", ComponentType::TREE_VIEW},
        {"textButtons", ComponentType::TEXT_BUTTON},
        {"imageButtons", ComponentType::IMAGE_BUTTON},
        {"textLabels", ComponentType::TEXT_LABEL},
        {"imageLabels", ComponentType::IMAGE_LABEL},
        {"canvases", ComponentType::CANVAS},
        {"checkboxes", ComponentType::CHECKBOX},
        {"dropdowns", ComponentType::DROPDOWN},
        {"tabBars", ComponentType::TAB_BAR},
        {"sliders", ComponentType::SLIDER},
        {"radioButtons", ComponentType::RADIO_BUTTON},
        {"collapsibleHeaders", ComponentType::COLLAPSIBLE_HEADER},
    };
    return names;
}

} // namespace Amethyst
