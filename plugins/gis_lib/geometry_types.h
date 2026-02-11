#ifndef GIS_LIB_GEOMETRY_TYPES_H
#define GIS_LIB_GEOMETRY_TYPES_H

#include <unordered_set>
#include <variant>
#include <vector>

#include "geographic_srids.h"

#include <boost/geometry.hpp>
#include <boost/geometry/geometries/geometries.hpp>
#include <boost/geometry/geometries/point_xy.hpp>

namespace gis_lib {

namespace bg = boost::geometry;

// --- Cartesian 2D types ---
using Point = bg::model::d2::point_xy<double>;
using Linestring = bg::model::linestring<Point>;
using Ring = bg::model::ring<Point>;
using Polygon = bg::model::polygon<Point>;
using MultiPoint = bg::model::multi_point<Point>;
using MultiLinestring = bg::model::multi_linestring<Linestring>;
using MultiPolygon = bg::model::multi_polygon<Polygon>;

// --- Geographic 2D types (WGS84, degrees) ---
using GeoPoint = bg::model::point<double, 2, bg::cs::geographic<bg::degree>>;
using GeoLinestring = bg::model::linestring<GeoPoint>;
using GeoRing = bg::model::ring<GeoPoint>;
using GeoPolygon = bg::model::polygon<GeoPoint>;
using GeoMultiPoint = bg::model::multi_point<GeoPoint>;
using GeoMultiLinestring = bg::model::multi_linestring<GeoLinestring>;
using GeoMultiPolygon = bg::model::multi_polygon<GeoPolygon>;

// WKB geometry type codes
enum class GeometryType : uint32_t {
  Point = 1,
  LineString = 2,
  Polygon = 3,
  MultiPoint = 4,
  MultiLineString = 5,
  MultiPolygon = 6,
  GeometryCollection = 7,
};

// Variants per coordinate system
using CartesianVariant = std::variant<Point, Linestring, Polygon, MultiPoint,
                                      MultiLinestring, MultiPolygon>;
using GeographicVariant =
    std::variant<GeoPoint, GeoLinestring, GeoPolygon, GeoMultiPoint,
                 GeoMultiLinestring, GeoMultiPolygon>;

struct ParseResult {
  uint32_t srid;
  GeometryType type;
  std::variant<CartesianVariant, GeographicVariant> geometry;
};

// Coordinate system traits for templated parsing
struct CartesianTraits {
  using point = Point;
  using linestring = Linestring;
  using ring = Ring;
  using polygon = Polygon;
  using multi_point = MultiPoint;
  using multi_linestring = MultiLinestring;
  using multi_polygon = MultiPolygon;
  using variant = CartesianVariant;
  static constexpr bool swap_xy = false;
};

struct GeographicTraits {
  using point = GeoPoint;
  using linestring = GeoLinestring;
  using ring = GeoRing;
  using polygon = GeoPolygon;
  using multi_point = GeoMultiPoint;
  using multi_linestring = GeoMultiLinestring;
  using multi_polygon = GeoMultiPolygon;
  using variant = GeographicVariant;
  // SRID 4326: WKB stores (lat, lon), boost expects (lon, lat)
  static constexpr bool swap_xy = true;
};

// Returns true if the SRID represents a geographic coordinate system.
// Uses auto-generated list from information_schema.ST_SPATIAL_REFERENCE_SYSTEMS.
inline bool is_geographic_srid(uint32_t srid) {
  return GEOGRAPHIC_SRIDS.count(srid) > 0;
}

}  // namespace gis_lib

#endif  // GIS_LIB_GEOMETRY_TYPES_H
