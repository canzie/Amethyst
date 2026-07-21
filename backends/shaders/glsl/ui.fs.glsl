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
layout(location = 9) in flat vec4 fragClipRect;
layout(location = 10) in vec2 fragWorldPos;
layout(location = 11) in flat uvec4 fragShapeData;
layout(location = 12) in flat uint fragGlyphBase;
layout(location = 13) in flat uint fragLineBase;
layout(location = 14) in flat uint fragLineCount;
layout(location = 15) in flat float fragLineHeight;
layout(location = 16) in flat uint fragTextBatched;
layout(location = 17) in flat uint fragGradientSlot;
layout(location = 18) in flat vec4 fragCornerRadii;

layout(set = 0, binding = 1) uniform sampler2D gTextures[];

struct GlyphQuad {
    uint posMin;
    uint posMax;
    uint uvMin;
    uint uvMax;
};

struct GlyphLine {
    uint glyphStart;
    uint glyphCount;
};

layout(std430, binding = 2) readonly buffer GlyphBuffer {
    GlyphQuad glyphs[];
};

layout(std430, binding = 3) readonly buffer LineBuffer {
    GlyphLine glyphLines[];
};

struct Gradient {
    uint header;
    float angle;
    uint radialCenter;
    float radialRadius;
    uint stopT[4];
    uint stopColor[8];
};

layout(std430, binding = 5) readonly buffer GradientBuffer {
    Gradient gradients[];
};

layout(location = 0) out vec4 outColor;

const uint INVALID_TEXTURE = 0xFFFFFFFF;
const uint INVALID_GRADIENT = 0xFFFFFFFF;

const uint PRIMITIVE_RECT = 0;
const uint PRIMITIVE_CIRCLE = 1;
const uint PRIMITIVE_TRIANGLE = 2;
const uint PRIMITIVE_LINE = 3;
const uint PRIMITIVE_TEXT = 4;
const uint PRIMITIVE_CANVAS_LINE = 5;
const uint PRIMITIVE_CANVAS_TRI = 6;
const uint PRIMITIVE_CANVAS_QUAD = 7;
const uint PRIMITIVE_CANVAS_CIRCLE = 8;
const uint PRIMITIVE_CANVAS_ELLIPSE = 9;
const uint PRIMITIVE_SVG = 10;
const uint PRIMITIVE_CANVAS_BEZIER = 11;

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

// iquilezles.org/articles/distfunctions2d
float sdSegment(vec2 p, vec2 a, vec2 b)
{
    vec2 pa = p - a, ba = b - a;
    float h = clamp(dot(pa, ba) / dot(ba, ba), 0.0, 1.0);
    return length(pa - ba * h);
}

// iquilezles.org/articles/distfunctions2d
// Unsigned distance to a quadratic Bezier defined by endpoints A, C and control B.
// Solves the cubic dq/dt = 0 in closed form. Degenerates to a division by zero when the
// control is colinear with the endpoints (b == 0), so fall back to a straight segment there.
float sdBezier(vec2 pos, vec2 A, vec2 B, vec2 C)
{
    vec2 a = B - A;
    vec2 b = A - 2.0 * B + C;
    vec2 c = a * 2.0;
    vec2 d = A - pos;

    if (dot(b, b) < 1e-6) {
        return sdSegment(pos, A, C);
    }

    float kk = 1.0 / dot(b, b);
    float kx = kk * dot(a, b);
    float ky = kk * (2.0 * dot(a, a) + dot(d, b)) / 3.0;
    float kz = kk * dot(d, a);

    float res = 0.0;
    float p = ky - kx * kx;
    float p3 = p * p * p;
    float q = kx * (2.0 * kx * kx - 3.0 * ky) + kz;
    float h = q * q + 4.0 * p3;

    if (h >= 0.0) {
        h = sqrt(h);
        vec2 x = (vec2(h, -h) - q) / 2.0;
        vec2 uv = sign(x) * pow(abs(x), vec2(1.0 / 3.0));
        float t = clamp(uv.x + uv.y - kx, 0.0, 1.0);
        vec2 qd = d + (c + b * t) * t;
        res = dot(qd, qd);
    } else {
        float z = sqrt(-p);
        float v = acos(q / (p * z * 2.0)) / 3.0;
        float m = cos(v);
        float n = sin(v) * 1.732050808;
        vec3 t = clamp(vec3(m + m, -n - m, n - m) * z - kx, 0.0, 1.0);
        vec2 qx = d + (c + b * t.x) * t.x;
        vec2 qy = d + (c + b * t.y) * t.y;
        res = min(dot(qx, qx), dot(qy, qy));
    }
    return sqrt(res);
}

// iquilezles.org/articles/distfunctions2d
float sdCanvasTriangle(vec2 p, vec2 p0, vec2 p1, vec2 p2)
{
    vec2 e0 = p1 - p0, e1 = p2 - p1, e2 = p0 - p2;
    vec2 v0 = p - p0,  v1 = p - p1,  v2 = p - p2;
    vec2 pq0 = v0 - e0 * clamp(dot(v0, e0) / dot(e0, e0), 0.0, 1.0);
    vec2 pq1 = v1 - e1 * clamp(dot(v1, e1) / dot(e1, e1), 0.0, 1.0);
    vec2 pq2 = v2 - e2 * clamp(dot(v2, e2) / dot(e2, e2), 0.0, 1.0);
    float s = sign(e0.x * e2.y - e0.y * e2.x);
    vec2 d = min(min(
        vec2(dot(pq0, pq0), s * (v0.x * e0.y - v0.y * e0.x)),
        vec2(dot(pq1, pq1), s * (v1.x * e1.y - v1.y * e1.x))),
        vec2(dot(pq2, pq2), s * (v2.x * e2.y - v2.y * e2.x)));
    return -sqrt(d.x) * sign(d.y);
}

float sdCanvasQuad(vec2 p, vec2 a, vec2 b, vec2 c, vec2 d)
{
    vec2 e0 = b - a, e1 = c - b, e2 = d - c, e3 = a - d;
    vec2 v0 = p - a,  v1 = p - b,  v2 = p - c,  v3 = p - d;
    vec2 pq0 = v0 - e0 * clamp(dot(v0, e0) / dot(e0, e0), 0.0, 1.0);
    vec2 pq1 = v1 - e1 * clamp(dot(v1, e1) / dot(e1, e1), 0.0, 1.0);
    vec2 pq2 = v2 - e2 * clamp(dot(v2, e2) / dot(e2, e2), 0.0, 1.0);
    vec2 pq3 = v3 - e3 * clamp(dot(v3, e3) / dot(e3, e3), 0.0, 1.0);
    float s = sign(e0.x * e3.y - e0.y * e3.x);
    vec2 dd = min(min(
        vec2(dot(pq0, pq0), s * (v0.x * e0.y - v0.y * e0.x)),
        vec2(dot(pq1, pq1), s * (v1.x * e1.y - v1.y * e1.x))),
        min(
        vec2(dot(pq2, pq2), s * (v2.x * e2.y - v2.y * e2.x)),
        vec2(dot(pq3, pq3), s * (v3.x * e3.y - v3.y * e3.x))));
    return -sqrt(dd.x) * sign(dd.y);
}

// iquilezles.org/articles/ellipsedist
float sdEllipse(vec2 p, vec2 ab)
{
    p = abs(p);
    if (p.x > p.y) { p = p.yx; ab = ab.yx; }
    float l = ab.y * ab.y - ab.x * ab.x;
    float m = ab.x * p.x / l;
    float m2 = m * m;
    float n = ab.y * p.y / l;
    float n2 = n * n;
    float c = (m2 + n2 - 1.0) / 3.0;
    float c3 = c * c * c;
    float q = c3 + m2 * n2 * 2.0;
    float d = c3 + m2 * n2;
    float g = m + m * n2;
    float co;
    if (d < 0.0) {
        float h = acos(q / c3) / 3.0;
        float s = cos(h);
        float t = sin(h) * sqrt(3.0);
        float rx = sqrt(-c * (s + t + 2.0) + m2);
        float ry = sqrt(-c * (s - t + 2.0) + m2);
        co = (ry + sign(l) * rx + abs(g) / (rx * ry) - m) / 2.0;
    } else {
        float h = 2.0 * m * n * sqrt(d);
        float s = sign(q + h) * pow(abs(q + h), 1.0 / 3.0);
        float u = sign(q - h) * pow(abs(q - h), 1.0 / 3.0);
        float rx = -s - u - c * 4.0 + 2.0 * m2;
        float ry = (s - u) * sqrt(3.0);
        float rm = sqrt(rx * rx + ry * ry);
        co = (ry / sqrt(rm - rx) + 2.0 * g / rm - m) / 2.0;
    }
    vec2 r = ab * vec2(co, sqrt(1.0 - co * co));
    return length(r - p) * sign(p.y - r.y);
}

vec3 gradientSrgbToLinear(vec3 c)
{
    return mix(c / 12.92, pow((c + 0.055) / 1.055, vec3(2.4)), step(0.04045, c));
}

float gradientStopT(Gradient grad, uint i)
{
    vec2 pair = unpackHalf2x16(grad.stopT[i >> 1u]);
    return (i % 2u == 0u) ? pair.x : pair.y;
}

vec4 gradientStopColor(Gradient grad, uint i)
{
    vec4 c = unpackUnorm4x8(grad.stopColor[i]);
    return vec4(gradientSrgbToLinear(c.rgb), c.a);
}

vec4 evalGradient(uint slot, vec2 uv)
{
    Gradient grad = gradients[slot];
    uint stopCount = (grad.header >> 8u) & 0xFFu;
    if (stopCount == 0u) {
        return vec4(1.0);
    }

    float angle = radians(grad.angle);
    vec2 dir = vec2(cos(angle), sin(angle));
    float t = clamp(dot(uv - 0.5, dir) + 0.5, 0.0, 1.0);

    vec4 prevColor = gradientStopColor(grad, 0u);
    float prevT = gradientStopT(grad, 0u);
    if (t <= prevT) {
        return prevColor;
    }

    for (uint i = 1u; i < stopCount; i++) {
        float curT = gradientStopT(grad, i);
        vec4 curColor = gradientStopColor(grad, i);
        if (t <= curT) {
            return mix(prevColor, curColor, (t - prevT) / max(curT - prevT, 1e-5));
        }
        prevColor = curColor;
        prevT = curT;
    }
    return prevColor;
}

void main()
{
    if (fragClipRect.z > 0.0 && fragClipRect.w > 0.0) {
        if (fragWorldPos.x < fragClipRect.x || fragWorldPos.y < fragClipRect.y ||
            fragWorldPos.x > fragClipRect.z || fragWorldPos.y > fragClipRect.w) {
            discard;
        }
    }

    if (fragPrimitiveType == PRIMITIVE_TEXT && fragTextBatched == 1u) {
        vec2 local = fragUV * fragSize;
        uint line = min(uint(local.y / fragLineHeight), fragLineCount - 1u);
        GlyphLine lr = glyphLines[fragLineBase + line];

        uint lo = lr.glyphStart;
        uint hi = lr.glyphStart + lr.glyphCount;
        while (lo < hi) {
            uint mid = (lo + hi) >> 1u;
            float xmin = float(glyphs[fragGlyphBase + mid].posMin & 0xFFFFu);
            if (xmin <= local.x) {
                lo = mid + 1u;
            } else {
                hi = mid;
            }
        }
        if (lo == lr.glyphStart) {
            discard;
        }

        GlyphQuad g = glyphs[fragGlyphBase + (lo - 1u)];
        vec2 pmin = vec2(float(g.posMin & 0xFFFFu), float(g.posMin >> 16u));
        vec2 pmax = vec2(float(g.posMax & 0xFFFFu), float(g.posMax >> 16u));
        if (any(lessThan(local, pmin)) || any(greaterThanEqual(local, pmax))) {
            discard;
        }

        vec2 luv = (local - pmin) / (pmax - pmin);
        vec2 amin = vec2(float(g.uvMin & 0xFFFFu), float(g.uvMin >> 16u));
        vec2 amax = vec2(float(g.uvMax & 0xFFFFu), float(g.uvMax >> 16u));
        vec2 atlasSize = vec2(textureSize(gTextures[fragTextureId], 0));
        float cov = textureLod(gTextures[fragTextureId], mix(amin, amax, luv) / atlasSize, 0.0).r;

        float alpha = cov * fragFillColor.a;
        if (alpha < 0.001) {
            discard;
        }
        outColor = vec4(fragFillColor.rgb, alpha);
        return;
    }

    if (fragPrimitiveType == PRIMITIVE_TEXT) {
        float texAlpha = texture(gTextures[fragTextureId], fragUV).r;
        float alpha = texAlpha * fragFillColor.a;
        if (alpha < 0.001) discard;
        outColor = vec4(fragFillColor.rgb, alpha);
        return;
    }

    if (fragPrimitiveType == PRIMITIVE_SVG) {
        vec4 texColor = texture(gTextures[fragTextureId], fragUV);
        vec4 tinted = vec4(texColor.rgb * fragFillColor.rgb, texColor.a * fragFillColor.a);
        if (tinted.a < 0.001) discard;
        outColor = tinted;
        return;
    }

    vec2 p = (fragUV - 0.5) * fragSize;
    vec2 halfSize = fragSize * 0.5;

    float dist;

    if (fragPrimitiveType == PRIMITIVE_RECT) {
        // sdfRect's vec4 is ordered (BR, TR, BL, TL) by its p.x/p.y sign selection; remap from
        // the (TL, TR, BR, BL) corner order carried in fragCornerRadii.
        dist = sdfRect(p, halfSize, vec4(fragCornerRadii.z, fragCornerRadii.y, fragCornerRadii.w, fragCornerRadii.x));
    } else if (fragPrimitiveType == PRIMITIVE_CIRCLE) {
        dist = sdfCircle(p, min(halfSize.x, halfSize.y));
    } else if (fragPrimitiveType == PRIMITIVE_TRIANGLE) {
        dist = sdfTriangle(p, halfSize);
    } else if (fragPrimitiveType == PRIMITIVE_LINE) {
        dist = sdfLine(p, halfSize, fragCornerRadius);
    } else if (fragPrimitiveType == PRIMITIVE_CANVAS_LINE) {
        vec2 a = unpackHalf2x16(fragShapeData.x);
        vec2 b = unpackHalf2x16(fragShapeData.y);
        dist = sdSegment(p, a, b) - fragCornerRadius;
    } else if (fragPrimitiveType == PRIMITIVE_CANVAS_BEZIER) {
        vec2 A = unpackHalf2x16(fragShapeData.x);
        vec2 B = unpackHalf2x16(fragShapeData.y);
        vec2 C = unpackHalf2x16(fragShapeData.z);
        dist = sdBezier(p, A, B, C) - fragCornerRadius;
    } else if (fragPrimitiveType == PRIMITIVE_CANVAS_TRI) {
        vec2 t0 = unpackHalf2x16(fragShapeData.x);
        vec2 t1 = unpackHalf2x16(fragShapeData.y);
        vec2 t2 = unpackHalf2x16(fragShapeData.z);
        dist = sdCanvasTriangle(p, t0, t1, t2);
    } else if (fragPrimitiveType == PRIMITIVE_CANVAS_QUAD) {
        vec2 q0 = unpackHalf2x16(fragShapeData.x);
        vec2 q1 = unpackHalf2x16(fragShapeData.y);
        vec2 q2 = unpackHalf2x16(fragShapeData.z);
        vec2 q3 = unpackHalf2x16(fragShapeData.w);
        dist = sdCanvasQuad(p, q0, q1, q2, q3);
    } else if (fragPrimitiveType == PRIMITIVE_CANVAS_CIRCLE) {
        dist = sdfCircle(p, fragCornerRadius);
    } else if (fragPrimitiveType == PRIMITIVE_CANVAS_ELLIPSE) {
        vec2 ab = unpackHalf2x16(fragShapeData.x);
        dist = sdEllipse(p, ab);
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

    vec4 fillColor = fragFillColor;
    if (fragGradientSlot != INVALID_GRADIENT) {
        vec4 g = evalGradient(fragGradientSlot, fragUV);
        fillColor = vec4(g.rgb * fragFillColor.rgb, g.a * fragFillColor.a);
    }

    vec4 color;
    if (fragBorderThickness > 0.0) {
        float fillMask = 1.0 - smoothstep(-aa, aa, dist - innerThreshold);
        color = mix(fragBorderColor, fillColor, fillMask);
    } else {
        color = fillColor;
    }

    if (fragTextureId != INVALID_TEXTURE) {
        vec4 texColor = texture(gTextures[fragTextureId], fragUV);
        color.rgb = mix(color.rgb, texColor.rgb, texColor.a);
    }

    float finalAlpha = shapeAlpha * color.a;
    if (finalAlpha < 0.001) discard;

    outColor = vec4(color.rgb, finalAlpha);
}
