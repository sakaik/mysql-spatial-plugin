#ifndef GIS_LIB_WKB_WRITER_H
#define GIS_LIB_WKB_WRITER_H

#include <cstring>
#include <string>
#include <variant>

#include "geometry_types.h"

namespace gis_lib {

namespace detail {

inline void append_uint32(std::string &buf, uint32_t v) {
  buf.append(reinterpret_cast<const char *>(&v), 4);
}

inline void append_double(std::string &buf, double v) {
  buf.append(reinterpret_cast<const char *>(&v), 8);
}

// Write WKB header: byte_order(1) + type(4).
inline void append_wkb_header(std::string &buf, GeometryType type) {
  buf.push_back(0x01);  // little-endian
  append_uint32(buf, static_cast<uint32_t>(type));
}

// Generic point coordinate writing (works for any boost::geometry point type).
template <typename PointT>
inline void append_point_coords(std::string &buf, const PointT &pt) {
  append_double(buf, bg::get<0>(pt));
  append_double(buf, bg::get<1>(pt));
}

// Write ring data: num_points + coordinates.
template <typename RingT>
inline void append_ring(std::string &buf, const RingT &ring) {
  append_uint32(buf, static_cast<uint32_t>(ring.size()));
  for (const auto &pt : ring) append_point_coords(buf, pt);
}

}  // namespace detail

// Write a Point as MySQL internal format (SRID + WKB).
// wkb_x/wkb_y are in MySQL WKB coordinate order.
inline std::string write_point_wkb(uint32_t srid, double wkb_x, double wkb_y) {
  std::string buf;
  buf.reserve(25);
  detail::append_uint32(buf, srid);
  detail::append_wkb_header(buf, GeometryType::Point);
  detail::append_double(buf, wkb_x);
  detail::append_double(buf, wkb_y);
  return buf;
}

// Write a Cartesian Point.
inline std::string write_point(uint32_t srid, const Point &pt) {
  return write_point_wkb(srid, pt.x(), pt.y());
}

// Write a Geographic Point (boost and MySQL WKB both use lon,lat order).
inline std::string write_point(uint32_t srid, const GeoPoint &pt) {
  return write_point_wkb(srid, bg::get<0>(pt), bg::get<1>(pt));
}

// Write a Cartesian LineString.
inline std::string write_linestring(uint32_t srid, const Linestring &ls) {
  std::string buf;
  buf.reserve(4 + 5 + 4 + 16 * ls.size());
  detail::append_uint32(buf, srid);
  detail::append_wkb_header(buf, GeometryType::LineString);
  detail::append_uint32(buf, static_cast<uint32_t>(ls.size()));
  for (const auto &pt : ls) {
    detail::append_double(buf, pt.x());
    detail::append_double(buf, pt.y());
  }
  return buf;
}

// Write a Geographic LineString (boost and MySQL WKB both use lon,lat order).
inline std::string write_linestring(uint32_t srid, const GeoLinestring &ls) {
  std::string buf;
  buf.reserve(4 + 5 + 4 + 16 * ls.size());
  detail::append_uint32(buf, srid);
  detail::append_wkb_header(buf, GeometryType::LineString);
  detail::append_uint32(buf, static_cast<uint32_t>(ls.size()));
  for (const auto &pt : ls) {
    detail::append_double(buf, bg::get<0>(pt));  // lon
    detail::append_double(buf, bg::get<1>(pt));  // lat
  }
  return buf;
}

// Write a Polygon (works for both Cartesian and Geographic).
template <typename PolygonT>
inline std::string write_polygon(uint32_t srid, const PolygonT &poly) {
  std::string buf;
  detail::append_uint32(buf, srid);
  detail::append_wkb_header(buf, GeometryType::Polygon);
  uint32_t num_rings = 1 + static_cast<uint32_t>(poly.inners().size());
  detail::append_uint32(buf, num_rings);
  detail::append_ring(buf, poly.outer());
  for (const auto &inner : poly.inners()) detail::append_ring(buf, inner);
  return buf;
}

// Write a MultiPoint (works for both Cartesian and Geographic).
template <typename MultiPointT>
inline std::string write_multi_point(uint32_t srid, const MultiPointT &mp) {
  std::string buf;
  detail::append_uint32(buf, srid);
  detail::append_wkb_header(buf, GeometryType::MultiPoint);
  detail::append_uint32(buf, static_cast<uint32_t>(mp.size()));
  for (const auto &pt : mp) {
    detail::append_wkb_header(buf, GeometryType::Point);
    detail::append_point_coords(buf, pt);
  }
  return buf;
}

// Write a MultiLineString (works for both Cartesian and Geographic).
template <typename MultiLinestringT>
inline std::string write_multi_linestring(uint32_t srid,
                                          const MultiLinestringT &mls) {
  std::string buf;
  detail::append_uint32(buf, srid);
  detail::append_wkb_header(buf, GeometryType::MultiLineString);
  detail::append_uint32(buf, static_cast<uint32_t>(mls.size()));
  for (const auto &ls : mls) {
    detail::append_wkb_header(buf, GeometryType::LineString);
    detail::append_uint32(buf, static_cast<uint32_t>(ls.size()));
    for (const auto &pt : ls) detail::append_point_coords(buf, pt);
  }
  return buf;
}

// Write a MultiPolygon (works for both Cartesian and Geographic).
template <typename MultiPolygonT>
inline std::string write_multi_polygon(uint32_t srid,
                                       const MultiPolygonT &mpoly) {
  std::string buf;
  detail::append_uint32(buf, srid);
  detail::append_wkb_header(buf, GeometryType::MultiPolygon);
  detail::append_uint32(buf, static_cast<uint32_t>(mpoly.size()));
  for (const auto &poly : mpoly) {
    detail::append_wkb_header(buf, GeometryType::Polygon);
    uint32_t num_rings = 1 + static_cast<uint32_t>(poly.inners().size());
    detail::append_uint32(buf, num_rings);
    detail::append_ring(buf, poly.outer());
    for (const auto &inner : poly.inners()) detail::append_ring(buf, inner);
  }
  return buf;
}

// Generic write_geometry: dispatches by variant alternative type.
inline std::string write_geometry(uint32_t srid,
                                  const CartesianVariant &geom) {
  return std::visit(
      [srid](const auto &g) -> std::string {
        using T = std::decay_t<decltype(g)>;
        if constexpr (std::is_same_v<T, Point>)
          return write_point(srid, g);
        else if constexpr (std::is_same_v<T, Linestring>)
          return write_linestring(srid, g);
        else if constexpr (std::is_same_v<T, Polygon>)
          return write_polygon(srid, g);
        else if constexpr (std::is_same_v<T, MultiPoint>)
          return write_multi_point(srid, g);
        else if constexpr (std::is_same_v<T, MultiLinestring>)
          return write_multi_linestring(srid, g);
        else if constexpr (std::is_same_v<T, MultiPolygon>)
          return write_multi_polygon(srid, g);
        else
          return {};
      },
      geom);
}

inline std::string write_geometry(uint32_t srid,
                                  const GeographicVariant &geom) {
  return std::visit(
      [srid](const auto &g) -> std::string {
        using T = std::decay_t<decltype(g)>;
        if constexpr (std::is_same_v<T, GeoPoint>)
          return write_point(srid, g);
        else if constexpr (std::is_same_v<T, GeoLinestring>)
          return write_linestring(srid, g);
        else if constexpr (std::is_same_v<T, GeoPolygon>)
          return write_polygon(srid, g);
        else if constexpr (std::is_same_v<T, GeoMultiPoint>)
          return write_multi_point(srid, g);
        else if constexpr (std::is_same_v<T, GeoMultiLinestring>)
          return write_multi_linestring(srid, g);
        else if constexpr (std::is_same_v<T, GeoMultiPolygon>)
          return write_multi_polygon(srid, g);
        else
          return {};
      },
      geom);
}

}  // namespace gis_lib

#endif  // GIS_LIB_WKB_WRITER_H
