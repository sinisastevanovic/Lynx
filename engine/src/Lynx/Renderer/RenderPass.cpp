#include "RenderPass.h"

#include "Lynx/Engine.h"
#include "Lynx/Asset/Shader.h"

namespace Lynx
{
    bool PipelineState::Update(std::function<void(std::shared_ptr<Shader>)> builder)
    {
        auto shader = Engine::Get().GetAssetManager().GetAsset<Shader>(m_Path);
        if (shader && shader->GetVersion() != m_Version)
        {
            builder(shader);
            m_Version = shader->GetVersion();
            return true;
        }
        return false;
    }
}
