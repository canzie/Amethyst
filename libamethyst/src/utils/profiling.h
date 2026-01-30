#pragma once

#ifdef TRACY_ENABLE
#include <tracy/Tracy.hpp>
#define AM_PROFILE_FUNCTION()  ZoneScoped
#define AM_PROFILE_SCOPE(name) ZoneScopedN(name)
#define AM_PROFILE_FRAME()     FrameMark
#else
#define AM_PROFILE_FUNCTION()
#define AM_PROFILE_SCOPE(name)
#define AM_PROFILE_FRAME()
#endif
