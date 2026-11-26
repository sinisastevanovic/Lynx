#include "AssetWindows.h"

#include "Lynx/Engine.h"

#include <algorithm>

namespace Lynx
{
    void AssetWindows::Open(AssetHandle asset)
    {
        if (!asset.IsValid())
            return;

        for (const std::unique_ptr<AssetWindow>& window : m_Windows)
        {
            if (window->GetAsset() == asset)
            {
                window->ComeToFront();
                return;
            }
        }

        const AssetMetadata metadata = Engine::Get().GetAssetRegistry().Get(asset);
        const auto kind = m_Kinds.find(metadata.Type);
        if (!metadata || kind == m_Kinds.end())
            return;

        m_Windows.push_back(kind->second(asset));
    }

    void AssetWindows::OnImGuiRender()
    {
        // By index: a window may open another while it is drawn, which moves the list.
        for (size_t index = 0; index < m_Windows.size(); ++index)
            m_Windows[index]->OnImGuiRender();

        std::erase_if(m_Windows, [](const std::unique_ptr<AssetWindow>& window) { return !window->IsOpen(); });
    }
}
