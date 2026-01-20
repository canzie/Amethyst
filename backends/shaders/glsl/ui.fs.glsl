#version 450
#extension GL_EXT_nonuniform_qualifier : require

layout(location = 0) in vec2 fragUV;
layout(location = 1) in flat uint fragPrimitiveType;
layout(location = 2) in flat vec4 fragFillColor;
layout(location = 3) in flat vec4 fragBorderColor;
layout(location = 4) in flat float fragBorderThickness;
layout(location = 5) in flat float fragCornerRadius;
layout(location = 6) in flat vec2 fragSize;
layout(location = 7) in flat uint fragBorderMode;
layout(location = 8) in flat uint fragTextureId;
layout(location = 9) in flat uint fragVisible;

layout(set = 0, binding = 1) uniform sampler2D gTextures[];

layout(location = 0) out vec4 outColor;

const uint INVALID_TEXTURE = 0xFFFFFFFF;

const uint PRIMITIVE_RECT = 0;
const uint PRIMITIVE_CIRCLE = 1;
const uint PRIMITIVE_TRIANGLE = 2;
const uint PRIMITIVE_LINE = 3;

const uint BORDER_OUTLINE = 0;
const uint BORDER_MIDDLE = 1;
const uint BORDER_INSET = 2;


float sdfRect(vec2 p, vec2 b, vec4 r)
{    
    r.xy = (p.x>0.0) ? r.xy : r.zw;
    r.x  = (p.y>0.0) ? r.x  : r.y;
    vec2 q = abs(p)-b+r.x;
    return min(max(q.x,q.y),0.0) + length(max(q, vec2(0.0))) - r.x;
}

float sdfCircle(vec2 p, float radius)
{
    return length(p) - radius;
}

float sdfTriangle(vec2 p, vec2 halfSize)
{
    p.x = abs(p.x);
    vec2 a = vec2(halfSize.x, -halfSize.y);
    vec2 b = vec2(0.0, halfSize.y);
    vec2 ba = b - a;
    vec2 pa = p - a;
    float h = clamp(dot(pa, ba) / dot(ba, ba), 0.0, 1.0);
    float d = length(pa - ba * h);
    float s = sign(pa.x * ba.y - pa.y * ba.x);
    return d * s;
}

float sdEquilateralTriangle(vec2 p, float r)
{
    const float k = sqrt(3.0);
    p.x = abs(p.x) - r;
    p.y = p.y + r/k;
    if( p.x+k*p.y>0.0 ) p = vec2(p.x-k*p.y,-k*p.x-p.y)/2.0;
    p.x -= clamp( p.x, -2.0*r, 0.0 );
    return -length(p)*sign(p.y);
}

float sdfLine(vec2 p, vec2 halfSize, float thickness)
{
    float d = abs(p.y) - thickness * 0.5;
    d = max(d, abs(p.x) - halfSize.x);
    return d;
}

void main()
{
    if (fragVisible == 0u) discard;

    vec2 p = (fragUV - 0.5) * fragSize;
    vec2 halfSize = fragSize * 0.5;

    float dist;

    if (fragPrimitiveType == PRIMITIVE_RECT) {
        dist = sdfRect(p, halfSize, vec4(fragCornerRadius));
    } else if (fragPrimitiveType == PRIMITIVE_CIRCLE) {
        dist = sdfCircle(p, min(halfSize.x, halfSize.y));
    } else if (fragPrimitiveType == PRIMITIVE_TRIANGLE) {
        dist = sdfTriangle(p, halfSize);
    } else if (fragPrimitiveType == PRIMITIVE_LINE) {
        dist = sdfLine(p, halfSize, fragCornerRadius);
    } else {
        dist = sdfRect(p, halfSize, vec4(0.0));
    }

    float aa = fwidth(dist);

    float outerThreshold = 0.0;
    float innerThreshold = 0.0;

    if (fragBorderThickness > 0.0) {
        if (fragBorderMode == BORDER_OUTLINE) {
            outerThreshold = fragBorderThickness;
            innerThreshold = 0.0;
        } else if (fragBorderMode == BORDER_MIDDLE) {
            outerThreshold = fragBorderThickness * 0.5;
            innerThreshold = -fragBorderThickness * 0.5;
        } else {
            outerThreshold = 0.0;
            innerThreshold = -fragBorderThickness;
        }
    }

    float shapeAlpha = 1.0 - smoothstep(-aa, aa, dist - outerThreshold);

    vec4 color;
    if (fragBorderThickness > 0.0) {
        float fillMask = 1.0 - smoothstep(-aa, aa, dist - innerThreshold);
        color = mix(fragBorderColor, fragFillColor, fillMask);
    } else {
        color = fragFillColor;
    }

    if (fragTextureId != INVALID_TEXTURE) {
        vec4 texColor = texture(gTextures[fragTextureId], fragUV);
        color.rgb = mix(color.rgb, texColor.rgb, texColor.a);
    }

    float finalAlpha = shapeAlpha * color.a;
    if (finalAlpha < 0.001) discard;

    outColor = vec4(color.rgb, finalAlpha);
}
