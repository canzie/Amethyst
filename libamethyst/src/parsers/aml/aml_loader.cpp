#include "parsers/aml/aml_loader.h"
#include "amethyst/Amethyst.h"
#include "parsers/aml/aml_tokenizer.h"

#include <fstream>
#include <sstream>

namespace Amethyst {

namespace {

float toFloat(const AmlValue &v)
{
    if (v.isFloat()) return static_cast<float>(v.asFloat());
    if (v.isInt()) return static_cast<float>(v.asInt());
    return 0.0f;
}

int64_t toInt(const AmlValue &v)
{
    if (v.isInt()) return v.asInt();
    if (v.isFloat()) return static_cast<int64_t>(v.asFloat());
    return 0;
}

Color3 toColor3(const AmlValue &v)
{
    if (!v.isArray()) return Color3();
    auto &arr = v.asArray();
    if (arr.size() >= 3) {
        return Color3(toFloat(arr[0]), toFloat(arr[1]), toFloat(arr[2]));
    }
    return Color3();
}

Color4 toColor4(const AmlValue &v)
{
    if (!v.isArray()) return Color4();
    auto &arr = v.asArray();
    if (arr.size() >= 4) {
        return Color4(toFloat(arr[0]), toFloat(arr[1]), toFloat(arr[2]), toFloat(arr[3]));
    }
    if (arr.size() >= 3) {
        return Color4(toFloat(arr[0]), toFloat(arr[1]), toFloat(arr[2]));
    }
    return Color4();
}

glm::vec2 toVec2(const AmlValue &v)
{
    if (!v.isArray()) return glm::vec2(0.0f);
    auto &arr = v.asArray();
    if (arr.size() >= 2) {
        return {toFloat(arr[0]), toFloat(arr[1])};
    }
    return glm::vec2(0.0f);
}

glm::vec3 toVec3(const AmlValue &v)
{
    if (!v.isArray()) return glm::vec3(toFloat(v));
    auto &arr = v.asArray();
    if (arr.size() >= 3) {
        return {toFloat(arr[0]), toFloat(arr[1]), toFloat(arr[2])};
    }
    return glm::vec3(0.0f);
}

UDim toUDim(const AmlValue &v)
{
    if (v.isInt() || v.isFloat()) return UDim::fromOffset(toFloat(v));
    if (!v.isArray()) return UDim{};
    auto &arr = v.asArray();
    if (arr.size() >= 2) {
        return UDim{toFloat(arr[0]), toFloat(arr[1])};
    }
    return UDim{};
}

// [scaleX, offsetX, scaleY, offsetY]
UDim2 toUDim2(const AmlValue &v)
{
    if (!v.isArray()) return UDim2{};
    auto &arr = v.asArray();
    if (arr.size() >= 4) {
        return UDim2(toFloat(arr[0]), toFloat(arr[1]), toFloat(arr[2]), toFloat(arr[3]));
    }
    if (arr.size() >= 2) {
        return UDim2::fromOffset(toFloat(arr[0]), toFloat(arr[1]));
    }
    return UDim2{};
}

UDim4 toUDim4(const AmlValue &v)
{
    if (v.isInt() || v.isFloat()) {
        UDim all = UDim::fromOffset(toFloat(v));
        return UDim4{all, all, all, all};
    }
    if (!v.isArray()) return UDim4{};
    auto &arr = v.asArray();
    if (arr.size() >= 4) {
        return UDim4{toUDim(arr[0]), toUDim(arr[1]), toUDim(arr[2]), toUDim(arr[3])};
    }
    if (arr.size() >= 2) {
        UDim vertical = toUDim(arr[0]);
        UDim horizontal = toUDim(arr[1]);
        return UDim4{vertical, horizontal, vertical, horizontal};
    }
    return UDim4{};
}

BorderMode parseBorderMode(const std::string &s)
{
    if (s == "outline") return BorderMode::OUTLINE;
    if (s == "middle") return BorderMode::MIDDLE;
    if (s == "inset") return BorderMode::INSET;
    return BorderMode::OUTLINE;
}

TextXAlignment parseTextXAlignment(const std::string &s)
{
    if (s == "left") return TextXAlignment::LEFT;
    if (s == "center") return TextXAlignment::CENTER;
    if (s == "right") return TextXAlignment::RIGHT;
    return TextXAlignment::LEFT;
}

TextYAlignment parseTextYAlignment(const std::string &s)
{
    if (s == "top") return TextYAlignment::TOP;
    if (s == "center") return TextYAlignment::CENTER;
    if (s == "bottom") return TextYAlignment::BOTTOM;
    return TextYAlignment::TOP;
}

TextTruncate parseTextTruncate(const std::string &s)
{
    if (s == "none") return TextTruncate::NONE;
    if (s == "atEnd") return TextTruncate::AT_END;
    if (s == "splitWord") return TextTruncate::SPLIT_WORD;
    return TextTruncate::NONE;
}

AutomaticSize parseAutomaticSize(const std::string &s)
{
    if (s == "x") return AutomaticSize::X;
    if (s == "y") return AutomaticSize::Y;
    if (s == "xy") return AutomaticSize::XY;
    return AutomaticSize::NONE;
}

ZIndexBehavior parseZIndexBehavior(const std::string &s)
{
    if (s == "global") return ZIndexBehavior::GLOBAL;
    if (s == "sibling") return ZIndexBehavior::SIBLING;
    return ZIndexBehavior::SIBLING;
}

FillDirection parseFillDirection(const std::string &s)
{
    if (s == "horizontal") return FillDirection::FILL_HORIZONTAL;
    if (s == "vertical") return FillDirection::FILL_VERTICAL;
    return FillDirection::FILL_VERTICAL;
}

HorizontalAlignment parseHorizontalAlignment(const std::string &s)
{
    if (s == "left") return HorizontalAlignment::ALIGN_LEFT;
    if (s == "center") return HorizontalAlignment::ALIGN_CENTER_H;
    if (s == "right") return HorizontalAlignment::ALIGN_RIGHT;
    return HorizontalAlignment::ALIGN_LEFT;
}

VerticalAlignment parseVerticalAlignment(const std::string &s)
{
    if (s == "top") return VerticalAlignment::ALIGN_TOP;
    if (s == "center") return VerticalAlignment::ALIGN_CENTER_V;
    if (s == "bottom") return VerticalAlignment::ALIGN_BOTTOM;
    return VerticalAlignment::ALIGN_TOP;
}

SortOrder parseSortOrder(const std::string &s)
{
    if (s == "name") return SortOrder::SORT_NAME;
    if (s == "layoutOrder") return SortOrder::SORT_LAYOUT_ORDER;
    return SortOrder::SORT_LAYOUT_ORDER;
}

UiFlexAlignment parseFlexAlignment(const std::string &s)
{
    if (s == "fill") return UiFlexAlignment::FILL;
    if (s == "spaceAround") return UiFlexAlignment::SPACE_AROUND;
    if (s == "spaceBetween") return UiFlexAlignment::SPACE_BETWEEN;
    if (s == "spaceEvenly") return UiFlexAlignment::SPACE_EVENLY;
    return UiFlexAlignment::NONE;
}

ItemLineAlignment parseItemLineAlignment(const std::string &s)
{
    if (s == "start") return ItemLineAlignment::START;
    if (s == "center") return ItemLineAlignment::CENTER;
    if (s == "end") return ItemLineAlignment::END;
    if (s == "stretch") return ItemLineAlignment::STRETCH;
    return ItemLineAlignment::AUTOMATIC;
}

StartCorner parseStartCorner(const std::string &s)
{
    if (s == "topLeft") return StartCorner::TOP_LEFT;
    if (s == "topRight") return StartCorner::TOP_RIGHT;
    if (s == "bottomLeft") return StartCorner::BOTTOM_LEFT;
    if (s == "bottomRight") return StartCorner::BOTTOM_RIGHT;
    return StartCorner::TOP_LEFT;
}

ScrollAxis parseScrollAxis(const std::string &s)
{
    if (s == "x") return ScrollAxis::X;
    if (s == "y") return ScrollAxis::Y;
    if (s == "xy") return ScrollAxis::XY;
    return ScrollAxis::Y;
}

ScrollBarVisibility parseScrollBarVisibility(const std::string &s)
{
    if (s == "always") return ScrollBarVisibility::ALWAYS;
    if (s == "auto") return ScrollBarVisibility::AUTO;
    if (s == "never") return ScrollBarVisibility::NEVER;
    return ScrollBarVisibility::AUTO;
}

LabelSide parseLabelSide(const std::string &s)
{
    if (s == "left") return LabelSide::LEFT;
    if (s == "right") return LabelSide::RIGHT;
    if (s == "top") return LabelSide::TOP;
    if (s == "bottom") return LabelSide::BOTTOM;
    return LabelSide::LEFT;
}

ValueControlLayout parseValueControlLayout(const std::string &s)
{
    if (s == "sideBySide") return ValueControlLayout::SIDE_BY_SIDE;
    if (s == "stacked") return ValueControlLayout::STACKED;
    return ValueControlLayout::SIDE_BY_SIDE;
}

DockZone parseDockZone(const std::string &s)
{
    if (s == "left") return DockZone::LEFT;
    if (s == "right") return DockZone::RIGHT;
    if (s == "top") return DockZone::TOP;
    if (s == "bottom") return DockZone::BOTTOM;
    if (s == "center") return DockZone::CENTER;
    return DockZone::CENTER;
}

void applyUIObjectAttrs(UIObject *obj, const std::unordered_map<std::string, AmlValue> &attrs)
{
    for (auto &[key, val] : attrs) {
        if (key == "name") obj->name = val.asString();
        else if (key == "size") obj->size = toUDim2(val);
        else if (key == "position") obj->position = toUDim2(val);
        else if (key == "anchorPoint") obj->anchorPoint = toVec2(val);
        else if (key == "backgroundColor") obj->backgroundColor = toColor3(val);
        else if (key == "backgroundTransparency") obj->backgroundTransparency = toFloat(val);
        else if (key == "borderMode") obj->borderMode = parseBorderMode(val.asString());
        else if (key == "borderPixelSize") obj->borderPixelSize = toFloat(val);
        else if (key == "borderColor") obj->borderColor = toColor3(val);
        else if (key == "borderTransparency") obj->borderTransparency = toFloat(val);
        else if (key == "cornerRadius") obj->cornerRadius = toFloat(val);
        else if (key == "clipsDescendants") obj->clipsDescendants = val.asBool();
        else if (key == "visible") obj->visible = val.asBool();
        else if (key == "interactable") obj->interactable = val.asBool();
        else if (key == "active") obj->active = val.asBool();
        else if (key == "rotation") obj->rotation = toFloat(val);
        else if (key == "zIndex") obj->zIndex = static_cast<int32_t>(toInt(val));
        else if (key == "zindexBehavior") obj->zindexBehavior = parseZIndexBehavior(val.asString());
        else if (key == "layoutOrder") obj->layoutOrder = static_cast<uint32_t>(toInt(val));
        else if (key == "automaticSize") obj->automaticSize = parseAutomaticSize(val.asString());
        else if (key == "padding") obj->padding = toUDim4(val);
    }
}

void applyTextAttrs(const std::unordered_map<std::string, AmlValue> &attrs, std::string &text, std::string &fontFamily,
                    float &fontSize, Color4 &textColor, TextXAlignment &textXAlign, TextYAlignment &textYAlign,
                    TextTruncate &textTruncate, bool &richText, bool &textWrapped, bool &textScaled, float &lineHeight,
                    float &strokeThickness, Color4 &strokeColor)
{
    for (auto &[key, val] : attrs) {
        if (key == "text") text = val.asString();
        else if (key == "fontFamily") fontFamily = val.asString();
        else if (key == "fontSize") fontSize = toFloat(val);
        else if (key == "textColor") textColor = toColor4(val);
        else if (key == "textXAlignment") textXAlign = parseTextXAlignment(val.asString());
        else if (key == "textYAlignment") textYAlign = parseTextYAlignment(val.asString());
        else if (key == "textTruncate") textTruncate = parseTextTruncate(val.asString());
        else if (key == "richText") richText = val.asBool();
        else if (key == "textWrapped") textWrapped = val.asBool();
        else if (key == "textScaled") textScaled = val.asBool();
        else if (key == "lineHeight") lineHeight = toFloat(val);
        else if (key == "strokeThickness") strokeThickness = toFloat(val);
        else if (key == "strokeColor") strokeColor = toColor4(val);
    }
}

void applyScrollingFrameAttrs(ScrollingFrame *sf, const std::unordered_map<std::string, AmlValue> &attrs)
{
    for (auto &[key, val] : attrs) {
        if (key == "scrollAxis") sf->scrollAxis = parseScrollAxis(val.asString());
        else if (key == "scrollBarVisibility") sf->scrollBarVisibility = parseScrollBarVisibility(val.asString());
        else if (key == "canvasSize") sf->canvasSize = toUDim2(val);
        else if (key == "canvasPosition") sf->canvasPosition = toUDim2(val);
        else if (key == "scrollBarColor") sf->scrollBarColor = toColor3(val);
        else if (key == "scrollBarTransparency") sf->scrollBarTransparency = toFloat(val);
        else if (key == "scrollBarThickness") sf->scrollBarThickness = toFloat(val);
        else if (key == "scrollBarThumbColor") sf->scrollBarThumbColor = toColor3(val);
        else if (key == "scrollBarThumbTransparency") sf->scrollBarThumbTransparency = toFloat(val);
        else if (key == "scrollSpeed") sf->scrollSpeed = toFloat(val);
        else if (key == "elasticScrolling") sf->elasticScrolling = val.asBool();
    }
}

void applyListLayoutAttrs(UIListLayout *ll, const std::unordered_map<std::string, AmlValue> &attrs)
{
    for (auto &[key, val] : attrs) {
        if (key == "fillDirection") ll->fillDirection = parseFillDirection(val.asString());
        else if (key == "horizontalAlignment") ll->horizontalAlignment = parseHorizontalAlignment(val.asString());
        else if (key == "verticalAlignment") ll->verticalAlignment = parseVerticalAlignment(val.asString());
        else if (key == "sortOrder") ll->sortOrder = parseSortOrder(val.asString());
        else if (key == "innerPadding") ll->innerPadding = toUDim(val);
        else if (key == "horizontalFlex") ll->horizontalFlex = parseFlexAlignment(val.asString());
        else if (key == "verticalFlex") ll->verticalFlex = parseFlexAlignment(val.asString());
        else if (key == "wraps") ll->wraps = val.asBool();
        else if (key == "itemLineAlignment") ll->itemLineAlignment = parseItemLineAlignment(val.asString());
    }
}

void applyGridLayoutAttrs(UIGridLayout *gl, const std::unordered_map<std::string, AmlValue> &attrs)
{
    for (auto &[key, val] : attrs) {
        if (key == "fillDirection") gl->fillDirection = parseFillDirection(val.asString());
        else if (key == "horizontalAlignment") gl->horizontalAlignment = parseHorizontalAlignment(val.asString());
        else if (key == "verticalAlignment") gl->verticalAlignment = parseVerticalAlignment(val.asString());
        else if (key == "sortOrder") gl->sortOrder = parseSortOrder(val.asString());
        else if (key == "cellPadding") gl->cellPadding = toUDim2(val);
        else if (key == "cellSize") gl->cellSize = toUDim2(val);
        else if (key == "startCorner") gl->startCorner = parseStartCorner(val.asString());
        else if (key == "fillDirectionMaxCells") gl->fillDirectionMaxCells = static_cast<uint32_t>(toInt(val));
    }
}

void applySizeConstraintAttrs(UISizeConstraint *sc, const std::unordered_map<std::string, AmlValue> &attrs)
{
    for (auto &[key, val] : attrs) {
        if (key == "maxSize") sc->maxSize = toVec2(val);
        else if (key == "minSize") sc->minSize = toVec2(val);
    }
}

void applyAspectRatioAttrs(UIAspectRatioConstraint *ar, const std::unordered_map<std::string, AmlValue> &attrs)
{
    for (auto &[key, val] : attrs) {
        if (key == "aspectRatio") ar->aspectRatio = toFloat(val);
    }
}

void applyTextSizeConstraintAttrs(UITextSizeConstraint *tc, const std::unordered_map<std::string, AmlValue> &attrs)
{
    for (auto &[key, val] : attrs) {
        if (key == "maxTextSize") tc->maxTextSize = toFloat(val);
        else if (key == "minTextSize") tc->minTextSize = toFloat(val);
    }
}

void applySliderAttrs(Slider *s, const std::unordered_map<std::string, AmlValue> &attrs)
{
    for (auto &[key, val] : attrs) {
        if (key == "sliderColor") s->sliderColor = toColor3(val);
        else if (key == "sliderTransparency") s->sliderTransparency = toFloat(val);
        else if (key == "thumbColor") s->thumbColor = toColor3(val);
        else if (key == "thumbTransparency") s->thumbTransparency = toFloat(val);
        else if (key == "trackCornerRadius") s->trackCornerRadius = toFloat(val);
        else if (key == "thumbCornerRadius") s->thumbCornerRadius = toFloat(val);
        else if (key == "label") s->label = val.asString();
        else if (key == "labelColor") s->labelColor = toColor4(val);
        else if (key == "labelSide") s->labelSide = parseLabelSide(val.asString());
        else if (key == "labelPadding") s->labelPadding = toUDim(val);
        else if (key == "valueColor") s->valueColor = toColor4(val);
        else if (key == "valueSuffix") s->valueSuffix = val.asString();
        else if (key == "fontSize") s->fontSize = toFloat(val);
        else if (key == "layout") s->layout = parseValueControlLayout(val.asString());
    }
}

void applyTextInputAttrs(TextInput *ti, const std::unordered_map<std::string, AmlValue> &attrs)
{
    for (auto &[key, val] : attrs) {
        if (key == "placeholderText") ti->placeholderText = val.asString();
        else if (key == "textColor") ti->textColor = toColor4(val);
        else if (key == "placeholderColor") ti->placeholderColor = toColor4(val);
        else if (key == "selectionColor") ti->selectionColor = toColor4(val);
        else if (key == "cursorColor") ti->cursorColor = toColor4(val);
        else if (key == "fontSize") ti->fontSize = toFloat(val);
        else if (key == "fontFamily") ti->fontFamily = val.asString();
        else if (key == "multiline") ti->multiline = val.asBool();
        else if (key == "maxLength") ti->maxLength = static_cast<int32_t>(toInt(val));
        else if (key == "readOnly") ti->readOnly = val.asBool();
        else if (key == "cursorBlinkRate") ti->cursorBlinkRate = toFloat(val);
        else if (key == "textXAlignment") ti->textXAlignment = parseTextXAlignment(val.asString());
    }
}

bool isExtensionTag(const std::string &tag)
{
    return tag == "UIListLayout" || tag == "UIGridLayout" || tag == "UISizeConstraint" || tag == "UIAspectRatioConstraint" ||
           tag == "UIDragDetector" || tag == "UITextSizeConstraint";
}

void applyExtension(UIObject *parent, const AmlNode &node)
{
    if (node.tag == "UIListLayout") {
        auto *ext = parent->addExtension<UIListLayout>();
        applyListLayoutAttrs(ext, node.attributes);
    } else if (node.tag == "UIGridLayout") {
        auto *ext = parent->addExtension<UIGridLayout>();
        applyGridLayoutAttrs(ext, node.attributes);
    } else if (node.tag == "UISizeConstraint") {
        auto *ext = parent->addExtension<UISizeConstraint>();
        applySizeConstraintAttrs(ext, node.attributes);
    } else if (node.tag == "UIAspectRatioConstraint") {
        auto *ext = parent->addExtension<UIAspectRatioConstraint>();
        applyAspectRatioAttrs(ext, node.attributes);
    } else if (node.tag == "UIDragDetector") {
        parent->addExtension<UIDragDetector>();
    } else if (node.tag == "UITextSizeConstraint") {
        auto *ext = parent->addExtension<UITextSizeConstraint>();
        applyTextSizeConstraintAttrs(ext, node.attributes);
    }
}

Instance *buildNode(const AmlNode &node, Instance *parent, std::vector<std::unique_ptr<Instance>> &allInstances,
                    std::string &error);

DockZone oppositeZone(DockZone zone)
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

int32_t buildDockChild(DockingLayer *dl, const AmlNode &child, int32_t targetLeaf, DockZone zone, float ratio,
                       std::vector<std::unique_ptr<Instance>> &allInstances, std::string &error)
{
    if (child.tag == "DockRegion") {
        DockZone innerZone = DockZone::CENTER;
        float innerRatio = 0.5f;
        for (auto &[key, val] : child.attributes) {
            if (key == "zone") innerZone = parseDockZone(val.asString());
            else if (key == "ratio") innerRatio = toFloat(val);
        }

        if (child.children.empty()) {
            AM_LOG_WARN("DockRegion has no children, skipping");
            return targetLeaf;
        }

        int32_t leaf = buildDockChild(dl, child.children[0], targetLeaf, zone, ratio, allInstances, error);
        if (!error.empty()) return -1;

        if (child.children.size() > 1) {
            leaf = buildDockChild(dl, child.children[1], leaf, oppositeZone(innerZone), innerRatio, allInstances, error);
            if (!error.empty()) return -1;
        }

        for (size_t i = 2; i < child.children.size(); i++) {
            leaf = buildDockChild(dl, child.children[i], leaf, DockZone::CENTER, 0.0f, allInstances, error);
            if (!error.empty()) return -1;
        }
        return leaf;
    }

    auto *built = buildNode(child, nullptr, allInstances, error);
    if (!error.empty()) return -1;

    auto *base2d = dynamic_cast<UIBase2D *>(built);
    if (!base2d) {
        error = "DockingLayer child must be a UIBase2D";
        return -1;
    }

    return dl->dock(base2d, targetLeaf, zone, ratio);
}

Instance *buildNode(const AmlNode &node, Instance *parent, std::vector<std::unique_ptr<Instance>> &allInstances, std::string &error)
{
    auto &tag = node.tag;

    if (tag == "Frame") {
        auto inst = std::make_unique<Frame>(parent);
        auto *ptr = inst.get();
        applyUIObjectAttrs(ptr, node.attributes);
        ptr->markDirty();
        allInstances.push_back(std::move(inst));

        for (auto &child : node.children) {
            if (isExtensionTag(child.tag)) {
                applyExtension(ptr, child);
            } else {
                buildNode(child, ptr, allInstances, error);
                if (!error.empty()) return nullptr;
            }
        }
        return ptr;
    }

    if (tag == "TextLabel") {
        auto inst = std::make_unique<TextLabel>(parent);
        auto *ptr = inst.get();
        applyUIObjectAttrs(ptr, node.attributes);
        applyTextAttrs(node.attributes, ptr->text, ptr->fontFamily, ptr->fontSize, ptr->textColor, ptr->textXAlignment,
                       ptr->textYAlignment, ptr->textTruncate, ptr->richText, ptr->textWrapped, ptr->textScaled, ptr->lineHeight,
                       ptr->strokeThickness, ptr->strokeColor);
        ptr->markDirty();
        allInstances.push_back(std::move(inst));

        for (auto &child : node.children) {
            if (isExtensionTag(child.tag)) {
                applyExtension(ptr, child);
            } else {
                buildNode(child, ptr, allInstances, error);
                if (!error.empty()) return nullptr;
            }
        }
        return ptr;
    }

    if (tag == "TextButton") {
        auto inst = std::make_unique<TextButton>(parent);
        auto *ptr = inst.get();
        applyUIObjectAttrs(ptr, node.attributes);
        applyTextAttrs(node.attributes, ptr->text, ptr->fontFamily, ptr->fontSize, ptr->textColor, ptr->textXAlignment,
                       ptr->textYAlignment, ptr->textTruncate, ptr->richText, ptr->textWrapped, ptr->textScaled, ptr->lineHeight,
                       ptr->strokeThickness, ptr->strokeColor);

        for (auto &[key, val] : node.attributes) {
            if (key == "autoButtonColor") ptr->autoButtonColor = val.asBool();
            else if (key == "modal") ptr->modal = val.asBool();
        }

        ptr->markDirty();
        allInstances.push_back(std::move(inst));

        for (auto &child : node.children) {
            if (isExtensionTag(child.tag)) {
                applyExtension(ptr, child);
            } else {
                buildNode(child, ptr, allInstances, error);
                if (!error.empty()) return nullptr;
            }
        }
        return ptr;
    }

    if (tag == "ImageLabel") {
        auto inst = std::make_unique<ImageLabel>(parent);
        auto *ptr = inst.get();
        applyUIObjectAttrs(ptr, node.attributes);

        for (auto &[key, val] : node.attributes) {
            if (key == "imageColor") ptr->imageColor = toColor4(val);
            else if (key == "imageTransparency") ptr->imageTransparency = toFloat(val);
        }

        ptr->markDirty();
        allInstances.push_back(std::move(inst));

        for (auto &child : node.children) {
            if (isExtensionTag(child.tag)) {
                applyExtension(ptr, child);
            } else {
                buildNode(child, ptr, allInstances, error);
                if (!error.empty()) return nullptr;
            }
        }
        return ptr;
    }

    if (tag == "ImageButton") {
        auto inst = std::make_unique<ImageButton>(parent);
        auto *ptr = inst.get();
        applyUIObjectAttrs(ptr, node.attributes);

        for (auto &[key, val] : node.attributes) {
            if (key == "imageColor") ptr->imageColor = toColor4(val);
            else if (key == "imageTransparency") ptr->imageTransparency = toFloat(val);
            else if (key == "autoButtonColor") ptr->autoButtonColor = val.asBool();
            else if (key == "modal") ptr->modal = val.asBool();
        }

        ptr->markDirty();
        allInstances.push_back(std::move(inst));

        for (auto &child : node.children) {
            if (isExtensionTag(child.tag)) {
                applyExtension(ptr, child);
            } else {
                buildNode(child, ptr, allInstances, error);
                if (!error.empty()) return nullptr;
            }
        }
        return ptr;
    }

    if (tag == "ScrollingFrame") {
        auto inst = std::make_unique<ScrollingFrame>(parent);
        auto *ptr = inst.get();
        applyUIObjectAttrs(ptr, node.attributes);
        applyScrollingFrameAttrs(ptr, node.attributes);
        ptr->markDirty();
        allInstances.push_back(std::move(inst));

        for (auto &child : node.children) {
            if (isExtensionTag(child.tag)) {
                applyExtension(ptr, child);
            } else {
                buildNode(child, ptr, allInstances, error);
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
        auto inst = std::make_unique<InvisibleButton>(parent);
        auto *ptr = inst.get();
        applyUIObjectAttrs(ptr, node.attributes);

        for (auto &[key, val] : node.attributes) {
            if (key == "autoButtonColor") ptr->autoButtonColor = val.asBool();
            else if (key == "modal") ptr->modal = val.asBool();
        }

        ptr->markDirty();
        allInstances.push_back(std::move(inst));

        for (auto &child : node.children) {
            if (isExtensionTag(child.tag)) {
                applyExtension(ptr, child);
            } else {
                buildNode(child, ptr, allInstances, error);
                if (!error.empty()) return nullptr;
            }
        }
        return ptr;
    }

    if (tag == "TextInput") {
        auto inst = std::make_unique<TextInput>(parent);
        auto *ptr = inst.get();
        applyUIObjectAttrs(ptr, node.attributes);
        applyTextInputAttrs(ptr, node.attributes);
        ptr->markDirty();
        allInstances.push_back(std::move(inst));

        for (auto &child : node.children) {
            if (isExtensionTag(child.tag)) {
                applyExtension(ptr, child);
            } else {
                buildNode(child, ptr, allInstances, error);
                if (!error.empty()) return nullptr;
            }
        }
        return ptr;
    }

    if (tag == "SliderFloat") {
        auto inst = std::make_unique<SliderFloat>(parent);
        auto *ptr = inst.get();
        applyUIObjectAttrs(ptr, node.attributes);
        applySliderAttrs(ptr, node.attributes);

        for (auto &[key, val] : node.attributes) {
            if (key == "min") ptr->min = toFloat(val);
            else if (key == "max") ptr->max = toFloat(val);
            else if (key == "speed") ptr->speed = toFloat(val);
        }

        ptr->markDirty();
        allInstances.push_back(std::move(inst));

        for (auto &child : node.children) {
            if (isExtensionTag(child.tag)) {
                applyExtension(ptr, child);
            } else {
                buildNode(child, ptr, allInstances, error);
                if (!error.empty()) return nullptr;
            }
        }
        return ptr;
    }

    if (tag == "SliderInt") {
        auto inst = std::make_unique<SliderInt>(parent);
        auto *ptr = inst.get();
        applyUIObjectAttrs(ptr, node.attributes);
        applySliderAttrs(ptr, node.attributes);

        for (auto &[key, val] : node.attributes) {
            if (key == "min") ptr->min = static_cast<int>(toInt(val));
            else if (key == "max") ptr->max = static_cast<int>(toInt(val));
            else if (key == "speed") ptr->speed = toFloat(val);
        }

        ptr->markDirty();
        allInstances.push_back(std::move(inst));

        for (auto &child : node.children) {
            if (isExtensionTag(child.tag)) {
                applyExtension(ptr, child);
            } else {
                buildNode(child, ptr, allInstances, error);
                if (!error.empty()) return nullptr;
            }
        }
        return ptr;
    }

    if (tag == "SliderVec2") {
        auto inst = std::make_unique<SliderVec2>(parent);
        auto *ptr = inst.get();
        applyUIObjectAttrs(ptr, node.attributes);
        applySliderAttrs(ptr, node.attributes);

        for (auto &[key, val] : node.attributes) {
            if (key == "min") ptr->min = toVec2(val);
            else if (key == "max") ptr->max = toVec2(val);
            else if (key == "speed") ptr->speed = toFloat(val);
        }

        ptr->markDirty();
        allInstances.push_back(std::move(inst));

        for (auto &child : node.children) {
            if (isExtensionTag(child.tag)) {
                applyExtension(ptr, child);
            } else {
                buildNode(child, ptr, allInstances, error);
                if (!error.empty()) return nullptr;
            }
        }
        return ptr;
    }

    if (tag == "SliderVec3") {
        auto inst = std::make_unique<SliderVec3>(parent);
        auto *ptr = inst.get();
        applyUIObjectAttrs(ptr, node.attributes);
        applySliderAttrs(ptr, node.attributes);

        for (auto &[key, val] : node.attributes) {
            if (key == "min") ptr->min = toVec3(val);
            else if (key == "max") ptr->max = toVec3(val);
            else if (key == "speed") ptr->speed = toFloat(val);
        }

        ptr->markDirty();
        allInstances.push_back(std::move(inst));

        for (auto &child : node.children) {
            if (isExtensionTag(child.tag)) {
                applyExtension(ptr, child);
            } else {
                buildNode(child, ptr, allInstances, error);
                if (!error.empty()) return nullptr;
            }
        }
        return ptr;
    }

    if (tag == "DockingLayer") {
        auto inst = std::make_unique<DockingLayer>(parent);
        auto *ptr = inst.get();

        for (auto &[key, val] : node.attributes) {
            if (key == "name") ptr->name = val.asString();
            else if (key == "visible") ptr->visible = val.asBool();
            else if (key == "outerSpacing") ptr->outerSpacing = toFloat(val);
            else if (key == "innerSpacing") ptr->innerSpacing = toFloat(val);
        }

        ptr->markDirty();
        allInstances.push_back(std::move(inst));

        for (auto &child : node.children) {
            if (child.tag != "DockRegion") {
                AM_LOG_WARN("DockingLayer only accepts DockRegion children, got '{}'", child.tag);
                continue;
            }
            buildDockChild(ptr, child, -1, DockZone::CENTER, 0.0f, allInstances, error);
            if (!error.empty()) return nullptr;
        }
        return ptr;
    }

    if (tag == "OverlayLayer") {
        auto inst = std::make_unique<OverlayLayer>();
        auto *ptr = inst.get();
        if (parent) ptr->setParent(parent);

        for (auto &[key, val] : node.attributes) {
            if (key == "name") ptr->name = val.asString();
            else if (key == "visible") ptr->visible = val.asBool();
        }

        ptr->markDirty();
        allInstances.push_back(std::move(inst));

        for (auto &child : node.children) {
            buildNode(child, ptr, allInstances, error);
            if (!error.empty()) return nullptr;
        }
        return ptr;
    }

    if (tag == "PanelLayer") {
        auto inst = std::make_unique<PanelLayer>();
        auto *ptr = inst.get();
        if (parent) ptr->setParent(parent);

        for (auto &[key, val] : node.attributes) {
            if (key == "name") ptr->name = val.asString();
            else if (key == "visible") ptr->visible = val.asBool();
        }

        ptr->markDirty();
        allInstances.push_back(std::move(inst));

        for (auto &child : node.children) {
            buildNode(child, ptr, allInstances, error);
            if (!error.empty()) return nullptr;
        }
        return ptr;
    }

    error = "[" + std::to_string(node.line) + ":" + std::to_string(node.column) + "] Unknown tag '" + tag + "'";
    return nullptr;
}

} // anonymous namespace

AmlLoadResult::~AmlLoadResult()
{
    for (auto it = instances.rbegin(); it != instances.rend(); ++it) {
        it->reset();
    }
}

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
        buildNode(rootNode, nullptr, result.instances, result.error);
        if (!result.ok()) {
            result.instances.clear();
            return result;
        }
    }

    return result;
}

} // namespace Amethyst
