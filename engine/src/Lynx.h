#pragma once

// Core
#include "Lynx/Core.h"
#include "Lynx/UUID.h"
#include "Lynx/Log.h"

// Math
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

// Common Systems
#include "Lynx/Engine.h"
#include "Lynx/Input.h"
#include "Lynx/KeyCodes.h"

// Scene & ECS
#include "Lynx/Scene/Scene.h"
#include "Lynx/Scene/Entity.h"
#include "Lynx/Scene/Components/Components.h"
#include "Lynx/Asset/AssetRef.h"

// UI
#include "Lynx/UI/Core/UIElement.h"
#include "Lynx/UI/Core/UIElementRef.h"

// Utils
#include "Lynx/Utils/MathHelpers.h"
#include "Lynx/Utils/JsonHelpers.h"

// Standard Library
#include <memory>
#include <string>
#include <vector>
#include <functional>
#include <algorithm>

using namespace Lynx;