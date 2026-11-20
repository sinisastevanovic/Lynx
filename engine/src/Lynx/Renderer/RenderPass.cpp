#include "RenderPass.h"

#include "Lynx/Engine.h"
#include "Lynx/Asset/Shader.h"

namespace Lynx
{
    bool PipelineState::Update(std::function<void(std::shared_ptr<Shader>)> builder)
    {
        // Looked up once. A reload changes the asset in place and raises its version,
        // so comparing versions is all that is needed in the frames after.
        if (!m_Shader)
            m_Shader = Engine::Get().GetAssetManager().GetEngineAsset<Shader>(m_ShaderPath);

        if (m_Shader && m_Shader->GetVersion() != m_Version)
        {
            builder(m_Shader);
            m_Version = m_Shader->GetVersion();
            return true;
        }
        return false;
    }
}
