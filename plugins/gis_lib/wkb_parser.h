#ifndef GIS_LIB_WKB_PARSER_H
#define GIS_LIB_WKB_PARSER_H

#include <cstdint>
#include <optional>
#include <string>

#include "geometry_types.h"

namespace gis_lib {

// Parse MySQL internal geometry format (4-byte SRID + WKB).
// Returns std::nullopt on parse error.
std::optional<ParseResult> parse_geometry(const char *data, size_t length);

}  // namespace gis_lib

#endif  // GIS_LIB_WKB_PARSER_H
