#include "modules/style.h"
#include "modules/style_properties.def"
#include "parsers/am_theme/am_theme_parser.h"
#include "utils/am_assert.h"

#include <algorithm>
#include <array>
#include <utility>

namespace Amethyst {

static Style s_instance;

static void s_applySparse(DenseSet &d, const SparseSet &s)
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
#define X(PROP, key, Type, tag, dflt) m_defaults[static_cast<size_t>(StyleProperty::PROP)] = StyleValue(dflt);
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

void Style::addTypeValue(ComponentType type, StyleProperty prop, const StyleValue &value)
{
    m_rawType[type].push_back({prop, value});
}

void Style::addClassRule(StyleKey classToken, uint32_t order, StyleProperty prop, const StyleValue &value, uint16_t state)
{
    Rule &rule = m_classRules[ClassKey{classToken, state}];
    rule.decls.push_back({prop, value});
    rule.order = order;
}

void Style::addTypeClassRule(ComponentType type, StyleKey classToken, uint32_t order, StyleProperty prop, const StyleValue &value,
                             uint16_t state)
{
    Rule &rule = m_typeClassRules[TypeClassKey{type, classToken, state}];
    rule.decls.push_back({prop, value});
    rule.order = order;
}

void Style::addPartRule(ComponentPart part, uint32_t order, StyleProperty prop, const StyleValue &value, uint16_t state)
{
    Rule &rule = m_partRules[PartKey{part, state}];
    rule.decls.push_back({prop, value});
    rule.order = order;
}

void Style::addClassPartRule(StyleKey classToken, ComponentPart part, uint32_t order, StyleProperty prop, const StyleValue &value,
                             uint16_t state)
{
    Rule &rule = m_classPartRules[ClassPartKey{classToken, part, state}];
    rule.decls.push_back({prop, value});
    rule.order = order;
}

const std::unordered_map<std::string, ComponentPart> &Style::getPartNames()
{
    static const std::unordered_map<std::string, ComponentPart> names = {
        {"tab", ComponentPart::TAB},
        {"entry", ComponentPart::ENTRY},
        {"header", ComponentPart::HEADER},
        {"indicator", ComponentPart::INDICATOR},
        {"action", ComponentPart::ACTION},
        {"toggle", ComponentPart::TOGGLE},
        {"radio", ComponentPart::RADIO},
        {"separator", ComponentPart::SEPARATOR},
        {"submenu", ComponentPart::SUBMENU},
    };
    return names;
}

void Style::clearResolved()
{
    m_typeBaked.clear();
    m_lru.clear();
    m_cacheIndex.clear();
}

const DenseSet &Style::bakedFor(ComponentType type)
{
    auto found = m_typeBaked.find(type);
    if (found != m_typeBaked.end()) {
        return found->second;
    }

    DenseSet d = m_defaults;
    std::span<const ComponentType> hierarchy = getTypeHierarchy(type);
    for (auto it = hierarchy.rbegin(); it != hierarchy.rend(); ++it) {
        auto raw = m_rawType.find(*it);
        if (raw != m_rawType.end()) {
            s_applySparse(d, raw->second);
        }
    }

    auto [inserted, _] = m_typeBaked.emplace(type, std::move(d));
    return inserted->second;
}

const DenseSet &Style::resolveSet(ComponentType type, std::span<const StyleKey> classes, uint16_t state, ComponentPart part)
{
    if (classes.empty() && part == ComponentPart::NONE) {
        return bakedFor(type);
    }

    CacheKey key;
    key.type = type;
    key.classes.assign(classes.begin(), classes.end());
    std::sort(key.classes.begin(), key.classes.end());
    key.state = state;
    key.part = part;

    auto cached = m_cacheIndex.find(key);
    if (cached != m_cacheIndex.end()) {
        m_lru.splice(m_lru.begin(), m_lru, cached->second);
        return m_lru.front().second;
    }

    DenseSet d = bakedFor(type);

    // Specificity triple (a, b, c): a = parts, b = classes+pseudos, c = types. Later application wins,
    // so contributors are gathered here and applied in ascending specificity order below.
    struct Contributor {
        int a;
        int b;
        int c;
        int depth; // type-hierarchy derivedness; only meaningful for type-qualified contributors
        uint32_t order;
        const SparseSet *set;
    };
    std::vector<Contributor> contributors;

    std::span<const ComponentType> hierarchy = getTypeHierarchy(type);

    // Probes only the tokens this node carries (its own classes, its part), never the full rule
    // tables, so per-node cost is independent of theme size.
    auto gatherForState = [&](uint16_t st, int pseudoBonus) {
        for (StyleKey cls : classes) {
            auto it = m_classRules.find(ClassKey{cls, st});
            if (it != m_classRules.end()) {
                contributors.push_back({0, 1 + pseudoBonus, 0, 0, it->second.order, &it->second.decls});
            }
        }
        for (StyleKey cls : classes) {
            for (size_t i = 0; i < hierarchy.size(); ++i) {
                auto it = m_typeClassRules.find(TypeClassKey{hierarchy[i], cls, st});
                if (it != m_typeClassRules.end()) {
                    int depth = static_cast<int>(hierarchy.size() - 1 - i);
                    contributors.push_back({0, 1 + pseudoBonus, 1, depth, it->second.order, &it->second.decls});
                }
            }
        }
        if (part != ComponentPart::NONE) {
            auto it = m_partRules.find(PartKey{part, st});
            if (it != m_partRules.end()) {
                contributors.push_back({1, 0 + pseudoBonus, 1, 0, it->second.order, &it->second.decls});
            }
            for (StyleKey cls : classes) {
                auto it2 = m_classPartRules.find(ClassPartKey{cls, part, st});
                if (it2 != m_classPartRules.end()) {
                    contributors.push_back({1, 1 + pseudoBonus, 0, 0, it2->second.order, &it2->second.decls});
                }
            }
        }
    };

    gatherForState(GUI_STATE_NONE, 0);
    // Walk each active pseudo bit individually so "hovered and pressed at once" matches both an
    // authored :hover and :pressed rule, each gaining +1 in the b (classes+pseudos) slot.
    for (uint16_t remaining = state; remaining != GUI_STATE_NONE;) {
        uint16_t bit = static_cast<uint16_t>(remaining & static_cast<uint16_t>(-static_cast<int16_t>(remaining)));
        remaining = static_cast<uint16_t>(remaining & ~bit);
        gatherForState(bit, 1);
    }

    std::sort(contributors.begin(), contributors.end(), [](const Contributor &x, const Contributor &y) {
        if (x.a != y.a) {
            return x.a < y.a;
        }
        if (x.b != y.b) {
            return x.b < y.b;
        }
        if (x.c != y.c) {
            return x.c < y.c;
        }
        if (x.depth != y.depth) {
            return x.depth < y.depth;
        }
        return x.order < y.order;
    });

    for (const Contributor &c : contributors) {
        s_applySparse(d, *c.set);
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
    h = h * 1099511628211ull ^ k.state;
    h = h * 1099511628211ull ^ static_cast<size_t>(k.part);
    return h;
}

size_t Style::ClassKeyHash::operator()(const ClassKey &k) const
{
    size_t h = std::hash<size_t>{}(k.cls);
    h = h * 1099511628211ull ^ k.state;
    return h;
}

size_t Style::TypeClassKeyHash::operator()(const TypeClassKey &k) const
{
    size_t h = std::hash<size_t>{}(static_cast<size_t>(k.type));
    h = h * 1099511628211ull ^ k.cls;
    h = h * 1099511628211ull ^ k.state;
    return h;
}

size_t Style::PartKeyHash::operator()(const PartKey &k) const
{
    size_t h = std::hash<size_t>{}(static_cast<size_t>(k.part));
    h = h * 1099511628211ull ^ k.state;
    return h;
}

size_t Style::ClassPartKeyHash::operator()(const ClassPartKey &k) const
{
    size_t h = std::hash<size_t>{}(k.cls);
    h = h * 1099511628211ull ^ static_cast<size_t>(k.part);
    h = h * 1099511628211ull ^ k.state;
    return h;
}

BaseStyleProperties Style::getBaseStyle(ComponentType type, std::span<const StyleKey> classes, uint16_t state, ComponentPart part)
{
    const DenseSet &d = resolveSet(type, classes, state, part);
    BaseStyleProperties r;
#define X(PROP, field, Type) r.field = std::get<Type>(d[static_cast<size_t>(StyleProperty::PROP)]);
    AM_BASE_STYLE_FIELDS(X)
#undef X
    return r;
}

TextStyleProperties Style::getTextStyle(ComponentType type, std::span<const StyleKey> classes, uint16_t state, ComponentPart part)
{
    const DenseSet &d = resolveSet(type, classes, state, part);
    TextStyleProperties r;
#define X(PROP, field, Type) r.field = std::get<Type>(d[static_cast<size_t>(StyleProperty::PROP)]);
    AM_TEXT_STYLE_FIELDS(X)
#undef X
    r.fontFamily = fontName(std::get<FontHandle>(d[static_cast<size_t>(StyleProperty::FONT_FAMILY)]));
    return r;
}

ScrollingFrameStyleProperties Style::getScrollingFrameStyle(ComponentType type, std::span<const StyleKey> classes, uint16_t state,
                                                            ComponentPart part)
{
    const DenseSet &d = resolveSet(type, classes, state, part);
    ScrollingFrameStyleProperties r;
#define X(PROP, field, Type) r.field = std::get<Type>(d[static_cast<size_t>(StyleProperty::PROP)]);
    AM_SCROLLING_FRAME_STYLE_FIELDS(X)
#undef X
    return r;
}

SliderStyleProperties Style::getSliderStyle(ComponentType type, std::span<const StyleKey> classes, uint16_t state,
                                            ComponentPart part)
{
    const DenseSet &d = resolveSet(type, classes, state, part);
    SliderStyleProperties r;
#define X(PROP, field, Type) r.field = std::get<Type>(d[static_cast<size_t>(StyleProperty::PROP)]);
    AM_SLIDER_STYLE_FIELDS(X)
#undef X
    r.thumb.backgroundColor = std::get<Color3>(d[static_cast<size_t>(StyleProperty::THUMB_COLOR)]);
    r.thumb.backgroundTransparency = std::get<float>(d[static_cast<size_t>(StyleProperty::THUMB_TRANSPARENCY)]);
    r.thumb.cornerRadius = std::get<float>(d[static_cast<size_t>(StyleProperty::THUMB_CORNER_RADIUS)]);
    r.text = getTextStyle(type, classes, state, part);
    return r;
}

DragStyleProperties Style::getDragStyle(ComponentType type, std::span<const StyleKey> classes, uint16_t state, ComponentPart part)
{
    DragStyleProperties r;
    r.text = getTextStyle(type, classes, state, part);
    return r;
}

TabBarStyleProperties Style::getTabBarStyle(ComponentType type, std::span<const StyleKey> classes, uint16_t state,
                                            ComponentPart part)
{
    const DenseSet &d = resolveSet(type, classes, state, part);
    TabBarStyleProperties r;
#define X(PROP, field, Type) r.field = std::get<Type>(d[static_cast<size_t>(StyleProperty::PROP)]);
    AM_TAB_BAR_STYLE_FIELDS(X)
#undef X
    return r;
}

TableStyleProperties Style::getTableStyle(ComponentType type, std::span<const StyleKey> classes, uint16_t state, ComponentPart part)
{
    const DenseSet &d = resolveSet(type, classes, state, part);
    TableStyleProperties r;
#define X(PROP, field, Type) r.field = std::get<Type>(d[static_cast<size_t>(StyleProperty::PROP)]);
    AM_TABLE_STYLE_FIELDS(X)
#undef X
    return r;
}

TreeViewStyleProperties Style::getTreeViewStyle(ComponentType type, std::span<const StyleKey> classes, uint16_t state,
                                                ComponentPart part)
{
    const DenseSet &d = resolveSet(type, classes, state, part);
    TreeViewStyleProperties r;
#define X(PROP, field, Type) r.field = std::get<Type>(d[static_cast<size_t>(StyleProperty::PROP)]);
    AM_TREE_VIEW_STYLE_FIELDS(X)
#undef X
    return r;
}

CheckboxStyleProperties Style::getCheckboxStyle(ComponentType type, std::span<const StyleKey> classes, uint16_t state,
                                                ComponentPart part)
{
    const DenseSet &d = resolveSet(type, classes, state, part);
    CheckboxStyleProperties r;
#define X(PROP, field, Type) r.field = std::get<Type>(d[static_cast<size_t>(StyleProperty::PROP)]);
    AM_CHECKBOX_STYLE_FIELDS(X)
#undef X
    return r;
}

CollapsibleHeaderStyleProperties Style::getCollapsibleHeaderStyle(ComponentType type, std::span<const StyleKey> classes,
                                                                  uint16_t state, ComponentPart part)
{
    const DenseSet &d = resolveSet(type, classes, state, part);
    CollapsibleHeaderStyleProperties r;
#define X(PROP, field, Type) r.field = std::get<Type>(d[static_cast<size_t>(StyleProperty::PROP)]);
    AM_COLLAPSIBLE_HEADER_STYLE_FIELDS(X)
#undef X
    r.titleStyle.fontSize = std::get<float>(d[static_cast<size_t>(StyleProperty::FONT_SIZE)]);
    r.titleStyle.textColor = std::get<Color4>(d[static_cast<size_t>(StyleProperty::TEXT_COLOR)]);
    r.titleStyle.textXAlignment = std::get<TextXAlignment>(d[static_cast<size_t>(StyleProperty::TEXT_X_ALIGNMENT)]);
    r.titleStyle.textYAlignment = std::get<TextYAlignment>(d[static_cast<size_t>(StyleProperty::TEXT_Y_ALIGNMENT)]);
    r.titleStyle.fontFamily = fontName(std::get<FontHandle>(d[static_cast<size_t>(StyleProperty::FONT_FAMILY)]));
    return r;
}

TextInputStyleProperties Style::getTextInputStyle(ComponentType type, std::span<const StyleKey> classes, uint16_t state,
                                                  ComponentPart part)
{
    const DenseSet &d = resolveSet(type, classes, state, part);
    TextInputStyleProperties r;
    r.text = getTextStyle(type, classes, state, part);
#define X(PROP, field, Type) r.field = std::get<Type>(d[static_cast<size_t>(StyleProperty::PROP)]);
    AM_TEXT_INPUT_STYLE_FIELDS(X)
#undef X
    return r;
}

ImageStyleProperties Style::getImageStyle(ComponentType type, std::span<const StyleKey> classes, uint16_t state, ComponentPart part)
{
    const DenseSet &d = resolveSet(type, classes, state, part);
    ImageStyleProperties r;
#define X(PROP, field, Type) r.field = std::get<Type>(d[static_cast<size_t>(StyleProperty::PROP)]);
    AM_IMAGE_STYLE_FIELDS(X)
#undef X
    return r;
}

MenuBarStyleProperties Style::getMenuBarStyle(ComponentType type, std::span<const StyleKey> classes, uint16_t state,
                                              ComponentPart part)
{
    const DenseSet &d = resolveSet(type, classes, state, part);
    MenuBarStyleProperties r;
#define X(PROP, field, Type) r.field = std::get<Type>(d[static_cast<size_t>(StyleProperty::PROP)]);
    AM_MENU_BAR_STYLE_FIELDS(X)
#undef X
    return r;
}

SplineStyleProperties Style::getSplineStyle(ComponentType type, std::span<const StyleKey> classes, uint16_t state,
                                            ComponentPart part)
{
    const DenseSet &d = resolveSet(type, classes, state, part);
    SplineStyleProperties r;
#define X(PROP, field, Type) r.field = std::get<Type>(d[static_cast<size_t>(StyleProperty::PROP)]);
    AM_SPLINE_STYLE_FIELDS(X)
#undef X
    return r;
}

ContextMenuStyleProperties Style::getContextMenuStyle(ComponentType type, std::span<const StyleKey> classes, uint16_t state,
                                                      ComponentPart part)
{
    const DenseSet &d = resolveSet(type, classes, state, part);
    ContextMenuStyleProperties r;
#define X(PROP, field, Type) r.field = std::get<Type>(d[static_cast<size_t>(StyleProperty::PROP)]);
    AM_CONTEXT_MENU_STYLE_FIELDS(X)
#undef X
    return r;
}

DropdownStyleProperties Style::getDropdownStyle(ComponentType type, std::span<const StyleKey> classes, uint16_t state,
                                                ComponentPart part)
{
    const DenseSet &d = resolveSet(type, classes, state, part);
    DropdownStyleProperties r;
#define X(PROP, field, Type) r.field = std::get<Type>(d[static_cast<size_t>(StyleProperty::PROP)]);
    AM_DROPDOWN_STYLE_FIELDS(X)
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
    static const std::array<ComponentType, 2> textInput = {ComponentType::TEXT_INPUT, ComponentType::UI_OBJECT};
    static const std::array<ComponentType, 2> drag = {ComponentType::DRAG, ComponentType::UI_OBJECT};
    static const std::array<ComponentType, 3> menuBar = {ComponentType::MENU_BAR, ComponentType::FRAME, ComponentType::UI_OBJECT};
    static const std::array<ComponentType, 2> spline = {ComponentType::SPLINE, ComponentType::UI_OBJECT};
    static const std::array<ComponentType, 3> contextMenu = {ComponentType::CONTEXT_MENU, ComponentType::FRAME,
                                                             ComponentType::UI_OBJECT};

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
    case ComponentType::TEXT_INPUT:
        return textInput;
    case ComponentType::DRAG:
        return drag;
    case ComponentType::MENU_BAR:
        return menuBar;
    case ComponentType::SPLINE:
        return spline;
    case ComponentType::CONTEXT_MENU:
        return contextMenu;
    }
    return uiObject;
}

const std::unordered_map<std::string, ComponentType> &Style::getComponentTypeNames()
{
    static const std::unordered_map<std::string, ComponentType> names = {
        {"ui-object", ComponentType::UI_OBJECT},
        {"ui-button", ComponentType::UI_BUTTON},
        {"ui-label", ComponentType::UI_LABEL},
        {"frame", ComponentType::FRAME},
        {"scrolling-frame", ComponentType::SCROLLING_FRAME},
        {"table", ComponentType::TABLE},
        {"tree-view", ComponentType::TREE_VIEW},
        {"text-button", ComponentType::TEXT_BUTTON},
        {"image-button", ComponentType::IMAGE_BUTTON},
        {"text-label", ComponentType::TEXT_LABEL},
        {"image-label", ComponentType::IMAGE_LABEL},
        {"canvas", ComponentType::CANVAS},
        {"checkbox", ComponentType::CHECKBOX},
        {"dropdown", ComponentType::DROPDOWN},
        {"tab-bar", ComponentType::TAB_BAR},
        {"slider", ComponentType::SLIDER},
        {"radio-button", ComponentType::RADIO_BUTTON},
        {"collapsible-header", ComponentType::COLLAPSIBLE_HEADER},
        {"text-input", ComponentType::TEXT_INPUT},
        {"drag", ComponentType::DRAG},
        {"menu-bar", ComponentType::MENU_BAR},
        {"spline", ComponentType::SPLINE},
        {"context-menu", ComponentType::CONTEXT_MENU},
    };
    return names;
}

} // namespace Amethyst
