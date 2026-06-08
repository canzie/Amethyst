#version 450

layout(push_constant) uniform PushConstants {
    vec2 screenSize;
} pc;

struct InstanceData {
    vec2 translation;             // 8B position
    vec2 scale;                   // 8B size
    vec4 clipRect;                // 16B clip rect
    uint fillColor;               // 4B packed RGBA
    uint borderColor;             // 4B packed RGBA
    uint shapeData[4];            // 16B packed half2 pairs for text UV etc
    uint rotationBorderThickness; // 4B: rotation(16) | borderThickness(16 half)
    uint cornerPrimitiveMode;     // 4B: cornerRadius(16 half) | primitiveType(8) | borderMode(8)
    uint textureId;               // 4B texture handle
    int zIndex;                   // 4B z-index
    uint flags;                   // 4B: visible(1) | padding(31)
};

layout(std430, binding = 0) readonly buffer InstanceBuffer {
    InstanceData instances[];
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
const float PI = 3.14159265359;
const uint INSTANCE_FLAG_VISIBLE = 0x00000001u;

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

float unpackRotation(uint packed) {
    return float(packed) * (2.0 * PI / 65535.0);
}

void main()
{
    InstanceData inst = instances[gl_InstanceIndex];

    // GPU-side visibility culling
    if ((inst.flags & INSTANCE_FLAG_VISIBLE) == 0u) {
        gl_Position = vec4(0.0, 0.0, 0.0, 0.0);
        return;
    }

    // Unpack rotation from lower 16 bits
    uint rotationPacked = inst.rotationBorderThickness & 0xFFFFu;
    float rotation = unpackRotation(rotationPacked);
    float c = cos(rotation);
    float s = sin(rotation);

    // Build transform: scale, rotate, translate
    vec2 localPos = positions[gl_VertexIndex];
    vec2 scaled = localPos * inst.scale;
    vec2 rotated = vec2(scaled.x * c - scaled.y * s, scaled.x * s + scaled.y * c);
    vec2 worldPos = rotated + inst.translation;

    vec2 ndc = (worldPos / pc.screenSize) * 2.0 - 1.0;
    gl_Position = vec4(ndc, 0.0, 1.0);

    fragUV = uvs[gl_VertexIndex];

    // Unpack primitive type from bits 16-23
    uint primitiveType = (inst.cornerPrimitiveMode >> 16u) & 0xFFu;

    if (primitiveType == PRIMITIVE_TEXT || primitiveType == PRIMITIVE_SVG) {
        vec2 uvMin = unpackHalf2x16(inst.shapeData[0]);
        vec2 uvMax = unpackHalf2x16(inst.shapeData[1]);
        fragUV = mix(uvMin, uvMax, uvs[gl_VertexIndex]);
    }

    fragPrimitiveType = primitiveType;
    fragFillColor = unpackColor(inst.fillColor);
    fragBorderColor = unpackColor(inst.borderColor);

    // Unpack border thickness from upper 16 bits as half float
    fragBorderThickness = unpackHalf(inst.rotationBorderThickness >> 16u);

    // Unpack corner radius from lower 16 bits as half float
    fragCornerRadius = unpackHalf(inst.cornerPrimitiveMode & 0xFFFFu);

    fragSize = inst.scale;

    // Unpack border mode from bits 24-31
    fragBorderMode = (inst.cornerPrimitiveMode >> 24u) & 0xFFu;

    fragTextureId = inst.textureId;
    fragClipRect = inst.clipRect;
    fragWorldPos = worldPos;
    fragShapeData = uvec4(inst.shapeData[0], inst.shapeData[1], inst.shapeData[2], inst.shapeData[3]);
}
