#include "lxpch.h"
#include "Random.h"

namespace Lynx
{
    std::mt19937 Random::RandomEngine_;
    std::uniform_int_distribution<std::mt19937::result_type> Random::Distribution_;
}
