#ifndef SPATIAL_PLUGIN_VERSION_H
#define SPATIAL_PLUGIN_VERSION_H

// ---- Version: change these three values ----
#define STX_VERSION_MAJOR  0
#define STX_VERSION_MINOR  0   // 0-15
#define STX_VERSION_PATCH  1   // 0-15

// ---- Auto-generated from above ----
#define STX_STR2(x) #x
#define STX_STR(x) STX_STR2(x)
#define STX_PLUGIN_VERSION \
    STX_STR(STX_VERSION_MAJOR) "." STX_STR(STX_VERSION_MINOR) "." STX_STR(STX_VERSION_PATCH)
#define STX_PLUGIN_VERSION_HEX \
    ((STX_VERSION_MAJOR << 8) | (STX_VERSION_MINOR << 4) | STX_VERSION_PATCH)

// ---- Metadata ----
#define STX_PLUGIN_REQUIRES    "8.0"  // minimum MySQL version
#define STX_PLUGIN_AUTHOR      "sakaik"
#define STX_PLUGIN_DESCRIPTION "Spatial Functions Extensions(STX_*) powered by Boost.Geometry"

#endif
