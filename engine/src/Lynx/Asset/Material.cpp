#include "Material.h"

namespace Lynx
{
    Material::Material(const std::string& filepath)
        : m_Filepath(filepath)
    {
        Material::Reload();
    }

    Material::Material()
    {
    }

    bool Material::Reload()
    {
        return true;
    }
}
