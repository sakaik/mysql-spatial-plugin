#include "wkb_parser.h"

#include <cstring>

#include <boost/geometry/algorithms/correct.hpp>

namespace gis_lib {

namespace {

// Reader that tracks position and handles byte order.
class WkbReader {
 public:
  WkbReader(const char *data, size_t length)
      : data_(data), length_(length), pos_(0), little_endian_(true) {}

  bool has_bytes(size_t n) const { return pos_ + n <= length_; }

  bool read_byte_order() {
    if (!has_bytes(1)) return false;
    uint8_t bo = static_cast<uint8_t>(data_[pos_++]);
    if (bo == 0x01)
      little_endian_ = true;
    else if (bo == 0x00)
      little_endian_ = false;
    else
      return false;
    return true;
  }

  bool read_uint32(uint32_t *out) {
    if (!has_bytes(4)) return false;
    uint32_t v;
    std::memcpy(&v, data_ + pos_, 4);
    pos_ += 4;
    if (!little_endian_) v = swap32(v);
    *out = v;
    return true;
  }

  bool read_double(double *out) {
    if (!has_bytes(8)) return false;
    uint64_t v;
    std::memcpy(&v, data_ + pos_, 8);
    pos_ += 8;
    if (!little_endian_) v = swap64(v);
    std::memcpy(out, &v, 8);
    return true;
  }

  // Read a uint32 as little-endian regardless of WKB byte order (for SRID).
  bool read_uint32_le(uint32_t *out) {
    if (!has_bytes(4)) return false;
    uint32_t v;
    std::memcpy(&v, data_ + pos_, 4);
    pos_ += 4;
    *out = v;
    return true;
  }

 private:
  static uint32_t swap32(uint32_t v) {
    return ((v & 0xFF) << 24) | ((v & 0xFF00) << 8) | ((v >> 8) & 0xFF00) |
           ((v >> 24) & 0xFF);
  }

  static uint64_t swap64(uint64_t v) {
    v = ((v & 0x00000000FFFFFFFFull) << 32) |
        ((v & 0xFFFFFFFF00000000ull) >> 32);
    v = ((v & 0x0000FFFF0000FFFFull) << 16) |
        ((v & 0xFFFF0000FFFF0000ull) >> 16);
    v = ((v & 0x00FF00FF00FF00FFull) << 8) |
        ((v & 0xFF00FF00FF00FF00ull) >> 8);
    return v;
  }

  const char *data_;
  size_t length_;
  size_t pos_;
  bool little_endian_;
};

// Templated parser for Cartesian/Geographic coordinate systems.
template <typename Traits>
struct Parser {
  using point_t = typename Traits::point;
  using linestring_t = typename Traits::linestring;
  using ring_t = typename Traits::ring;
  using polygon_t = typename Traits::polygon;
  using multi_point_t = typename Traits::multi_point;
  using multi_linestring_t = typename Traits::multi_linestring;
  using multi_polygon_t = typename Traits::multi_polygon;
  using variant_t = typename Traits::variant;

  static bool parse_point(WkbReader &r, point_t &pt) {
    double v1, v2;
    if (!r.read_double(&v1) || !r.read_double(&v2)) return false;
    if constexpr (Traits::swap_xy) {
      // SRID 4326: WKB has (lat, lon), boost geographic wants (lon, lat)
      bg::set<0>(pt, v2);
      bg::set<1>(pt, v1);
    } else {
      bg::set<0>(pt, v1);
      bg::set<1>(pt, v2);
    }
    return true;
  }

  static bool parse_linestring(WkbReader &r, linestring_t &ls) {
    uint32_t num_points;
    if (!r.read_uint32(&num_points)) return false;
    ls.resize(num_points);
    for (uint32_t i = 0; i < num_points; ++i) {
      if (!parse_point(r, ls[i])) return false;
    }
    return true;
  }

  static bool parse_ring(WkbReader &r, ring_t &ring) {
    uint32_t num_points;
    if (!r.read_uint32(&num_points)) return false;
    ring.resize(num_points);
    for (uint32_t i = 0; i < num_points; ++i) {
      if (!parse_point(r, ring[i])) return false;
    }
    return true;
  }

  static bool parse_polygon(WkbReader &r, polygon_t &poly) {
    uint32_t num_rings;
    if (!r.read_uint32(&num_rings)) return false;
    if (num_rings == 0) return true;

    if (!parse_ring(r, poly.outer())) return false;

    poly.inners().resize(num_rings - 1);
    for (uint32_t i = 0; i < num_rings - 1; ++i) {
      if (!parse_ring(r, poly.inners()[i])) return false;
    }

    bg::correct(poly);
    return true;
  }

  static bool parse_wkb_geometry(WkbReader &r, GeometryType &type_out,
                                 variant_t &geom_out) {
    if (!r.read_byte_order()) return false;

    uint32_t type_code;
    if (!r.read_uint32(&type_code)) return false;
    type_out = static_cast<GeometryType>(type_code);

    switch (type_out) {
      case GeometryType::Point: {
        point_t pt;
        if (!parse_point(r, pt)) return false;
        geom_out = pt;
        return true;
      }
      case GeometryType::LineString: {
        linestring_t ls;
        if (!parse_linestring(r, ls)) return false;
        geom_out = ls;
        return true;
      }
      case GeometryType::Polygon: {
        polygon_t poly;
        if (!parse_polygon(r, poly)) return false;
        geom_out = poly;
        return true;
      }
      case GeometryType::MultiPoint: {
        uint32_t count;
        if (!r.read_uint32(&count)) return false;
        multi_point_t mp;
        mp.resize(count);
        for (uint32_t i = 0; i < count; ++i) {
          if (!r.read_byte_order()) return false;
          uint32_t sub_type;
          if (!r.read_uint32(&sub_type)) return false;
          if (static_cast<GeometryType>(sub_type) != GeometryType::Point)
            return false;
          if (!parse_point(r, mp[i])) return false;
        }
        geom_out = mp;
        return true;
      }
      case GeometryType::MultiLineString: {
        uint32_t count;
        if (!r.read_uint32(&count)) return false;
        multi_linestring_t mls;
        mls.resize(count);
        for (uint32_t i = 0; i < count; ++i) {
          if (!r.read_byte_order()) return false;
          uint32_t sub_type;
          if (!r.read_uint32(&sub_type)) return false;
          if (static_cast<GeometryType>(sub_type) != GeometryType::LineString)
            return false;
          if (!parse_linestring(r, mls[i])) return false;
        }
        geom_out = mls;
        return true;
      }
      case GeometryType::MultiPolygon: {
        uint32_t count;
        if (!r.read_uint32(&count)) return false;
        multi_polygon_t mpoly;
        mpoly.resize(count);
        for (uint32_t i = 0; i < count; ++i) {
          if (!r.read_byte_order()) return false;
          uint32_t sub_type;
          if (!r.read_uint32(&sub_type)) return false;
          if (static_cast<GeometryType>(sub_type) != GeometryType::Polygon)
            return false;
          if (!parse_polygon(r, mpoly[i])) return false;
        }
        geom_out = mpoly;
        return true;
      }
      default:
        return false;
    }
  }
};

}  // namespace

std::optional<ParseResult> parse_geometry(const char *data, size_t length) {
  if (!data || length < 9) return std::nullopt;

  WkbReader reader(data, length);

  ParseResult result;
  if (!reader.read_uint32_le(&result.srid)) return std::nullopt;

  if (is_geographic_srid(result.srid)) {
    GeographicVariant geom;
    if (!Parser<GeographicTraits>::parse_wkb_geometry(reader, result.type,
                                                      geom))
      return std::nullopt;
    result.geometry = geom;
  } else {
    CartesianVariant geom;
    if (!Parser<CartesianTraits>::parse_wkb_geometry(reader, result.type, geom))
      return std::nullopt;
    result.geometry = geom;
  }

  return result;
}

}  // namespace gis_lib
