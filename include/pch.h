#pragma once

// STL
#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <unordered_map>
#include <cstdint>
#include <algorithm>

// GLM
#include "glm.hpp"
#include "fwd.hpp"
#include "ext/matrix_transform.hpp"
#include "ext/matrix_clip_space.hpp"
#include "gtc/quaternion.hpp"

// Flecs - 80k lines, compile once
#include "flecs.h"
#include "simdjson.h"
