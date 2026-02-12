#ifndef GIS_LIB_WKB_WRITER_H
#define GIS_LIB_WKB_WRITER_H

#include <cstring>
#include <string>

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

}  // namespace gis_lib

#endif  // GIS_LIB_WKB_WRITER_H
