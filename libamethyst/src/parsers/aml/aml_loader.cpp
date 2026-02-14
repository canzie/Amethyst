#include "parsers/aml/aml_loader.h"
#include "amethyst/Amethyst.h"
#include "parsers/aml/aml_tokenizer.h"

#include <fstream>
#include <sstream>

namespace Amethyst {

static float s_toFloat(const AmlValue &v)
{
    if (v.isFloat()) return static_cast<float>(v.asFloat());
    if (v.isInt()) return static_cast<float>(v.asInt());
    return 0.0f;
}

static int64_t s_toInt(const AmlValue &v)
{
    if (v.isInt()) return v.asInt();
    if (v.isFloat()) return static_cast<int64_t>(v.asFloat());
    return 0;
}

static Color3 s_toColor3(const AmlValue &v)
{
    if (!v.isArray()) return Color3();
    auto &arr = v.asArray();
    if (arr.size() >= 3) {
        return Color3(s_toFloat(arr[0]), s_toFloat(arr[1]), s_toFloat(arr[2]));
    }
    return Color3();
}

static Color4 s_toColor4(const AmlValue &v)
{
    if (!v.isArray()) return Color4();
    auto &arr = v.asArray();
    if (arr.size() >= 4) {
        return Color4(s_toFloat(arr[0]), s_toFloat(arr[1]), s_toFloat(arr[2]), s_toFloat(arr[3]));
    }
    if (arr.size() >= 3) {
        return Color4(s_toFloat(arr[0]), s_toFloat(arr[1]), s_toFloat(arr[2]));
    }
    return Color4();
}

static glm::vec2 s_toVec2(const AmlValue &v)
{
    if (!v.isArray()) return glm::vec2(0.0f);
    auto &arr = v.asArray();
    if (arr.size() >= 2) {
        return {s_toFloat(arr[0]), s_toFloat(arr[1])};
    }
    return glm::vec2(0.0f);
}

static glm::vec3 s_toVec3(const AmlValue &v)
{
    if (!v.isArray()) return glm::vec3(s_toFloat(v));
    auto &arr = v.asArray();
    if (arr.size() >= 3) {
        return {s_toFloat(arr[0]), s_toFloat(arr[1]), s_toFloat(arr[2])};
    }
    return glm::vec3(0.0f);
}

static UDim s_toUDim(const AmlValue &v)
{
    if (v.isInt() || v.isFloat()) return UDim::fromOffset(s_toFloat(v));
    if (!v.isArray()) return UDim{};
    auto &arr = v.asArray();
    if (arr.size() >= 2) {
        return UDim{s_toFloat(arr[0]), s_toFloat(arr[1])};
    }
    return UDim{};
}

// [scaleX, offsetX, scaleY, offsetY]
static UDim2 s_toUDim2(const AmlValue &v)
{
    if (!v.isArray()) return UDim2{};
    auto &arr = v.asArray();
    if (arr.size() >= 4) {
        return UDim2(s_toFloat(arr[0]), s_toFloat(arr[1]), s_toFloat(arr[2]), s_toFloat(arr[3]));
    }
    if (arr.size() >= 2) {
        return UDim2::fromOffset(s_toFloat(arr[0]), s_toFloat(arr[1]));
    }
    return UDim2{};
}

static UDim4 s_toUDim4(const AmlValue &v)
{
    if (v.isInt() || v.isFloat()) {
        UDim all = UDim::fromOffset(s_toFloat(v));
        return UDim4{all, all, all, all};
    }
    if (!v.isArray()) return UDim4{};
    auto &arr = v.asArray();
    if (arr.size() >= 4) {
        return UDim4{s_toUDim(arr[0]), s_toUDim(arr[1]), s_toUDim(arr[2]), s_toUDim(arr[3])};
    }
    if (arr.size() >= 2) {
        UDim vertical = s_toUDim(arr[0]);
        UDim horizontal = s_toUDim(arr[1]);
        return UDim4{vertical, horizontal, vertical, horizontal};
    }
    return UDim4{};
}

static BorderMode s_parseBorderMode(const std::string &s)
{
    if (s == "outline") return BorderMode::OUTLINE;
    if (s == "middle") return BorderMode::MIDDLE;
    if (s == "inset") return BorderMode::INSET;
    return BorderMode::OUTLINE;
}

static TextXAlignment s_parseTextXAlignment(const std::string &s)
{
    if (s == "left") return TextXAlignment::LEFT;
    if (s == "center") return TextXAlignment::CENTER;
    if (s == "right") return TextXAlignment::RIGHT;
    return TextXAlignment::LEFT;
}

static TextYAlignment s_parseTextYAlignment(const std::string &s)
{
    if (s == "top") return TextYAlignment::TOP;
    if (s == "center") return TextYAlignment::CENTER;
    if (s == "bottom") return TextYAlignment::BOTTOM;
    return TextYAlignment::TOP;
}

static TextTruncate s_parseTextTruncate(const std::string &s)
{
    if (s == "none") return TextTruncate::NONE;
    if (s == "atEnd") return TextTruncate::AT_END;
    if (s == "splitWord") return TextTruncate::SPLIT_WORD;
    return TextTruncate::NONE;
}

static AutomaticSize s_parseAutomaticSize(const std::string &s)
{
    if (s == "x") return AutomaticSize::X;
    if (s == "y") return AutomaticSize::Y;
    if (s == "xy") return AutomaticSize::XY;
    return AutomaticSize::NONE;
}

static ZIndexBehavior s_parseZIndexBehavior(const std::string &s)
{
    if (s == "global") return ZIndexBehavior::GLOBAL;
    if (s == "sibling") return ZIndexBehavior::SIBLING;
    return ZIndexBehavior::SIBLING;
}

static FillDirection s_parseFillDirection(const std::string &s)
{
    if (s == "horizontal") return FillDirection::FILL_HORIZONTAL;
    if (s == "vertical") return FillDirection::FILL_VERTICAL;
    return FillDirection::FILL_VERTICAL;
}

static HorizontalAlignment s_parseHorizontalAlignment(const std::string &s)
{
    if (s == "left") return HorizontalAlignment::ALIGN_LEFT;
    if (s == "center") return HorizontalAlignment::ALIGN_CENTER_H;
    if (s == "right") return HorizontalAlignment::ALIGN_RIGHT;
    return HorizontalAlignment::ALIGN_LEFT;
}

static VerticalAlignment s_parseVerticalAlignment(const std::string &s)
{
    if (s == "top") return VerticalAlignment::ALIGN_TOP;
    if (s == "center") return VerticalAlignment::ALIGN_CENTER_V;
    if (s == "bottom") return VerticalAlignment::ALIGN_BOTTOM;
    return VerticalAlignment::ALIGN_TOP;
}

static SortOrder s_parseSortOrder(const std::string &s)
{
    if (s == "name") return SortOrder::SORT_NAME;
    if (s == "layoutOrder") return SortOrder::SORT_LAYOUT_ORDER;
    return SortOrder::SORT_LAYOUT_ORDER;
}

static UiFlexAlignment s_parseFlexAlignment(const std::string &s)
{
    if (s == "fill") return UiFlexAlignment::FILL;
    if (s == "spaceAround") return UiFlexAlignment::SPACE_AROUND;
    if (s == "spaceBetween") return UiFlexAlignment::SPACE_BETWEEN;
    if (s == "spaceEvenly") return UiFlexAlignment::SPACE_EVENLY;
    return UiFlexAlignment::NONE;
}

static ItemLineAlignment s_parseItemLineAlignment(const std::string &s)
{
    if (s == "start") return ItemLineAlignment::START;
    if (s == "center") return ItemLineAlignment::CENTER;
    if (s == "end") return ItemLineAlignment::END;
    if (s == "stretch") return ItemLineAlignment::STRETCH;
    return ItemLineAlignment::AUTOMATIC;
}

static StartCorner s_parseStartCorner(const std::string &s)
{
    if (s == "topLeft") return StartCorner::TOP_LEFT;
    if (s == "topRight") return StartCorner::TOP_RIGHT;
    if (s == "bottomLeft") return StartCorner::BOTTOM_LEFT;
    if (s == "bottomRight") return StartCorner::BOTTOM_RIGHT;
    return StartCorner::TOP_LEFT;
}

static ScrollAxis s_parseScrollAxis(const std::string &s)
{
    if (s == "x") return ScrollAxis::X;
    if (s == "y") return ScrollAxis::Y;
    if (s == "xy") return ScrollAxis::XY;
    return ScrollAxis::Y;
}

static ScrollBarVisibility s_parseScrollBarVisibility(const std::string &s)
{
    if (s == "always") return ScrollBarVisibility::ALWAYS;
    if (s == "auto") return ScrollBarVisibility::AUTO;
    if (s == "never") return ScrollBarVisibility::NEVER;
    return ScrollBarVisibility::AUTO;
}

static LabelSide s_parseLabelSide(const std::string &s)
{
    if (s == "left") return LabelSide::LEFT;
    if (s == "right") return LabelSide::RIGHT;
    if (s == "top") return LabelSide::TOP;
    if (s == "bottom") return LabelSide::BOTTOM;
    return LabelSide::LEFT;
}

static ValueControlLayout s_parseValueControlLayout(const std::string &s)
{
    if (s == "sideBySide") return ValueControlLayout::SIDE_BY_SIDE;
    if (s == "stacked") return ValueControlLayout::STACKED;
    return ValueControlLayout::SIDE_BY_SIDE;
}

static DockZone s_parseDockZone(const std::string &s)
{
    if (s == "left") return DockZone::LEFT;
    if (s == "right") return DockZone::RIGHT;
    if (s == "top") return DockZone::TOP;
    if (s == "bottom") return DockZone::BOTTOM;
    if (s == "center") return DockZone::CENTER;
    return DockZone::CENTER;
}

static void s_applyUIObjectAttrs(UIObject *obj, const std::unordered_map<std::string, AmlValue> &attrs)
{
    for (auto &[key, val] : attrs) {
        if (key == "name") obj->name = val.asString();
        else if (key == "size") obj->size = s_toUDim2(val);
        else if (key == "position") obj->position = s_toUDim2(val);
        else if (key == "anchorPoint") obj->anchorPoint = s_toVec2(val);
        else if (key == "backgroundColor") obj->backgroundColor = s_toColor3(val);
        else if (key == "backgroundTransparency") obj->backgroundTransparency = s_toFloat(val);
        else if (key == "borderMode") obj->borderMode = s_parseBorderMode(val.asString());
        else if (key == "borderPixelSize") obj->borderPixelSize = s_toFloat(val);
        else if (key == "borderColor") obj->borderColor = s_toColor3(val);
        else if (key == "borderTransparency") obj->borderTransparency = s_toFloat(val);
        else if (key == "cornerRadius") obj->cornerRadius = s_toFloat(val);
        else if (key == "clipsDescendants") obj->clipsDescendants = val.asBool();
        else if (key == "visible") obj->visible = val.asBool();
        else if (key == "interactable") obj->interactable = val.asBool();
        else if (key == "active") obj->active = val.asBool();
        else if (key == "rotation") obj->rotation = s_toFloat(val);
        else if (key == "zIndex") obj->zIndex = static_cast<int32_t>(s_toInt(val));
        else if (key == "zindexBehavior") obj->zindexBehavior = s_parseZIndexBehavior(val.asString());
        else if (key == "layoutOrder") obj->layoutOrder = static_cast<uint32_t>(s_toInt(val));
        else if (key == "automaticSize") obj->automaticSize = s_parseAutomaticSize(val.asString());
        else if (key == "padding") obj->padding = s_toUDim4(val);
    }
}

static void s_applyTextAttrs(const std::unordered_map<std::string, AmlValue> &attrs, std::string &text, std::string &fontFamily,
                    float &fontSize, Color4 &textColor, TextXAlignment &textXAlign, TextYAlignment &textYAlign,
                    TextTruncate &textTruncate, bool &richText, bool &textWrapped, bool &textScaled, float &lineHeight,
                    float &strokeThickness, Color4 &strokeColor)
{
    for (auto &[key, val] : attrs) {
        if (key == "text") text = val.asString();
        else if (key == "fontFamily") fontFamily = val.asString();
        else if (key == "fontSize") fontSize = s_toFloat(val);
        else if (key == "textColor") textColor = s_toColor4(val);
        else if (key == "textXAlignment") textXAlign = s_parseTextXAlignment(val.asString());
        else if (key == "textYAlignment") textYAlign = s_parseTextYAlignment(val.asString());
        else if (key == "textTruncate") textTruncate = s_parseTextTruncate(val.asString());
        else if (key == "richText") richText = val.asBool();
        else if (key == "textWrapped") textWrapped = val.asBool();
        else if (key == "textScaled") textScaled = val.asBool();
        else if (key == "lineHeight") lineHeight = s_toFloat(val);
        else if (key == "strokeThickness") strokeThickness = s_toFloat(val);
        else if (key == "strokeColor") strokeColor = s_toColor4(val);
    }
}

static void s_applyScrollingFrameAttrs(ScrollingFrame *sf, const std::unordered_map<std::string, AmlValue> &attrs)
{
    for (auto &[key, val] : attrs) {
        if (key == "scrollAxis") sf->scrollAxis = s_parseScrollAxis(val.asString());
        else if (key == "scrollBarVisibility") sf->scrollBarVisibility = s_parseScrollBarVisibility(val.asString());
        else if (key == "canvasSize") sf->canvasSize = s_toUDim2(val);
        else if (key == "canvasPosition") sf->canvasPosition = s_toUDim2(val);
        else if (key == "scrollBarColor") sf->scrollBarColor = s_toColor3(val);
        else if (key == "scrollBarTransparency") sf->scrollBarTransparency = s_toFloat(val);
        else if (key == "scrollBarThickness") sf->scrollBarThickness = s_toFloat(val);
        else if (key == "scrollBarThumbColor") sf->scrollBarThumbColor = s_toColor3(val);
        else if (key == "scrollBarThumbTransparency") sf->scrollBarThumbTransparency = s_toFloat(val);
        else if (key == "scrollSpeed") sf->scrollSpeed = s_toFloat(val);
        else if (key == "elasticScrolling") sf->elasticScrolling = val.asBool();
    }
}

static void s_applyListLayoutAttrs(UIListLayout *ll, const std::unordered_map<std::string, AmlValue> &attrs)
{
    for (auto &[key, val] : attrs) {
        if (key == "fillDirection") ll->fillDirection = s_parseFillDirection(val.asString());
        else if (key == "horizontalAlignment") ll->horizontalAlignment = s_parseHorizontalAlignment(val.asString());
        else if (key == "verticalAlignment") ll->verticalAlignment = s_parseVerticalAlignment(val.asString());
        else if (key == "sortOrder") ll->sortOrder = s_parseSortOrder(val.asString());
        else if (key == "innerPadding") ll->innerPadding = s_toUDim(val);
        else if (key == "horizontalFlex") ll->horizontalFlex = s_parseFlexAlignment(val.asString());
        else if (key == "verticalFlex") ll->verticalFlex = s_parseFlexAlignment(val.asString());
        else if (key == "wraps") ll->wraps = val.asBool();
        else if (key == "itemLineAlignment") ll->itemLineAlignment = s_parseItemLineAlignment(val.asString());
    }
}

static void s_applyGridLayoutAttrs(UIGridLayout *gl, const std::unordered_map<std::string, AmlValue> &attrs)
{
    for (auto &[key, val] : attrs) {
        if (key == "fillDirection") gl->fillDirection = s_parseFillDirection(val.asString());
        else if (key == "horizontalAlignment") gl->horizontalAlignment = s_parseHorizontalAlignment(val.asString());
        else if (key == "verticalAlignment") gl->verticalAlignment = s_parseVerticalAlignment(val.asString());
        else if (key == "sortOrder") gl->sortOrder = s_parseSortOrder(val.asString());
        else if (key == "cellPadding") gl->cellPadding = s_toUDim2(val);
        else if (key == "cellSize") gl->cellSize = s_toUDim2(val);
        else if (key == "startCorner") gl->startCorner = s_parseStartCorner(val.asString());
        else if (key == "fillDirectionMaxCells") gl->fillDirectionMaxCells = static_cast<uint32_t>(s_toInt(val));
    }
}

static void s_applySizeConstraintAttrs(UISizeConstraint *sc, const std::unordered_map<std::string, AmlValue> &attrs)
{
    for (auto &[key, val] : attrs) {
        if (key == "maxSize") sc->maxSize = s_toVec2(val);
        else if (key == "minSize") sc->minSize = s_toVec2(val);
    }
}

static void s_applyAspectRatioAttrs(UIAspectRatioConstraint *ar, const std::unordered_map<std::string, AmlValue> &attrs)
{
    for (auto &[key, val] : attrs) {
        if (key == "aspectRatio") ar->aspectRatio = s_toFloat(val);
    }
}

static void s_applyTextSizeConstraintAttrs(UITextSizeConstraint *tc, const std::unordered_map<std::string, AmlValue> &attrs)
{
    for (auto &[key, val] : attrs) {
        if (key == "maxTextSize") tc->maxTextSize = s_toFloat(val);
        else if (key == "minTextSize") tc->minTextSize = s_toFloat(val);
    }
}

static void s_applySliderAttrs(Slider *s, const std::unordered_map<std::string, AmlValue> &attrs)
{
    for (auto &[key, val] : attrs) {
        if (key == "sliderColor") s->sliderColor = s_toColor3(val);
        else if (key == "sliderTransparency") s->sliderTransparency = s_toFloat(val);
        else if (key == "thumbColor") s->thumbColor = s_toColor3(val);
        else if (key == "thumbTransparency") s->thumbTransparency = s_toFloat(val);
        else if (key == "trackCornerRadius") s->trackCornerRadius = s_toFloat(val);
        else if (key == "thumbCornerRadius") s->thumbCornerRadius = s_toFloat(val);
        else if (key == "label") s->label = val.asString();
        else if (key == "labelColor") s->labelColor = s_toColor4(val);
        else if (key == "labelSide") s->labelSide = s_parseLabelSide(val.asString());
        else if (key == "labelPadding") s->labelPadding = s_toUDim(val);
        else if (key == "valueColor") s->valueColor = s_toColor4(val);
        else if (key == "valueSuffix") s->valueSuffix = val.asString();
        else if (key == "fontSize") s->fontSize = s_toFloat(val);
        else if (key == "layout") s->layout = s_parseValueControlLayout(val.asString());
    }
}

static void s_applyTextInputAttrs(TextInput *ti, const std::unordered_map<std::string, AmlValue> &attrs)
{
    for (auto &[key, val] : attrs) {
        if (key == "placeholderText") ti->placeholderText = val.asString();
        else if (key == "textColor") ti->textColor = s_toColor4(val);
        else if (key == "placeholderColor") ti->placeholderColor = s_toColor4(val);
        else if (key == "selectionColor") ti->selectionColor = s_toColor4(val);
        else if (key == "cursorColor") ti->cursorColor = s_toColor4(val);
        else if (key == "fontSize") ti->fontSize = s_toFloat(val);
        else if (key == "fontFamily") ti->fontFamily = val.asString();
        else if (key == "multiline") ti->multiline = val.asBool();
        else if (key == "maxLength") ti->maxLength = static_cast<int32_t>(s_toInt(val));
        else if (key == "readOnly") ti->readOnly = val.asBool();
        else if (key == "cursorBlinkRate") ti->cursorBlinkRate = s_toFloat(val);
        else if (key == "textXAlignment") ti->textXAlignment = s_parseTextXAlignment(val.asString());
    }
}

static bool s_isExtensionTag(const std::string &tag)
{
    return tag == "UIListLayout" || tag == "UIGridLayout" || tag == "UISizeConstraint" || tag == "UIAspectRatioConstraint" ||
           tag == "UIDragDetector" || tag == "UITextSizeConstraint";
}

static void s_applyExtension(UIObject *parent, const AmlNode &node)
{
    if (node.tag == "UIListLayout") {
        auto *ext = parent->addExtension<UIListLayout>();
        s_applyListLayoutAttrs(ext, node.attributes);
    } else if (node.tag == "UIGridLayout") {
        auto *ext = parent->addExtension<UIGridLayout>();
        s_applyGridLayoutAttrs(ext, node.attributes);
    } else if (node.tag == "UISizeConstraint") {
        auto *ext = parent->addExtension<UISizeConstraint>();
        s_applySizeConstraintAttrs(ext, node.attributes);
    } else if (node.tag == "UIAspectRatioConstraint") {
        auto *ext = parent->addExtension<UIAspectRatioConstraint>();
        s_applyAspectRatioAttrs(ext, node.attributes);
    } else if (node.tag == "UIDragDetector") {
        parent->addExtension<UIDragDetector>();
    } else if (node.tag == "UITextSizeConstraint") {
        auto *ext = parent->addExtension<UITextSizeConstraint>();
        s_applyTextSizeConstraintAttrs(ext, node.attributes);
    }
}

template <typename T, typename... Args>
static T *s_createInst(Instance *parent, std::vector<std::unique_ptr<Instance>> &roots, Args &&...args)
{
    if (parent) {
        return parent->add<T>(std::forward<Args>(args)...);
    }
    auto inst = std::make_unique<T>(std::forward<Args>(args)...);
    T *ptr = inst.get();
    roots.push_back(std::move(inst));
    return ptr;
}

static Instance *s_buildNode(const AmlNode &node, Instance *parent, std::vector<std::unique_ptr<Instance>> &roots,
                             std::string &error);

static DockZone s_oppositeZone(DockZone zone)
{
    switch (zone) {
    case DockZone::LEFT:
        return DockZone::RIGHT;
    case DockZone::RIGHT:
        return DockZone::LEFT;
    case DockZone::TOP:
        return DockZone::BOTTOM;
    case DockZone::BOTTOM:
        return DockZone::TOP;
    default:
        return DockZone::CENTER;
    }
}

static int32_t s_buildDockChild(DockingLayer *dl, const AmlNode &child, int32_t targetLeaf, DockZone zone, float ratio,
                       std::vector<std::unique_ptr<Instance>> &allInstances, std::string &error)
{
    if (child.tag == "DockRegion") {
        DockZone innerZone = DockZone::CENTER;
        float innerRatio = 0.5f;
        for (auto &[key, val] : child.attributes) {
            if (key == "zone") innerZone = s_parseDockZone(val.asString());
            else if (key == "ratio") innerRatio = s_toFloat(val);
        }

        if (child.children.empty()) {
            AM_LOG_WARN("DockRegion has no children, skipping");
            return targetLeaf;
        }

        int32_t leaf = s_buildDockChild(dl, child.children[0], targetLeaf, zone, ratio, allInstances, error);
        if (!error.empty()) return -1;

        if (child.children.size() > 1) {
            leaf = s_buildDockChild(dl, child.children[1], leaf, s_oppositeZone(innerZone), innerRatio, allInstances, error);
            if (!error.empty()) return -1;
        }

        for (size_t i = 2; i < child.children.size(); i++) {
            leaf = s_buildDockChild(dl, child.children[i], leaf, DockZone::CENTER, 0.0f, allInstances, error);
            if (!error.empty()) return -1;
        }
        return leaf;
    }

    s_buildNode(child, nullptr, allInstances, error);
    if (!error.empty()) return -1;

    auto inst = std::move(allInstances.back());
    allInstances.pop_back();

    if (!inst->as<UIBase2D>()) {
        error = "DockingLayer child must be a UIBase2D";
        return -1;
    }

    return dl->dock(std::move(inst), targetLeaf, zone, ratio);
}

static Instance *s_buildNode(const AmlNode &node, Instance *parent, std::vector<std::unique_ptr<Instance>> &allInstances, std::string &error)
{
    auto &tag = node.tag;

    if (tag == "Frame") {
        auto *ptr = s_createInst<Frame>(parent, allInstances);
        s_applyUIObjectAttrs(ptr, node.attributes);
        ptr->markDirty();

        for (auto &child : node.children) {
            if (s_isExtensionTag(child.tag)) {
                s_applyExtension(ptr, child);
            } else {
                s_buildNode(child, ptr, allInstances, error);
                if (!error.empty()) return nullptr;
            }
        }
        return ptr;
    }

    if (tag == "TextLabel") {
        auto *ptr = s_createInst<TextLabel>(parent, allInstances);
        s_applyUIObjectAttrs(ptr, node.attributes);
        s_applyTextAttrs(node.attributes, ptr->text, ptr->fontFamily, ptr->fontSize, ptr->textColor, ptr->textXAlignment,
                       ptr->textYAlignment, ptr->textTruncate, ptr->richText, ptr->textWrapped, ptr->textScaled, ptr->lineHeight,
                       ptr->strokeThickness, ptr->strokeColor);
        ptr->markDirty();

        for (auto &child : node.children) {
            if (s_isExtensionTag(child.tag)) {
                s_applyExtension(ptr, child);
            } else {
                s_buildNode(child, ptr, allInstances, error);
                if (!error.empty()) return nullptr;
            }
        }
        return ptr;
    }

    if (tag == "TextButton") {
        auto *ptr = s_createInst<TextButton>(parent, allInstances);
        s_applyUIObjectAttrs(ptr, node.attributes);
        s_applyTextAttrs(node.attributes, ptr->text, ptr->fontFamily, ptr->fontSize, ptr->textColor, ptr->textXAlignment,
                       ptr->textYAlignment, ptr->textTruncate, ptr->richText, ptr->textWrapped, ptr->textScaled, ptr->lineHeight,
                       ptr->strokeThickness, ptr->strokeColor);

        for (auto &[key, val] : node.attributes) {
            if (key == "autoButtonColor") ptr->autoButtonColor = val.asBool();
            else if (key == "modal") ptr->modal = val.asBool();
        }

        ptr->markDirty();

        for (auto &child : node.children) {
            if (s_isExtensionTag(child.tag)) {
                s_applyExtension(ptr, child);
            } else {
                s_buildNode(child, ptr, allInstances, error);
                if (!error.empty()) return nullptr;
            }
        }
        return ptr;
    }

    if (tag == "ImageLabel") {
        auto *ptr = s_createInst<ImageLabel>(parent, allInstances);
        s_applyUIObjectAttrs(ptr, node.attributes);

        for (auto &[key, val] : node.attributes) {
            if (key == "imageColor") ptr->imageColor = s_toColor4(val);
            else if (key == "imageTransparency") ptr->imageTransparency = s_toFloat(val);
        }

        ptr->markDirty();

        for (auto &child : node.children) {
            if (s_isExtensionTag(child.tag)) {
                s_applyExtension(ptr, child);
            } else {
                s_buildNode(child, ptr, allInstances, error);
                if (!error.empty()) return nullptr;
            }
        }
        return ptr;
    }

    if (tag == "ImageButton") {
        auto *ptr = s_createInst<ImageButton>(parent, allInstances);
        s_applyUIObjectAttrs(ptr, node.attributes);

        for (auto &[key, val] : node.attributes) {
            if (key == "imageColor") ptr->imageColor = s_toColor4(val);
            else if (key == "imageTransparency") ptr->imageTransparency = s_toFloat(val);
            else if (key == "autoButtonColor") ptr->autoButtonColor = val.asBool();
            else if (key == "modal") ptr->modal = val.asBool();
        }

        ptr->markDirty();

        for (auto &child : node.children) {
            if (s_isExtensionTag(child.tag)) {
                s_applyExtension(ptr, child);
            } else {
                s_buildNode(child, ptr, allInstances, error);
                if (!error.empty()) return nullptr;
            }
        }
        return ptr;
    }

    if (tag == "ScrollingFrame") {
        auto *ptr = s_createInst<ScrollingFrame>(parent, allInstances);
        s_applyUIObjectAttrs(ptr, node.attributes);
        s_applyScrollingFrameAttrs(ptr, node.attributes);
        ptr->markDirty();

        for (auto &child : node.children) {
            if (s_isExtensionTag(child.tag)) {
                s_applyExtension(ptr, child);
            } else {
                s_buildNode(child, ptr, allInstances, error);
                if (!error.empty()) return nullptr;
            }
        }
        return ptr;
    }

    if (tag == "Checkbox") {
    }

    if (tag == "Dropdown") {
    }

    if (tag == "InvisibleButton") {
        auto *ptr = s_createInst<InvisibleButton>(parent, allInstances);
        s_applyUIObjectAttrs(ptr, node.attributes);

        for (auto &[key, val] : node.attributes) {
            if (key == "autoButtonColor") ptr->autoButtonColor = val.asBool();
            else if (key == "modal") ptr->modal = val.asBool();
        }

        ptr->markDirty();

        for (auto &child : node.children) {
            if (s_isExtensionTag(child.tag)) {
                s_applyExtension(ptr, child);
            } else {
                s_buildNode(child, ptr, allInstances, error);
                if (!error.empty()) return nullptr;
            }
        }
        return ptr;
    }

    if (tag == "TextInput") {
        auto *ptr = s_createInst<TextInput>(parent, allInstances);
        s_applyUIObjectAttrs(ptr, node.attributes);
        s_applyTextInputAttrs(ptr, node.attributes);
        ptr->markDirty();

        for (auto &child : node.children) {
            if (s_isExtensionTag(child.tag)) {
                s_applyExtension(ptr, child);
            } else {
                s_buildNode(child, ptr, allInstances, error);
                if (!error.empty()) return nullptr;
            }
        }
        return ptr;
    }

    if (tag == "SliderFloat") {
        auto *ptr = s_createInst<SliderFloat>(parent, allInstances);
        s_applyUIObjectAttrs(ptr, node.attributes);
        s_applySliderAttrs(ptr, node.attributes);

        for (auto &[key, val] : node.attributes) {
            if (key == "min") ptr->min = s_toFloat(val);
            else if (key == "max") ptr->max = s_toFloat(val);
            else if (key == "speed") ptr->speed = s_toFloat(val);
        }

        ptr->markDirty();

        for (auto &child : node.children) {
            if (s_isExtensionTag(child.tag)) {
                s_applyExtension(ptr, child);
            } else {
                s_buildNode(child, ptr, allInstances, error);
                if (!error.empty()) return nullptr;
            }
        }
        return ptr;
    }

    if (tag == "SliderInt") {
        auto *ptr = s_createInst<SliderInt>(parent, allInstances);
        s_applyUIObjectAttrs(ptr, node.attributes);
        s_applySliderAttrs(ptr, node.attributes);

        for (auto &[key, val] : node.attributes) {
            if (key == "min") ptr->min = static_cast<int>(s_toInt(val));
            else if (key == "max") ptr->max = static_cast<int>(s_toInt(val));
            else if (key == "speed") ptr->speed = s_toFloat(val);
        }

        ptr->markDirty();

        for (auto &child : node.children) {
            if (s_isExtensionTag(child.tag)) {
                s_applyExtension(ptr, child);
            } else {
                s_buildNode(child, ptr, allInstances, error);
                if (!error.empty()) return nullptr;
            }
        }
        return ptr;
    }

    if (tag == "SliderVec2") {
        auto *ptr = s_createInst<SliderVec2>(parent, allInstances);
        s_applyUIObjectAttrs(ptr, node.attributes);
        s_applySliderAttrs(ptr, node.attributes);

        for (auto &[key, val] : node.attributes) {
            if (key == "min") ptr->min = s_toVec2(val);
            else if (key == "max") ptr->max = s_toVec2(val);
            else if (key == "speed") ptr->speed = s_toFloat(val);
        }

        ptr->markDirty();

        for (auto &child : node.children) {
            if (s_isExtensionTag(child.tag)) {
                s_applyExtension(ptr, child);
            } else {
                s_buildNode(child, ptr, allInstances, error);
                if (!error.empty()) return nullptr;
            }
        }
        return ptr;
    }

    if (tag == "SliderVec3") {
        auto *ptr = s_createInst<SliderVec3>(parent, allInstances);
        s_applyUIObjectAttrs(ptr, node.attributes);
        s_applySliderAttrs(ptr, node.attributes);

        for (auto &[key, val] : node.attributes) {
            if (key == "min") ptr->min = s_toVec3(val);
            else if (key == "max") ptr->max = s_toVec3(val);
            else if (key == "speed") ptr->speed = s_toFloat(val);
        }

        ptr->markDirty();

        for (auto &child : node.children) {
            if (s_isExtensionTag(child.tag)) {
                s_applyExtension(ptr, child);
            } else {
                s_buildNode(child, ptr, allInstances, error);
                if (!error.empty()) return nullptr;
            }
        }
        return ptr;
    }

    if (tag == "DockingLayer") {
        auto *ptr = s_createInst<DockingLayer>(parent, allInstances);

        for (auto &[key, val] : node.attributes) {
            if (key == "name") ptr->name = val.asString();
            else if (key == "visible") ptr->visible = val.asBool();
            else if (key == "outerSpacing") ptr->outerSpacing = s_toFloat(val);
            else if (key == "innerSpacing") ptr->innerSpacing = s_toFloat(val);
        }

        ptr->markDirty();

        for (auto &child : node.children) {
            if (child.tag != "DockRegion") {
                AM_LOG_WARN("DockingLayer only accepts DockRegion children, got '{}'", child.tag);
                continue;
            }
            s_buildDockChild(ptr, child, -1, DockZone::CENTER, 0.0f, allInstances, error);
            if (!error.empty()) return nullptr;
        }
        return ptr;
    }

    if (tag == "OverlayLayer") {
        auto *ptr = s_createInst<OverlayLayer>(parent, allInstances);

        for (auto &[key, val] : node.attributes) {
            if (key == "name") ptr->name = val.asString();
            else if (key == "visible") ptr->visible = val.asBool();
        }

        ptr->markDirty();

        for (auto &child : node.children) {
            s_buildNode(child, ptr, allInstances, error);
            if (!error.empty()) return nullptr;
        }
        return ptr;
    }

    if (tag == "PanelLayer") {
        auto *ptr = s_createInst<PanelLayer>(parent, allInstances);

        for (auto &[key, val] : node.attributes) {
            if (key == "name") ptr->name = val.asString();
            else if (key == "visible") ptr->visible = val.asBool();
        }

        ptr->markDirty();

        for (auto &child : node.children) {
            s_buildNode(child, ptr, allInstances, error);
            if (!error.empty()) return nullptr;
        }
        return ptr;
    }

    error = "[" + std::to_string(node.line) + ":" + std::to_string(node.column) + "] Unknown tag '" + tag + "'";
    return nullptr;
}


AmlLoadResult::~AmlLoadResult() = default;

AmlLoadResult AmlLoader::loadFile(const std::filesystem::path &path)
{
    std::ifstream file(path);
    if (!file.is_open()) {
        AmlLoadResult r;
        r.error = "Failed to open file: " + path.string();
        return r;
    }
    std::ostringstream ss;
    ss << file.rdbuf();
    return loadString(ss.str());
}

AmlLoadResult AmlLoader::loadString(const std::string &source)
{
    AmlTokenizer tokenizer;
    auto tokResult = tokenizer.tokenize(source);
    if (!tokResult.ok()) {
        AmlLoadResult r;
        r.error = tokResult.error;
        return r;
    }

    AmlParser parser;
    auto parseResult = parser.parse(tokResult.tokens);
    if (!parseResult.ok()) {
        AmlLoadResult r;
        r.error = parseResult.error;
        return r;
    }

    return loadNodes(parseResult.roots);
}

AmlLoadResult AmlLoader::loadNodes(const std::vector<AmlNode> &roots)
{
    AmlLoadResult result;

    for (auto &rootNode : roots) {
        s_buildNode(rootNode, nullptr, result.instances, result.error);
        if (!result.ok()) {
            result.instances.clear();
            return result;
        }
    }

    return result;
}

} // namespace Amethyst
