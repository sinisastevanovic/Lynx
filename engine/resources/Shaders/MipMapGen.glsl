#type compute
#version 450

layout(local_size_x = 16, local_size_y = 16, local_size_z = 1) in;

struct PushConstants {
    uint NumLODs; // Number of levels to generate in this pass (1 to 4)
    float TexSizeX; // Size of the source mip level
    float TexSizeY;
};

layout(push_constant) uniform Constants {
    PushConstants u_Push;
};

// Input: The mip level we are reading from
layout(binding = 0) uniform texture2D t_InputTex;
layout(binding = 1) uniform sampler s_InputSampler;

// Output: Up to 4 mip levels we are writing to
layout(binding = 2, rgba8) uniform writeonly image2D u_Output[4];

// Shared memory for reduction
shared vec4 s_Reduction[16][16];

vec4 reduce(vec4 a, vec4 b, vec4 c, vec4 d) {
    return (a + b + c + d) * 0.25;
}

void main() {
    uvec2 groupIdx = gl_WorkGroupID.xy;
    uvec2 globalIdx = gl_GlobalInvocationID.xy;
    uvec2 threadIdx = gl_LocalInvocationID.xy;

    vec2 texSize = vec2(u_Push.TexSizeX, u_Push.TexSizeY);

    // 1. Sample from Input (Source Mip)
    // We use UVs to sample the center of the pixel in the source mip
    vec2 uv = (vec2(globalIdx) + 0.5) / texSize;

    vec4 value;
    if (globalIdx.x < uint(texSize.x) && globalIdx.y < uint(texSize.y)) {
        value = textureLod(sampler2D(t_InputTex, s_InputSampler), uv, 0.0);
    } else {
        value = vec4(0.0, 0.0, 0.0, 0.0); // Edge padding
    }

    // 2. Generate up to 4 Mip Levels
    for (uint i = 0; i < u_Push.NumLODs; ++i) {
        // Determine group size for this level (starts at 16, then 8, 4, 2)
        uint outGroupSize = 16 >> (i + 1);
        uint inGroupSize = outGroupSize << 1;

        // Write to shared memory for reduction
        if (threadIdx.x < inGroupSize && threadIdx.y < inGroupSize) {
            s_Reduction[threadIdx.y][threadIdx.x] = value;
        }

        barrier();

        // Threads active for the NEXT reduction
        if (threadIdx.x < outGroupSize && threadIdx.y < outGroupSize) {
            vec4 a = s_Reduction[threadIdx.y * 2 + 0][threadIdx.x * 2 + 0];
            vec4 b = s_Reduction[threadIdx.y * 2 + 0][threadIdx.x * 2 + 1];
            vec4 c = s_Reduction[threadIdx.y * 2 + 1][threadIdx.x * 2 + 0];
            vec4 d = s_Reduction[threadIdx.y * 2 + 1][threadIdx.x * 2 + 1];

            value = reduce(a, b, c, d);

            // --- DEBUG: RAINBOW MIPS START ---
            // Tint based on which level we are writing in this pass (0 to 3)
            // This effectively colors Mip 1 Red, Mip 2 Green, etc.
            /* if (i == 0) value = mix(value, vec4(1.0, 0.0, 0.0, 1.0), 0.8); // Red
            else if (i == 1) value = mix(value, vec4(0.0, 1.0, 0.0, 1.0), 0.8); // Green
            else if (i == 2) value = mix(value, vec4(0.0, 0.0, 1.0, 1.0), 0.8); // Blue
            else if (i == 3) value = mix(value, vec4(1.0, 1.0, 0.0, 1.0), 0.8); // Yellow */
            // --- DEBUG: RAINBOW MIPS END ---

            // Write to the Output UAV
            // Output[i] corresponds to Mip (Base + i + 1)
            ivec2 writePos = ivec2(groupIdx * outGroupSize + threadIdx);
            imageStore(u_Output[i], writePos, value);
        }

        // Prepare for next iteration (value now holds the reduced color)
        barrier();
    }
}