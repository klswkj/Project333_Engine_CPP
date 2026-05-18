#pragma once

//=============================================================================
// Included by:
//   - Public engine-facing headers that require shared core types/utilities.
//
// Notes:
//   - Keep this header lightweight.
//   - Do not include heavy STL headers unless broadly needed.
//   - Avoid Windows.h here.
//	 - Avoid platform-specific includes.
//   - Avoid renderer-speific includes.
// 
// Typical includers : 
//   - // Engine/Public/Application/Application.h 
//   - or Engine/Public/*.h
//=============================================================================


#include <cstdint>
#include <memory>
#include <string>
// #include <string_view>
// #include <vector>

// Must be included before any other headers that use the logging system
#include "Logging/Logger.h"
#include "Diagnostics/DebugCheck.h"