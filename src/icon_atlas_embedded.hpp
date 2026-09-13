#pragma once
#include <cstddef>

struct EmbeddedIconEntry { const char* key; int x; int y; int w; int h; };
extern const char AL_ICON_ATLAS_PNG_BASE64[];
extern const EmbeddedIconEntry AL_ICON_ENTRIES[];
extern const std::size_t AL_ICON_ENTRY_COUNT;
extern const int AL_ICON_ATLAS_WIDTH;
extern const int AL_ICON_ATLAS_HEIGHT;
