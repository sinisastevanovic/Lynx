#include "Prefab.h"

namespace Lynx
{
    Prefab::Prefab(const std::string& filePath)
        : Asset(filePath)
    {
    }

    bool Prefab::LoadSourceData()
    {
        m_Data.clear();
        
        if (m_FilePath.empty())
            return true;
        
        std::ifstream stream(m_FilePath);
        if (!stream.is_open())
        {
            LX_CORE_ERROR("Failed to open font asset file: {0}", m_FilePath);
            return false;
        }

        try
        {
            stream >> m_Data;
        }
        catch (const std::exception& e)
        {
            LX_CORE_ERROR("Failed to parse font file: {0}", m_FilePath);
            return false;
        }
        
        return true;
    }

    bool Prefab::Reload()
    {
        return LoadSourceData();
    }
}
