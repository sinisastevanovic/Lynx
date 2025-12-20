#include "AssetTypes.h"

#include <vector>
#include <string>
#include <unordered_set>
#include <unordered_map>

namespace Lynx::AssetUtils
{
    using namespace std::string_literals;
    
    struct AssetFilter
    {
        const char* Description;
        std::vector<const char*> Extensions;
    };

    static const std::unordered_map<AssetType, AssetFilter> s_AssetFilters = {
        { AssetType::Scene, { "Lynx Scene", { "*.lxscene" } } },
        { AssetType::Texture, {"Texture File",  { "*.png", "*.jpg", "*.jpeg", "*.tga" } } },
        { AssetType::Material, { "Material File", { "*.lxmat" } } },
        { AssetType::Shader, { "Shader File", { "*.glsl" } } },
        { AssetType::Mesh, { "3D Model",      { "*.fbx", "*.obj", "*.gltf" } } }
    };

    
    const char* GetFilterForSupportedAssets()
    {
        static std::string filterString;

        if (filterString.empty())
        {
            std::stringstream ss;

            ss << "All Supported Assets (";
            std::string allExtensions;
            for (const auto& [type, filter] : s_AssetFilters)
            {
                for (const auto& extension : filter.Extensions)
                {
                    allExtensions += std::string(extension) + ";";
                }
            }
            allExtensions.pop_back();
            ss << allExtensions << ")\0"s << allExtensions << "\0"s;

            for (const auto& [type, filter] : s_AssetFilters)
            {
                ss << filter.Description << " (";
                std::string extensionsPart;
                for (const auto& extension : filter.Extensions)
                {
                    extensionsPart += std::string(extension) + ";";
                }
                extensionsPart.pop_back();

                ss << extensionsPart << ")\0"s << extensionsPart << "\0"s;
            }

            ss << '\0';

            filterString = ss.str();
        }

        return filterString.c_str();
    }

    const char* GetFilterForAssetType(AssetType type)
    {
        if (!s_AssetFilters.contains(type))
            return nullptr;

        static std::string filterString;
        const auto& filter = s_AssetFilters.at(type);
        std::stringstream ss;
        ss << filter.Description << " (";
        std::string extensionsPart;
        for (const auto& extension : filter.Extensions)
        {
            extensionsPart += std::string(extension) + ";";
        }
        extensionsPart.pop_back();
        ss << extensionsPart << ")\0"s << extensionsPart << "\0"s;
        filterString = ss.str();
        ss << '\0';
        
        return filterString.c_str();
    }

    AssetType GetAssetTypeFromExtension(const std::filesystem::path& path)
    {
        std::string extension = path.extension().string();
        if (extension.empty())
            return AssetType::None;
        
        for (const auto& [type, filter] : s_AssetFilters)
        {
            for (const auto& ext : filter.Extensions)
            {
                std::string extensionsPart = std::string(ext).substr(1);
                if (extension == extensionsPart)
                    return type;
            }
        }

        return AssetType::None;
    }

    bool IsAssetExtensionSupported(const std::filesystem::path& path)
    {
        static std::unordered_set<std::string> s_SupportedExtensions;
        
        if (s_SupportedExtensions.empty())
        {
            for (const auto& [type, filter] : s_AssetFilters)
            {
                for (const auto& ext : filter.Extensions)
                {
                    s_SupportedExtensions.insert(std::string(ext).substr(1));
                }
            }
        }

        std::string extension = path.extension().string();
        if (extension.empty())
            return false;

        return s_SupportedExtensions.contains(extension);
    }
}
