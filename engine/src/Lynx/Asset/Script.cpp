#include "Script.h"

#include "Lynx/Engine.h"

namespace Lynx
{
    bool Script::Reload()
    {
        Engine::Get().GetScriptEngine()->ReloadScript(m_Handle);
        return true;
    }
}
