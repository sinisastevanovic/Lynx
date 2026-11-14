# Material System V2: Shader-Driven Architecture

## Overview
Currently, LynxEngine uses a "Hardcoded Material" approach. The `Material` class has specific C++ fields (`Albedo`, `Roughness`, `Emission`) that map to a specific `Standard.glsl` shader.

**Goal:** Move to a data-driven system where the **Shader** defines the available properties, and the **Material** is a generic container for that data. This allows creating custom shaders (Water, Hologram, Foliage) with custom properties (`WaveSpeed`, `GlitchIntensity`) without modifying the C++ engine code.

## Core Components

### 1. Shader Reflection (SPIRV-Cross)
The engine needs to understand the memory layout of the shader's uniform blocks.
*   **Library:** Integrate `SPIRV-Cross` (via vcpkg).
*   **Process:**
    1.  Compile GLSL to SPIR-V (already done by shaderc).
    2.  Pass SPIR-V binary to SPIRV-Cross.
    3.  Query active resources:
        *   **Uniform Buffers:** Get name, set, binding, and size.
        *   **Members:** For each uniform block, get list of members (Name, Offset, Size, Type).
        *   **Textures:** Get binding slots and names.

### 2. The `Shader` Class Upgrade
Update the `Shader` class to store this reflection data.

```cpp
enum class ShaderDataType { None, Float, Vec2, Vec3, Vec4, Mat3, Mat4, Int, Bool };

struct ShaderResourceDeclaration
{
    std::string Name;
    uint32_t Register;
    uint32_t Set;
    uint32_t Count;
};

struct ShaderBufferUniform
{
    std::string Name;
    ShaderDataType Type;
    uint32_t Size;
    uint32_t Offset;
};

struct ShaderBuffer
{
    std::string Name;
    uint32_t Size;
    std::unordered_map<std::string, ShaderBufferUniform> Uniforms;
};
```

### 3. The `Material` Class Refactor
The Material class stops being a POD struct and becomes a buffer manager.

```cpp
class Material
{
public:
    Material(std::shared_ptr<Shader> shader);
    
    // Setters using string names (hashed for performance)
    void Set(const std::string& name, float value);
    void Set(const std::string& name, const glm::vec4& value);
    void SetTexture(const std::string& name, std::shared_ptr<Texture> texture);

    // Binds the buffer to the GPU
    void Bind();

private:
    std::shared_ptr<Shader> m_Shader;
    std::vector<uint8_t> m_UniformStorage; // Raw byte buffer matching shader layout
    std::unordered_map<std::string, std::shared_ptr<Texture>> m_Textures;
};
```

### 4. Material Serialization
The `.lxmat` format needs to be flexible.

```json
{
  "Shader": "assets/Shaders/Water.glsl",
  "Properties": {
    "Albedo": [0.0, 0.5, 1.0, 1.0],
    "WaveSpeed": 2.5,
    "FoamThreshold": 0.8
  },
  "Textures": {
    "NormalMap": "UUID-..."
  }
}
```

### 5. Editor UI Generation
The Inspector panel reads the `Shader` reflection data to generate UI widgets dynamically.

```cpp
// Pseudocode
for (auto& [name, uniform] : shader->GetUniforms())
{
    if (uniform.Type == ShaderDataType::Float)
    {
        float value = material->GetFloat(name);
        if (ImGui::DragFloat(name.c_str(), &value))
            material->Set(name, value);
    }
    else if (uniform.Type == ShaderDataType::Vec4)
    {
        // Check naming convention for Color vs Vector
        if (name.contains("Color")) 
            ImGui::ColorEdit4(...)
        else 
            ImGui::DragFloat4(...)
    }
}
```

## Implementation Roadmap (Phase 6)

1.  [ ] **Dependency:** Add `spirv-cross` to `vcpkg.json`.
2.  [ ] **Reflection:** Implement `Shader::Reflect()` to parse SPIR-V.
3.  [ ] **MaterialBuffer:** Create a class to manage the `std::vector<uint8_t>` buffer and handle writing types to specific offsets.
4.  [ ] **Material Refactor:** Update `Material` to use `MaterialBuffer`.
    *   *Note:* This will break existing serialized materials! A migration script might be needed.
5.  [ ] **Renderer Update:** Update `ForwardPass` and `ParticlePass` to bind these generic material buffers instead of using specific PushConstants or hardcoded BindingSets.
