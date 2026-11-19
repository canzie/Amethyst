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

static uint64_t s_generation = 0;

bool Style::load(const std::filesystem::path &path)
{
    auto result = AmThemeParser::parseFile(path);
    if (result) {
        install(std::move(*result));
        return true;
    }
    return false;
}

bool Style::loadFromString(const std::string &source)
{
    auto result = AmThemeParser::parseString(source);
    if (result) {
        install(std::move(*result));
        return true;
    }
    return false;
}

void Style::install(Style &&style)
{
    s_instance = std::move(style);
    ++s_generation;
}

uint64_t Style::generation()
{
    return s_generation;
}

ComponentType defaultPartOwner(ComponentPart part)
{
    switch (part) {
    case ComponentPart::TAB:
    case ComponentPart::CLOSE:
        return ComponentType::TAB_BAR;
    case ComponentPart::ENTRY:
        return ComponentType::MENU_BAR;
    case ComponentPart::HEADER:
    case ComponentPart::INDICATOR:
        return ComponentType::COLLAPSIBLE_HEADER;
    case ComponentPart::ACTION:
    case ComponentPart::TOGGLE:
    case ComponentPart::RADIO:
    case ComponentPart::SEPARATOR:
    case ComponentPart::SUBMENU:
    case ComponentPart::NUMBER:
        return ComponentType::CONTEXT_MENU;
    case ComponentPart::NONE:
        break;
    }
    return ComponentType::UI_OBJECT;
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

void Style::addRule(const StyleSelector &selector, uint32_t order, StyleProperty prop, const StyleValue &value)
{
    bool hasClass = selector.cls.has_value();
    if (selector.type.has_value() && !hasClass && selector.part == ComponentPart::NONE && selector.state == GUI_STATE_NONE) {
        addTypeValue(*selector.type, prop, value);
        return;
    }

    RuleKey key;
    key.type = selector.type.has_value() ? static_cast<int32_t>(*selector.type) : -1;
    key.part = selector.part;
    key.state = selector.state;
    key.hasClass = hasClass;
    key.cls = hasClass ? *selector.cls : 0;

    if (key.type >= 0 && !hasClass && key.part == ComponentPart::NONE) {
        m_hasTypeStateRules = true;
    }

    // one Rule per source block: merging repeated selectors into a single rule would let a later
    // block drag an earlier block's declarations past rules authored in between
    std::vector<Rule> &rules = m_rules[key];
    if (rules.empty() || rules.back().order != order) {
        rules.push_back(Rule{{}, order});
    }
    rules.back().decls.push_back({prop, value});
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
        {"number", ComponentPart::NUMBER},
        {"close", ComponentPart::CLOSE},
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

const DenseSet &Style::resolveSet(ComponentType type, std::span<const StyleKey> classes, uint16_t state, const StylePart &part)
{
    bool stateMatters = state != GUI_STATE_NONE && (m_hasTypeStateRules || !classes.empty());
    if (classes.empty() && part.part == ComponentPart::NONE && !stateMatters) {
        return bakedFor(type);
    }

    CacheKey key;
    key.type = type;
    key.classes.assign(classes.begin(), classes.end());
    std::sort(key.classes.begin(), key.classes.end());
    key.state = state;
    key.part = part.part;
    if (part.part != ComponentPart::NONE) {
        key.owner = part.owner;
        key.inherited = part.inheritedClasses;
        std::sort(key.inherited.begin(), key.inherited.end());
    }

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
    std::span<const ComponentType> ownerHierarchy = getTypeHierarchy(key.owner);

    // classes a part rule may match: the node's own plus whatever its owner handed down
    std::vector<StyleKey> partClasses;
    if (part.part != ComponentPart::NONE) {
        partClasses = key.classes;
        partClasses.insert(partClasses.end(), key.inherited.begin(), key.inherited.end());
        std::sort(partClasses.begin(), partClasses.end());
        partClasses.erase(std::unique(partClasses.begin(), partClasses.end()), partClasses.end());
    }

    auto probe = [&](const RuleKey &rk, int a, int b, int c, int depth) {
        auto it = m_rules.find(rk);
        if (it == m_rules.end()) {
            return;
        }
        for (const Rule &rule : it->second) {
            contributors.push_back({a, b, c, depth, rule.order, &rule.decls});
        }
    };
    auto probeTyped = [&](std::span<const ComponentType> chain, RuleKey rk, int a, int b) {
        for (size_t i = 0; i < chain.size(); ++i) {
            rk.type = static_cast<int32_t>(chain[i]);
            probe(rk, a, b, 1, static_cast<int>(chain.size() - 1 - i));
        }
    };

    // Probes only the tokens this node carries (its own classes, its part), never the full rule
    // tables, so per-node cost is independent of theme size.
    auto gatherForState = [&](uint16_t st, int pseudo) {
        if (st != GUI_STATE_NONE && m_hasTypeStateRules) {
            probeTyped(hierarchy, RuleKey{-1, ComponentPart::NONE, st, false, 0}, 0, pseudo);
        }
        for (StyleKey cls : key.classes) {
            RuleKey rk{-1, ComponentPart::NONE, st, true, cls};
            probe(rk, 0, 1 + pseudo, 0, 0);
            probeTyped(hierarchy, rk, 0, 1 + pseudo);
        }
        if (part.part != ComponentPart::NONE) {
            RuleKey rk{-1, part.part, st, false, 0};
            probe(rk, 1, pseudo, 0, 0);
            probeTyped(ownerHierarchy, rk, 1, pseudo);
            for (StyleKey cls : partClasses) {
                RuleKey crk{-1, part.part, st, true, cls};
                probe(crk, 1, 1 + pseudo, 0, 0);
                probeTyped(ownerHierarchy, crk, 1, 1 + pseudo);
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

    std::stable_sort(contributors.begin(), contributors.end(), [](const Contributor &x, const Contributor &y) {
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
    h = h * 1099511628211ull ^ static_cast<size_t>(k.owner);
    for (StyleKey c : k.inherited) {
        h = h * 1099511628211ull ^ c;
    }
    return h;
}

size_t Style::RuleKeyHash::operator()(const RuleKey &k) const
{
    size_t h = std::hash<size_t>{}(static_cast<size_t>(k.cls));
    h = h * 1099511628211ull ^ static_cast<size_t>(static_cast<uint32_t>(k.type));
    h = h * 1099511628211ull ^ static_cast<size_t>(k.part);
    h = h * 1099511628211ull ^ k.state;
    h = h * 1099511628211ull ^ static_cast<size_t>(k.hasClass);
    return h;
}

BaseStyleProperties Style::getBaseStyle(ComponentType type, std::span<const StyleKey> classes, uint16_t state,
                                        const StylePart &part)
{
    const DenseSet &d = resolveSet(type, classes, state, part);
    BaseStyleProperties r;
#define X(PROP, field, Type) r.field = std::get<Type>(d[static_cast<size_t>(StyleProperty::PROP)]);
    AM_BASE_STYLE_FIELDS(X)
#undef X
    return r;
}

UDim4 Style::getPadding(ComponentType type, std::span<const StyleKey> classes, uint16_t state, const StylePart &part)
{
    const DenseSet &d = resolveSet(type, classes, state, part);
    UDim4 r;
    r.top = std::get<UDim>(d[static_cast<size_t>(StyleProperty::PADDING_TOP)]);
    r.right = std::get<UDim>(d[static_cast<size_t>(StyleProperty::PADDING_RIGHT)]);
    r.bottom = std::get<UDim>(d[static_cast<size_t>(StyleProperty::PADDING_BOTTOM)]);
    r.left = std::get<UDim>(d[static_cast<size_t>(StyleProperty::PADDING_LEFT)]);
    return r;
}

TextStyleProperties Style::getTextStyle(ComponentType type, std::span<const StyleKey> classes, uint16_t state,
                                        const StylePart &part)
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
                                                            const StylePart &part)
{
    const DenseSet &d = resolveSet(type, classes, state, part);
    ScrollingFrameStyleProperties r;
#define X(PROP, field, Type) r.field = std::get<Type>(d[static_cast<size_t>(StyleProperty::PROP)]);
    AM_SCROLLING_FRAME_STYLE_FIELDS(X)
#undef X
    return r;
}

SliderStyleProperties Style::getSliderStyle(ComponentType type, std::span<const StyleKey> classes, uint16_t state,
                                            const StylePart &part)
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

DragStyleProperties Style::getDragStyle(ComponentType type, std::span<const StyleKey> classes, uint16_t state,
                                        const StylePart &part)
{
    DragStyleProperties r;
    r.text = getTextStyle(type, classes, state, part);
    return r;
}

TabBarStyleProperties Style::getTabBarStyle(ComponentType type, std::span<const StyleKey> classes, uint16_t state,
                                            const StylePart &part)
{
    const DenseSet &d = resolveSet(type, classes, state, part);
    TabBarStyleProperties r;
#define X(PROP, field, Type) r.field = std::get<Type>(d[static_cast<size_t>(StyleProperty::PROP)]);
    AM_TAB_BAR_STYLE_FIELDS(X)
#undef X
    return r;
}

TableStyleProperties Style::getTableStyle(ComponentType type, std::span<const StyleKey> classes, uint16_t state,
                                          const StylePart &part)
{
    const DenseSet &d = resolveSet(type, classes, state, part);
    TableStyleProperties r;
#define X(PROP, field, Type) r.field = std::get<Type>(d[static_cast<size_t>(StyleProperty::PROP)]);
    AM_TABLE_STYLE_FIELDS(X)
#undef X
    return r;
}

TreeViewStyleProperties Style::getTreeViewStyle(ComponentType type, std::span<const StyleKey> classes, uint16_t state,
                                                const StylePart &part)
{
    const DenseSet &d = resolveSet(type, classes, state, part);
    TreeViewStyleProperties r;
#define X(PROP, field, Type) r.field = std::get<Type>(d[static_cast<size_t>(StyleProperty::PROP)]);
    AM_TREE_VIEW_STYLE_FIELDS(X)
#undef X
    return r;
}

CheckboxStyleProperties Style::getCheckboxStyle(ComponentType type, std::span<const StyleKey> classes, uint16_t state,
                                                const StylePart &part)
{
    const DenseSet &d = resolveSet(type, classes, state, part);
    CheckboxStyleProperties r;
#define X(PROP, field, Type) r.field = std::get<Type>(d[static_cast<size_t>(StyleProperty::PROP)]);
    AM_CHECKBOX_STYLE_FIELDS(X)
#undef X
    return r;
}

CollapsibleHeaderStyleProperties Style::getCollapsibleHeaderStyle(ComponentType type, std::span<const StyleKey> classes,
                                                                  uint16_t state, const StylePart &part)
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
                                                  const StylePart &part)
{
    const DenseSet &d = resolveSet(type, classes, state, part);
    TextInputStyleProperties r;
    r.text = getTextStyle(type, classes, state, part);
#define X(PROP, field, Type) r.field = std::get<Type>(d[static_cast<size_t>(StyleProperty::PROP)]);
    AM_TEXT_INPUT_STYLE_FIELDS(X)
#undef X
    return r;
}

TextAreaStyleProperties Style::getTextAreaStyle(ComponentType type, std::span<const StyleKey> classes, uint16_t state,
                                                const StylePart &part)
{
    const DenseSet &d = resolveSet(type, classes, state, part);
    TextAreaStyleProperties r;
    r.text = getTextStyle(type, classes, state, part);
#define X(PROP, field, Type) r.field = std::get<Type>(d[static_cast<size_t>(StyleProperty::PROP)]);
    AM_TEXT_AREA_STYLE_FIELDS(X)
#undef X
    return r;
}

ImageStyleProperties Style::getImageStyle(ComponentType type, std::span<const StyleKey> classes, uint16_t state,
                                          const StylePart &part)
{
    const DenseSet &d = resolveSet(type, classes, state, part);
    ImageStyleProperties r;
#define X(PROP, field, Type) r.field = std::get<Type>(d[static_cast<size_t>(StyleProperty::PROP)]);
    AM_IMAGE_STYLE_FIELDS(X)
#undef X
    return r;
}

MenuBarStyleProperties Style::getMenuBarStyle(ComponentType type, std::span<const StyleKey> classes, uint16_t state,
                                              const StylePart &part)
{
    const DenseSet &d = resolveSet(type, classes, state, part);
    MenuBarStyleProperties r;
#define X(PROP, field, Type) r.field = std::get<Type>(d[static_cast<size_t>(StyleProperty::PROP)]);
    AM_MENU_BAR_STYLE_FIELDS(X)
#undef X
    return r;
}

SplineStyleProperties Style::getSplineStyle(ComponentType type, std::span<const StyleKey> classes, uint16_t state,
                                            const StylePart &part)
{
    const DenseSet &d = resolveSet(type, classes, state, part);
    SplineStyleProperties r;
#define X(PROP, field, Type) r.field = std::get<Type>(d[static_cast<size_t>(StyleProperty::PROP)]);
    AM_SPLINE_STYLE_FIELDS(X)
#undef X
    return r;
}

ContextMenuStyleProperties Style::getContextMenuStyle(ComponentType type, std::span<const StyleKey> classes, uint16_t state,
                                                      const StylePart &part)
{
    const DenseSet &d = resolveSet(type, classes, state, part);
    ContextMenuStyleProperties r;
#define X(PROP, field, Type) r.field = std::get<Type>(d[static_cast<size_t>(StyleProperty::PROP)]);
    AM_CONTEXT_MENU_STYLE_FIELDS(X)
#undef X
    return r;
}

DropdownStyleProperties Style::getDropdownStyle(ComponentType type, std::span<const StyleKey> classes, uint16_t state,
                                                const StylePart &part)
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
    static const std::array<ComponentType, 2> textArea = {ComponentType::TEXT_AREA, ComponentType::UI_OBJECT};
    static const std::array<ComponentType, 2> drag = {ComponentType::DRAG, ComponentType::UI_OBJECT};
    static const std::array<ComponentType, 3> menuBar = {ComponentType::MENU_BAR, ComponentType::FRAME, ComponentType::UI_OBJECT};
    static const std::array<ComponentType, 2> spline = {ComponentType::SPLINE, ComponentType::UI_OBJECT};
    static const std::array<ComponentType, 3> contextMenu = {ComponentType::CONTEXT_MENU, ComponentType::FRAME,
                                                             ComponentType::UI_OBJECT};
    static const std::array<ComponentType, 2> shape = {ComponentType::SHAPE, ComponentType::UI_OBJECT};

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
    case ComponentType::TEXT_AREA:
        return textArea;
    case ComponentType::DRAG:
        return drag;
    case ComponentType::MENU_BAR:
        return menuBar;
    case ComponentType::SPLINE:
        return spline;
    case ComponentType::CONTEXT_MENU:
        return contextMenu;
    case ComponentType::SHAPE:
        return shape;
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
        {"text-area", ComponentType::TEXT_AREA},
        {"drag", ComponentType::DRAG},
        {"menu-bar", ComponentType::MENU_BAR},
        {"spline", ComponentType::SPLINE},
        {"context-menu", ComponentType::CONTEXT_MENU},
        {"shape", ComponentType::SHAPE},
    };
    return names;
}

} // namespace Amethyst
