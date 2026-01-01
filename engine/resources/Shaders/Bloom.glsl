#type vertex
#version 450
layout (location = 0) out vec2 v_TexCoord;

void main() {
    v_TexCoord = vec2((gl_VertexIndex << 1) & 2, gl_VertexIndex & 2);
    gl_Position = vec4(v_TexCoord * 2.0f - 1.0f, 0.0f, 1.0f);
}

#type pixel
#version 450
layout (location = 0) in vec2 v_TexCoord;
layout (location = 0) out vec4 outColor;

layout (set = 0, binding = 0) uniform texture2D u_InputTexture;
layout (set = 0, binding = 1) uniform sampler u_Sampler;

layout (push_constant) uniform BloomPush {
    vec4 u_Params; // x: Threshold, y: Knee, z: Radius, w: Lod
    int u_Mode;    // 0: Prefilter, 1: Downsample, 2: Upsample
} push;

// Standard "Karis Average" Downsample (13-tap)
vec3 Downsample(vec2 uv, vec2 texSize) {
    vec2 texel = 1.0 / texSize;
    float x = texel.x;
    float y = texel.y;

    // Center
    vec3 a = texture(sampler2D(u_InputTexture, u_Sampler), uv + vec2(-2*x, 2*y)).rgb;
    vec3 b = texture(sampler2D(u_InputTexture, u_Sampler), uv + vec2(0, 2*y)).rgb;
    vec3 c = texture(sampler2D(u_InputTexture, u_Sampler), uv + vec2(2*x, 2*y)).rgb;
    vec3 d = texture(sampler2D(u_InputTexture, u_Sampler), uv + vec2(-2*x, 0)).rgb;
    vec3 e = texture(sampler2D(u_InputTexture, u_Sampler), uv + vec2(0, 0)).rgb;
    vec3 f = texture(sampler2D(u_InputTexture, u_Sampler), uv + vec2(2*x, 0)).rgb;
    vec3 g = texture(sampler2D(u_InputTexture, u_Sampler), uv + vec2(-2*x, -2*y)).rgb;
    vec3 h = texture(sampler2D(u_InputTexture, u_Sampler), uv + vec2(0, -2*y)).rgb;
    vec3 i = texture(sampler2D(u_InputTexture, u_Sampler), uv + vec2(2*x, -2*y)).rgb;
    vec3 j = texture(sampler2D(u_InputTexture, u_Sampler), uv + vec2(-x, y)).rgb;
    vec3 k = texture(sampler2D(u_InputTexture, u_Sampler), uv + vec2(x, y)).rgb;
    vec3 l = texture(sampler2D(u_InputTexture, u_Sampler), uv + vec2(-x, -y)).rgb;
    vec3 m = texture(sampler2D(u_InputTexture, u_Sampler), uv + vec2(x, -y)).rgb;

    vec3 result = e * 0.125;
    result += (a+c+g+i) * 0.03125;
    result += (b+d+f+h) * 0.0625;
    result += (j+k+l+m) * 0.125;
    return result;
}

// 3x3 Tent Upsample
vec3 Upsample(vec2 uv, vec2 texSize) {
    vec2 texel = 1.0 / texSize;
    float x = texel.x;
    float y = texel.y;
    // Radius controls spread
    float r = push.u_Params.z;

    vec3 a = texture(sampler2D(u_InputTexture, u_Sampler), uv + vec2(-x, y) * r).rgb;
    vec3 b = texture(sampler2D(u_InputTexture, u_Sampler), uv + vec2(0, y) * r).rgb;
    vec3 c = texture(sampler2D(u_InputTexture, u_Sampler), uv + vec2(x, y) * r).rgb;
    vec3 d = texture(sampler2D(u_InputTexture, u_Sampler), uv + vec2(-x, 0) * r).rgb;
    vec3 e = texture(sampler2D(u_InputTexture, u_Sampler), uv + vec2(0, 0) * r).rgb;
    vec3 f = texture(sampler2D(u_InputTexture, u_Sampler), uv + vec2(x, 0) * r).rgb;
    vec3 g = texture(sampler2D(u_InputTexture, u_Sampler), uv + vec2(-x, -y) * r).rgb;
    vec3 h = texture(sampler2D(u_InputTexture, u_Sampler), uv + vec2(0, -y) * r).rgb;
    vec3 i = texture(sampler2D(u_InputTexture, u_Sampler), uv + vec2(x, -y) * r).rgb;

    return (e * 4.0 + (b+d+f+h) * 2.0 + (a+c+g+i)) * (1.0 / 16.0);
}

vec3 Prefilter(vec3 c) {
    float brightness = max(c.r, max(c.g, c.b));
    float knee = push.u_Params.y;
    float threshold = push.u_Params.x;

    float soft = brightness - threshold + knee;
    soft = clamp(soft, 0.0, 2.0 * knee);
    soft = soft * soft / (4.0 * knee + 0.00001);

    float contribution = max(soft, brightness - threshold);
    contribution /= max(brightness, 0.00001);

    return c * contribution;
}

void main() {
    vec2 size = textureSize(sampler2D(u_InputTexture, u_Sampler), 0);

    if (push.u_Mode == 0) { // Prefilter
        vec3 color = texture(sampler2D(u_InputTexture, u_Sampler), v_TexCoord).rgb;
        outColor = vec4(Prefilter(color), 1.0);
    }
    else if (push.u_Mode == 1) { // Downsample
        outColor = vec4(Downsample(v_TexCoord, size), 1.0);
    }
    else if (push.u_Mode == 2) { // Upsample
        // Note: We expect standard Additive blending to be enabled in the pipeline!
        // So we just output the upsampled color, and the hardware adds it to the destination.
        outColor = vec4(Upsample(v_TexCoord, size), 1.0);
    }
}