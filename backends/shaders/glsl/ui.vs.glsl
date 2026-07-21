#version 450

layout(push_constant) uniform PushConstants {
    vec2 screenSize;
    uint glyphOffset;
    uint lineOffset;
    uint sliceOffset;
} pc;

struct GlyphSlice {
    uint firstGlyph;
    uint firstLine;
    uint packed;
    uint pad;
};

struct InstanceData {
    vec2 translation;             // 8B position
    vec2 scale;                   // 8B size
    vec4 clipRect;                // 16B clip rect
    uint fillColor;               // 4B packed RGBA
    uint borderColor;             // 4B packed RGBA
    uint shapeData[4];            // 16B packed half2 pairs for text UV etc
    uint rotationBorderThickness; // 4B: rotation(16) | borderThickness(16 half)
    uint primitiveMode;           // 4B: unused(16) | primitiveType(8) | borderMode(8)
    uint cornerRadii;             // 4B: rounded-box per-corner radii tl|tr|br|bl (uint8), else radius(16 half)
    uint textureId;               // 4B texture handle
    int zIndex;                   // 4B z-index
    uint flags;                   // 4B: visible(1) | padding(31)
};

layout(std430, binding = 0) readonly buffer InstanceBuffer {
    InstanceData instances[];
};

layout(std430, binding = 4) readonly buffer SliceBuffer {
    GlyphSlice slices[];
};

layout(location = 0) out vec2 fragUV;
layout(location = 1) out flat uint fragPrimitiveType;
layout(location = 2) out flat vec4 fragFillColor;
layout(location = 3) out flat vec4 fragBorderColor;
layout(location = 4) out flat float fragBorderThickness;
layout(location = 5) out flat float fragCornerRadius;
layout(location = 6) out flat vec2 fragSize;
layout(location = 7) out flat uint fragBorderMode;
layout(location = 8) out flat uint fragTextureId;
layout(location = 9) out flat vec4 fragClipRect;
layout(location = 10) out vec2 fragWorldPos;
layout(location = 11) out flat uvec4 fragShapeData;
layout(location = 12) out flat uint fragGlyphBase;
layout(location = 13) out flat uint fragLineBase;
layout(location = 14) out flat uint fragLineCount;
layout(location = 15) out flat float fragLineHeight;
layout(location = 16) out flat uint fragTextBatched;
layout(location = 17) out flat uint fragGradientSlot;
layout(location = 18) out flat vec4 fragCornerRadii;

const vec2 positions[4] = vec2[](
    vec2(-0.5, -0.5),
    vec2( 0.5, -0.5),
    vec2( 0.5,  0.5),
    vec2(-0.5,  0.5)
);

const vec2 uvs[4] = vec2[](
    vec2(0.0, 0.0),
    vec2(1.0, 0.0),
    vec2(1.0, 1.0),
    vec2(0.0, 1.0)
);

const uint PRIMITIVE_TEXT = 4;
const uint PRIMITIVE_SVG = 10;
const uint BORDER_OUTLINE = 0u;
const uint BORDER_MIDDLE = 1u;
const uint BORDER_INSET = 2u;
// Outward bleed for the outer-edge AA fringe (smoothstep over ~fwidth(dist), fwidth is
// fragment-only so this must be a constant). 1px covers the typical ~1px fringe.
const float BORDER_AA_PAD = 1.0;
const float PI = 3.14159265359;
const uint INSTANCE_FLAG_VISIBLE = 0x00000001u;
const uint INSTANCE_FLAG_TEXT_RICH = 0x00000002u;
const uint INSTANCE_FLAG_GRADIENT = 0x00000004u;
const uint INVALID_GRADIENT = 0xFFFFFFFFu;

vec3 srgbToLinear(vec3 c) {
    return mix(c / 12.92, pow((c + 0.055) / 1.055, vec3(2.4)), step(0.04045, c));
}

vec4 unpackColor(uint packed) {
    vec4 c = vec4(
        float(packed & 0xFFu) / 255.0,
        float((packed >> 8u) & 0xFFu) / 255.0,
        float((packed >> 16u) & 0xFFu) / 255.0,
        float((packed >> 24u) & 0xFFu) / 255.0
    );
    c.rgb = srgbToLinear(c.rgb);
    return c;
}

float unpackHalf(uint packed) {
    return unpackHalf2x16(packed).x;
}

#define INST_ROTATION(rbt)         (float((rbt) & 0xFFFFu) * (2.0 * PI / 65535.0))
#define INST_BORDER_THICKNESS(rbt) unpackHalf((rbt) >> 16u)
#define INST_PRIMITIVE_TYPE(pm)    (((pm) >> 16u) & 0xFFu)
#define INST_BORDER_MODE(pm)       (((pm) >> 24u) & 0xFFu)
#define INST_CORNER_RADIUS(cr)     unpackHalf(cr)
#define INST_CORNER_TL(cr)         ((cr) & 0xFFu)
#define INST_CORNER_TR(cr)         (((cr) >> 8u) & 0xFFu)
#define INST_CORNER_BR(cr)         (((cr) >> 16u) & 0xFFu)
#define INST_CORNER_BL(cr)         (((cr) >> 24u) & 0xFFu)

void main()
{
    InstanceData inst = instances[gl_InstanceIndex];

    // GPU-side visibility culling
    if ((inst.flags & INSTANCE_FLAG_VISIBLE) == 0u) {
        gl_Position = vec4(0.0, 0.0, 0.0, 0.0);
        return;
    }

    // Unpack rotation from lower 16 bits
    float rotation = INST_ROTATION(inst.rotationBorderThickness);
    float c = cos(rotation);
    float s = sin(rotation);

    float borderThickness = INST_BORDER_THICKNESS(inst.rotationBorderThickness);
    uint borderMode = INST_BORDER_MODE(inst.primitiveMode);

    // Outward border modes draw outside the shape edge, but the quad is sized to the shape so
    // they have no fragments to land on. Grow the quad by the outward extent (+ AA fringe) and
    // extend fragUV by the same ratio so the shape still maps to UV [0,1].
    float outward = (borderMode == BORDER_OUTLINE) ? borderThickness
                  : (borderMode == BORDER_MIDDLE)  ? borderThickness * 0.5
                                                   : 0.0;
    float pad = (outward > 0.0) ? (outward + BORDER_AA_PAD) : 0.0;
    vec2 paddedScale = inst.scale + 2.0 * pad;

    // Build transform: scale, rotate, translate
    vec2 localPos = positions[gl_VertexIndex];
    vec2 scaled = localPos * paddedScale;
    vec2 rotated = vec2(scaled.x * c - scaled.y * s, scaled.x * s + scaled.y * c);
    vec2 worldPos = rotated + inst.translation;

    vec2 ndc = (worldPos / pc.screenSize) * 2.0 - 1.0;
    gl_Position = vec4(ndc, 0.0, 1.0);

    vec2 padRatio = (pad > 0.0) ? (vec2(pad) / inst.scale) : vec2(0.0);
    fragUV = uvs[gl_VertexIndex] * (1.0 + 2.0 * padRatio) - padRatio;

    // Unpack primitive type from bits 16-23
    uint primitiveType = INST_PRIMITIVE_TYPE(inst.primitiveMode);

    bool textBatched = (primitiveType == PRIMITIVE_TEXT) && ((inst.flags & INSTANCE_FLAG_TEXT_RICH) == 0u);
    fragTextBatched = textBatched ? 1u : 0u;
    fragGlyphBase = 0u;
    fragLineBase = 0u;
    fragLineCount = 0u;
    fragLineHeight = 0.0;

    if (textBatched) {
        GlyphSlice sl = slices[pc.sliceOffset + inst.shapeData[0]];
        fragGlyphBase = pc.glyphOffset + sl.firstGlyph;
        fragLineBase = pc.lineOffset + sl.firstLine;
        fragLineCount = sl.packed & 0xFFFFu;
        fragLineHeight = unpackHalf2x16(sl.packed).y;
    } else if (primitiveType == PRIMITIVE_TEXT || primitiveType == PRIMITIVE_SVG) {
        vec2 uvMin = unpackHalf2x16(inst.shapeData[0]);
        vec2 uvMax = unpackHalf2x16(inst.shapeData[1]);
        fragUV = mix(uvMin, uvMax, uvs[gl_VertexIndex]);
    }

    fragPrimitiveType = primitiveType;
    fragFillColor = unpackColor(inst.fillColor);
    fragBorderColor = unpackColor(inst.borderColor);

    // Unpack border thickness from upper 16 bits as half float
    fragBorderThickness = borderThickness;

    // The cornerRadii word is primitive-dependent: a rounded box reads four per-corner radius
    // bytes, every other primitive reads its low 16 bits as a stroke/circle radius half float.
    fragCornerRadius = INST_CORNER_RADIUS(inst.cornerRadii);
    fragCornerRadii = vec4(INST_CORNER_TL(inst.cornerRadii), INST_CORNER_TR(inst.cornerRadii),
                           INST_CORNER_BR(inst.cornerRadii), INST_CORNER_BL(inst.cornerRadii));

    fragSize = inst.scale;

    // Unpack border mode from bits 24-31
    fragBorderMode = borderMode;

    fragTextureId = inst.textureId;
    fragClipRect = inst.clipRect;
    fragWorldPos = worldPos;
    fragShapeData = uvec4(inst.shapeData[0], inst.shapeData[1], inst.shapeData[2], inst.shapeData[3]);

    fragGradientSlot = ((inst.flags & INSTANCE_FLAG_GRADIENT) != 0u) ? inst.shapeData[1] : INVALID_GRADIENT;
}
