#ifndef MYSQL_DYNAMIC_PLUGIN
#define MYSQL_DYNAMIC_PLUGIN
#endif

#include <mysql/plugin.h>
#include <mysql.h>
#include <mysql/service_plugin_registry.h>
#include <mysql/components/my_service.h>
#include <mysql/components/services/udf_registration.h>
#include <mysql/components/services/udf_metadata.h>
#include <mysql/components/services/mysql_runtime_error_service.h>
#include <mysqld_error.h>
#include <string.h>

#include <algorithm>
#include <cmath>
#include <random>
#include <sstream>
#include <string>

#include "plugin_version.h"

#include <boost/geometry/algorithms/area.hpp>
#include <boost/geometry/algorithms/covered_by.hpp>
#include <boost/geometry/algorithms/distance.hpp>
#include <boost/geometry/algorithms/equals.hpp>
#include <boost/geometry/algorithms/intersects.hpp>
#include <boost/geometry/algorithms/length.hpp>
#include <boost/geometry/algorithms/densify.hpp>
#include <boost/geometry/algorithms/envelope.hpp>
#include <boost/geometry/algorithms/perimeter.hpp>
#include <boost/geometry/algorithms/reverse.hpp>
#include <boost/geometry/algorithms/touches.hpp>
#include <boost/geometry/algorithms/unique.hpp>
#include <boost/geometry/algorithms/within.hpp>
#include <boost/geometry/io/wkt/wkt.hpp>
#include <boost/geometry/formulas/vincenty_direct.hpp>
#include <boost/geometry/formulas/vincenty_inverse.hpp>
#include <boost/geometry/srs/spheroid.hpp>

#include "gis_lib/geometry_types.h"
#include "gis_lib/wkb_parser.h"
#include "gis_lib/wkb_writer.h"
#include "gis_lib/geos_helper.h"

using namespace gis_lib;

// Geometry type name lookup for error messages
static const char *geometry_type_name(GeometryType t) {
  switch (t) {
    case GeometryType::Point: return "POINT";
    case GeometryType::LineString: return "LINESTRING";
    case GeometryType::Polygon: return "POLYGON";
    case GeometryType::MultiPoint: return "MULTIPOINT";
    case GeometryType::MultiLineString: return "MULTILINESTRING";
    case GeometryType::MultiPolygon: return "MULTIPOLYGON";
    case GeometryType::GeometryCollection: return "GEOMETRYCOLLECTION";
    default: return "UNKNOWN";
  }
}

// Global pointer to udf_metadata service (for setting result charset)
static SERVICE_TYPE(mysql_udf_metadata) *udf_metadata_svc = nullptr;

// Set UDF result charset to utf8mb4 (call from _init functions of text-returning UDFs)
static void set_result_charset_utf8(UDF_INIT *initid) {
  if (udf_metadata_svc) {
    const char *charset = "utf8mb4";
    udf_metadata_svc->result_set(initid, "charset",
                                  static_cast<void *>(const_cast<char *>(charset)));
  }
}

static const bg::srs::spheroid<double> WGS84(6378137.0, 6356752.314245179);
static constexpr double DEG2RAD = M_PI / 180.0;
static constexpr double RAD2DEG = 180.0 / M_PI;

// ===== Helper: parse two geometry args with same SRID family =================

struct TwoGeom {
  ParseResult r1, r2;
};

static std::optional<TwoGeom> parse_two_geoms(UDF_ARGS *args) {
  auto r1 = parse_geometry(args->args[0], args->lengths[0]);
  auto r2 = parse_geometry(args->args[1], args->lengths[1]);
  if (!r1 || !r2) return std::nullopt;
  // Must be same coordinate system
  bool both_cart = std::holds_alternative<CartesianVariant>(r1->geometry) &&
                   std::holds_alternative<CartesianVariant>(r2->geometry);
  bool both_geo = std::holds_alternative<GeographicVariant>(r1->geometry) &&
                  std::holds_alternative<GeographicVariant>(r2->geometry);
  if (!both_cart && !both_geo) return std::nullopt;
  return TwoGeom{*r1, *r2};
}

// ===== Perimeter =============================================================

static std::optional<double> perimeter_cartesian(const CartesianVariant &geom) {
  if (auto *p = std::get_if<Polygon>(&geom)) return bg::perimeter(*p);
  if (auto *p = std::get_if<MultiPolygon>(&geom)) return bg::perimeter(*p);
  return std::nullopt;
}

static std::optional<double> perimeter_geographic(const GeographicVariant &geom) {
  if (auto *p = std::get_if<GeoPolygon>(&geom)) return bg::perimeter(*p);
  if (auto *p = std::get_if<GeoMultiPolygon>(&geom)) return bg::perimeter(*p);
  return std::nullopt;
}

// ===== CoveredBy =============================================================

static long long covered_by_cartesian(const CartesianVariant &g1,
                                      const CartesianVariant &g2) {
  if (auto *p1 = std::get_if<Point>(&g1)) {
    if (auto *g = std::get_if<Polygon>(&g2))
      return bg::covered_by(*p1, *g) ? 1 : 0;
    if (auto *g = std::get_if<MultiPolygon>(&g2)) {
      for (const auto &poly : *g)
        if (bg::covered_by(*p1, poly)) return 1;
      return 0;
    }
    if (auto *g = std::get_if<Point>(&g2))
      return bg::covered_by(*p1, *g) ? 1 : 0;
    if (auto *g = std::get_if<Linestring>(&g2))
      return bg::covered_by(*p1, *g) ? 1 : 0;
  }
  if (auto *l1 = std::get_if<Linestring>(&g1)) {
    if (auto *g = std::get_if<Polygon>(&g2))
      return bg::covered_by(*l1, *g) ? 1 : 0;
    if (auto *g = std::get_if<MultiPolygon>(&g2)) {
      for (const auto &poly : *g)
        if (bg::covered_by(*l1, poly)) return 1;
      return 0;
    }
    if (auto *g = std::get_if<Linestring>(&g2))
      return bg::covered_by(*l1, *g) ? 1 : 0;
  }
  if (auto *a1 = std::get_if<Polygon>(&g1)) {
    if (auto *g = std::get_if<Polygon>(&g2))
      return bg::covered_by(*a1, *g) ? 1 : 0;
    if (auto *g = std::get_if<MultiPolygon>(&g2)) {
      for (const auto &poly : *g)
        if (bg::covered_by(*a1, poly)) return 1;
      return 0;
    }
  }
  if (auto *mp1 = std::get_if<MultiPoint>(&g1)) {
    if (auto *g = std::get_if<Polygon>(&g2)) {
      for (const auto &pt : *mp1)
        if (!bg::covered_by(pt, *g)) return 0;
      return 1;
    }
    if (auto *g = std::get_if<MultiPolygon>(&g2)) {
      for (const auto &pt : *mp1) {
        bool ok = false;
        for (const auto &poly : *g)
          if (bg::covered_by(pt, poly)) { ok = true; break; }
        if (!ok) return 0;
      }
      return 1;
    }
  }
  return -1;
}

static long long covered_by_geographic(const GeographicVariant &g1,
                                       const GeographicVariant &g2) {
  if (auto *p1 = std::get_if<GeoPoint>(&g1)) {
    if (auto *g = std::get_if<GeoPolygon>(&g2))
      return bg::covered_by(*p1, *g) ? 1 : 0;
    if (auto *g = std::get_if<GeoMultiPolygon>(&g2)) {
      for (const auto &poly : *g)
        if (bg::covered_by(*p1, poly)) return 1;
      return 0;
    }
    if (auto *g = std::get_if<GeoPoint>(&g2))
      return bg::covered_by(*p1, *g) ? 1 : 0;
    if (auto *g = std::get_if<GeoLinestring>(&g2))
      return bg::covered_by(*p1, *g) ? 1 : 0;
  }
  if (auto *a1 = std::get_if<GeoPolygon>(&g1)) {
    if (auto *g = std::get_if<GeoPolygon>(&g2))
      return bg::covered_by(*a1, *g) ? 1 : 0;
    if (auto *g = std::get_if<GeoMultiPolygon>(&g2)) {
      for (const auto &poly : *g)
        if (bg::covered_by(*a1, poly)) return 1;
      return 0;
    }
  }
  return -1;
}

// ===== Distance helpers (for stx_dwithin) ====================================

static std::optional<double> calc_distance(const ParseResult &r1,
                                           const ParseResult &r2) {
  auto *c1 = std::get_if<CartesianVariant>(&r1.geometry);
  auto *c2 = std::get_if<CartesianVariant>(&r2.geometry);
  if (c1 && c2) {
    // Use std::visit to dispatch distance for any pair
    return std::visit(
        [](const auto &a, const auto &b) -> std::optional<double> {
          return bg::distance(a, b);
        },
        *c1, *c2);
  }
  auto *g1 = std::get_if<GeographicVariant>(&r1.geometry);
  auto *g2 = std::get_if<GeographicVariant>(&r2.geometry);
  if (g1 && g2) {
    return std::visit(
        [](const auto &a, const auto &b) -> std::optional<double> {
          return bg::distance(a, b);
        },
        *g1, *g2);
  }
  return std::nullopt;
}

// ===== LineLocatePoint (cartesian) ===========================================
// Returns fraction [0,1] along line closest to point.

template <typename PointT, typename LinestringT>
static double line_locate_point_impl(const LinestringT &line,
                                     const PointT &pt) {
  double total_len = bg::length(line);
  if (total_len == 0.0 || line.size() < 2) return 0.0;

  double min_dist_sq = std::numeric_limits<double>::max();
  double best_frac = 0.0;
  double accumulated = 0.0;

  for (size_t i = 0; i + 1 < line.size(); ++i) {
    double x0 = bg::get<0>(line[i]), y0 = bg::get<1>(line[i]);
    double x1 = bg::get<0>(line[i + 1]), y1 = bg::get<1>(line[i + 1]);
    double dx = x1 - x0, dy = y1 - y0;
    double seg_len = std::sqrt(dx * dx + dy * dy);

    double t = 0.0;
    if (seg_len > 0.0) {
      t = ((bg::get<0>(pt) - x0) * dx + (bg::get<1>(pt) - y0) * dy) /
          (seg_len * seg_len);
      t = std::max(0.0, std::min(1.0, t));
    }

    double px = x0 + t * dx, py = y0 + t * dy;
    double ddx = bg::get<0>(pt) - px, ddy = bg::get<1>(pt) - py;
    double dist_sq = ddx * ddx + ddy * ddy;

    if (dist_sq < min_dist_sq) {
      min_dist_sq = dist_sq;
      best_frac = (accumulated + t * seg_len) / total_len;
    }
    accumulated += seg_len;
  }
  return best_frac;
}

// ===== LineSubstring (cartesian) =============================================
// Returns the point at a given fraction along a linestring.

template <typename PointT, typename LinestringT>
static PointT line_interpolate_impl(const LinestringT &line, double frac) {
  PointT result;
  double total_len = bg::length(line);
  if (total_len == 0.0 || line.size() < 2) {
    bg::set<0>(result, bg::get<0>(line[0]));
    bg::set<1>(result, bg::get<1>(line[0]));
    return result;
  }
  double target = frac * total_len;
  double accumulated = 0.0;
  for (size_t i = 0; i + 1 < line.size(); ++i) {
    double seg_len = bg::distance(line[i], line[i + 1]);
    double seg_end = accumulated + seg_len;
    if (seg_end >= target) {
      double t = (seg_len > 0.0) ? (target - accumulated) / seg_len : 0.0;
      t = std::max(0.0, std::min(1.0, t));
      bg::set<0>(result, bg::get<0>(line[i]) +
                         t * (bg::get<0>(line[i + 1]) - bg::get<0>(line[i])));
      bg::set<1>(result, bg::get<1>(line[i]) +
                         t * (bg::get<1>(line[i + 1]) - bg::get<1>(line[i])));
      return result;
    }
    accumulated = seg_end;
  }
  // fraction == 1.0: return last point
  bg::set<0>(result, bg::get<0>(line[line.size() - 1]));
  bg::set<1>(result, bg::get<1>(line[line.size() - 1]));
  return result;
}

// Returns sub-linestring between fractions [start, end].

template <typename PointT, typename LinestringT>
static LinestringT line_substring_impl(const LinestringT &line, double start_frac,
                                       double end_frac) {
  LinestringT result;
  double total_len = bg::length(line);
  if (total_len == 0.0 || line.size() < 2 || start_frac >= end_frac) {
    return result;
  }

  double start_dist = start_frac * total_len;
  double end_dist = end_frac * total_len;
  double accumulated = 0.0;
  bool started = false;

  for (size_t i = 0; i + 1 < line.size(); ++i) {
    double seg_len = bg::distance(line[i], line[i + 1]);
    double seg_end = accumulated + seg_len;

    if (!started && seg_end >= start_dist) {
      // Interpolate start point
      double t = (seg_len > 0.0) ? (start_dist - accumulated) / seg_len : 0.0;
      t = std::max(0.0, std::min(1.0, t));
      PointT p;
      bg::set<0>(p, bg::get<0>(line[i]) +
                         t * (bg::get<0>(line[i + 1]) - bg::get<0>(line[i])));
      bg::set<1>(p, bg::get<1>(line[i]) +
                         t * (bg::get<1>(line[i + 1]) - bg::get<1>(line[i])));
      result.push_back(p);
      started = true;
    }

    if (started) {
      if (seg_end >= end_dist) {
        // Interpolate end point
        double t =
            (seg_len > 0.0) ? (end_dist - accumulated) / seg_len : 0.0;
        t = std::max(0.0, std::min(1.0, t));
        PointT p;
        bg::set<0>(
            p, bg::get<0>(line[i]) +
                   t * (bg::get<0>(line[i + 1]) - bg::get<0>(line[i])));
        bg::set<1>(
            p, bg::get<1>(line[i]) +
                   t * (bg::get<1>(line[i + 1]) - bg::get<1>(line[i])));
        result.push_back(p);
        break;
      } else {
        result.push_back(line[i + 1]);
      }
    }
    accumulated = seg_end;
  }
  return result;
}

// ===== Coordinate transform helpers ==========================================
// Apply a coordinate transformation function to all points in a geometry.

template <typename PointT, typename Fn>
static void xform_point(PointT &pt, Fn &&fn) {
  double x = bg::get<0>(pt);
  double y = bg::get<1>(pt);
  fn(x, y);
  bg::set<0>(pt, x);
  bg::set<1>(pt, y);
}

template <typename Geom, typename Fn>
static void apply_transform(Geom &g, Fn &&fn) {
  using T = std::decay_t<Geom>;
  if constexpr (std::is_same_v<T, Point> || std::is_same_v<T, GeoPoint>) {
    xform_point(g, fn);
  } else if constexpr (std::is_same_v<T, Linestring> ||
                        std::is_same_v<T, GeoLinestring>) {
    for (auto &pt : g) xform_point(pt, fn);
  } else if constexpr (std::is_same_v<T, Polygon> ||
                        std::is_same_v<T, GeoPolygon>) {
    for (auto &pt : g.outer()) xform_point(pt, fn);
    for (auto &ring : g.inners())
      for (auto &pt : ring) xform_point(pt, fn);
  } else if constexpr (std::is_same_v<T, MultiPoint> ||
                        std::is_same_v<T, GeoMultiPoint>) {
    for (auto &pt : g) xform_point(pt, fn);
  } else if constexpr (std::is_same_v<T, MultiLinestring> ||
                        std::is_same_v<T, GeoMultiLinestring>) {
    for (auto &ls : g)
      for (auto &pt : ls) xform_point(pt, fn);
  } else if constexpr (std::is_same_v<T, MultiPolygon> ||
                        std::is_same_v<T, GeoMultiPolygon>) {
    for (auto &poly : g) {
      for (auto &pt : poly.outer()) xform_point(pt, fn);
      for (auto &ring : poly.inners())
        for (auto &pt : ring) xform_point(pt, fn);
    }
  }
}

template <typename Fn>
static void apply_transform_variant(CartesianVariant &v, Fn &&fn) {
  std::visit([&fn](auto &g) { apply_transform(g, fn); }, v);
}

template <typename Fn>
static void apply_transform_variant(GeographicVariant &v, Fn &&fn) {
  std::visit([&fn](auto &g) { apply_transform(g, fn); }, v);
}

// Apply a transform to parsed geometry and return WKB result.
template <typename Fn>
static std::string transform_and_write(ParseResult &r, Fn &&fn) {
  if (auto *cv = std::get_if<CartesianVariant>(&r.geometry)) {
    apply_transform_variant(*cv, fn);
    return write_geometry(r.srid, *cv);
  }
  if (auto *gv = std::get_if<GeographicVariant>(&r.geometry)) {
    apply_transform_variant(*gv, fn);
    return write_geometry(r.srid, *gv);
  }
  return {};
}

// Helper: return geometry WKB from a UDF (handles buffer vs malloc).
static char *return_wkb(UDF_INIT *initid, const std::string &wkb, char *result,
                        unsigned long *length) {
  *length = static_cast<unsigned long>(wkb.size());
  if (wkb.size() <= 255) {
    std::memcpy(result, wkb.data(), wkb.size());
    return result;
  }
  initid->ptr = static_cast<char *>(malloc(wkb.size()));
  std::memcpy(initid->ptr, wkb.data(), wkb.size());
  return initid->ptr;
}

// =============================================================================
// UDF implementations
// =============================================================================

extern "C" {

// ----- stx_perimeter ---------------------------------------------------------

static bool stx_perimeter_init(UDF_INIT *initid, UDF_ARGS *args, char *msg) {
  if (args->arg_count != 1) {
    strcpy(msg, "stx_perimeter() requires 1 argument");
    return true;
  }
  args->arg_type[0] = STRING_RESULT;
  initid->maybe_null = 1;
  initid->decimals = 31;  // DECIMAL_NOT_SPECIFIED — auto-trim trailing zeros
  return false;
}

static double stx_perimeter(UDF_INIT *, UDF_ARGS *args, char *is_null,
                            char *error) {
  if (!args->args[0]) { *is_null = 1; return 0.0; }
  auto r = parse_geometry(args->args[0], args->lengths[0]);
  if (!r) { *error = 1; return 0.0; }
  std::optional<double> result;
  if (auto *c = std::get_if<CartesianVariant>(&r->geometry))
    result = perimeter_cartesian(*c);
  else if (auto *g = std::get_if<GeographicVariant>(&r->geometry))
    result = perimeter_geographic(*g);
  if (!result) {
    my_error(ER_UNEXPECTED_GEOMETRY_TYPE, MYF(0),
             "POLYGON/MULTIPOLYGON",
             geometry_type_name(r->type),
             "stx_perimeter");
    *error = 1;
    return 0.0;
  }
  return *result;
}

static void stx_perimeter_deinit(UDF_INIT *) {}

// ----- stx_coveredby ---------------------------------------------------------

static bool stx_coveredby_init(UDF_INIT *initid, UDF_ARGS *args, char *msg) {
  if (args->arg_count != 2) {
    strcpy(msg, "stx_coveredby() requires 2 arguments");
    return true;
  }
  args->arg_type[0] = STRING_RESULT;
  args->arg_type[1] = STRING_RESULT;
  initid->maybe_null = 1;
  return false;
}

static long long stx_coveredby(UDF_INIT *, UDF_ARGS *args, char *is_null,
                               char *error) {
  if (!args->args[0] || !args->args[1]) { *is_null = 1; return 0; }
  auto two = parse_two_geoms(args);
  if (!two) { *error = 1; return 0; }
  long long ret = -1;
  auto *c1 = std::get_if<CartesianVariant>(&two->r1.geometry);
  auto *c2 = std::get_if<CartesianVariant>(&two->r2.geometry);
  if (c1 && c2) ret = covered_by_cartesian(*c1, *c2);
  auto *g1 = std::get_if<GeographicVariant>(&two->r1.geometry);
  auto *g2 = std::get_if<GeographicVariant>(&two->r2.geometry);
  if (g1 && g2) ret = covered_by_geographic(*g1, *g2);
  if (ret < 0) { *error = 1; return 0; }
  return ret;
}

static void stx_coveredby_deinit(UDF_INIT *) {}

// ----- stx_covers ------------------------------------------------------------

static bool stx_covers_init(UDF_INIT *initid, UDF_ARGS *args, char *msg) {
  if (args->arg_count != 2) {
    strcpy(msg, "stx_covers() requires 2 arguments");
    return true;
  }
  args->arg_type[0] = STRING_RESULT;
  args->arg_type[1] = STRING_RESULT;
  initid->maybe_null = 1;
  return false;
}

static long long stx_covers(UDF_INIT *, UDF_ARGS *args, char *is_null,
                            char *error) {
  if (!args->args[0] || !args->args[1]) { *is_null = 1; return 0; }
  // covers(a,b) = coveredby(b,a) — swap args
  auto r1 = parse_geometry(args->args[1], args->lengths[1]);
  auto r2 = parse_geometry(args->args[0], args->lengths[0]);
  if (!r1 || !r2) { *error = 1; return 0; }
  long long ret = -1;
  auto *c1 = std::get_if<CartesianVariant>(&r1->geometry);
  auto *c2 = std::get_if<CartesianVariant>(&r2->geometry);
  if (c1 && c2) ret = covered_by_cartesian(*c1, *c2);
  auto *g1 = std::get_if<GeographicVariant>(&r1->geometry);
  auto *g2 = std::get_if<GeographicVariant>(&r2->geometry);
  if (g1 && g2) ret = covered_by_geographic(*g1, *g2);
  if (ret < 0) { *error = 1; return 0; }
  return ret;
}

static void stx_covers_deinit(UDF_INIT *) {}

// ----- stx_dwithin -----------------------------------------------------------

static bool stx_dwithin_init(UDF_INIT *initid, UDF_ARGS *args, char *msg) {
  if (args->arg_count != 3) {
    strcpy(msg, "stx_dwithin() requires 3 arguments (geom1, geom2, distance)");
    return true;
  }
  args->arg_type[0] = STRING_RESULT;
  args->arg_type[1] = STRING_RESULT;
  args->arg_type[2] = REAL_RESULT;
  initid->maybe_null = 1;
  return false;
}

static long long stx_dwithin(UDF_INIT *, UDF_ARGS *args, char *is_null,
                             char *error) {
  if (!args->args[0] || !args->args[1] || !args->args[2]) {
    *is_null = 1;
    return 0;
  }
  auto r1 = parse_geometry(args->args[0], args->lengths[0]);
  auto r2 = parse_geometry(args->args[1], args->lengths[1]);
  if (!r1 || !r2) { *error = 1; return 0; }
  double threshold = *reinterpret_cast<double *>(args->args[2]);
  auto dist = calc_distance(*r1, *r2);
  if (!dist) { *error = 1; return 0; }
  return (*dist <= threshold) ? 1 : 0;
}

static void stx_dwithin_deinit(UDF_INIT *) {}

// ----- stx_azimuth -----------------------------------------------------------
// Returns azimuth in radians (clockwise from north) between two points.

static bool stx_azimuth_init(UDF_INIT *initid, UDF_ARGS *args, char *msg) {
  if (args->arg_count != 2) {
    strcpy(msg, "stx_azimuth() requires 2 point arguments");
    return true;
  }
  args->arg_type[0] = STRING_RESULT;
  args->arg_type[1] = STRING_RESULT;
  initid->maybe_null = 1;
  initid->decimals = 31;  // DECIMAL_NOT_SPECIFIED — auto-trim trailing zeros
  return false;
}

static double stx_azimuth(UDF_INIT *, UDF_ARGS *args, char *is_null,
                          char *error) {
  if (!args->args[0] || !args->args[1]) { *is_null = 1; return 0.0; }
  auto r1 = parse_geometry(args->args[0], args->lengths[0]);
  auto r2 = parse_geometry(args->args[1], args->lengths[1]);
  if (!r1 || !r2) { *error = 1; return 0.0; }

  // Geographic: Vincenty inverse
  auto *g1 = std::get_if<GeographicVariant>(&r1->geometry);
  auto *g2 = std::get_if<GeographicVariant>(&r2->geometry);
  if (g1 && g2) {
    auto *p1 = std::get_if<GeoPoint>(g1);
    auto *p2 = std::get_if<GeoPoint>(g2);
    if (!p1 || !p2) { *error = 1; return 0.0; }
    double lon1 = bg::get<0>(*p1) * DEG2RAD, lat1 = bg::get<1>(*p1) * DEG2RAD;
    double lon2 = bg::get<0>(*p2) * DEG2RAD, lat2 = bg::get<1>(*p2) * DEG2RAD;
    using vi = bg::formula::vincenty_inverse<double, true, true>;
    auto res = vi::apply(lon1, lat1, lon2, lat2, WGS84);
    double az = res.azimuth;
    if (az < 0.0) az += 2.0 * M_PI;
    return az;
  }

  // Cartesian: atan2
  auto *c1 = std::get_if<CartesianVariant>(&r1->geometry);
  auto *c2 = std::get_if<CartesianVariant>(&r2->geometry);
  if (c1 && c2) {
    auto *p1 = std::get_if<Point>(c1);
    auto *p2 = std::get_if<Point>(c2);
    if (!p1 || !p2) { *error = 1; return 0.0; }
    double dx = p2->x() - p1->x();
    double dy = p2->y() - p1->y();
    double az = std::atan2(dx, dy);  // north=0, clockwise
    if (az < 0.0) az += 2.0 * M_PI;
    return az;
  }

  *error = 1;
  return 0.0;
}

static void stx_azimuth_deinit(UDF_INIT *) {}

// ----- stx_project -----------------------------------------------------------
// Project a point by distance (meters for geo) and azimuth (radians).
// Returns a Point geometry.

static bool stx_project_init(UDF_INIT *initid, UDF_ARGS *args, char *msg) {
  if (args->arg_count != 3) {
    strcpy(msg,
           "stx_project() requires 3 arguments (point, distance, azimuth)");
    return true;
  }
  args->arg_type[0] = STRING_RESULT;
  args->arg_type[1] = REAL_RESULT;
  args->arg_type[2] = REAL_RESULT;
  initid->maybe_null = 1;
  initid->max_length = 25;  // Point WKB is always 25 bytes
  return false;
}

static char *stx_project(UDF_INIT *initid, UDF_ARGS *args, char *result,
                         unsigned long *length, char *is_null, char *error) {
  if (!args->args[0] || !args->args[1] || !args->args[2]) {
    *is_null = 1;
    return nullptr;
  }
  auto r = parse_geometry(args->args[0], args->lengths[0]);
  if (!r) { *error = 1; return nullptr; }
  double dist = *reinterpret_cast<double *>(args->args[1]);
  double azimuth = *reinterpret_cast<double *>(args->args[2]);

  std::string wkb;

  if (auto *gv = std::get_if<GeographicVariant>(&r->geometry)) {
    auto *pt = std::get_if<GeoPoint>(gv);
    if (!pt) { *error = 1; return nullptr; }
    double lon1 = bg::get<0>(*pt) * DEG2RAD;
    double lat1 = bg::get<1>(*pt) * DEG2RAD;
    using vd = bg::formula::vincenty_direct<double, true>;
    auto res = vd::apply(lon1, lat1, dist, azimuth, WGS84);
    GeoPoint dst;
    bg::set<0>(dst, res.lon2 * RAD2DEG);
    bg::set<1>(dst, res.lat2 * RAD2DEG);
    wkb = write_point(r->srid, dst);
  } else if (auto *cv = std::get_if<CartesianVariant>(&r->geometry)) {
    auto *pt = std::get_if<Point>(cv);
    if (!pt) { *error = 1; return nullptr; }
    double x = pt->x() + dist * std::sin(azimuth);
    double y = pt->y() + dist * std::cos(azimuth);
    wkb = write_point(r->srid, Point(x, y));
  } else {
    *error = 1;
    return nullptr;
  }

  *length = static_cast<unsigned long>(wkb.size());
  std::memcpy(result, wkb.data(), wkb.size());
  return result;
}

static void stx_project_deinit(UDF_INIT *) {}

// ----- stx_linelocatepoint ---------------------------------------------------
// Returns fraction [0,1] along line closest to point.

static bool stx_linelocatepoint_init(UDF_INIT *initid, UDF_ARGS *args,
                                     char *msg) {
  if (args->arg_count != 2) {
    strcpy(msg, "stx_linelocatepoint() requires 2 arguments (line, point)");
    return true;
  }
  args->arg_type[0] = STRING_RESULT;
  args->arg_type[1] = STRING_RESULT;
  initid->maybe_null = 1;
  initid->decimals = 31;  // DECIMAL_NOT_SPECIFIED — auto-trim trailing zeros
  return false;
}

static double stx_linelocatepoint(UDF_INIT *, UDF_ARGS *args, char *is_null,
                                  char *error) {
  if (!args->args[0] || !args->args[1]) { *is_null = 1; return 0.0; }
  auto r1 = parse_geometry(args->args[0], args->lengths[0]);
  auto r2 = parse_geometry(args->args[1], args->lengths[1]);
  if (!r1 || !r2) { *error = 1; return 0.0; }

  if (r1->type != GeometryType::LineString) {
    my_error(ER_UNEXPECTED_GEOMETRY_TYPE, MYF(0),
             "LINESTRING", geometry_type_name(r1->type),
             "stx_linelocatepoint");
    *error = 1; return 0.0;
  }
  if (r2->type != GeometryType::Point) {
    my_error(ER_UNEXPECTED_GEOMETRY_TYPE, MYF(0),
             "POINT", geometry_type_name(r2->type),
             "stx_linelocatepoint");
    *error = 1; return 0.0;
  }

  if (auto *c1 = std::get_if<CartesianVariant>(&r1->geometry)) {
    auto *ls = std::get_if<Linestring>(c1);
    auto *c2 = std::get_if<CartesianVariant>(&r2->geometry);
    auto *pt = c2 ? std::get_if<Point>(c2) : nullptr;
    if (!ls || !pt) { *error = 1; return 0.0; }
    return line_locate_point_impl<Point, Linestring>(*ls, *pt);
  }
  if (auto *g1 = std::get_if<GeographicVariant>(&r1->geometry)) {
    auto *ls = std::get_if<GeoLinestring>(g1);
    auto *g2 = std::get_if<GeographicVariant>(&r2->geometry);
    auto *pt = g2 ? std::get_if<GeoPoint>(g2) : nullptr;
    if (!ls || !pt) { *error = 1; return 0.0; }
    return line_locate_point_impl<GeoPoint, GeoLinestring>(*ls, *pt);
  }

  *error = 1;
  return 0.0;
}

static void stx_linelocatepoint_deinit(UDF_INIT *) {}

// ----- stx_linesubstring -----------------------------------------------------
// Returns sub-linestring between start_fraction and end_fraction [0,1].

static bool stx_linesubstring_init(UDF_INIT *initid, UDF_ARGS *args,
                                   char *msg) {
  if (args->arg_count != 3) {
    strcpy(msg,
           "stx_linesubstring() requires 3 arguments (line, start, end)");
    return true;
  }
  args->arg_type[0] = STRING_RESULT;
  args->arg_type[1] = REAL_RESULT;
  args->arg_type[2] = REAL_RESULT;
  initid->maybe_null = 1;
  initid->ptr = nullptr;
  return false;
}

static char *stx_linesubstring(UDF_INIT *initid, UDF_ARGS *args, char *result,
                               unsigned long *length, char *is_null,
                               char *error) {
  if (initid->ptr) { free(initid->ptr); initid->ptr = nullptr; }
  if (!args->args[0] || !args->args[1] || !args->args[2]) {
    *is_null = 1;
    return nullptr;
  }
  auto r = parse_geometry(args->args[0], args->lengths[0]);
  if (!r) { *error = 1; return nullptr; }
  if (r->type != GeometryType::LineString) {
    my_error(ER_UNEXPECTED_GEOMETRY_TYPE, MYF(0),
             "LINESTRING", geometry_type_name(r->type),
             "stx_linesubstring");
    *error = 1; return nullptr;
  }
  double sf = *reinterpret_cast<double *>(args->args[1]);
  double ef = *reinterpret_cast<double *>(args->args[2]);
  sf = std::max(0.0, std::min(1.0, sf));
  ef = std::max(0.0, std::min(1.0, ef));

  if (sf > ef) {
    my_error(ER_WRONG_ARGUMENTS, MYF(0),
             "stx_linesubstring: start fraction must be <= end fraction");
    *error = 1;
    return nullptr;
  }

  std::string wkb;

  if (sf == ef) {
    // start == end: return POINT at that position
    if (auto *cv = std::get_if<CartesianVariant>(&r->geometry)) {
      auto &ls = std::get<Linestring>(*cv);
      auto pt = line_interpolate_impl<Point, Linestring>(ls, sf);
      wkb = write_point(r->srid, pt);
    } else if (auto *gv = std::get_if<GeographicVariant>(&r->geometry)) {
      auto &ls = std::get<GeoLinestring>(*gv);
      auto pt = line_interpolate_impl<GeoPoint, GeoLinestring>(ls, sf);
      wkb = write_point(r->srid, pt);
    } else {
      *error = 1;
      return nullptr;
    }
  } else if (auto *cv = std::get_if<CartesianVariant>(&r->geometry)) {
    auto &ls = std::get<Linestring>(*cv);
    auto sub = line_substring_impl<Point, Linestring>(ls, sf, ef);
    wkb = write_linestring(r->srid, sub);
  } else if (auto *gv = std::get_if<GeographicVariant>(&r->geometry)) {
    auto &ls = std::get<GeoLinestring>(*gv);
    auto sub = line_substring_impl<GeoPoint, GeoLinestring>(ls, sf, ef);
    wkb = write_linestring(r->srid, sub);
  } else {
    *error = 1;
    return nullptr;
  }

  *length = static_cast<unsigned long>(wkb.size());
  if (wkb.size() <= 255) {
    std::memcpy(result, wkb.data(), wkb.size());
    return result;
  }
  initid->ptr = static_cast<char *>(malloc(wkb.size()));
  std::memcpy(initid->ptr, wkb.data(), wkb.size());
  return initid->ptr;
}

static void stx_linesubstring_deinit(UDF_INIT *initid) {
  if (initid->ptr) free(initid->ptr);
}

// ----- stx_angle -------------------------------------------------------------
// Returns the angle (radians) at P2 formed by rays P2->P1 and P2->P3.
// Result is in [0, 2*pi), measured counterclockwise from P2->P1 to P2->P3.

static bool stx_angle_init(UDF_INIT *initid, UDF_ARGS *args, char *msg) {
  if (args->arg_count != 3) {
    strcpy(msg, "stx_angle() requires 3 point arguments");
    return true;
  }
  args->arg_type[0] = STRING_RESULT;
  args->arg_type[1] = STRING_RESULT;
  args->arg_type[2] = STRING_RESULT;
  initid->maybe_null = 1;
  initid->decimals = 31;  // DECIMAL_NOT_SPECIFIED — auto-trim trailing zeros
  return false;
}

static double stx_angle(UDF_INIT *, UDF_ARGS *args, char *is_null,
                         char *error) {
  if (!args->args[0] || !args->args[1] || !args->args[2]) {
    *is_null = 1;
    return 0.0;
  }
  auto r1 = parse_geometry(args->args[0], args->lengths[0]);
  auto r2 = parse_geometry(args->args[1], args->lengths[1]);
  auto r3 = parse_geometry(args->args[2], args->lengths[2]);
  if (!r1 || !r2 || !r3) { *error = 1; return 0.0; }

  double x1, y1, x2, y2, x3, y3;

  // Extract coordinates from Cartesian points
  if (auto *c1 = std::get_if<CartesianVariant>(&r1->geometry)) {
    auto *p1 = std::get_if<Point>(c1);
    auto *c2 = std::get_if<CartesianVariant>(&r2->geometry);
    auto *p2 = c2 ? std::get_if<Point>(c2) : nullptr;
    auto *c3 = std::get_if<CartesianVariant>(&r3->geometry);
    auto *p3 = c3 ? std::get_if<Point>(c3) : nullptr;
    if (!p1 || !p2 || !p3) { *error = 1; return 0.0; }
    x1 = p1->x(); y1 = p1->y();
    x2 = p2->x(); y2 = p2->y();
    x3 = p3->x(); y3 = p3->y();
  } else if (auto *g1 = std::get_if<GeographicVariant>(&r1->geometry)) {
    auto *p1 = std::get_if<GeoPoint>(g1);
    auto *g2 = std::get_if<GeographicVariant>(&r2->geometry);
    auto *p2 = g2 ? std::get_if<GeoPoint>(g2) : nullptr;
    auto *g3 = std::get_if<GeographicVariant>(&r3->geometry);
    auto *p3 = g3 ? std::get_if<GeoPoint>(g3) : nullptr;
    if (!p1 || !p2 || !p3) { *error = 1; return 0.0; }
    x1 = bg::get<0>(*p1); y1 = bg::get<1>(*p1);
    x2 = bg::get<0>(*p2); y2 = bg::get<1>(*p2);
    x3 = bg::get<0>(*p3); y3 = bg::get<1>(*p3);
  } else {
    *error = 1;
    return 0.0;
  }

  double dx1 = x1 - x2, dy1 = y1 - y2;
  double dx2 = x3 - x2, dy2 = y3 - y2;
  double angle = std::atan2(dy2, dx2) - std::atan2(dy1, dx1);
  if (angle < 0.0) angle += 2.0 * M_PI;
  return angle;
}

static void stx_angle_deinit(UDF_INIT *) {}

// ----- stx_translate ---------------------------------------------------------
// Translates a geometry by (dx, dy).

static bool stx_translate_init(UDF_INIT *initid, UDF_ARGS *args, char *msg) {
  if (args->arg_count != 3) {
    strcpy(msg, "stx_translate() requires 3 arguments (geometry, dx, dy)");
    return true;
  }
  args->arg_type[0] = STRING_RESULT;
  args->arg_type[1] = REAL_RESULT;
  args->arg_type[2] = REAL_RESULT;
  initid->maybe_null = 1;
  initid->ptr = nullptr;
  return false;
}

static char *stx_translate(UDF_INIT *initid, UDF_ARGS *args, char *result,
                            unsigned long *length, char *is_null, char *error) {
  if (initid->ptr) { free(initid->ptr); initid->ptr = nullptr; }
  if (!args->args[0] || !args->args[1] || !args->args[2]) {
    *is_null = 1;
    return nullptr;
  }
  auto r = parse_geometry(args->args[0], args->lengths[0]);
  if (!r) { *error = 1; return nullptr; }
  double dx = *reinterpret_cast<double *>(args->args[1]);
  double dy = *reinterpret_cast<double *>(args->args[2]);

  auto wkb = transform_and_write(*r, [dx, dy](double &x, double &y) {
    x += dx;
    y += dy;
  });
  if (wkb.empty()) { *error = 1; return nullptr; }
  return return_wkb(initid, wkb, result, length);
}

static void stx_translate_deinit(UDF_INIT *initid) {
  if (initid->ptr) free(initid->ptr);
}

// ----- stx_translate_latlon --------------------------------------------------
// Translates a geometry by (delta_lat, delta_lon). Geographic SRIDs only.

static bool stx_translate_latlon_init(UDF_INIT *initid, UDF_ARGS *args,
                                       char *msg) {
  if (args->arg_count != 3) {
    strcpy(msg,
           "stx_translate_latlon() requires 3 arguments"
           " (geometry, delta_lat, delta_lon)");
    return true;
  }
  args->arg_type[0] = STRING_RESULT;
  args->arg_type[1] = REAL_RESULT;
  args->arg_type[2] = REAL_RESULT;
  initid->maybe_null = 1;
  initid->ptr = nullptr;
  return false;
}

static char *stx_translate_latlon(UDF_INIT *initid, UDF_ARGS *args,
                                   char *result, unsigned long *length,
                                   char *is_null, char *error) {
  if (initid->ptr) { free(initid->ptr); initid->ptr = nullptr; }
  if (!args->args[0] || !args->args[1] || !args->args[2]) {
    *is_null = 1;
    return nullptr;
  }
  uint32_t srid = extract_srid(args->args[0], args->lengths[0]);
  if (!is_geographic_srid(srid)) {
    my_error(ER_SRS_NOT_GEOGRAPHIC, MYF(0), "stx_translate_latlon", srid);
    *error = 1;
    return nullptr;
  }
  auto r = parse_geometry(args->args[0], args->lengths[0]);
  if (!r) { *error = 1; return nullptr; }
  double dlat = *reinterpret_cast<double *>(args->args[1]);
  double dlon = *reinterpret_cast<double *>(args->args[2]);

  // WKB stores (lon, lat) for geographic SRIDs, so x=lon, y=lat.
  auto wkb = transform_and_write(*r, [dlon, dlat](double &x, double &y) {
    x += dlon;
    y += dlat;
  });
  if (wkb.empty()) { *error = 1; return nullptr; }
  return return_wkb(initid, wkb, result, length);
}

static void stx_translate_latlon_deinit(UDF_INIT *initid) {
  if (initid->ptr) free(initid->ptr);
}

// ----- stx_scale -------------------------------------------------------------
// Scales a geometry by (sx, sy).
// 3 args: (geometry, sx, sy) — scale relative to origin
// 4 args: (geometry, sx, sy, center_point) — scale relative to POINT geometry
// 5 args: (geometry, sx, sy, cx, cy) — scale relative to point (cx, cy)

static bool stx_scale_init(UDF_INIT *initid, UDF_ARGS *args, char *msg) {
  if (args->arg_count < 3 || args->arg_count > 5) {
    strcpy(msg, "stx_scale() requires 3-5 arguments (geometry, sx, sy[, center_point | cx, cy])");
    return true;
  }
  args->arg_type[0] = STRING_RESULT;
  args->arg_type[1] = REAL_RESULT;
  args->arg_type[2] = REAL_RESULT;
  if (args->arg_count == 4) {
    args->arg_type[3] = STRING_RESULT;  // POINT geometry
  } else if (args->arg_count == 5) {
    args->arg_type[3] = REAL_RESULT;
    args->arg_type[4] = REAL_RESULT;
  }
  initid->maybe_null = 1;
  initid->ptr = nullptr;
  return false;
}

static char *stx_scale(UDF_INIT *initid, UDF_ARGS *args, char *result,
                        unsigned long *length, char *is_null, char *error) {
  if (initid->ptr) { free(initid->ptr); initid->ptr = nullptr; }
  if (!args->args[0] || !args->args[1] || !args->args[2]) {
    *is_null = 1;
    return nullptr;
  }
  auto r = parse_geometry(args->args[0], args->lengths[0]);
  if (!r) { *error = 1; return nullptr; }
  double sx = *reinterpret_cast<double *>(args->args[1]);
  double sy = *reinterpret_cast<double *>(args->args[2]);
  double cx = 0.0, cy = 0.0;

  if (args->arg_count == 4) {
    if (!args->args[3]) { *is_null = 1; return nullptr; }
    auto cp = parse_geometry(args->args[3], args->lengths[3]);
    if (!cp) { *error = 1; return nullptr; }
    if (cp->srid != r->srid) {
      *error = 1;
      return nullptr;
    }
    if (auto *cv = std::get_if<CartesianVariant>(&cp->geometry)) {
      auto *pt = std::get_if<Point>(cv);
      if (!pt) { *error = 1; return nullptr; }
      cx = bg::get<0>(*pt);
      cy = bg::get<1>(*pt);
    } else if (auto *gv = std::get_if<GeographicVariant>(&cp->geometry)) {
      auto *pt = std::get_if<GeoPoint>(gv);
      if (!pt) { *error = 1; return nullptr; }
      cx = bg::get<0>(*pt);
      cy = bg::get<1>(*pt);
    }
  } else if (args->arg_count == 5) {
    if (!args->args[3] || !args->args[4]) { *is_null = 1; return nullptr; }
    cx = *reinterpret_cast<double *>(args->args[3]);
    cy = *reinterpret_cast<double *>(args->args[4]);
  }

  auto wkb =
      transform_and_write(*r, [sx, sy, cx, cy](double &x, double &y) {
        x = cx + (x - cx) * sx;
        y = cy + (y - cy) * sy;
      });
  if (wkb.empty()) { *error = 1; return nullptr; }
  return return_wkb(initid, wkb, result, length);
}

static void stx_scale_deinit(UDF_INIT *initid) {
  if (initid->ptr) free(initid->ptr);
}

// ----- stx_rotate ------------------------------------------------------------
// Rotates a geometry by angle (radians).
// 2 args: (geometry, angle) — rotate around origin
// 3 args: (geometry, angle, center_point) — rotate around POINT geometry
// 4 args: (geometry, angle, cx, cy) — rotate around point (cx, cy)

static bool stx_rotate_init(UDF_INIT *initid, UDF_ARGS *args, char *msg) {
  if (args->arg_count < 2 || args->arg_count > 4) {
    strcpy(msg, "stx_rotate() requires 2-4 arguments (geometry, angle[, center_point | cx, cy])");
    return true;
  }
  args->arg_type[0] = STRING_RESULT;
  args->arg_type[1] = REAL_RESULT;
  if (args->arg_count == 3) {
    args->arg_type[2] = STRING_RESULT;  // POINT geometry
  } else if (args->arg_count == 4) {
    args->arg_type[2] = REAL_RESULT;
    args->arg_type[3] = REAL_RESULT;
  }
  initid->maybe_null = 1;
  initid->ptr = nullptr;
  return false;
}

static char *stx_rotate(UDF_INIT *initid, UDF_ARGS *args, char *result,
                         unsigned long *length, char *is_null, char *error) {
  if (initid->ptr) { free(initid->ptr); initid->ptr = nullptr; }
  if (!args->args[0] || !args->args[1]) {
    *is_null = 1;
    return nullptr;
  }
  auto r = parse_geometry(args->args[0], args->lengths[0]);
  if (!r) { *error = 1; return nullptr; }
  double angle = *reinterpret_cast<double *>(args->args[1]);
  double cos_a = std::cos(angle);
  double sin_a = std::sin(angle);
  double cx = 0.0, cy = 0.0;

  if (args->arg_count == 3) {
    if (!args->args[2]) { *is_null = 1; return nullptr; }
    auto cp = parse_geometry(args->args[2], args->lengths[2]);
    if (!cp) { *error = 1; return nullptr; }
    if (cp->srid != r->srid) {
      // SRID mismatch — return error
      *error = 1;
      return nullptr;
    }
    // Extract coordinates from the center point
    if (auto *cv = std::get_if<CartesianVariant>(&cp->geometry)) {
      auto *pt = std::get_if<Point>(cv);
      if (!pt) { *error = 1; return nullptr; }  // not a POINT
      cx = bg::get<0>(*pt);
      cy = bg::get<1>(*pt);
    } else if (auto *gv = std::get_if<GeographicVariant>(&cp->geometry)) {
      auto *pt = std::get_if<GeoPoint>(gv);
      if (!pt) { *error = 1; return nullptr; }  // not a POINT
      cx = bg::get<0>(*pt);
      cy = bg::get<1>(*pt);
    }
  } else if (args->arg_count == 4) {
    if (!args->args[2] || !args->args[3]) { *is_null = 1; return nullptr; }
    cx = *reinterpret_cast<double *>(args->args[2]);
    cy = *reinterpret_cast<double *>(args->args[3]);
  }

  auto wkb =
      transform_and_write(*r, [cos_a, sin_a, cx, cy](double &x, double &y) {
        double dx = x - cx;
        double dy = y - cy;
        x = cx + dx * cos_a - dy * sin_a;
        y = cy + dx * sin_a + dy * cos_a;
      });
  if (wkb.empty()) { *error = 1; return nullptr; }
  return return_wkb(initid, wkb, result, length);
}

static void stx_rotate_deinit(UDF_INIT *initid) {
  if (initid->ptr) free(initid->ptr);
}

// ----- stx_reverse -----------------------------------------------------------
// Reverses vertex order of a geometry.

static bool stx_reverse_init(UDF_INIT *initid, UDF_ARGS *args, char *msg) {
  if (args->arg_count != 1) {
    strcpy(msg, "stx_reverse() requires 1 argument");
    return true;
  }
  args->arg_type[0] = STRING_RESULT;
  initid->maybe_null = 1;
  initid->ptr = nullptr;
  return false;
}

static char *stx_reverse(UDF_INIT *initid, UDF_ARGS *args, char *result,
                          unsigned long *length, char *is_null, char *error) {
  if (initid->ptr) { free(initid->ptr); initid->ptr = nullptr; }
  if (!args->args[0]) { *is_null = 1; return nullptr; }
  auto r = parse_geometry(args->args[0], args->lengths[0]);
  if (!r) { *error = 1; return nullptr; }

  // Apply bg::reverse to the geometry
  if (auto *cv = std::get_if<CartesianVariant>(&r->geometry)) {
    std::visit([](auto &g) { bg::reverse(g); }, *cv);
    auto wkb = write_geometry(r->srid, *cv);
    if (wkb.empty()) { *error = 1; return nullptr; }
    return return_wkb(initid, wkb, result, length);
  }
  if (auto *gv = std::get_if<GeographicVariant>(&r->geometry)) {
    std::visit([](auto &g) { bg::reverse(g); }, *gv);
    auto wkb = write_geometry(r->srid, *gv);
    if (wkb.empty()) { *error = 1; return nullptr; }
    return return_wkb(initid, wkb, result, length);
  }

  *error = 1;
  return nullptr;
}

static void stx_reverse_deinit(UDF_INIT *initid) {
  if (initid->ptr) free(initid->ptr);
}

}  // extern "C" (pause for template helpers)


// DE-9IM pattern matching helper (outside extern "C" since it's C++ only).
static bool de9im_match(const std::string &matrix, const char *pattern,
                         size_t pat_len) {
  if (matrix.size() != 9 || pat_len != 9) return false;
  for (int i = 0; i < 9; ++i) {
    char m = matrix[i];
    char p = pattern[i];
    if (p == '*') continue;
    if (p == 'T' || p == 't') {
      if (m == 'F' || m == '-' || m == 'f') return false;
    } else if (p == 'F' || p == 'f') {
      if (m != 'F' && m != 'f') return false;
    } else {
      if (m != p) return false;
    }
  }
  return true;
}

extern "C" {  // resume extern "C" for UDF functions

// ----- stx_pointonsurface ----------------------------------------------------
// Returns a point guaranteed to lie on the surface of the geometry.
// Uses GEOS GEOSPointOnSurface() — works with any geometry type.

static bool stx_pointonsurface_init(UDF_INIT *initid, UDF_ARGS *args,
                                     char *msg) {
  if (args->arg_count != 1) {
    strcpy(msg, "stx_pointonsurface() requires 1 argument");
    return true;
  }
  args->arg_type[0] = STRING_RESULT;
  initid->maybe_null = 1;
  initid->ptr = nullptr;
  return false;
}

static char *stx_pointonsurface(UDF_INIT *initid, UDF_ARGS *args, char *result,
                                 unsigned long *length, char *is_null,
                                 char *error) {
  if (initid->ptr) { free(initid->ptr); initid->ptr = nullptr; }
  if (!args->args[0]) { *is_null = 1; return nullptr; }

  uint32_t srid = extract_srid(args->args[0], args->lengths[0]);

  auto geom = mysql_to_geos(args->args[0], args->lengths[0]);
  if (!geom) { *error = 1; return nullptr; }

  auto ctx = get_geos_context();
  GEOSGeomPtr pt(GEOSPointOnSurface_r(ctx, geom.get()));
  if (!pt) { *error = 1; return nullptr; }

  const GEOSCoordSequence *cs = GEOSGeom_getCoordSeq_r(ctx, pt.get());
  if (!cs) { *error = 1; return nullptr; }

  double x, y;
  GEOSCoordSeq_getX_r(ctx, cs, 0, &x);
  GEOSCoordSeq_getY_r(ctx, cs, 0, &y);

  auto wkb = write_point_wkb(srid, x, y);
  return return_wkb(initid, wkb, result, length);
}

static void stx_pointonsurface_deinit(UDF_INIT *initid) {
  if (initid->ptr) free(initid->ptr);
}

}  // extern "C" (pause for relate templates)

// ----- stx_relate helpers (outside extern "C") -------------------------------
// Build DE-9IM using basic boost predicates (intersects, within, covered_by,
// touches, crosses, overlaps, equals) instead of bg::relation() which crashes
// in MySQL plugin environment.

// Build DE-9IM matrix string using basic predicates.
// Only supports combinations where boost predicates are implemented.
// Uses get_if dispatch to avoid instantiating unsupported template combinations.

// Point-Point
template <typename PointT>
static std::string relate_pp(const PointT &p1, const PointT &p2) {
  if (bg::equals(p1, p2)) return "0FFFFFFF2";
  return "FF0FFF0F2";
}

// Point-LineString
template <typename PointT, typename LinestringT>
static std::string relate_pl(const PointT &p, const LinestringT &ls) {
  if (!bg::intersects(p, ls)) return "FF0FFF102";
  if (bg::covered_by(p, ls)) {
    if (bg::touches(p, ls)) return "F0FFFF102";
    return "0FFFFF102";
  }
  return "FF0FFF102";
}

// Point-Polygon
template <typename PointT, typename PolygonT>
static std::string relate_pa(const PointT &p, const PolygonT &poly) {
  if (!bg::intersects(p, poly)) return "FF0FFF212";
  if (bg::within(p, poly)) return "0FFFFF212";
  if (bg::covered_by(p, poly)) return "F0FFFF212";
  return "FF0FFF212";
}

// LineString-LineString
template <typename LinestringT>
static std::string relate_ll(const LinestringT &l1, const LinestringT &l2) {
  if (!bg::intersects(l1, l2)) return "FF1FF0102";
  if (bg::equals(l1, l2)) return "1FFF0FFF2";
  if (bg::covered_by(l1, l2)) return "1FF0FF102";
  if (bg::covered_by(l2, l1)) return "10F0FF1F2";
  if (bg::touches(l1, l2)) return "FF10F0102";
  return "0F1FF01F2";
}

// LineString-Polygon
template <typename LinestringT, typename PolygonT>
static std::string relate_la(const LinestringT &ls, const PolygonT &poly) {
  if (!bg::intersects(ls, poly)) return "FF1FF0212";
  if (bg::within(ls, poly)) return "1FF0FF212";
  if (bg::covered_by(ls, poly)) return "1FF00F212";
  if (bg::touches(ls, poly)) return "FF1F0F212";
  return "1010F0212";
}

// Polygon-Polygon
template <typename PolygonT>
static std::string relate_aa(const PolygonT &p1, const PolygonT &p2) {
  if (!bg::intersects(p1, p2)) return "FF2FF1212";
  if (bg::equals(p1, p2)) return "2FFF1FFF2";
  if (bg::within(p1, p2)) return "2FF1FF212";
  if (bg::within(p2, p1)) return "212FF1FF2";
  if (bg::touches(p1, p2)) return "FF2F11212";
  return "212101212";
}

// Dispatch relate for Cartesian types using get_if (no std::visit).
static std::string relate_cartesian(const CartesianVariant &v1,
                                     const CartesianVariant &v2) {
  auto *pt1 = std::get_if<Point>(&v1);
  auto *ls1 = std::get_if<Linestring>(&v1);
  auto *pg1 = std::get_if<Polygon>(&v1);
  auto *pt2 = std::get_if<Point>(&v2);
  auto *ls2 = std::get_if<Linestring>(&v2);
  auto *pg2 = std::get_if<Polygon>(&v2);

  if (pt1 && pt2) return relate_pp(*pt1, *pt2);
  if (pt1 && ls2) return relate_pl(*pt1, *ls2);
  if (pt1 && pg2) return relate_pa(*pt1, *pg2);
  if (ls1 && pt2) { auto r = relate_pl(*pt2, *ls1); /* transpose */ return {r[0],r[3],r[6],r[1],r[4],r[7],r[2],r[5],r[8]}; }
  if (ls1 && ls2) return relate_ll(*ls1, *ls2);
  if (ls1 && pg2) return relate_la(*ls1, *pg2);
  if (pg1 && pt2) { auto r = relate_pa(*pt2, *pg1); return {r[0],r[3],r[6],r[1],r[4],r[7],r[2],r[5],r[8]}; }
  if (pg1 && ls2) { auto r = relate_la(*ls2, *pg1); return {r[0],r[3],r[6],r[1],r[4],r[7],r[2],r[5],r[8]}; }
  if (pg1 && pg2) return relate_aa(*pg1, *pg2);
  return "FFFFFFFFF";
}

static std::string relate_geographic(const GeographicVariant &v1,
                                      const GeographicVariant &v2) {
  auto *pt1 = std::get_if<GeoPoint>(&v1);
  auto *ls1 = std::get_if<GeoLinestring>(&v1);
  auto *pg1 = std::get_if<GeoPolygon>(&v1);
  auto *pt2 = std::get_if<GeoPoint>(&v2);
  auto *ls2 = std::get_if<GeoLinestring>(&v2);
  auto *pg2 = std::get_if<GeoPolygon>(&v2);

  if (pt1 && pt2) return relate_pp(*pt1, *pt2);
  if (pt1 && ls2) return relate_pl(*pt1, *ls2);
  if (pt1 && pg2) return relate_pa(*pt1, *pg2);
  if (ls1 && pt2) { auto r = relate_pl(*pt2, *ls1); return {r[0],r[3],r[6],r[1],r[4],r[7],r[2],r[5],r[8]}; }
  if (ls1 && ls2) return relate_ll(*ls1, *ls2);
  if (ls1 && pg2) return relate_la(*ls1, *pg2);
  if (pg1 && pt2) { auto r = relate_pa(*pt2, *pg1); return {r[0],r[3],r[6],r[1],r[4],r[7],r[2],r[5],r[8]}; }
  if (pg1 && ls2) { auto r = relate_la(*ls2, *pg1); return {r[0],r[3],r[6],r[1],r[4],r[7],r[2],r[5],r[8]}; }
  if (pg1 && pg2) return relate_aa(*pg1, *pg2);
  return "FFFFFFFFF";
}

extern "C" {  // resume extern "C" for closestpoint/relate/etc UDFs

// ----- stx_closestpoint ------------------------------------------------------
// Returns the closest point on g1 to g2 (matches PostGIS ST_ClosestPoint).
// Uses GEOS GEOSNearestPoints() — index 0 is the point on g1.

static bool stx_closestpoint_init(UDF_INIT *initid, UDF_ARGS *args,
                                   char *msg) {
  if (args->arg_count != 2) {
    strcpy(msg, "stx_closestpoint() requires 2 arguments");
    return true;
  }
  args->arg_type[0] = STRING_RESULT;
  args->arg_type[1] = STRING_RESULT;
  initid->maybe_null = 1;
  initid->ptr = nullptr;
  return false;
}

static char *stx_closestpoint(UDF_INIT *initid, UDF_ARGS *args, char *result,
                               unsigned long *length, char *is_null,
                               char *error) {
  if (initid->ptr) { free(initid->ptr); initid->ptr = nullptr; }
  if (!args->args[0] || !args->args[1]) { *is_null = 1; return nullptr; }

  uint32_t srid = extract_srid(args->args[0], args->lengths[0]);

  auto geom1 = mysql_to_geos(args->args[0], args->lengths[0]);
  auto geom2 = mysql_to_geos(args->args[1], args->lengths[1]);
  if (!geom1 || !geom2) { *error = 1; return nullptr; }

  auto ctx = get_geos_context();
  GEOSCoordSequence *cs = GEOSNearestPoints_r(ctx, geom1.get(), geom2.get());
  if (!cs) { *error = 1; return nullptr; }

  double x, y;
  GEOSCoordSeq_getX_r(ctx, cs, 0, &x);
  GEOSCoordSeq_getY_r(ctx, cs, 0, &y);
  GEOSCoordSeq_destroy_r(ctx, cs);

  auto wkb = write_point_wkb(srid, x, y);
  return return_wkb(initid, wkb, result, length);
}

static void stx_closestpoint_deinit(UDF_INIT *initid) {
  if (initid->ptr) free(initid->ptr);
}

// ----- stx_relate ------------------------------------------------------------
// Returns DE-9IM matrix string.

static bool stx_relate_init(UDF_INIT *initid, UDF_ARGS *args, char *msg) {
  if (args->arg_count != 2) {
    strcpy(msg, "stx_relate() requires 2 arguments");
    return true;
  }
  args->arg_type[0] = STRING_RESULT;
  args->arg_type[1] = STRING_RESULT;
  initid->maybe_null = 1;
  initid->max_length = 9;
  set_result_charset_utf8(initid);
  return false;
}

static char *stx_relate(UDF_INIT *initid, UDF_ARGS *args, char *result,
                         unsigned long *length, char *is_null, char *error) {
  if (!args->args[0] || !args->args[1]) { *is_null = 1; return nullptr; }
  auto two = parse_two_geoms(args);
  if (!two) { *error = 1; return nullptr; }

  std::string matrix_str;

  if (auto *c1 = std::get_if<CartesianVariant>(&two->r1.geometry)) {
    auto *c2 = std::get_if<CartesianVariant>(&two->r2.geometry);
    if (!c2) { *error = 1; return nullptr; }
    matrix_str = relate_cartesian(*c1, *c2);
  } else if (auto *g1 = std::get_if<GeographicVariant>(&two->r1.geometry)) {
    auto *g2 = std::get_if<GeographicVariant>(&two->r2.geometry);
    if (!g2) { *error = 1; return nullptr; }
    matrix_str = relate_geographic(*g1, *g2);
  } else {
    *error = 1;
    return nullptr;
  }

  *length = static_cast<unsigned long>(matrix_str.size());
  std::memcpy(result, matrix_str.data(), matrix_str.size());
  return result;
}

static void stx_relate_deinit(UDF_INIT *) {}

// ----- stx_relatematch -------------------------------------------------------

static bool stx_relatematch_init(UDF_INIT *initid, UDF_ARGS *args, char *msg) {
  if (args->arg_count != 3) {
    strcpy(msg,
           "stx_relatematch() requires 3 arguments (geom1, geom2, pattern)");
    return true;
  }
  args->arg_type[0] = STRING_RESULT;
  args->arg_type[1] = STRING_RESULT;
  args->arg_type[2] = STRING_RESULT;
  initid->maybe_null = 1;
  return false;
}

static long long stx_relatematch(UDF_INIT *, UDF_ARGS *args, char *is_null,
                                  char *error) {
  if (!args->args[0] || !args->args[1] || !args->args[2]) {
    *is_null = 1;
    return 0;
  }

  const char *pattern = args->args[2];
  size_t pat_len = args->lengths[2];

  auto r1 = parse_geometry(args->args[0], args->lengths[0]);
  auto r2 = parse_geometry(args->args[1], args->lengths[1]);
  if (!r1 || !r2) { *error = 1; return 0; }

  bool both_cart = std::holds_alternative<CartesianVariant>(r1->geometry) &&
                   std::holds_alternative<CartesianVariant>(r2->geometry);
  bool both_geo = std::holds_alternative<GeographicVariant>(r1->geometry) &&
                  std::holds_alternative<GeographicVariant>(r2->geometry);
  if (!both_cart && !both_geo) { *error = 1; return 0; }

  std::string matrix_str;

  if (both_cart) {
    auto *c1 = std::get_if<CartesianVariant>(&r1->geometry);
    auto *c2 = std::get_if<CartesianVariant>(&r2->geometry);
    matrix_str = relate_cartesian(*c1, *c2);
  } else {
    auto *g1 = std::get_if<GeographicVariant>(&r1->geometry);
    auto *g2 = std::get_if<GeographicVariant>(&r2->geometry);
    matrix_str = relate_geographic(*g1, *g2);
  }

  return de9im_match(matrix_str, pattern, pat_len) ? 1 : 0;
}

static void stx_relatematch_deinit(UDF_INIT *) {}

// ----- stx_makepoint ---------------------------------------------------------
// Creates a POINT geometry from coordinates: stx_makepoint(x, y [, srid])

static bool stx_makepoint_init(UDF_INIT *initid, UDF_ARGS *args, char *msg) {
  if (args->arg_count < 2 || args->arg_count > 3) {
    strcpy(msg, "stx_makepoint() requires 2 or 3 arguments (x, y [, srid])");
    return true;
  }
  args->arg_type[0] = REAL_RESULT;
  args->arg_type[1] = REAL_RESULT;
  if (args->arg_count == 3) args->arg_type[2] = INT_RESULT;
  initid->maybe_null = 1;
  initid->max_length = 25;
  return false;
}

static char *stx_makepoint(UDF_INIT *, UDF_ARGS *args, char *result,
                            unsigned long *length, char *is_null,
                            char *error) {
  if (!args->args[0] || !args->args[1]) { *is_null = 1; return nullptr; }
  if (args->arg_count == 3 && !args->args[2]) { *is_null = 1; return nullptr; }
  double arg1 = *reinterpret_cast<double *>(args->args[0]);
  double arg2 = *reinterpret_cast<double *>(args->args[1]);
  uint32_t srid = 0;
  if (args->arg_count == 3)
    srid = static_cast<uint32_t>(*reinterpret_cast<long long *>(args->args[2]));

  // Range validation for geographic SRIDs (lat/lon bounds).
  if (is_geographic_srid(srid)) {
    // Determine which arg is lat and which is lon based on axis order.
    double lat = needs_axis_swap(srid) ? arg1 : arg2;
    double lon = needs_axis_swap(srid) ? arg2 : arg1;
    if (lat < -90.0 || lat > 90.0) {
      my_error(ER_LATITUDE_OUT_OF_RANGE, MYF(0), lat, "stx_makepoint",
               -90.0, 90.0);
      *error = 1; return nullptr;
    }
    if (lon < -180.0 || lon > 180.0) {
      my_error(ER_LONGITUDE_OUT_OF_RANGE, MYF(0), lon, "stx_makepoint",
               -180.0, 180.0);
      *error = 1; return nullptr;
    }
  }

  // Swap coordinates when first axis is NORTH (lat/northing first in SRS).
  // Internal WKB always stores (x=easting/lon, y=northing/lat).
  double wkb_x = needs_axis_swap(srid) ? arg2 : arg1;
  double wkb_y = needs_axis_swap(srid) ? arg1 : arg2;

  auto wkb = write_point_wkb(srid, wkb_x, wkb_y);
  *length = static_cast<unsigned long>(wkb.size());
  std::memcpy(result, wkb.data(), wkb.size());
  return result;
}

static void stx_makepoint_deinit(UDF_INIT *) {}

// ----- stx_affine ------------------------------------------------------------
// General 2D affine transformation:
//   x' = a*x + b*y + xoff
//   y' = d*x + e*y + yoff
// stx_affine(geom, a, b, d, e, xoff, yoff)

static bool stx_affine_init(UDF_INIT *initid, UDF_ARGS *args, char *msg) {
  if (args->arg_count != 7) {
    strcpy(msg,
           "stx_affine() requires 7 arguments (geom, a, b, d, e, xoff, yoff)");
    return true;
  }
  args->arg_type[0] = STRING_RESULT;
  for (unsigned i = 1; i <= 6; ++i) args->arg_type[i] = REAL_RESULT;
  initid->maybe_null = 1;
  initid->ptr = nullptr;
  return false;
}

static char *stx_affine(UDF_INIT *initid, UDF_ARGS *args, char *result,
                          unsigned long *length, char *is_null, char *error) {
  if (initid->ptr) { free(initid->ptr); initid->ptr = nullptr; }
  for (unsigned i = 0; i < 7; ++i) {
    if (!args->args[i]) { *is_null = 1; return nullptr; }
  }
  auto r = parse_geometry(args->args[0], args->lengths[0]);
  if (!r) { *error = 1; return nullptr; }
  double a    = *reinterpret_cast<double *>(args->args[1]);
  double b    = *reinterpret_cast<double *>(args->args[2]);
  double d    = *reinterpret_cast<double *>(args->args[3]);
  double e    = *reinterpret_cast<double *>(args->args[4]);
  double xoff = *reinterpret_cast<double *>(args->args[5]);
  double yoff = *reinterpret_cast<double *>(args->args[6]);

  auto wkb =
      transform_and_write(*r, [a, b, d, e, xoff, yoff](double &x, double &y) {
        double nx = a * x + b * y + xoff;
        double ny = d * x + e * y + yoff;
        x = nx;
        y = ny;
      });
  if (wkb.empty()) { *error = 1; return nullptr; }
  return return_wkb(initid, wkb, result, length);
}

static void stx_affine_deinit(UDF_INIT *initid) {
  if (initid->ptr) free(initid->ptr);
}

// ----- stx_snaptogrid --------------------------------------------------------
// Snaps coordinates to a grid: stx_snaptogrid(geom, size)
//   or stx_snaptogrid(geom, size_x, size_y)

static bool stx_snaptogrid_init(UDF_INIT *initid, UDF_ARGS *args, char *msg) {
  if (args->arg_count < 2 || args->arg_count > 3) {
    strcpy(msg,
           "stx_snaptogrid() requires 2 or 3 arguments (geom, size [, size_y])");
    return true;
  }
  args->arg_type[0] = STRING_RESULT;
  args->arg_type[1] = REAL_RESULT;
  if (args->arg_count == 3) args->arg_type[2] = REAL_RESULT;
  initid->maybe_null = 1;
  initid->ptr = nullptr;
  return false;
}

static char *stx_snaptogrid(UDF_INIT *initid, UDF_ARGS *args, char *result,
                              unsigned long *length, char *is_null,
                              char *error) {
  if (initid->ptr) { free(initid->ptr); initid->ptr = nullptr; }
  if (!args->args[0] || !args->args[1]) { *is_null = 1; return nullptr; }
  if (args->arg_count == 3 && !args->args[2]) { *is_null = 1; return nullptr; }

  auto r = parse_geometry(args->args[0], args->lengths[0]);
  if (!r) { *error = 1; return nullptr; }
  double sx = *reinterpret_cast<double *>(args->args[1]);
  double sy = (args->arg_count == 3)
                  ? *reinterpret_cast<double *>(args->args[2])
                  : sx;

  auto wkb = transform_and_write(*r, [sx, sy](double &x, double &y) {
    if (sx != 0.0) x = std::round(x / sx) * sx;
    if (sy != 0.0) y = std::round(y / sy) * sy;
  });
  if (wkb.empty()) { *error = 1; return nullptr; }
  return return_wkb(initid, wkb, result, length);
}

static void stx_snaptogrid_deinit(UDF_INIT *initid) {
  if (initid->ptr) free(initid->ptr);
}

}  // extern "C" (pause for removerepeatedpoints/segmentize/generatepoints templates)

// ----- stx_removerepeatedpoints helpers --------------------------------------

// Remove consecutive points within tolerance distance from a range.
template <typename Container>
static void remove_repeated_with_tolerance(Container &pts, double tol2,
                                           size_t min_keep) {
  if (pts.size() <= min_keep) return;
  Container out;
  out.push_back(pts[0]);
  for (size_t i = 1; i < pts.size(); ++i) {
    double dx = bg::get<0>(pts[i]) - bg::get<0>(out.back());
    double dy = bg::get<1>(pts[i]) - bg::get<1>(out.back());
    if (dx * dx + dy * dy > tol2) {
      out.push_back(pts[i]);
    }
  }
  // For rings: always keep closing point identical to first
  if (out.size() >= 2 && min_keep >= 4) {
    auto &first = out.front();
    auto &last = out.back();
    if (bg::get<0>(first) != bg::get<0>(last) ||
        bg::get<1>(first) != bg::get<1>(last)) {
      out.push_back(first);
    }
  }
  if (out.size() < min_keep) return;  // don't reduce below minimum
  pts = std::move(out);
}

// Apply remove-repeated to a full geometry (tolerance version).
template <typename Variant>
static void remove_repeated_variant(Variant &v, double tol2) {
  std::visit(
      [tol2](auto &g) {
        using T = std::decay_t<decltype(g)>;
        if constexpr (std::is_same_v<T, Linestring> ||
                      std::is_same_v<T, GeoLinestring>) {
          remove_repeated_with_tolerance(g, tol2, 2);
        } else if constexpr (std::is_same_v<T, Polygon> ||
                              std::is_same_v<T, GeoPolygon>) {
          remove_repeated_with_tolerance(g.outer(), tol2, 4);
          for (auto &ring : g.inners())
            remove_repeated_with_tolerance(ring, tol2, 4);
        } else if constexpr (std::is_same_v<T, MultiLinestring> ||
                              std::is_same_v<T, GeoMultiLinestring>) {
          for (auto &ls : g)
            remove_repeated_with_tolerance(ls, tol2, 2);
        } else if constexpr (std::is_same_v<T, MultiPolygon> ||
                              std::is_same_v<T, GeoMultiPolygon>) {
          for (auto &poly : g) {
            remove_repeated_with_tolerance(poly.outer(), tol2, 4);
            for (auto &ring : poly.inners())
              remove_repeated_with_tolerance(ring, tol2, 4);
          }
        }
        // MultiPoint: remove duplicate points (unordered, so any duplicate)
        else if constexpr (std::is_same_v<T, MultiPoint> ||
                            std::is_same_v<T, GeoMultiPoint>) {
          using PtType = typename T::value_type;
          T out;
          for (size_t i = 0; i < g.size(); ++i) {
            bool dup = false;
            for (size_t j = 0; j < out.size(); ++j) {
              double dx = bg::get<0>(g[i]) - bg::get<0>(out[j]);
              double dy = bg::get<1>(g[i]) - bg::get<1>(out[j]);
              if (dx * dx + dy * dy <= tol2) {
                dup = true;
                break;
              }
            }
            if (!dup) out.push_back(g[i]);
          }
          if (!out.empty()) g = std::move(out);
        }
        // Point: single point, nothing to remove
      },
      v);
}

// Densify (segmentize) helper: apply bg::densify per geometry type.
template <typename Variant>
static std::string densify_and_write(uint32_t srid, const Variant &v,
                                     double max_length) {
  return std::visit(
      [srid, max_length](const auto &g) -> std::string {
        using T = std::decay_t<decltype(g)>;
        // Point types: no edges to densify, return as-is
        if constexpr (std::is_same_v<T, Point> || std::is_same_v<T, GeoPoint> ||
                      std::is_same_v<T, MultiPoint> ||
                      std::is_same_v<T, GeoMultiPoint>) {
          return write_geometry(srid, Variant(g));
        } else {
          T out;
          bg::densify(g, out, max_length);
          return write_geometry(srid, Variant(out));
        }
      },
      v);
}

// Generate random points inside a polygon using rejection sampling.
template <typename PointT, typename PolygonT, typename MultiPointT>
static MultiPointT generate_points_impl(const PolygonT &poly, int npoints,
                                        std::mt19937 &rng) {
  using BoxT = bg::model::box<PointT>;
  BoxT box;
  bg::envelope(poly, box);
  double minx = bg::get<bg::min_corner, 0>(box);
  double miny = bg::get<bg::min_corner, 1>(box);
  double maxx = bg::get<bg::max_corner, 0>(box);
  double maxy = bg::get<bg::max_corner, 1>(box);

  std::uniform_real_distribution<double> dist_x(minx, maxx);
  std::uniform_real_distribution<double> dist_y(miny, maxy);

  MultiPointT result;
  int max_attempts = npoints * 1000;
  int attempts = 0;
  int found = 0;
  while (found < npoints && attempts < max_attempts) {
    PointT pt;
    bg::set<0>(pt, dist_x(rng));
    bg::set<1>(pt, dist_y(rng));
    if (bg::within(pt, poly)) {
      result.push_back(pt);
      ++found;
    }
    ++attempts;
  }
  return result;
}

extern "C" {  // resume extern "C"

// ----- stx_removerepeatedpoints ----------------------------------------------
// Removes consecutive duplicate vertices.
// stx_removerepeatedpoints(geom) or stx_removerepeatedpoints(geom, tolerance)

static bool stx_removerepeatedpoints_init(UDF_INIT *initid, UDF_ARGS *args,
                                           char *msg) {
  if (args->arg_count < 1 || args->arg_count > 2) {
    strcpy(msg,
           "stx_removerepeatedpoints() requires 1 or 2 arguments "
           "(geom [, tolerance])");
    return true;
  }
  args->arg_type[0] = STRING_RESULT;
  if (args->arg_count == 2) args->arg_type[1] = REAL_RESULT;
  initid->maybe_null = 1;
  initid->ptr = nullptr;
  return false;
}

static char *stx_removerepeatedpoints(UDF_INIT *initid, UDF_ARGS *args,
                                       char *result, unsigned long *length,
                                       char *is_null, char *error) {
  if (initid->ptr) { free(initid->ptr); initid->ptr = nullptr; }
  if (!args->args[0]) { *is_null = 1; return nullptr; }
  if (args->arg_count == 2 && !args->args[1]) { *is_null = 1; return nullptr; }

  auto r = parse_geometry(args->args[0], args->lengths[0]);
  if (!r) { *error = 1; return nullptr; }

  double tolerance = 0.0;
  if (args->arg_count == 2)
    tolerance = *reinterpret_cast<double *>(args->args[1]);

  std::string wkb;
  if (tolerance <= 0.0) {
    // Exact duplicate removal: use bg::unique, plus set-based dedup for MultiPoint
    if (auto *cv = std::get_if<CartesianVariant>(&r->geometry)) {
      std::visit([](auto &g) { bg::unique(g); }, *cv);
      remove_repeated_variant(*cv, 0.0);
      wkb = write_geometry(r->srid, *cv);
    } else if (auto *gv = std::get_if<GeographicVariant>(&r->geometry)) {
      std::visit([](auto &g) { bg::unique(g); }, *gv);
      remove_repeated_variant(*gv, 0.0);
      wkb = write_geometry(r->srid, *gv);
    }
  } else {
    // Tolerance-based removal
    double tol2 = tolerance * tolerance;
    if (auto *cv = std::get_if<CartesianVariant>(&r->geometry)) {
      remove_repeated_variant(*cv, tol2);
      wkb = write_geometry(r->srid, *cv);
    } else if (auto *gv = std::get_if<GeographicVariant>(&r->geometry)) {
      remove_repeated_variant(*gv, tol2);
      wkb = write_geometry(r->srid, *gv);
    }
  }

  if (wkb.empty()) { *error = 1; return nullptr; }
  return return_wkb(initid, wkb, result, length);
}

static void stx_removerepeatedpoints_deinit(UDF_INIT *initid) {
  if (initid->ptr) free(initid->ptr);
}

// ----- stx_segmentize --------------------------------------------------------
// Densifies geometry by adding points so no segment exceeds max_length.
// stx_segmentize(geom, max_length)

static bool stx_segmentize_init(UDF_INIT *initid, UDF_ARGS *args, char *msg) {
  if (args->arg_count != 2) {
    strcpy(msg, "stx_segmentize() requires 2 arguments (geom, max_length)");
    return true;
  }
  args->arg_type[0] = STRING_RESULT;
  args->arg_type[1] = REAL_RESULT;
  initid->maybe_null = 1;
  initid->ptr = nullptr;
  return false;
}

static char *stx_segmentize(UDF_INIT *initid, UDF_ARGS *args, char *result,
                              unsigned long *length, char *is_null,
                              char *error) {
  if (initid->ptr) { free(initid->ptr); initid->ptr = nullptr; }
  if (!args->args[0] || !args->args[1]) { *is_null = 1; return nullptr; }

  auto r = parse_geometry(args->args[0], args->lengths[0]);
  if (!r) { *error = 1; return nullptr; }
  double max_len = *reinterpret_cast<double *>(args->args[1]);
  if (max_len <= 0.0) { *error = 1; return nullptr; }

  std::string wkb;
  if (auto *cv = std::get_if<CartesianVariant>(&r->geometry)) {
    wkb = densify_and_write(r->srid, *cv, max_len);
  } else if (auto *gv = std::get_if<GeographicVariant>(&r->geometry)) {
    wkb = densify_and_write(r->srid, *gv, max_len);
  }
  if (wkb.empty()) { *error = 1; return nullptr; }
  return return_wkb(initid, wkb, result, length);
}

static void stx_segmentize_deinit(UDF_INIT *initid) {
  if (initid->ptr) free(initid->ptr);
}

// ----- stx_generatepoints ----------------------------------------------------
// Generates random points inside a polygon.
// stx_generatepoints(geom, npoints [, seed])

static bool stx_generatepoints_init(UDF_INIT *initid, UDF_ARGS *args,
                                     char *msg) {
  if (args->arg_count < 2 || args->arg_count > 3) {
    strcpy(msg,
           "stx_generatepoints() requires 2 or 3 arguments "
           "(geom, npoints [, seed])");
    return true;
  }
  args->arg_type[0] = STRING_RESULT;
  args->arg_type[1] = INT_RESULT;
  if (args->arg_count == 3) args->arg_type[2] = INT_RESULT;
  initid->maybe_null = 1;
  initid->ptr = nullptr;
  return false;
}

static char *stx_generatepoints(UDF_INIT *initid, UDF_ARGS *args, char *result,
                                  unsigned long *length, char *is_null,
                                  char *error) {
  if (initid->ptr) { free(initid->ptr); initid->ptr = nullptr; }
  if (!args->args[0] || !args->args[1]) { *is_null = 1; return nullptr; }
  if (args->arg_count == 3 && !args->args[2]) { *is_null = 1; return nullptr; }

  auto r = parse_geometry(args->args[0], args->lengths[0]);
  if (!r) { *error = 1; return nullptr; }
  int npoints = static_cast<int>(*reinterpret_cast<long long *>(args->args[1]));
  if (npoints <= 0) { *error = 1; return nullptr; }

  // Seed: use arg[2] if provided, otherwise random_device
  std::mt19937 rng;
  if (args->arg_count == 3) {
    auto seed =
        static_cast<uint32_t>(*reinterpret_cast<long long *>(args->args[2]));
    rng.seed(seed);
  } else {
    std::random_device rd;
    rng.seed(rd());
  }

  std::string wkb;
  if (auto *cv = std::get_if<CartesianVariant>(&r->geometry)) {
    if (auto *poly = std::get_if<Polygon>(cv)) {
      auto mp = generate_points_impl<Point, Polygon, MultiPoint>(*poly,
                                                                  npoints, rng);
      wkb = write_multi_point(r->srid, mp);
    } else if (auto *mpoly = std::get_if<MultiPolygon>(cv)) {
      // Distribute points by area ratio
      MultiPoint all_pts;
      double total_area = 0.0;
      std::vector<double> areas;
      for (const auto &p : *mpoly) {
        double a = std::abs(bg::area(p));
        areas.push_back(a);
        total_area += a;
      }
      if (total_area <= 0.0) { *error = 1; return nullptr; }
      int assigned = 0;
      for (size_t i = 0; i < mpoly->size(); ++i) {
        int n = (i == mpoly->size() - 1)
                    ? (npoints - assigned)
                    : static_cast<int>(std::round(npoints * areas[i] / total_area));
        if (n > 0) {
          auto mp = generate_points_impl<Point, Polygon, MultiPoint>(
              (*mpoly)[i], n, rng);
          all_pts.insert(all_pts.end(), mp.begin(), mp.end());
          assigned += n;
        }
      }
      wkb = write_multi_point(r->srid, all_pts);
    } else {
      *error = 1;
      return nullptr;
    }
  } else if (auto *gv = std::get_if<GeographicVariant>(&r->geometry)) {
    if (auto *poly = std::get_if<GeoPolygon>(gv)) {
      auto mp =
          generate_points_impl<GeoPoint, GeoPolygon, GeoMultiPoint>(*poly,
                                                                     npoints, rng);
      wkb = write_multi_point(r->srid, mp);
    } else if (auto *mpoly = std::get_if<GeoMultiPolygon>(gv)) {
      GeoMultiPoint all_pts;
      double total_area = 0.0;
      std::vector<double> areas;
      for (const auto &p : *mpoly) {
        double a = std::abs(bg::area(p));
        areas.push_back(a);
        total_area += a;
      }
      if (total_area <= 0.0) { *error = 1; return nullptr; }
      int assigned = 0;
      for (size_t i = 0; i < mpoly->size(); ++i) {
        int n = (i == mpoly->size() - 1)
                    ? (npoints - assigned)
                    : static_cast<int>(std::round(npoints * areas[i] / total_area));
        if (n > 0) {
          auto mp =
              generate_points_impl<GeoPoint, GeoPolygon, GeoMultiPoint>(
                  (*mpoly)[i], n, rng);
          all_pts.insert(all_pts.end(), mp.begin(), mp.end());
          assigned += n;
        }
      }
      wkb = write_multi_point(r->srid, all_pts);
    } else {
      *error = 1;
      return nullptr;
    }
  }

  if (wkb.empty()) { *error = 1; return nullptr; }
  return return_wkb(initid, wkb, result, length);
}

static void stx_generatepoints_deinit(UDF_INIT *initid) {
  if (initid->ptr) free(initid->ptr);
}

}  // extern "C" (pause for MBC/grid helpers)

// ===== MinimumBoundingCircle helpers =========================================

struct MBCircle {
  double cx, cy, r;
};

static MBCircle mbc_from_one(double x, double y) { return {x, y, 0.0}; }

static MBCircle mbc_from_two(double x1, double y1, double x2, double y2) {
  return {(x1 + x2) / 2.0, (y1 + y2) / 2.0,
          std::hypot(x2 - x1, y2 - y1) / 2.0};
}

static MBCircle mbc_from_three(double x1, double y1, double x2, double y2,
                                double x3, double y3) {
  double D = 2.0 * (x1 * (y2 - y3) + x2 * (y3 - y1) + x3 * (y1 - y2));
  if (std::abs(D) < 1e-14) {
    // Collinear: use the two farthest points
    double d12 = std::hypot(x2 - x1, y2 - y1);
    double d23 = std::hypot(x3 - x2, y3 - y2);
    double d13 = std::hypot(x3 - x1, y3 - y1);
    if (d12 >= d23 && d12 >= d13) return mbc_from_two(x1, y1, x2, y2);
    if (d23 >= d13) return mbc_from_two(x2, y2, x3, y3);
    return mbc_from_two(x1, y1, x3, y3);
  }
  double a = x1 * x1 + y1 * y1;
  double b = x2 * x2 + y2 * y2;
  double c = x3 * x3 + y3 * y3;
  double ux = (a * (y2 - y3) + b * (y3 - y1) + c * (y1 - y2)) / D;
  double uy = (a * (x3 - x2) + b * (x1 - x3) + c * (x2 - x1)) / D;
  return {ux, uy, std::hypot(x1 - ux, y1 - uy)};
}

static bool mbc_contains(const MBCircle &c, double px, double py) {
  double dx = px - c.cx, dy = py - c.cy;
  return dx * dx + dy * dy <= (c.r + 1e-10) * (c.r + 1e-10);
}

// Welzl's algorithm (iterative, expected O(n)).
static MBCircle welzl(std::vector<std::pair<double, double>> &pts) {
  std::mt19937 rng(42);
  std::shuffle(pts.begin(), pts.end(), rng);
  size_t n = pts.size();
  if (n == 0) return {0, 0, 0};
  if (n == 1) return mbc_from_one(pts[0].first, pts[0].second);

  MBCircle c = mbc_from_two(pts[0].first, pts[0].second,
                             pts[1].first, pts[1].second);
  for (size_t i = 2; i < n; ++i) {
    if (!mbc_contains(c, pts[i].first, pts[i].second)) {
      c = mbc_from_two(pts[0].first, pts[0].second,
                        pts[i].first, pts[i].second);
      for (size_t j = 1; j < i; ++j) {
        if (!mbc_contains(c, pts[j].first, pts[j].second)) {
          c = mbc_from_two(pts[j].first, pts[j].second,
                            pts[i].first, pts[i].second);
          for (size_t k = 0; k < j; ++k) {
            if (!mbc_contains(c, pts[k].first, pts[k].second)) {
              c = mbc_from_three(pts[k].first, pts[k].second,
                                  pts[j].first, pts[j].second,
                                  pts[i].first, pts[i].second);
            }
          }
        }
      }
    }
  }
  return c;
}

// Collect all coordinates from a variant geometry.
template <typename Variant>
static void collect_points(const Variant &geom,
                            std::vector<std::pair<double, double>> &pts) {
  std::visit(
      [&pts](const auto &g) {
        using T = std::decay_t<decltype(g)>;
        if constexpr (std::is_same_v<T, Point> ||
                      std::is_same_v<T, GeoPoint>) {
          pts.emplace_back(bg::get<0>(g), bg::get<1>(g));
        } else if constexpr (std::is_same_v<T, Linestring> ||
                              std::is_same_v<T, GeoLinestring>) {
          for (const auto &pt : g)
            pts.emplace_back(bg::get<0>(pt), bg::get<1>(pt));
        } else if constexpr (std::is_same_v<T, Polygon> ||
                              std::is_same_v<T, GeoPolygon>) {
          for (const auto &pt : g.outer())
            pts.emplace_back(bg::get<0>(pt), bg::get<1>(pt));
          for (const auto &inner : g.inners())
            for (const auto &pt : inner)
              pts.emplace_back(bg::get<0>(pt), bg::get<1>(pt));
        } else if constexpr (std::is_same_v<T, MultiPoint> ||
                              std::is_same_v<T, GeoMultiPoint>) {
          for (const auto &pt : g)
            pts.emplace_back(bg::get<0>(pt), bg::get<1>(pt));
        } else if constexpr (std::is_same_v<T, MultiLinestring> ||
                              std::is_same_v<T, GeoMultiLinestring>) {
          for (const auto &ls : g)
            for (const auto &pt : ls)
              pts.emplace_back(bg::get<0>(pt), bg::get<1>(pt));
        } else if constexpr (std::is_same_v<T, MultiPolygon> ||
                              std::is_same_v<T, GeoMultiPolygon>) {
          for (const auto &poly : g) {
            for (const auto &pt : poly.outer())
              pts.emplace_back(bg::get<0>(pt), bg::get<1>(pt));
            for (const auto &inner : poly.inners())
              for (const auto &pt : inner)
                pts.emplace_back(bg::get<0>(pt), bg::get<1>(pt));
          }
        }
      },
      geom);
}

// Build a circle polygon approximation.
template <typename PointT, typename PolygonT>
static PolygonT make_circle_polygon(double cx, double cy, double r,
                                     int num_segs) {
  PolygonT poly;
  for (int i = 0; i <= num_segs; ++i) {
    double angle = 2.0 * M_PI * i / num_segs;
    PointT pt;
    bg::set<0>(pt, cx + r * std::cos(angle));
    bg::set<1>(pt, cy + r * std::sin(angle));
    poly.outer().push_back(pt);
  }
  return poly;
}

// ===== Grid helpers ==========================================================

// Write a GeometryCollection of Polygons as MySQL internal format (SRID + WKB).
template <typename PolygonT>
static std::string write_gc_polygons(uint32_t srid,
                                      const std::vector<PolygonT> &polys) {
  using namespace gis_lib::detail;
  std::string buf;
  append_uint32(buf, srid);
  append_wkb_header(buf, gis_lib::GeometryType::GeometryCollection);
  append_uint32(buf, static_cast<uint32_t>(polys.size()));
  for (const auto &poly : polys) {
    append_wkb_header(buf, gis_lib::GeometryType::Polygon);
    uint32_t num_rings = 1 + static_cast<uint32_t>(poly.inners().size());
    append_uint32(buf, num_rings);
    append_ring(buf, poly.outer());
    for (const auto &inner : poly.inners()) append_ring(buf, inner);
  }
  return buf;
}

// Get bounding box from a variant geometry.
template <typename PointT, typename Variant>
static void get_bbox(const Variant &geom, double &minx, double &miny,
                      double &maxx, double &maxy) {
  using BoxT = bg::model::box<PointT>;
  BoxT box;
  std::visit([&box](const auto &g) { bg::envelope(g, box); }, geom);
  minx = bg::get<bg::min_corner, 0>(box);
  miny = bg::get<bg::min_corner, 1>(box);
  maxx = bg::get<bg::max_corner, 0>(box);
  maxy = bg::get<bg::max_corner, 1>(box);
}

static constexpr int MAX_GRID_CELLS = 1000000;

// Generate square grid cells covering a bounding box (snapped to origin).
template <typename PointT, typename PolygonT>
static std::vector<PolygonT> make_square_grid(double size, double minx,
                                               double miny, double maxx,
                                               double maxy) {
  std::vector<PolygonT> cells;
  double x0 = std::floor(minx / size) * size;
  double y0 = std::floor(miny / size) * size;
  for (double y = y0; y < maxy; y += size) {
    for (double x = x0; x < maxx; x += size) {
      if (static_cast<int>(cells.size()) >= MAX_GRID_CELLS) return cells;
      PolygonT poly;
      double x2 = x + size, y2 = y + size;
      PointT p;
      bg::set<0>(p, x);  bg::set<1>(p, y);  poly.outer().push_back(p);
      bg::set<0>(p, x2); bg::set<1>(p, y);  poly.outer().push_back(p);
      bg::set<0>(p, x2); bg::set<1>(p, y2); poly.outer().push_back(p);
      bg::set<0>(p, x);  bg::set<1>(p, y2); poly.outer().push_back(p);
      bg::set<0>(p, x);  bg::set<1>(p, y);  poly.outer().push_back(p);
      cells.push_back(std::move(poly));
    }
  }
  return cells;
}

// Generate flat-top hexagonal grid cells (snapped to origin).
// size = edge length of hexagon.
template <typename PointT, typename PolygonT>
static std::vector<PolygonT> make_hex_grid(double size, double minx,
                                            double miny, double maxx,
                                            double maxy) {
  std::vector<PolygonT> cells;
  double dx = 1.5 * size;                // column spacing
  double dy = size * std::sqrt(3.0);     // row spacing

  // Build bbox polygon for precise intersection filtering
  PolygonT bbox;
  PointT bp;
  bg::set<0>(bp, minx); bg::set<1>(bp, miny); bbox.outer().push_back(bp);
  bg::set<0>(bp, maxx); bg::set<1>(bp, miny); bbox.outer().push_back(bp);
  bg::set<0>(bp, maxx); bg::set<1>(bp, maxy); bbox.outer().push_back(bp);
  bg::set<0>(bp, minx); bg::set<1>(bp, maxy); bbox.outer().push_back(bp);
  bg::set<0>(bp, minx); bg::set<1>(bp, miny); bbox.outer().push_back(bp);

  int col_start = static_cast<int>(std::floor(minx / dx));
  int col_end = static_cast<int>(std::ceil(maxx / dx));

  for (int col = col_start; col <= col_end; ++col) {
    double cx = col * dx;
    if (cx + size < minx || cx - size > maxx) continue;

    double y_off = (std::abs(col) % 2 == 1) ? dy / 2.0 : 0.0;
    int row_start = static_cast<int>(std::floor((miny - y_off) / dy));
    int row_end = static_cast<int>(std::ceil((maxy - y_off) / dy));

    for (int row = row_start; row <= row_end; ++row) {
      double cy = row * dy + y_off;
      double half_h = size * std::sqrt(3.0) / 2.0;
      if (cy + half_h < miny || cy - half_h > maxy) continue;
      if (static_cast<int>(cells.size()) >= MAX_GRID_CELLS) return cells;

      PolygonT poly;
      for (int i = 0; i <= 6; ++i) {
        double angle = M_PI / 3.0 * (i % 6);
        PointT pt;
        bg::set<0>(pt, cx + size * std::cos(angle));
        bg::set<1>(pt, cy + size * std::sin(angle));
        poly.outer().push_back(pt);
      }
      if (!bg::intersects(poly, bbox)) continue;
      cells.push_back(std::move(poly));
    }
  }
  return cells;
}

extern "C" {  // resume extern "C" for MBC/grid UDFs

// ----- stx_minimumboundingcircle ---------------------------------------------
// Returns minimum bounding circle as a Polygon.
// stx_minimumboundingcircle(geom [, segs_per_quarter])

static bool stx_minimumboundingcircle_init(UDF_INIT *initid, UDF_ARGS *args,
                                            char *msg) {
  if (args->arg_count < 1 || args->arg_count > 2) {
    strcpy(msg,
           "stx_minimumboundingcircle() requires 1 or 2 arguments "
           "(geom [, segs_per_quarter])");
    return true;
  }
  args->arg_type[0] = STRING_RESULT;
  if (args->arg_count == 2) args->arg_type[1] = INT_RESULT;
  initid->maybe_null = 1;
  initid->ptr = nullptr;
  return false;
}

static char *stx_minimumboundingcircle(UDF_INIT *initid, UDF_ARGS *args,
                                        char *result, unsigned long *length,
                                        char *is_null, char *error) {
  if (initid->ptr) { free(initid->ptr); initid->ptr = nullptr; }
  if (!args->args[0]) { *is_null = 1; return nullptr; }
  if (args->arg_count == 2 && !args->args[1]) { *is_null = 1; return nullptr; }

  auto r = parse_geometry(args->args[0], args->lengths[0]);
  if (!r) { *error = 1; return nullptr; }

  int segs_per_quarter = 48;
  if (args->arg_count == 2) {
    segs_per_quarter =
        static_cast<int>(*reinterpret_cast<long long *>(args->args[1]));
    if (segs_per_quarter < 1) segs_per_quarter = 1;
  }
  int num_segs = segs_per_quarter * 4;

  std::vector<std::pair<double, double>> pts;
  if (auto *cv = std::get_if<CartesianVariant>(&r->geometry))
    collect_points(*cv, pts);
  else if (auto *gv = std::get_if<GeographicVariant>(&r->geometry))
    collect_points(*gv, pts);

  if (pts.empty()) { *error = 1; return nullptr; }

  MBCircle c = welzl(pts);

  std::string wkb;
  if (std::holds_alternative<CartesianVariant>(r->geometry)) {
    auto poly = make_circle_polygon<Point, Polygon>(c.cx, c.cy, c.r, num_segs);
    wkb = write_polygon(r->srid, poly);
  } else {
    auto poly =
        make_circle_polygon<GeoPoint, GeoPolygon>(c.cx, c.cy, c.r, num_segs);
    wkb = write_polygon(r->srid, poly);
  }

  if (wkb.empty()) { *error = 1; return nullptr; }
  return return_wkb(initid, wkb, result, length);
}

static void stx_minimumboundingcircle_deinit(UDF_INIT *initid) {
  if (initid->ptr) free(initid->ptr);
}

// ----- stx_squaregrid --------------------------------------------------------
// Generates a square grid covering the bounding box of a geometry.
// stx_squaregrid(size, geom)

static bool stx_squaregrid_init(UDF_INIT *initid, UDF_ARGS *args, char *msg) {
  if (args->arg_count != 2) {
    strcpy(msg, "stx_squaregrid() requires 2 arguments (size, geom)");
    return true;
  }
  args->arg_type[0] = REAL_RESULT;
  args->arg_type[1] = STRING_RESULT;
  initid->maybe_null = 1;
  initid->ptr = nullptr;
  return false;
}

static char *stx_squaregrid(UDF_INIT *initid, UDF_ARGS *args, char *result,
                              unsigned long *length, char *is_null,
                              char *error) {
  if (initid->ptr) { free(initid->ptr); initid->ptr = nullptr; }
  if (!args->args[0] || !args->args[1]) { *is_null = 1; return nullptr; }

  double size = *reinterpret_cast<double *>(args->args[0]);
  if (size <= 0.0) { *error = 1; return nullptr; }

  auto r = parse_geometry(args->args[1], args->lengths[1]);
  if (!r) { *error = 1; return nullptr; }

  double minx, miny, maxx, maxy;
  std::string wkb;
  if (auto *cv = std::get_if<CartesianVariant>(&r->geometry)) {
    get_bbox<Point>(*cv, minx, miny, maxx, maxy);
    auto cells = make_square_grid<Point, Polygon>(size, minx, miny, maxx, maxy);
    wkb = write_gc_polygons(r->srid, cells);
  } else if (auto *gv = std::get_if<GeographicVariant>(&r->geometry)) {
    get_bbox<GeoPoint>(*gv, minx, miny, maxx, maxy);
    auto cells =
        make_square_grid<GeoPoint, GeoPolygon>(size, minx, miny, maxx, maxy);
    wkb = write_gc_polygons(r->srid, cells);
  }

  if (wkb.empty()) { *error = 1; return nullptr; }
  return return_wkb(initid, wkb, result, length);
}

static void stx_squaregrid_deinit(UDF_INIT *initid) {
  if (initid->ptr) free(initid->ptr);
}

// ----- stx_hexgrid -----------------------------------------------------------
// Generates a flat-top hexagonal grid covering the bounding box of a geometry.
// stx_hexgrid(size, geom)

static bool stx_hexgrid_init(UDF_INIT *initid, UDF_ARGS *args, char *msg) {
  if (args->arg_count != 2) {
    strcpy(msg, "stx_hexgrid() requires 2 arguments (size, geom)");
    return true;
  }
  args->arg_type[0] = REAL_RESULT;
  args->arg_type[1] = STRING_RESULT;
  initid->maybe_null = 1;
  initid->ptr = nullptr;
  return false;
}

static char *stx_hexgrid(UDF_INIT *initid, UDF_ARGS *args, char *result,
                            unsigned long *length, char *is_null,
                            char *error) {
  if (initid->ptr) { free(initid->ptr); initid->ptr = nullptr; }
  if (!args->args[0] || !args->args[1]) { *is_null = 1; return nullptr; }

  double size = *reinterpret_cast<double *>(args->args[0]);
  if (size <= 0.0) { *error = 1; return nullptr; }

  auto r = parse_geometry(args->args[1], args->lengths[1]);
  if (!r) { *error = 1; return nullptr; }

  double minx, miny, maxx, maxy;
  std::string wkb;
  if (auto *cv = std::get_if<CartesianVariant>(&r->geometry)) {
    get_bbox<Point>(*cv, minx, miny, maxx, maxy);
    auto cells = make_hex_grid<Point, Polygon>(size, minx, miny, maxx, maxy);
    wkb = write_gc_polygons(r->srid, cells);
  } else if (auto *gv = std::get_if<GeographicVariant>(&r->geometry)) {
    get_bbox<GeoPoint>(*gv, minx, miny, maxx, maxy);
    auto cells =
        make_hex_grid<GeoPoint, GeoPolygon>(size, minx, miny, maxx, maxy);
    wkb = write_gc_polygons(r->srid, cells);
  }

  if (wkb.empty()) { *error = 1; return nullptr; }
  return return_wkb(initid, wkb, result, length);
}

static void stx_hexgrid_deinit(UDF_INIT *initid) {
  if (initid->ptr) free(initid->ptr);
}

}  // extern "C" (pause for I/O format helpers)

// ===== I/O Format Helpers (outside extern "C" for templates) =================

// ----- Encoded Polyline helpers ----------------------------------------------
// Google Encoded Polyline Algorithm:
// https://developers.google.com/maps/documentation/utilities/polylinealgorithm

static std::string encode_polyline_value(int value) {
  value = (value < 0) ? ~(value << 1) : (value << 1);
  std::string out;
  while (value >= 0x20) {
    out.push_back(static_cast<char>((value & 0x1F) | 0x20) + 63);
    value >>= 5;
  }
  out.push_back(static_cast<char>(value) + 63);
  return out;
}

template <typename LinestringT>
static std::string encode_polyline(const LinestringT &ls, int precision) {
  double factor = std::pow(10.0, precision);
  std::string out;
  int prev_lat = 0, prev_lng = 0;
  for (const auto &pt : ls) {
    // Encoded polyline uses (lat, lng) order
    int lat = static_cast<int>(std::round(bg::get<1>(pt) * factor));
    int lng = static_cast<int>(std::round(bg::get<0>(pt) * factor));
    out += encode_polyline_value(lat - prev_lat);
    out += encode_polyline_value(lng - prev_lng);
    prev_lat = lat;
    prev_lng = lng;
  }
  return out;
}

template <typename PointT, typename LinestringT>
static LinestringT decode_polyline(const char *encoded, size_t len,
                                   int precision) {
  double factor = std::pow(10.0, precision);
  LinestringT ls;
  size_t i = 0;
  int lat = 0, lng = 0;
  while (i < len) {
    // Decode latitude
    int shift = 0, result = 0, b;
    do {
      if (i >= len) break;
      b = static_cast<unsigned char>(encoded[i++]) - 63;
      result |= (b & 0x1F) << shift;
      shift += 5;
    } while (b >= 0x20);
    lat += (result & 1) ? ~(result >> 1) : (result >> 1);

    // Decode longitude
    shift = 0;
    result = 0;
    do {
      if (i >= len) break;
      b = static_cast<unsigned char>(encoded[i++]) - 63;
      result |= (b & 0x1F) << shift;
      shift += 5;
    } while (b >= 0x20);
    lng += (result & 1) ? ~(result >> 1) : (result >> 1);

    PointT pt;
    bg::set<0>(pt, lng / factor);  // x = longitude
    bg::set<1>(pt, lat / factor);  // y = latitude
    ls.push_back(pt);
  }
  return ls;
}

// ----- SVG output helpers ----------------------------------------------------

static void svg_append_coord(std::string &buf, double x, double y, int prec) {
  char bx[64], by[64];
  snprintf(bx, sizeof(bx), "%.*g", prec, x);
  snprintf(by, sizeof(by), "%.*g", prec, -y);
  buf += bx;
  buf += ' ';
  buf += by;
}

template <typename PointT>
static std::string svg_point(const PointT &pt, int prec) {
  char bx[64], by[64];
  snprintf(bx, sizeof(bx), "%.*g", prec, bg::get<0>(pt));
  snprintf(by, sizeof(by), "%.*g", prec, -bg::get<1>(pt));
  std::string s = "cx=\"";
  s += bx;
  s += "\" cy=\"";
  s += by;
  s += "\"";
  return s;
}

template <typename LinestringT>
static std::string svg_linestring(const LinestringT &ls, int rel, int prec) {
  if (ls.empty()) return {};
  std::string buf;
  buf += "M ";
  svg_append_coord(buf, bg::get<0>(ls[0]), bg::get<1>(ls[0]), prec);
  if (rel == 0) {
    for (size_t i = 1; i < ls.size(); ++i) {
      buf += " L ";
      svg_append_coord(buf, bg::get<0>(ls[i]), bg::get<1>(ls[i]), prec);
    }
  } else {
    for (size_t i = 1; i < ls.size(); ++i) {
      buf += " l ";
      svg_append_coord(buf, bg::get<0>(ls[i]) - bg::get<0>(ls[i - 1]),
                        bg::get<1>(ls[i]) - bg::get<1>(ls[i - 1]), prec);
    }
  }
  return buf;
}

template <typename RingT>
static void svg_ring(std::string &buf, const RingT &ring, int rel, int prec,
                     bool first_ring) {
  if (ring.empty()) return;
  if (first_ring) {
    buf += "M ";
  } else {
    buf += " M ";
  }
  svg_append_coord(buf, bg::get<0>(ring[0]), bg::get<1>(ring[0]), prec);
  if (rel == 0) {
    for (size_t i = 1; i < ring.size(); ++i) {
      buf += " L ";
      svg_append_coord(buf, bg::get<0>(ring[i]), bg::get<1>(ring[i]), prec);
    }
  } else {
    for (size_t i = 1; i < ring.size(); ++i) {
      buf += " l ";
      svg_append_coord(buf, bg::get<0>(ring[i]) - bg::get<0>(ring[i - 1]),
                        bg::get<1>(ring[i]) - bg::get<1>(ring[i - 1]), prec);
    }
  }
  buf += " Z";
}

template <typename PolygonT>
static std::string svg_polygon(const PolygonT &poly, int rel, int prec) {
  std::string buf;
  svg_ring(buf, poly.outer(), rel, prec, true);
  for (const auto &inner : poly.inners())
    svg_ring(buf, inner, rel, prec, false);
  return buf;
}

template <typename Variant>
static std::string format_svg(const Variant &v, int rel, int prec) {
  return std::visit(
      [rel, prec](const auto &g) -> std::string {
        using T = std::decay_t<decltype(g)>;
        if constexpr (std::is_same_v<T, Point> || std::is_same_v<T, GeoPoint>)
          return svg_point(g, prec);
        else if constexpr (std::is_same_v<T, Linestring> ||
                            std::is_same_v<T, GeoLinestring>)
          return svg_linestring(g, rel, prec);
        else if constexpr (std::is_same_v<T, Polygon> ||
                            std::is_same_v<T, GeoPolygon>)
          return svg_polygon(g, rel, prec);
        else if constexpr (std::is_same_v<T, MultiPoint> ||
                            std::is_same_v<T, GeoMultiPoint>) {
          std::string buf;
          for (const auto &pt : g) {
            if (!buf.empty()) buf += ',';
            buf += svg_point(pt, prec);
          }
          return buf;
        } else if constexpr (std::is_same_v<T, MultiLinestring> ||
                              std::is_same_v<T, GeoMultiLinestring>) {
          std::string buf;
          for (const auto &ls : g) {
            if (!buf.empty()) buf += ' ';
            buf += svg_linestring(ls, rel, prec);
          }
          return buf;
        } else if constexpr (std::is_same_v<T, MultiPolygon> ||
                              std::is_same_v<T, GeoMultiPolygon>) {
          std::string buf;
          for (const auto &p : g) {
            if (!buf.empty()) buf += ' ';
            buf += svg_polygon(p, rel, prec);
          }
          return buf;
        } else
          return {};
      },
      v);
}

// ----- KML output helpers ----------------------------------------------------

static void kml_append_coords(std::string &buf, double x, double y, int prec) {
  char bx[64], by[64];
  snprintf(bx, sizeof(bx), "%.*g", prec, x);
  snprintf(by, sizeof(by), "%.*g", prec, y);
  buf += bx;
  buf += ',';
  buf += by;
}

template <typename Variant>
static std::string format_kml(const Variant &v, int prec) {
  return std::visit(
      [prec](const auto &g) -> std::string {
        using T = std::decay_t<decltype(g)>;
        if constexpr (std::is_same_v<T, Point> || std::is_same_v<T, GeoPoint>) {
          std::string buf = "<Point><coordinates>";
          kml_append_coords(buf, bg::get<0>(g), bg::get<1>(g), prec);
          buf += "</coordinates></Point>";
          return buf;
        } else if constexpr (std::is_same_v<T, Linestring> ||
                              std::is_same_v<T, GeoLinestring>) {
          std::string buf = "<LineString><coordinates>";
          for (size_t i = 0; i < g.size(); ++i) {
            if (i > 0) buf += ' ';
            kml_append_coords(buf, bg::get<0>(g[i]), bg::get<1>(g[i]), prec);
          }
          buf += "</coordinates></LineString>";
          return buf;
        } else if constexpr (std::is_same_v<T, Polygon> ||
                              std::is_same_v<T, GeoPolygon>) {
          std::string buf = "<Polygon>";
          buf += "<outerBoundaryIs><LinearRing><coordinates>";
          for (size_t i = 0; i < g.outer().size(); ++i) {
            if (i > 0) buf += ' ';
            kml_append_coords(buf, bg::get<0>(g.outer()[i]),
                              bg::get<1>(g.outer()[i]), prec);
          }
          buf += "</coordinates></LinearRing></outerBoundaryIs>";
          for (const auto &inner : g.inners()) {
            buf += "<innerBoundaryIs><LinearRing><coordinates>";
            for (size_t i = 0; i < inner.size(); ++i) {
              if (i > 0) buf += ' ';
              kml_append_coords(buf, bg::get<0>(inner[i]),
                                bg::get<1>(inner[i]), prec);
            }
            buf += "</coordinates></LinearRing></innerBoundaryIs>";
          }
          buf += "</Polygon>";
          return buf;
        } else if constexpr (std::is_same_v<T, MultiPoint> ||
                              std::is_same_v<T, GeoMultiPoint>) {
          std::string buf = "<MultiGeometry>";
          for (const auto &pt : g) {
            buf += "<Point><coordinates>";
            kml_append_coords(buf, bg::get<0>(pt), bg::get<1>(pt), prec);
            buf += "</coordinates></Point>";
          }
          buf += "</MultiGeometry>";
          return buf;
        } else if constexpr (std::is_same_v<T, MultiLinestring> ||
                              std::is_same_v<T, GeoMultiLinestring>) {
          std::string buf = "<MultiGeometry>";
          for (const auto &ls : g) {
            buf += "<LineString><coordinates>";
            for (size_t i = 0; i < ls.size(); ++i) {
              if (i > 0) buf += ' ';
              kml_append_coords(buf, bg::get<0>(ls[i]), bg::get<1>(ls[i]),
                                prec);
            }
            buf += "</coordinates></LineString>";
          }
          buf += "</MultiGeometry>";
          return buf;
        } else if constexpr (std::is_same_v<T, MultiPolygon> ||
                              std::is_same_v<T, GeoMultiPolygon>) {
          std::string buf = "<MultiGeometry>";
          for (const auto &poly : g) {
            buf += "<Polygon><outerBoundaryIs><LinearRing><coordinates>";
            for (size_t i = 0; i < poly.outer().size(); ++i) {
              if (i > 0) buf += ' ';
              kml_append_coords(buf, bg::get<0>(poly.outer()[i]),
                                bg::get<1>(poly.outer()[i]), prec);
            }
            buf += "</coordinates></LinearRing></outerBoundaryIs>";
            for (const auto &inner : poly.inners()) {
              buf += "<innerBoundaryIs><LinearRing><coordinates>";
              for (size_t i = 0; i < inner.size(); ++i) {
                if (i > 0) buf += ' ';
                kml_append_coords(buf, bg::get<0>(inner[i]),
                                  bg::get<1>(inner[i]), prec);
              }
              buf += "</coordinates></LinearRing></innerBoundaryIs>";
            }
            buf += "</Polygon>";
          }
          buf += "</MultiGeometry>";
          return buf;
        } else
          return {};
      },
      v);
}

// ----- EWKT helpers ----------------------------------------------------------

template <typename Variant>
static std::string format_ewkt(uint32_t srid, const Variant &v) {
  std::string result = "SRID=" + std::to_string(srid) + ";";
  std::ostringstream oss;
  oss << std::setprecision(15);
  std::visit([&oss](const auto &g) { oss << bg::wkt(g); }, v);
  result += oss.str();
  return result;
}

// Simple WKT parser for EWKT input.
// Supports POINT, LINESTRING, POLYGON, MULTIPOINT, MULTILINESTRING, MULTIPOLYGON.
static std::optional<ParseResult> parse_ewkt(const char *text, size_t len) {
  std::string s(text, len);
  uint32_t srid = 0;
  size_t wkt_start = 0;

  // Parse optional SRID= prefix
  if (s.size() > 5 && (s[0] == 'S' || s[0] == 's') &&
      (s[1] == 'R' || s[1] == 'r') && (s[2] == 'I' || s[2] == 'i') &&
      (s[3] == 'D' || s[3] == 'd') && s[4] == '=') {
    size_t semi = s.find(';', 5);
    if (semi == std::string::npos) return std::nullopt;
    srid = static_cast<uint32_t>(std::stoul(s.substr(5, semi - 5)));
    wkt_start = semi + 1;
  }

  std::string wkt = s.substr(wkt_start);
  // Trim leading whitespace
  size_t first_non_space = wkt.find_first_not_of(" \t\r\n");
  if (first_non_space == std::string::npos) return std::nullopt;
  wkt = wkt.substr(first_non_space);

  // Detect geometry type
  std::string upper;
  for (char c : wkt) upper += static_cast<char>(toupper(c));

  bool geo = is_geographic_srid(srid);
  ParseResult result;
  result.srid = srid;

  try {
    if (upper.find("MULTIPOLYGON") == 0) {
      if (geo) {
        GeoMultiPolygon g;
        bg::read_wkt(wkt, g);
        result.geometry = GeographicVariant(std::move(g));
      } else {
        MultiPolygon g;
        bg::read_wkt(wkt, g);
        result.geometry = CartesianVariant(std::move(g));
      }
    } else if (upper.find("MULTILINESTRING") == 0) {
      if (geo) {
        GeoMultiLinestring g;
        bg::read_wkt(wkt, g);
        result.geometry = GeographicVariant(std::move(g));
      } else {
        MultiLinestring g;
        bg::read_wkt(wkt, g);
        result.geometry = CartesianVariant(std::move(g));
      }
    } else if (upper.find("MULTIPOINT") == 0) {
      if (geo) {
        GeoMultiPoint g;
        bg::read_wkt(wkt, g);
        result.geometry = GeographicVariant(std::move(g));
      } else {
        MultiPoint g;
        bg::read_wkt(wkt, g);
        result.geometry = CartesianVariant(std::move(g));
      }
    } else if (upper.find("POLYGON") == 0) {
      if (geo) {
        GeoPolygon g;
        bg::read_wkt(wkt, g);
        result.geometry = GeographicVariant(std::move(g));
      } else {
        Polygon g;
        bg::read_wkt(wkt, g);
        result.geometry = CartesianVariant(std::move(g));
      }
    } else if (upper.find("LINESTRING") == 0) {
      if (geo) {
        GeoLinestring g;
        bg::read_wkt(wkt, g);
        result.geometry = GeographicVariant(std::move(g));
      } else {
        Linestring g;
        bg::read_wkt(wkt, g);
        result.geometry = CartesianVariant(std::move(g));
      }
    } else if (upper.find("POINT") == 0) {
      if (geo) {
        GeoPoint g;
        bg::read_wkt(wkt, g);
        result.geometry = GeographicVariant(std::move(g));
      } else {
        Point g;
        bg::read_wkt(wkt, g);
        result.geometry = CartesianVariant(std::move(g));
      }
    } else {
      return std::nullopt;
    }
  } catch (...) {
    return std::nullopt;
  }

  return result;
}

// Helper: return a string result from a UDF (handles buffer vs malloc).
static char *return_string(UDF_INIT *initid, const std::string &str,
                           char *result, unsigned long *length) {
  *length = static_cast<unsigned long>(str.size());
  if (str.size() <= 255) {
    std::memcpy(result, str.data(), str.size());
    return result;
  }
  initid->ptr = static_cast<char *>(malloc(str.size()));
  std::memcpy(initid->ptr, str.data(), str.size());
  return initid->ptr;
}

extern "C" {  // resume extern "C" for I/O format UDFs

// ----- stx_asencodedpolyline ------------------------------------------------
// Encodes a LineString as a Google Encoded Polyline string.
// stx_asencodedpolyline(geom [, precision])

static bool stx_asencodedpolyline_init(UDF_INIT *initid, UDF_ARGS *args,
                                        char *msg) {
  if (args->arg_count < 1 || args->arg_count > 2) {
    strcpy(msg,
           "stx_asencodedpolyline() requires 1 or 2 arguments "
           "(geom [, precision])");
    return true;
  }
  args->arg_type[0] = STRING_RESULT;
  if (args->arg_count == 2) args->arg_type[1] = INT_RESULT;
  initid->maybe_null = 1;
  initid->ptr = nullptr;
  set_result_charset_utf8(initid);
  return false;
}

static char *stx_asencodedpolyline(UDF_INIT *initid, UDF_ARGS *args,
                                    char *result, unsigned long *length,
                                    char *is_null, char *error) {
  if (initid->ptr) { free(initid->ptr); initid->ptr = nullptr; }
  if (!args->args[0]) { *is_null = 1; return nullptr; }
  if (args->arg_count == 2 && !args->args[1]) { *is_null = 1; return nullptr; }

  auto r = parse_geometry(args->args[0], args->lengths[0]);
  if (!r) { *error = 1; return nullptr; }

  int precision = 5;
  if (args->arg_count == 2)
    precision = static_cast<int>(*reinterpret_cast<long long *>(args->args[1]));

  std::string encoded;
  if (auto *cv = std::get_if<CartesianVariant>(&r->geometry)) {
    if (auto *ls = std::get_if<Linestring>(cv))
      encoded = encode_polyline(*ls, precision);
    else { *error = 1; return nullptr; }
  } else if (auto *gv = std::get_if<GeographicVariant>(&r->geometry)) {
    if (auto *ls = std::get_if<GeoLinestring>(gv))
      encoded = encode_polyline(*ls, precision);
    else { *error = 1; return nullptr; }
  }

  if (encoded.empty()) { *error = 1; return nullptr; }
  return return_string(initid, encoded, result, length);
}

static void stx_asencodedpolyline_deinit(UDF_INIT *initid) {
  if (initid->ptr) free(initid->ptr);
}

// ----- stx_linefromencodedpolyline ------------------------------------------
// Decodes a Google Encoded Polyline string to a LineString.
// stx_linefromencodedpolyline(text [, srid [, precision]])

static bool stx_linefromencodedpolyline_init(UDF_INIT *initid, UDF_ARGS *args,
                                               char *msg) {
  if (args->arg_count < 1 || args->arg_count > 3) {
    strcpy(msg,
           "stx_linefromencodedpolyline() requires 1-3 arguments "
           "(text [, srid [, precision]])");
    return true;
  }
  args->arg_type[0] = STRING_RESULT;
  if (args->arg_count >= 2) args->arg_type[1] = INT_RESULT;
  if (args->arg_count == 3) args->arg_type[2] = INT_RESULT;
  initid->maybe_null = 1;
  initid->ptr = nullptr;
  return false;
}

static char *stx_linefromencodedpolyline(UDF_INIT *initid, UDF_ARGS *args,
                                           char *result, unsigned long *length,
                                           char *is_null, char *error) {
  if (initid->ptr) { free(initid->ptr); initid->ptr = nullptr; }
  if (!args->args[0]) { *is_null = 1; return nullptr; }

  uint32_t srid = 4326;
  if (args->arg_count >= 2 && args->args[1])
    srid = static_cast<uint32_t>(*reinterpret_cast<long long *>(args->args[1]));

  int precision = 5;
  if (args->arg_count == 3 && args->args[2])
    precision =
        static_cast<int>(*reinterpret_cast<long long *>(args->args[2]));

  std::string wkb;
  if (is_geographic_srid(srid)) {
    auto ls = decode_polyline<GeoPoint, GeoLinestring>(args->args[0],
                                                        args->lengths[0],
                                                        precision);
    wkb = write_linestring(srid, ls);
  } else {
    auto ls = decode_polyline<Point, Linestring>(args->args[0],
                                                  args->lengths[0], precision);
    wkb = write_linestring(srid, ls);
  }

  if (wkb.empty()) { *error = 1; return nullptr; }
  return return_wkb(initid, wkb, result, length);
}

static void stx_linefromencodedpolyline_deinit(UDF_INIT *initid) {
  if (initid->ptr) free(initid->ptr);
}

// ----- stx_assvg -------------------------------------------------------------
// Returns SVG path data for a geometry.
// stx_assvg(geom [, rel [, precision]])

static bool stx_assvg_init(UDF_INIT *initid, UDF_ARGS *args, char *msg) {
  if (args->arg_count < 1 || args->arg_count > 3) {
    strcpy(msg,
           "stx_assvg() requires 1-3 arguments (geom [, rel [, precision]])");
    return true;
  }
  args->arg_type[0] = STRING_RESULT;
  if (args->arg_count >= 2) args->arg_type[1] = INT_RESULT;
  if (args->arg_count == 3) args->arg_type[2] = INT_RESULT;
  initid->maybe_null = 1;
  initid->ptr = nullptr;
  set_result_charset_utf8(initid);
  return false;
}

static char *stx_assvg(UDF_INIT *initid, UDF_ARGS *args, char *result,
                        unsigned long *length, char *is_null, char *error) {
  if (initid->ptr) { free(initid->ptr); initid->ptr = nullptr; }
  if (!args->args[0]) { *is_null = 1; return nullptr; }

  auto r = parse_geometry(args->args[0], args->lengths[0]);
  if (!r) { *error = 1; return nullptr; }

  int rel = 0;
  if (args->arg_count >= 2 && args->args[1])
    rel = static_cast<int>(*reinterpret_cast<long long *>(args->args[1]));
  int prec = 15;
  if (args->arg_count == 3 && args->args[2])
    prec = static_cast<int>(*reinterpret_cast<long long *>(args->args[2]));

  std::string svg;
  if (auto *cv = std::get_if<CartesianVariant>(&r->geometry))
    svg = format_svg(*cv, rel, prec);
  else if (auto *gv = std::get_if<GeographicVariant>(&r->geometry))
    svg = format_svg(*gv, rel, prec);

  if (svg.empty()) { *error = 1; return nullptr; }
  return return_string(initid, svg, result, length);
}

static void stx_assvg_deinit(UDF_INIT *initid) {
  if (initid->ptr) free(initid->ptr);
}

// ----- stx_askml -------------------------------------------------------------
// Returns KML geometry element for a geometry.
// stx_askml(geom [, precision])

static bool stx_askml_init(UDF_INIT *initid, UDF_ARGS *args, char *msg) {
  if (args->arg_count < 1 || args->arg_count > 2) {
    strcpy(msg,
           "stx_askml() requires 1 or 2 arguments (geom [, precision])");
    return true;
  }
  args->arg_type[0] = STRING_RESULT;
  if (args->arg_count == 2) args->arg_type[1] = INT_RESULT;
  initid->maybe_null = 1;
  initid->ptr = nullptr;
  set_result_charset_utf8(initid);
  return false;
}

static char *stx_askml(UDF_INIT *initid, UDF_ARGS *args, char *result,
                        unsigned long *length, char *is_null, char *error) {
  if (initid->ptr) { free(initid->ptr); initid->ptr = nullptr; }
  if (!args->args[0]) { *is_null = 1; return nullptr; }

  auto r = parse_geometry(args->args[0], args->lengths[0]);
  if (!r) { *error = 1; return nullptr; }

  int prec = 15;
  if (args->arg_count == 2 && args->args[1])
    prec = static_cast<int>(*reinterpret_cast<long long *>(args->args[1]));

  std::string kml;
  if (auto *cv = std::get_if<CartesianVariant>(&r->geometry))
    kml = format_kml(*cv, prec);
  else if (auto *gv = std::get_if<GeographicVariant>(&r->geometry))
    kml = format_kml(*gv, prec);

  if (kml.empty()) { *error = 1; return nullptr; }
  return return_string(initid, kml, result, length);
}

static void stx_askml_deinit(UDF_INIT *initid) {
  if (initid->ptr) free(initid->ptr);
}

// ----- stx_asewkt ------------------------------------------------------------
// Returns EWKT (Extended WKT) string: "SRID=xxxx;POINT(x y)" etc.
// stx_asewkt(geom)

static bool stx_asewkt_init(UDF_INIT *initid, UDF_ARGS *args, char *msg) {
  if (args->arg_count != 1) {
    strcpy(msg, "stx_asewkt() requires 1 argument (geom)");
    return true;
  }
  args->arg_type[0] = STRING_RESULT;
  initid->maybe_null = 1;
  initid->ptr = nullptr;
  set_result_charset_utf8(initid);
  return false;
}

static char *stx_asewkt(UDF_INIT *initid, UDF_ARGS *args, char *result,
                          unsigned long *length, char *is_null, char *error) {
  if (initid->ptr) { free(initid->ptr); initid->ptr = nullptr; }
  if (!args->args[0]) { *is_null = 1; return nullptr; }

  auto r = parse_geometry(args->args[0], args->lengths[0]);
  if (!r) { *error = 1; return nullptr; }

  std::string ewkt;
  if (auto *cv = std::get_if<CartesianVariant>(&r->geometry))
    ewkt = format_ewkt(r->srid, *cv);
  else if (auto *gv = std::get_if<GeographicVariant>(&r->geometry))
    ewkt = format_ewkt(r->srid, *gv);

  if (ewkt.empty()) { *error = 1; return nullptr; }
  return return_string(initid, ewkt, result, length);
}

static void stx_asewkt_deinit(UDF_INIT *initid) {
  if (initid->ptr) free(initid->ptr);
}

// ----- stx_geomfromewkt ------------------------------------------------------
// Parses EWKT string and returns geometry binary.
// stx_geomfromewkt(text)

static bool stx_geomfromewkt_init(UDF_INIT *initid, UDF_ARGS *args,
                                   char *msg) {
  if (args->arg_count != 1) {
    strcpy(msg, "stx_geomfromewkt() requires 1 argument (text)");
    return true;
  }
  args->arg_type[0] = STRING_RESULT;
  initid->maybe_null = 1;
  initid->ptr = nullptr;
  return false;
}

static char *stx_geomfromewkt(UDF_INIT *initid, UDF_ARGS *args, char *result,
                                unsigned long *length, char *is_null,
                                char *error) {
  if (initid->ptr) { free(initid->ptr); initid->ptr = nullptr; }
  if (!args->args[0]) { *is_null = 1; return nullptr; }

  auto r = parse_ewkt(args->args[0], args->lengths[0]);
  if (!r) { *error = 1; return nullptr; }

  std::string wkb;
  if (auto *cv = std::get_if<CartesianVariant>(&r->geometry))
    wkb = write_geometry(r->srid, *cv);
  else if (auto *gv = std::get_if<GeographicVariant>(&r->geometry))
    wkb = write_geometry(r->srid, *gv);

  if (wkb.empty()) { *error = 1; return nullptr; }
  return return_wkb(initid, wkb, result, length);
}

static void stx_geomfromewkt_deinit(UDF_INIT *initid) {
  if (initid->ptr) free(initid->ptr);
}

// =============================================================================
// GEOS-based functions (Phase 4)
// =============================================================================

// ----- stx_makevalid ---------------------------------------------------------
// Repairs invalid geometry using GEOS MakeValid algorithm.
// stx_makevalid(geom)

static bool stx_makevalid_init(UDF_INIT *initid, UDF_ARGS *args, char *msg) {
  if (args->arg_count != 1) {
    strcpy(msg, "stx_makevalid() requires 1 argument (geom)");
    return true;
  }
  args->arg_type[0] = STRING_RESULT;
  initid->maybe_null = 1;
  initid->ptr = nullptr;
  return false;
}

static char *stx_makevalid(UDF_INIT *initid, UDF_ARGS *args, char *result,
                            unsigned long *length, char *is_null, char *error) {
  if (initid->ptr) { free(initid->ptr); initid->ptr = nullptr; }
  if (!args->args[0]) { *is_null = 1; return nullptr; }

  uint32_t srid = extract_srid(args->args[0], args->lengths[0]);
  auto geom = mysql_to_geos(args->args[0], args->lengths[0]);
  if (!geom) { *error = 1; return nullptr; }

  auto ctx = get_geos_context();
  GEOSGeomPtr valid(GEOSMakeValid_r(ctx, geom.get()));
  if (!valid) { *error = 1; return nullptr; }

  auto wkb = geos_to_mysql(valid.get(), srid);
  if (wkb.empty()) { *error = 1; return nullptr; }
  return return_wkb(initid, wkb, result, length);
}

static void stx_makevalid_deinit(UDF_INIT *initid) {
  if (initid->ptr) free(initid->ptr);
}

// ----- stx_linemerge ---------------------------------------------------------
// Merges a MultiLineString into connected LineStrings.
// stx_linemerge(geom)

static bool stx_linemerge_init(UDF_INIT *initid, UDF_ARGS *args, char *msg) {
  if (args->arg_count != 1) {
    strcpy(msg, "stx_linemerge() requires 1 argument (geom)");
    return true;
  }
  args->arg_type[0] = STRING_RESULT;
  initid->maybe_null = 1;
  initid->ptr = nullptr;
  return false;
}

static char *stx_linemerge(UDF_INIT *initid, UDF_ARGS *args, char *result,
                            unsigned long *length, char *is_null, char *error) {
  if (initid->ptr) { free(initid->ptr); initid->ptr = nullptr; }
  if (!args->args[0]) { *is_null = 1; return nullptr; }

  uint32_t srid = extract_srid(args->args[0], args->lengths[0]);
  auto geom = mysql_to_geos(args->args[0], args->lengths[0]);
  if (!geom) { *error = 1; return nullptr; }

  auto ctx = get_geos_context();
  GEOSGeomPtr merged(GEOSLineMerge_r(ctx, geom.get()));
  if (!merged) { *error = 1; return nullptr; }

  auto wkb = geos_to_mysql(merged.get(), srid);
  if (wkb.empty()) { *error = 1; return nullptr; }
  return return_wkb(initid, wkb, result, length);
}

static void stx_linemerge_deinit(UDF_INIT *initid) {
  if (initid->ptr) free(initid->ptr);
}

// ----- stx_voronoi -----------------------------------------------------------
// Creates Voronoi diagram from point geometry.
// stx_voronoi(geom [, tolerance [, envelope]])

static bool stx_voronoi_init(UDF_INIT *initid, UDF_ARGS *args, char *msg) {
  if (args->arg_count < 1 || args->arg_count > 3) {
    strcpy(msg,
           "stx_voronoi() requires 1-3 arguments (geom [, tolerance [, envelope]])");
    return true;
  }
  args->arg_type[0] = STRING_RESULT;
  if (args->arg_count >= 2) args->arg_type[1] = REAL_RESULT;
  if (args->arg_count >= 3) args->arg_type[2] = STRING_RESULT;
  initid->maybe_null = 1;
  initid->ptr = nullptr;
  return false;
}

static char *stx_voronoi(UDF_INIT *initid, UDF_ARGS *args, char *result,
                          unsigned long *length, char *is_null, char *error) {
  if (initid->ptr) { free(initid->ptr); initid->ptr = nullptr; }
  if (!args->args[0]) { *is_null = 1; return nullptr; }

  uint32_t srid = extract_srid(args->args[0], args->lengths[0]);
  auto geom = mysql_to_geos(args->args[0], args->lengths[0]);
  if (!geom) { *error = 1; return nullptr; }

  double tolerance = 0.0;
  if (args->arg_count >= 2 && args->args[1])
    tolerance = *reinterpret_cast<double *>(args->args[1]);

  GEOSGeometry *env_raw = nullptr;
  GEOSGeomPtr env_holder;
  if (args->arg_count >= 3 && args->args[2]) {
    env_holder = mysql_to_geos(args->args[2], args->lengths[2]);
    env_raw = env_holder.get();
  }

  auto ctx = get_geos_context();
  GEOSGeomPtr voronoi(GEOSVoronoiDiagram_r(ctx, geom.get(), env_raw,
                                             tolerance, 0));
  if (!voronoi) { *error = 1; return nullptr; }

  auto wkb = geos_to_mysql(voronoi.get(), srid);
  if (wkb.empty()) { *error = 1; return nullptr; }
  return return_wkb(initid, wkb, result, length);
}

static void stx_voronoi_deinit(UDF_INIT *initid) {
  if (initid->ptr) free(initid->ptr);
}

// ----- stx_delaunay ----------------------------------------------------------
// Creates Delaunay triangulation from geometry vertices.
// stx_delaunay(geom [, tolerance [, edges_only]])

static bool stx_delaunay_init(UDF_INIT *initid, UDF_ARGS *args, char *msg) {
  if (args->arg_count < 1 || args->arg_count > 3) {
    strcpy(msg,
           "stx_delaunay() requires 1-3 arguments (geom [, tolerance [, edges_only]])");
    return true;
  }
  args->arg_type[0] = STRING_RESULT;
  if (args->arg_count >= 2) args->arg_type[1] = REAL_RESULT;
  if (args->arg_count >= 3) args->arg_type[2] = INT_RESULT;
  initid->maybe_null = 1;
  initid->ptr = nullptr;
  return false;
}

static char *stx_delaunay(UDF_INIT *initid, UDF_ARGS *args, char *result,
                           unsigned long *length, char *is_null, char *error) {
  if (initid->ptr) { free(initid->ptr); initid->ptr = nullptr; }
  if (!args->args[0]) { *is_null = 1; return nullptr; }

  uint32_t srid = extract_srid(args->args[0], args->lengths[0]);
  auto geom = mysql_to_geos(args->args[0], args->lengths[0]);
  if (!geom) { *error = 1; return nullptr; }

  double tolerance = 0.0;
  if (args->arg_count >= 2 && args->args[1])
    tolerance = *reinterpret_cast<double *>(args->args[1]);

  int edges_only = 0;
  if (args->arg_count >= 3 && args->args[2])
    edges_only = (*reinterpret_cast<long long *>(args->args[2])) ? 1 : 0;

  auto ctx = get_geos_context();
  GEOSGeomPtr tri(GEOSDelaunayTriangulation_r(ctx, geom.get(), tolerance,
                                               edges_only));
  if (!tri) { *error = 1; return nullptr; }

  auto wkb = geos_to_mysql(tri.get(), srid);
  if (wkb.empty()) { *error = 1; return nullptr; }
  return return_wkb(initid, wkb, result, length);
}

static void stx_delaunay_deinit(UDF_INIT *initid) {
  if (initid->ptr) free(initid->ptr);
}

// ----- stx_offsetcurve -------------------------------------------------------
// Returns a line offset from the input line by the given distance.
// Positive distance = left side, negative = right side.
// stx_offsetcurve(geom, distance [, quad_segs [, join_style [, mitre_limit]]])
// join_style: 1=round (default), 2=mitre, 3=bevel

static bool stx_offsetcurve_init(UDF_INIT *initid, UDF_ARGS *args, char *msg) {
  if (args->arg_count < 2 || args->arg_count > 5) {
    strcpy(msg,
           "stx_offsetcurve() requires 2-5 arguments "
           "(geom, distance [, quad_segs [, join_style [, mitre_limit]]])");
    return true;
  }
  args->arg_type[0] = STRING_RESULT;
  args->arg_type[1] = REAL_RESULT;
  if (args->arg_count >= 3) args->arg_type[2] = INT_RESULT;
  if (args->arg_count >= 4) args->arg_type[3] = INT_RESULT;
  if (args->arg_count >= 5) args->arg_type[4] = REAL_RESULT;
  initid->maybe_null = 1;
  initid->ptr = nullptr;
  return false;
}

static char *stx_offsetcurve(UDF_INIT *initid, UDF_ARGS *args, char *result,
                              unsigned long *length, char *is_null,
                              char *error) {
  if (initid->ptr) { free(initid->ptr); initid->ptr = nullptr; }
  if (!args->args[0]) { *is_null = 1; return nullptr; }
  if (!args->args[1]) { *is_null = 1; return nullptr; }

  // OffsetCurve only works on LineString — check type from WKB header
  uint32_t wkb_type = 0;
  if (args->lengths[0] >= 9) std::memcpy(&wkb_type, args->args[0] + 5, 4);
  GeometryType gt = static_cast<GeometryType>(wkb_type);
  if (gt != GeometryType::LineString) {
    my_error(ER_UNEXPECTED_GEOMETRY_TYPE, MYF(0),
             "LINESTRING", geometry_type_name(gt), "stx_offsetcurve");
    *error = 1; return nullptr;
  }

  uint32_t srid = extract_srid(args->args[0], args->lengths[0]);
  auto geom = mysql_to_geos(args->args[0], args->lengths[0]);
  if (!geom) { *error = 1; return nullptr; }

  double distance = *reinterpret_cast<double *>(args->args[1]);

  int quad_segs = 8;  // GEOS default
  if (args->arg_count >= 3 && args->args[2])
    quad_segs = static_cast<int>(*reinterpret_cast<long long *>(args->args[2]));

  int join_style = 1;  // GEOSBUF_JOIN_ROUND
  if (args->arg_count >= 4 && args->args[3])
    join_style =
        static_cast<int>(*reinterpret_cast<long long *>(args->args[3]));

  double mitre_limit = 5.0;  // GEOS default
  if (args->arg_count >= 5 && args->args[4])
    mitre_limit = *reinterpret_cast<double *>(args->args[4]);

  auto ctx = get_geos_context();
  GEOSGeomPtr offset(
      GEOSOffsetCurve_r(ctx, geom.get(), distance, quad_segs, join_style,
                         mitre_limit));
  if (!offset) { *error = 1; return nullptr; }

  auto wkb = geos_to_mysql(offset.get(), srid);
  if (wkb.empty()) { *error = 1; return nullptr; }
  return return_wkb(initid, wkb, result, length);
}

static void stx_offsetcurve_deinit(UDF_INIT *initid) {
  if (initid->ptr) free(initid->ptr);
}

// ----- stx_concavehull -------------------------------------------------------
// Computes the concave hull of a geometry.
// ratio: 0 = maximum concavity, 1 = convex hull.
// stx_concavehull(geom, ratio [, allow_holes])

static bool stx_concavehull_init(UDF_INIT *initid, UDF_ARGS *args, char *msg) {
  if (args->arg_count < 2 || args->arg_count > 3) {
    strcpy(msg,
           "stx_concavehull() requires 2-3 arguments "
           "(geom, ratio [, allow_holes])");
    return true;
  }
  args->arg_type[0] = STRING_RESULT;
  args->arg_type[1] = REAL_RESULT;
  if (args->arg_count >= 3) args->arg_type[2] = INT_RESULT;
  initid->maybe_null = 1;
  initid->ptr = nullptr;
  return false;
}

static char *stx_concavehull(UDF_INIT *initid, UDF_ARGS *args, char *result,
                              unsigned long *length, char *is_null,
                              char *error) {
  if (initid->ptr) { free(initid->ptr); initid->ptr = nullptr; }
  if (!args->args[0]) { *is_null = 1; return nullptr; }
  if (!args->args[1]) { *is_null = 1; return nullptr; }

  uint32_t srid = extract_srid(args->args[0], args->lengths[0]);
  auto geom = mysql_to_geos(args->args[0], args->lengths[0]);
  if (!geom) { *error = 1; return nullptr; }

  double ratio = *reinterpret_cast<double *>(args->args[1]);

  unsigned int allow_holes = 0;
  if (args->arg_count >= 3 && args->args[2])
    allow_holes =
        (*reinterpret_cast<long long *>(args->args[2])) ? 1 : 0;

  auto ctx = get_geos_context();
  GEOSGeomPtr hull(GEOSConcaveHull_r(ctx, geom.get(), ratio, allow_holes));
  if (!hull) { *error = 1; return nullptr; }

  auto wkb = geos_to_mysql(hull.get(), srid);
  if (wkb.empty()) { *error = 1; return nullptr; }
  return return_wkb(initid, wkb, result, length);
}

static void stx_concavehull_deinit(UDF_INIT *initid) {
  if (initid->ptr) free(initid->ptr);
}

// ----- stx_snap --------------------------------------------------------------
// Snaps vertices of geom1 to vertices of geom2 within the given tolerance.
// stx_snap(geom1, geom2, tolerance)

static bool stx_snap_init(UDF_INIT *initid, UDF_ARGS *args, char *msg) {
  if (args->arg_count != 3) {
    strcpy(msg, "stx_snap() requires 3 arguments (geom1, geom2, tolerance)");
    return true;
  }
  args->arg_type[0] = STRING_RESULT;
  args->arg_type[1] = STRING_RESULT;
  args->arg_type[2] = REAL_RESULT;
  initid->maybe_null = 1;
  initid->ptr = nullptr;
  return false;
}

static char *stx_snap(UDF_INIT *initid, UDF_ARGS *args, char *result,
                       unsigned long *length, char *is_null, char *error) {
  if (initid->ptr) { free(initid->ptr); initid->ptr = nullptr; }
  if (!args->args[0] || !args->args[1] || !args->args[2]) {
    *is_null = 1;
    return nullptr;
  }

  uint32_t srid = extract_srid(args->args[0], args->lengths[0]);
  auto geom1 = mysql_to_geos(args->args[0], args->lengths[0]);
  if (!geom1) { *error = 1; return nullptr; }

  auto geom2 = mysql_to_geos(args->args[1], args->lengths[1]);
  if (!geom2) { *error = 1; return nullptr; }

  double tolerance = *reinterpret_cast<double *>(args->args[2]);

  auto ctx = get_geos_context();
  GEOSGeomPtr snapped(GEOSSnap_r(ctx, geom1.get(), geom2.get(), tolerance));
  if (!snapped) { *error = 1; return nullptr; }

  auto wkb = geos_to_mysql(snapped.get(), srid);
  if (wkb.empty()) { *error = 1; return nullptr; }
  return return_wkb(initid, wkb, result, length);
}

static void stx_snap_deinit(UDF_INIT *initid) {
  if (initid->ptr) free(initid->ptr);
}

// ----- stx_polygonize --------------------------------------------------------
// Creates polygons from a set of linework (GeometryCollection of LineStrings).
// stx_polygonize(geom)

static bool stx_polygonize_init(UDF_INIT *initid, UDF_ARGS *args, char *msg) {
  if (args->arg_count != 1) {
    strcpy(msg, "stx_polygonize() requires 1 argument (geom)");
    return true;
  }
  args->arg_type[0] = STRING_RESULT;
  initid->maybe_null = 1;
  initid->ptr = nullptr;
  return false;
}

static char *stx_polygonize(UDF_INIT *initid, UDF_ARGS *args, char *result,
                              unsigned long *length, char *is_null,
                              char *error) {
  if (initid->ptr) { free(initid->ptr); initid->ptr = nullptr; }
  if (!args->args[0]) { *is_null = 1; return nullptr; }

  uint32_t srid = extract_srid(args->args[0], args->lengths[0]);
  auto geom = mysql_to_geos(args->args[0], args->lengths[0]);
  if (!geom) { *error = 1; return nullptr; }

  auto ctx = get_geos_context();

  // Extract individual geometries from input for GEOSPolygonize_r
  int n = GEOSGetNumGeometries_r(ctx, geom.get());
  if (n < 0) { *error = 1; return nullptr; }

  std::vector<const GEOSGeometry *> geoms;
  if (n == 0) {
    // Single geometry (not a collection) — pass it directly
    geoms.push_back(geom.get());
  } else {
    geoms.reserve(n);
    for (int i = 0; i < n; ++i)
      geoms.push_back(GEOSGetGeometryN_r(ctx, geom.get(), i));
  }

  GEOSGeomPtr poly(
      GEOSPolygonize_r(ctx, geoms.data(),
                        static_cast<unsigned int>(geoms.size())));
  if (!poly) { *error = 1; return nullptr; }

  auto wkb = geos_to_mysql(poly.get(), srid);
  if (wkb.empty()) { *error = 1; return nullptr; }
  return return_wkb(initid, wkb, result, length);
}

static void stx_polygonize_deinit(UDF_INIT *initid) {
  if (initid->ptr) free(initid->ptr);
}

// ----- stx_buildarea ---------------------------------------------------------
// Creates an areal geometry from linework. Interior rings become holes.
// stx_buildarea(geom)

static bool stx_buildarea_init(UDF_INIT *initid, UDF_ARGS *args, char *msg) {
  if (args->arg_count != 1) {
    strcpy(msg, "stx_buildarea() requires 1 argument (geom)");
    return true;
  }
  args->arg_type[0] = STRING_RESULT;
  initid->maybe_null = 1;
  initid->ptr = nullptr;
  return false;
}

static char *stx_buildarea(UDF_INIT *initid, UDF_ARGS *args, char *result,
                            unsigned long *length, char *is_null,
                            char *error) {
  if (initid->ptr) { free(initid->ptr); initid->ptr = nullptr; }
  if (!args->args[0]) { *is_null = 1; return nullptr; }

  uint32_t srid = extract_srid(args->args[0], args->lengths[0]);
  auto geom = mysql_to_geos(args->args[0], args->lengths[0]);
  if (!geom) { *error = 1; return nullptr; }

  auto ctx = get_geos_context();
  GEOSGeomPtr area(GEOSBuildArea_r(ctx, geom.get()));
  if (!area) { *error = 1; return nullptr; }

  auto wkb = geos_to_mysql(area.get(), srid);
  if (wkb.empty()) { *error = 1; return nullptr; }
  return return_wkb(initid, wkb, result, length);
}

static void stx_buildarea_deinit(UDF_INIT *initid) {
  if (initid->ptr) free(initid->ptr);
}

// ----- stx_sharedpaths -------------------------------------------------------
// Returns shared paths between two lineal geometries.
// Result is GeometryCollection: [0]=same-direction paths, [1]=opposite-direction.
// stx_sharedpaths(geom1, geom2)

static bool stx_sharedpaths_init(UDF_INIT *initid, UDF_ARGS *args, char *msg) {
  if (args->arg_count != 2) {
    strcpy(msg, "stx_sharedpaths() requires 2 arguments (geom1, geom2)");
    return true;
  }
  args->arg_type[0] = STRING_RESULT;
  args->arg_type[1] = STRING_RESULT;
  initid->maybe_null = 1;
  initid->ptr = nullptr;
  return false;
}

static char *stx_sharedpaths(UDF_INIT *initid, UDF_ARGS *args, char *result,
                               unsigned long *length, char *is_null,
                               char *error) {
  if (initid->ptr) { free(initid->ptr); initid->ptr = nullptr; }
  if (!args->args[0] || !args->args[1]) { *is_null = 1; return nullptr; }

  uint32_t srid = extract_srid(args->args[0], args->lengths[0]);
  auto geom1 = mysql_to_geos(args->args[0], args->lengths[0]);
  if (!geom1) { *error = 1; return nullptr; }

  auto geom2 = mysql_to_geos(args->args[1], args->lengths[1]);
  if (!geom2) { *error = 1; return nullptr; }

  auto ctx = get_geos_context();
  GEOSGeomPtr paths(GEOSSharedPaths_r(ctx, geom1.get(), geom2.get()));
  if (!paths) { *error = 1; return nullptr; }

  // GEOS returns GeometryCollection(MultiLineString, MultiLineString).
  // MySQL rejects empty MultiLineStrings in WKB, so replace them with
  // empty GeometryCollections for compatibility.
  int n = GEOSGetNumGeometries_r(ctx, paths.get());
  std::vector<GEOSGeometry *> fixed;
  std::vector<GEOSGeomPtr> owned;  // prevent leaks
  bool needs_fix = false;
  for (int i = 0; i < n; ++i) {
    const GEOSGeometry *sub = GEOSGetGeometryN_r(ctx, paths.get(), i);
    if (GEOSisEmpty_r(ctx, sub)) {
      needs_fix = true;
      break;
    }
  }
  if (needs_fix) {
    for (int i = 0; i < n; ++i) {
      const GEOSGeometry *sub = GEOSGetGeometryN_r(ctx, paths.get(), i);
      if (GEOSisEmpty_r(ctx, sub)) {
        owned.emplace_back(GEOSGeom_createEmptyCollection_r(
            ctx, GEOS_GEOMETRYCOLLECTION));
        fixed.push_back(owned.back().get());
      } else {
        owned.emplace_back(GEOSGeom_clone_r(ctx, sub));
        fixed.push_back(owned.back().get());
      }
    }
    // Transfer ownership of sub-geometries to the new collection
    std::vector<GEOSGeometry *> transfer;
    for (auto &p : owned) transfer.push_back(p.release());
    paths.reset(GEOSGeom_createCollection_r(
        ctx, GEOS_GEOMETRYCOLLECTION, transfer.data(),
        static_cast<unsigned int>(transfer.size())));
    if (!paths) { *error = 1; return nullptr; }
  }

  auto wkb = geos_to_mysql(paths.get(), srid);
  if (wkb.empty()) { *error = 1; return nullptr; }
  return return_wkb(initid, wkb, result, length);
}

static void stx_sharedpaths_deinit(UDF_INIT *initid) {
  if (initid->ptr) free(initid->ptr);
}

// ----- stx_node --------------------------------------------------------------
// Fully nodes a set of linestrings, adding intersection points.
// stx_node(geom)

static bool stx_node_init(UDF_INIT *initid, UDF_ARGS *args, char *msg) {
  if (args->arg_count != 1) {
    strcpy(msg, "stx_node() requires 1 argument (geom)");
    return true;
  }
  args->arg_type[0] = STRING_RESULT;
  initid->maybe_null = 1;
  initid->ptr = nullptr;
  return false;
}

static char *stx_node(UDF_INIT *initid, UDF_ARGS *args, char *result,
                       unsigned long *length, char *is_null, char *error) {
  if (initid->ptr) { free(initid->ptr); initid->ptr = nullptr; }
  if (!args->args[0]) { *is_null = 1; return nullptr; }

  uint32_t srid = extract_srid(args->args[0], args->lengths[0]);
  auto geom = mysql_to_geos(args->args[0], args->lengths[0]);
  if (!geom) { *error = 1; return nullptr; }

  auto ctx = get_geos_context();
  GEOSGeomPtr noded(GEOSNode_r(ctx, geom.get()));
  if (!noded) { *error = 1; return nullptr; }

  auto wkb = geos_to_mysql(noded.get(), srid);
  if (wkb.empty()) { *error = 1; return nullptr; }
  return return_wkb(initid, wkb, result, length);
}

static void stx_node_deinit(UDF_INIT *initid) {
  if (initid->ptr) free(initid->ptr);
}

// ----- stx_simplifypreservetopology ------------------------------------------
// Simplifies geometry using Douglas-Peucker while preserving topology.
// Unlike ST_Simplify, this ensures polygons remain valid (no ring crossings).
// stx_simplifypreservetopology(geom, tolerance)

static bool stx_simplifypreservetopology_init(UDF_INIT *initid, UDF_ARGS *args,
                                               char *msg) {
  if (args->arg_count != 2) {
    strcpy(msg,
           "stx_simplifypreservetopology() requires 2 arguments "
           "(geom, tolerance)");
    return true;
  }
  args->arg_type[0] = STRING_RESULT;
  args->arg_type[1] = REAL_RESULT;
  initid->maybe_null = 1;
  initid->ptr = nullptr;
  return false;
}

static char *stx_simplifypreservetopology(UDF_INIT *initid, UDF_ARGS *args,
                                           char *result, unsigned long *length,
                                           char *is_null, char *error) {
  if (initid->ptr) { free(initid->ptr); initid->ptr = nullptr; }
  if (!args->args[0] || !args->args[1]) { *is_null = 1; return nullptr; }

  uint32_t srid = extract_srid(args->args[0], args->lengths[0]);
  auto geom = mysql_to_geos(args->args[0], args->lengths[0]);
  if (!geom) { *error = 1; return nullptr; }

  double tolerance = *reinterpret_cast<double *>(args->args[1]);

  auto ctx = get_geos_context();
  GEOSGeomPtr simplified(
      GEOSTopologyPreserveSimplify_r(ctx, geom.get(), tolerance));
  if (!simplified) { *error = 1; return nullptr; }

  auto wkb = geos_to_mysql(simplified.get(), srid);
  if (wkb.empty()) { *error = 1; return nullptr; }
  return return_wkb(initid, wkb, result, length);
}

static void stx_simplifypreservetopology_deinit(UDF_INIT *initid) {
  if (initid->ptr) free(initid->ptr);
}

// ----- stx_unaryunion --------------------------------------------------------
// Computes the union of all components of a geometry.
// Useful for dissolving overlapping MultiPolygons or fixing invalid geometries.
// stx_unaryunion(geom)

static bool stx_unaryunion_init(UDF_INIT *initid, UDF_ARGS *args, char *msg) {
  if (args->arg_count != 1) {
    strcpy(msg, "stx_unaryunion() requires 1 argument (geom)");
    return true;
  }
  args->arg_type[0] = STRING_RESULT;
  initid->maybe_null = 1;
  initid->ptr = nullptr;
  return false;
}

static char *stx_unaryunion(UDF_INIT *initid, UDF_ARGS *args, char *result,
                              unsigned long *length, char *is_null,
                              char *error) {
  if (initid->ptr) { free(initid->ptr); initid->ptr = nullptr; }
  if (!args->args[0]) { *is_null = 1; return nullptr; }

  uint32_t srid = extract_srid(args->args[0], args->lengths[0]);
  auto geom = mysql_to_geos(args->args[0], args->lengths[0]);
  if (!geom) { *error = 1; return nullptr; }

  auto ctx = get_geos_context();
  GEOSGeomPtr unioned(GEOSUnaryUnion_r(ctx, geom.get()));
  if (!unioned) { *error = 1; return nullptr; }

  auto wkb = geos_to_mysql(unioned.get(), srid);
  if (wkb.empty()) { *error = 1; return nullptr; }
  return return_wkb(initid, wkb, result, length);
}

static void stx_unaryunion_deinit(UDF_INIT *initid) {
  if (initid->ptr) free(initid->ptr);
}

// ----- stx_clipbyrect --------------------------------------------------------
// Fast clipping of a geometry by a 2D bounding box.
// stx_clipbyrect(geom, xmin, ymin, xmax, ymax)

static bool stx_clipbyrect_init(UDF_INIT *initid, UDF_ARGS *args, char *msg) {
  if (args->arg_count != 5) {
    strcpy(msg,
           "stx_clipbyrect() requires 5 arguments "
           "(geom, xmin, ymin, xmax, ymax)");
    return true;
  }
  args->arg_type[0] = STRING_RESULT;
  args->arg_type[1] = REAL_RESULT;
  args->arg_type[2] = REAL_RESULT;
  args->arg_type[3] = REAL_RESULT;
  args->arg_type[4] = REAL_RESULT;
  initid->maybe_null = 1;
  initid->ptr = nullptr;
  return false;
}

static char *stx_clipbyrect(UDF_INIT *initid, UDF_ARGS *args, char *result,
                              unsigned long *length, char *is_null,
                              char *error) {
  if (initid->ptr) { free(initid->ptr); initid->ptr = nullptr; }
  if (!args->args[0]) { *is_null = 1; return nullptr; }
  for (int i = 1; i <= 4; ++i) {
    if (!args->args[i]) { *is_null = 1; return nullptr; }
  }

  uint32_t srid = extract_srid(args->args[0], args->lengths[0]);
  auto geom = mysql_to_geos(args->args[0], args->lengths[0]);
  if (!geom) { *error = 1; return nullptr; }

  double xmin = *reinterpret_cast<double *>(args->args[1]);
  double ymin = *reinterpret_cast<double *>(args->args[2]);
  double xmax = *reinterpret_cast<double *>(args->args[3]);
  double ymax = *reinterpret_cast<double *>(args->args[4]);

  auto ctx = get_geos_context();
  GEOSGeomPtr clipped(
      GEOSClipByRect_r(ctx, geom.get(), xmin, ymin, xmax, ymax));
  if (!clipped) { *error = 1; return nullptr; }

  auto wkb = geos_to_mysql(clipped.get(), srid);
  if (wkb.empty()) { *error = 1; return nullptr; }
  return return_wkb(initid, wkb, result, length);
}

static void stx_clipbyrect_deinit(UDF_INIT *initid) {
  if (initid->ptr) free(initid->ptr);
}

// ----- stx_reduceprecision ---------------------------------------------------
// Reduces coordinate precision while maintaining geometry validity.
// Unlike stx_snaptogrid, collapsed geometry elements are removed and the
// result is guaranteed to be topologically valid.
// stx_reduceprecision(geom, gridsize)

static bool stx_reduceprecision_init(UDF_INIT *initid, UDF_ARGS *args,
                                      char *msg) {
  if (args->arg_count != 2) {
    strcpy(msg,
           "stx_reduceprecision() requires 2 arguments (geom, gridsize)");
    return true;
  }
  args->arg_type[0] = STRING_RESULT;
  args->arg_type[1] = REAL_RESULT;
  initid->maybe_null = 1;
  initid->ptr = nullptr;
  return false;
}

static char *stx_reduceprecision(UDF_INIT *initid, UDF_ARGS *args,
                                  char *result, unsigned long *length,
                                  char *is_null, char *error) {
  if (initid->ptr) { free(initid->ptr); initid->ptr = nullptr; }
  if (!args->args[0] || !args->args[1]) { *is_null = 1; return nullptr; }

  uint32_t srid = extract_srid(args->args[0], args->lengths[0]);
  auto geom = mysql_to_geos(args->args[0], args->lengths[0]);
  if (!geom) { *error = 1; return nullptr; }

  double gridsize = *reinterpret_cast<double *>(args->args[1]);

  auto ctx = get_geos_context();
  // flags=0: GEOS_PREC_VALID_OUTPUT (default, guarantees valid output)
  GEOSGeomPtr reduced(
      GEOSGeom_setPrecision_r(ctx, geom.get(), gridsize, 0));
  if (!reduced) { *error = 1; return nullptr; }

  auto wkb = geos_to_mysql(reduced.get(), srid);
  if (wkb.empty()) { *error = 1; return nullptr; }
  return return_wkb(initid, wkb, result, length);
}

static void stx_reduceprecision_deinit(UDF_INIT *initid) {
  if (initid->ptr) free(initid->ptr);
}

// ----- stx_maximuminscribedcircle --------------------------------------------
// Computes the maximum inscribed circle of a geometry.
// Returns a LineString from the center to the nearest boundary point.
// stx_maximuminscribedcircle(geom, tolerance)

static bool stx_maximuminscribedcircle_init(UDF_INIT *initid, UDF_ARGS *args,
                                             char *msg) {
  if (args->arg_count != 2) {
    strcpy(msg,
           "stx_maximuminscribedcircle() requires 2 arguments "
           "(geom, tolerance)");
    return true;
  }
  args->arg_type[0] = STRING_RESULT;
  args->arg_type[1] = REAL_RESULT;
  initid->maybe_null = 1;
  initid->ptr = nullptr;
  return false;
}

static char *stx_maximuminscribedcircle(UDF_INIT *initid, UDF_ARGS *args,
                                         char *result, unsigned long *length,
                                         char *is_null, char *error) {
  if (initid->ptr) { free(initid->ptr); initid->ptr = nullptr; }
  if (!args->args[0] || !args->args[1]) { *is_null = 1; return nullptr; }

  uint32_t srid = extract_srid(args->args[0], args->lengths[0]);
  auto geom = mysql_to_geos(args->args[0], args->lengths[0]);
  if (!geom) { *error = 1; return nullptr; }

  double tolerance = *reinterpret_cast<double *>(args->args[1]);

  auto ctx = get_geos_context();
  GEOSGeomPtr circle(
      GEOSMaximumInscribedCircle_r(ctx, geom.get(), tolerance));
  if (!circle) { *error = 1; return nullptr; }

  auto wkb = geos_to_mysql(circle.get(), srid);
  if (wkb.empty()) { *error = 1; return nullptr; }
  return return_wkb(initid, wkb, result, length);
}

static void stx_maximuminscribedcircle_deinit(UDF_INIT *initid) {
  if (initid->ptr) free(initid->ptr);
}

// ----- stx_minimumwidth ------------------------------------------------------
// Returns a LineString representing the minimum width of a geometry.
// The length of the returned LineString is the minimum diameter.
// stx_minimumwidth(geom)

static bool stx_minimumwidth_init(UDF_INIT *initid, UDF_ARGS *args,
                                   char *msg) {
  if (args->arg_count != 1) {
    strcpy(msg, "stx_minimumwidth() requires 1 argument (geom)");
    return true;
  }
  args->arg_type[0] = STRING_RESULT;
  initid->maybe_null = 1;
  initid->ptr = nullptr;
  return false;
}

static char *stx_minimumwidth(UDF_INIT *initid, UDF_ARGS *args, char *result,
                               unsigned long *length, char *is_null,
                               char *error) {
  if (initid->ptr) { free(initid->ptr); initid->ptr = nullptr; }
  if (!args->args[0]) { *is_null = 1; return nullptr; }

  uint32_t srid = extract_srid(args->args[0], args->lengths[0]);
  auto geom = mysql_to_geos(args->args[0], args->lengths[0]);
  if (!geom) { *error = 1; return nullptr; }

  auto ctx = get_geos_context();
  GEOSGeomPtr width(GEOSMinimumWidth_r(ctx, geom.get()));
  if (!width) { *error = 1; return nullptr; }

  auto wkb = geos_to_mysql(width.get(), srid);
  if (wkb.empty()) { *error = 1; return nullptr; }
  return return_wkb(initid, wkb, result, length);
}

static void stx_minimumwidth_deinit(UDF_INIT *initid) {
  if (initid->ptr) free(initid->ptr);
}

// ----- stx_simplifypolygonhull -----------------------------------------------
// Computes a simplified hull of a polygonal geometry.
// vertex_fraction: 1.0 = original, 0.0 = minimal (outer=convex hull, inner=triangle).
// is_outer: 1 = outer hull (default), 0 = inner hull.
// stx_simplifypolygonhull(geom, vertex_fraction [, is_outer])

static bool stx_simplifypolygonhull_init(UDF_INIT *initid, UDF_ARGS *args,
                                          char *msg) {
  if (args->arg_count < 2 || args->arg_count > 3) {
    strcpy(msg,
           "stx_simplifypolygonhull() requires 2-3 arguments "
           "(geom, vertex_fraction [, is_outer])");
    return true;
  }
  args->arg_type[0] = STRING_RESULT;
  args->arg_type[1] = REAL_RESULT;
  if (args->arg_count >= 3) args->arg_type[2] = INT_RESULT;
  initid->maybe_null = 1;
  initid->ptr = nullptr;
  return false;
}

static char *stx_simplifypolygonhull(UDF_INIT *initid, UDF_ARGS *args,
                                      char *result, unsigned long *length,
                                      char *is_null, char *error) {
  if (initid->ptr) { free(initid->ptr); initid->ptr = nullptr; }
  if (!args->args[0] || !args->args[1]) { *is_null = 1; return nullptr; }

  uint32_t srid = extract_srid(args->args[0], args->lengths[0]);
  auto geom = mysql_to_geos(args->args[0], args->lengths[0]);
  if (!geom) { *error = 1; return nullptr; }

  double vertex_fraction = *reinterpret_cast<double *>(args->args[1]);

  unsigned int is_outer = 1;  // default: outer hull
  if (args->arg_count >= 3 && args->args[2])
    is_outer = (*reinterpret_cast<long long *>(args->args[2])) ? 1 : 0;

  auto ctx = get_geos_context();
  GEOSGeomPtr hull(
      GEOSPolygonHullSimplify_r(ctx, geom.get(), is_outer, vertex_fraction));
  if (!hull) { *error = 1; return nullptr; }

  auto wkb = geos_to_mysql(hull.get(), srid);
  if (wkb.empty()) { *error = 1; return nullptr; }
  return return_wkb(initid, wkb, result, length);
}

static void stx_simplifypolygonhull_deinit(UDF_INIT *initid) {
  if (initid->ptr) free(initid->ptr);
}

// ----- stx_concavehullofpolygons ---------------------------------------------
// Computes the concave hull of a set of polygons.
// Polygons are treated as constraints (the hull contains them).
// stx_concavehullofpolygons(geom, ratio [, allow_holes])

static bool stx_concavehullofpolygons_init(UDF_INIT *initid, UDF_ARGS *args,
                                            char *msg) {
  if (args->arg_count < 2 || args->arg_count > 3) {
    strcpy(msg,
           "stx_concavehullofpolygons() requires 2-3 arguments "
           "(geom, ratio [, allow_holes])");
    return true;
  }
  args->arg_type[0] = STRING_RESULT;
  args->arg_type[1] = REAL_RESULT;
  if (args->arg_count >= 3) args->arg_type[2] = INT_RESULT;
  initid->maybe_null = 1;
  initid->ptr = nullptr;
  return false;
}

static char *stx_concavehullofpolygons(UDF_INIT *initid, UDF_ARGS *args,
                                        char *result, unsigned long *length,
                                        char *is_null, char *error) {
  if (initid->ptr) { free(initid->ptr); initid->ptr = nullptr; }
  if (!args->args[0] || !args->args[1]) { *is_null = 1; return nullptr; }

  uint32_t srid = extract_srid(args->args[0], args->lengths[0]);
  auto geom = mysql_to_geos(args->args[0], args->lengths[0]);
  if (!geom) { *error = 1; return nullptr; }

  double ratio = *reinterpret_cast<double *>(args->args[1]);

  // isTight=1: the hull tightly follows the input polygons
  unsigned int is_tight = 1;
  unsigned int allow_holes = 0;
  if (args->arg_count >= 3 && args->args[2])
    allow_holes = (*reinterpret_cast<long long *>(args->args[2])) ? 1 : 0;

  auto ctx = get_geos_context();
  GEOSGeomPtr hull(GEOSConcaveHullOfPolygons_r(ctx, geom.get(), ratio,
                                                 is_tight, allow_holes));
  if (!hull) { *error = 1; return nullptr; }

  auto wkb = geos_to_mysql(hull.get(), srid);
  if (wkb.empty()) { *error = 1; return nullptr; }
  return return_wkb(initid, wkb, result, length);
}

static void stx_concavehullofpolygons_deinit(UDF_INIT *initid) {
  if (initid->ptr) free(initid->ptr);
}

// ----- stx_npoints -----------------------------------------------------------
// Returns the total number of vertices in any geometry type.
// Unlike MySQL's ST_NumPoints() which only works for LineString,
// stx_npoints works for all geometry types (Point, LineString, Polygon,
// Multi*, GeometryCollection).

// Helper: count points from raw WKB (skipping 4-byte SRID prefix)
static long long count_points_wkb(const char *data, size_t length) {
  if (!data || length < 9) return -1;

  const unsigned char *p = reinterpret_cast<const unsigned char *>(data + 4);
  const unsigned char *end = reinterpret_cast<const unsigned char *>(data) + length;

  // Read byte order
  if (p >= end) return -1;
  bool le = (*p == 0x01);
  p++;

  auto read_u32 = [&]() -> uint32_t {
    if (p + 4 > end) return 0;
    uint32_t v;
    std::memcpy(&v, p, 4);
    p += 4;
    if (!le) v = __builtin_bswap32(v);
    return v;
  };

  uint32_t type = read_u32();

  switch (type) {
    case 1:  // Point
      return 1;
    case 2: {  // LineString
      uint32_t n = read_u32();
      return static_cast<long long>(n);
    }
    case 3: {  // Polygon
      uint32_t num_rings = read_u32();
      long long total = 0;
      for (uint32_t i = 0; i < num_rings; ++i) {
        uint32_t n = read_u32();
        total += n;
        p += n * 16;  // skip coordinates (2 doubles per point)
      }
      return total;
    }
    case 4: {  // MultiPoint
      uint32_t count = read_u32();
      return static_cast<long long>(count);
    }
    case 5: {  // MultiLineString
      uint32_t count = read_u32();
      long long total = 0;
      for (uint32_t i = 0; i < count; ++i) {
        p++;  // byte order
        p += 4;  // type
        uint32_t n = read_u32();
        total += n;
        p += n * 16;
      }
      return total;
    }
    case 6: {  // MultiPolygon
      uint32_t count = read_u32();
      long long total = 0;
      for (uint32_t i = 0; i < count; ++i) {
        p++;  // byte order
        p += 4;  // type
        uint32_t num_rings = read_u32();
        for (uint32_t j = 0; j < num_rings; ++j) {
          uint32_t n = read_u32();
          total += n;
          p += n * 16;
        }
      }
      return total;
    }
    case 7: {  // GeometryCollection - use GEOS for accurate counting
      auto geom = mysql_to_geos(data, length);
      if (!geom) return -1;
      auto ctx = get_geos_context();
      return static_cast<long long>(GEOSGetNumCoordinates_r(ctx, geom.get()));
    }
    default:
      return -1;
  }
}

static bool stx_npoints_init(UDF_INIT *initid, UDF_ARGS *args, char *msg) {
  if (args->arg_count != 1) {
    strcpy(msg, "stx_npoints() requires 1 argument");
    return true;
  }
  args->arg_type[0] = STRING_RESULT;
  initid->maybe_null = 1;
  return false;
}

static long long stx_npoints(UDF_INIT *, UDF_ARGS *args, char *is_null,
                              char *error) {
  if (!args->args[0]) { *is_null = 1; return 0; }
  long long n = count_points_wkb(args->args[0], args->lengths[0]);
  if (n < 0) { *error = 1; return 0; }
  return n;
}

static void stx_npoints_deinit(UDF_INIT *) {}

// ----- stx_makeline ----------------------------------------------------------
// Creates a LineString from two Points or from a MultiPoint.
// stx_makeline(point1, point2) -> LineString
// stx_makeline(multipoint) -> LineString

static bool stx_makeline_init(UDF_INIT *initid, UDF_ARGS *args, char *msg) {
  if (args->arg_count < 1 || args->arg_count > 2) {
    strcpy(msg,
           "stx_makeline() requires 1 or 2 arguments "
           "(multipoint) or (point1, point2)");
    return true;
  }
  args->arg_type[0] = STRING_RESULT;
  if (args->arg_count == 2) args->arg_type[1] = STRING_RESULT;
  initid->maybe_null = 1;
  initid->ptr = nullptr;
  return false;
}

static char *stx_makeline(UDF_INIT *initid, UDF_ARGS *args, char *result,
                           unsigned long *length, char *is_null, char *error) {
  if (initid->ptr) { free(initid->ptr); initid->ptr = nullptr; }
  if (!args->args[0]) { *is_null = 1; return nullptr; }

  if (args->arg_count == 2) {
    // Two points -> LineString
    if (!args->args[1]) { *is_null = 1; return nullptr; }

    auto r1 = parse_geometry(args->args[0], args->lengths[0]);
    auto r2 = parse_geometry(args->args[1], args->lengths[1]);
    if (!r1 || !r2) { *error = 1; return nullptr; }
    if (r1->type != GeometryType::Point) {
      my_error(ER_UNEXPECTED_GEOMETRY_TYPE, MYF(0),
               "POINT", geometry_type_name(r1->type), "stx_makeline");
      *error = 1; return nullptr;
    }
    if (r2->type != GeometryType::Point) {
      my_error(ER_UNEXPECTED_GEOMETRY_TYPE, MYF(0),
               "POINT", geometry_type_name(r2->type), "stx_makeline");
      *error = 1; return nullptr;
    }

    std::string wkb;
    if (auto *cv1 = std::get_if<CartesianVariant>(&r1->geometry)) {
      auto *cv2 = std::get_if<CartesianVariant>(&r2->geometry);
      if (!cv2) { *error = 1; return nullptr; }
      auto &p1 = std::get<Point>(*cv1);
      auto &p2 = std::get<Point>(*cv2);
      Linestring ls;
      ls.push_back(p1);
      ls.push_back(p2);
      wkb = write_linestring(r1->srid, ls);
    } else if (auto *gv1 = std::get_if<GeographicVariant>(&r1->geometry)) {
      auto *gv2 = std::get_if<GeographicVariant>(&r2->geometry);
      if (!gv2) { *error = 1; return nullptr; }
      auto &p1 = std::get<GeoPoint>(*gv1);
      auto &p2 = std::get<GeoPoint>(*gv2);
      GeoLinestring ls;
      ls.push_back(p1);
      ls.push_back(p2);
      wkb = write_linestring(r1->srid, ls);
    } else {
      *error = 1; return nullptr;
    }
    return return_wkb(initid, wkb, result, length);

  } else {
    // Single argument: MultiPoint -> LineString
    auto r = parse_geometry(args->args[0], args->lengths[0]);
    if (!r) { *error = 1; return nullptr; }
    if (r->type != GeometryType::MultiPoint) {
      my_error(ER_UNEXPECTED_GEOMETRY_TYPE, MYF(0),
               "MULTIPOINT", geometry_type_name(r->type), "stx_makeline");
      *error = 1; return nullptr;
    }

    std::string wkb;
    if (auto *cv = std::get_if<CartesianVariant>(&r->geometry)) {
      auto &mp = std::get<MultiPoint>(*cv);
      if (mp.size() < 2) { *error = 1; return nullptr; }
      Linestring ls(mp.begin(), mp.end());
      wkb = write_linestring(r->srid, ls);
    } else if (auto *gv = std::get_if<GeographicVariant>(&r->geometry)) {
      auto &mp = std::get<GeoMultiPoint>(*gv);
      if (mp.size() < 2) { *error = 1; return nullptr; }
      GeoLinestring ls(mp.begin(), mp.end());
      wkb = write_linestring(r->srid, ls);
    } else {
      *error = 1; return nullptr;
    }
    return return_wkb(initid, wkb, result, length);
  }
}

static void stx_makeline_deinit(UDF_INIT *initid) {
  if (initid->ptr) free(initid->ptr);
}

// ----- stx_makepolygon -------------------------------------------------------
// Creates a Polygon from a closed LineString (outer ring),
// with optional inner rings as a MultiLineString.
// stx_makepolygon(outer_ring) -> Polygon
// stx_makepolygon(outer_ring, inner_rings_multilinestring) -> Polygon

static bool stx_makepolygon_init(UDF_INIT *initid, UDF_ARGS *args, char *msg) {
  if (args->arg_count < 1 || args->arg_count > 2) {
    strcpy(msg,
           "stx_makepolygon() requires 1 or 2 arguments "
           "(outer_ring [, inner_rings])");
    return true;
  }
  args->arg_type[0] = STRING_RESULT;
  if (args->arg_count == 2) args->arg_type[1] = STRING_RESULT;
  initid->maybe_null = 1;
  initid->ptr = nullptr;
  return false;
}

static char *stx_makepolygon(UDF_INIT *initid, UDF_ARGS *args, char *result,
                              unsigned long *length, char *is_null,
                              char *error) {
  if (initid->ptr) { free(initid->ptr); initid->ptr = nullptr; }
  if (!args->args[0]) { *is_null = 1; return nullptr; }

  auto r_outer = parse_geometry(args->args[0], args->lengths[0]);
  if (!r_outer) { *error = 1; return nullptr; }
  if (r_outer->type != GeometryType::LineString) {
    my_error(ER_UNEXPECTED_GEOMETRY_TYPE, MYF(0),
             "LINESTRING", geometry_type_name(r_outer->type),
             "stx_makepolygon");
    *error = 1; return nullptr;
  }

  std::string wkb;
  if (auto *cv = std::get_if<CartesianVariant>(&r_outer->geometry)) {
    auto &outer_ls = std::get<Linestring>(*cv);
    if (outer_ls.size() < 4) { *error = 1; return nullptr; }
    Polygon poly;
    poly.outer().assign(outer_ls.begin(), outer_ls.end());

    if (args->arg_count == 2 && args->args[1]) {
      auto r_inner = parse_geometry(args->args[1], args->lengths[1]);
      if (!r_inner) { *error = 1; return nullptr; }
      if (r_inner->type != GeometryType::MultiLineString) {
        my_error(ER_UNEXPECTED_GEOMETRY_TYPE, MYF(0),
                 "MULTILINESTRING", geometry_type_name(r_inner->type),
                 "stx_makepolygon");
        *error = 1; return nullptr;
      }
      auto *cv_inner = std::get_if<CartesianVariant>(&r_inner->geometry);
      if (!cv_inner) { *error = 1; return nullptr; }
      auto &mls = std::get<MultiLinestring>(*cv_inner);
      poly.inners().resize(mls.size());
      for (size_t i = 0; i < mls.size(); ++i)
        poly.inners()[i].assign(mls[i].begin(), mls[i].end());
    }
    bg::correct(poly);
    wkb = write_polygon(r_outer->srid, poly);
  } else if (auto *gv = std::get_if<GeographicVariant>(&r_outer->geometry)) {
    auto &outer_ls = std::get<GeoLinestring>(*gv);
    if (outer_ls.size() < 4) { *error = 1; return nullptr; }
    GeoPolygon poly;
    poly.outer().assign(outer_ls.begin(), outer_ls.end());

    if (args->arg_count == 2 && args->args[1]) {
      auto r_inner = parse_geometry(args->args[1], args->lengths[1]);
      if (!r_inner) { *error = 1; return nullptr; }
      if (r_inner->type != GeometryType::MultiLineString) {
        my_error(ER_UNEXPECTED_GEOMETRY_TYPE, MYF(0),
                 "MULTILINESTRING", geometry_type_name(r_inner->type),
                 "stx_makepolygon");
        *error = 1; return nullptr;
      }
      auto *gv_inner = std::get_if<GeographicVariant>(&r_inner->geometry);
      if (!gv_inner) { *error = 1; return nullptr; }
      auto &mls = std::get<GeoMultiLinestring>(*gv_inner);
      poly.inners().resize(mls.size());
      for (size_t i = 0; i < mls.size(); ++i)
        poly.inners()[i].assign(mls[i].begin(), mls[i].end());
    }
    bg::correct(poly);
    wkb = write_polygon(r_outer->srid, poly);
  } else {
    *error = 1; return nullptr;
  }
  return return_wkb(initid, wkb, result, length);
}

static void stx_makepolygon_deinit(UDF_INIT *initid) {
  if (initid->ptr) free(initid->ptr);
}

// ----- stx_points ------------------------------------------------------------
// Extracts all vertices from any geometry type and returns them as MultiPoint.

// Helper: collect points from raw WKB (skipping 4-byte SRID prefix)
static bool collect_points_wkb(const char *data, size_t length,
                                std::vector<std::pair<double, double>> &pts) {
  if (!data || length < 9) return false;

  const unsigned char *p = reinterpret_cast<const unsigned char *>(data + 4);
  const unsigned char *end = reinterpret_cast<const unsigned char *>(data) + length;

  if (p >= end) return false;
  bool le = (*p == 0x01);
  p++;

  auto read_u32 = [&]() -> uint32_t {
    if (p + 4 > end) return 0;
    uint32_t v;
    std::memcpy(&v, p, 4);
    p += 4;
    if (!le) v = __builtin_bswap32(v);
    return v;
  };

  auto read_dbl = [&]() -> double {
    if (p + 8 > end) return 0.0;
    double v;
    uint64_t raw;
    std::memcpy(&raw, p, 8);
    p += 8;
    if (!le) {
      raw = ((raw & 0x00000000FFFFFFFFull) << 32) |
            ((raw & 0xFFFFFFFF00000000ull) >> 32);
      raw = ((raw & 0x0000FFFF0000FFFFull) << 16) |
            ((raw & 0xFFFF0000FFFF0000ull) >> 16);
      raw = ((raw & 0x00FF00FF00FF00FFull) << 8) |
            ((raw & 0xFF00FF00FF00FF00ull) >> 8);
    }
    std::memcpy(&v, &raw, 8);
    return v;
  };

  auto read_point = [&]() {
    double x = read_dbl();
    double y = read_dbl();
    pts.emplace_back(x, y);
  };

  uint32_t type = read_u32();

  switch (type) {
    case 1:  // Point
      read_point();
      return true;
    case 2: {  // LineString
      uint32_t n = read_u32();
      for (uint32_t i = 0; i < n; ++i) read_point();
      return true;
    }
    case 3: {  // Polygon
      uint32_t num_rings = read_u32();
      for (uint32_t i = 0; i < num_rings; ++i) {
        uint32_t n = read_u32();
        for (uint32_t j = 0; j < n; ++j) read_point();
      }
      return true;
    }
    case 4: {  // MultiPoint
      uint32_t count = read_u32();
      for (uint32_t i = 0; i < count; ++i) {
        p++;  // byte order
        p += 4;  // type
        read_point();
      }
      return true;
    }
    case 5: {  // MultiLineString
      uint32_t count = read_u32();
      for (uint32_t i = 0; i < count; ++i) {
        p++;  // byte order
        p += 4;  // type
        uint32_t n = read_u32();
        for (uint32_t j = 0; j < n; ++j) read_point();
      }
      return true;
    }
    case 6: {  // MultiPolygon
      uint32_t count = read_u32();
      for (uint32_t i = 0; i < count; ++i) {
        p++;  // byte order
        p += 4;  // type
        uint32_t num_rings = read_u32();
        for (uint32_t j = 0; j < num_rings; ++j) {
          uint32_t n = read_u32();
          for (uint32_t k = 0; k < n; ++k) read_point();
        }
      }
      return true;
    }
    case 7: {  // GeometryCollection - use GEOS
      auto geom = mysql_to_geos(data, length);
      if (!geom) return false;
      auto ctx = get_geos_context();
      const GEOSCoordSequence *cs = nullptr;
      // Flatten via WKT round-trip is complex; use coordinate extraction
      int ncoords = GEOSGetNumCoordinates_r(ctx, geom.get());
      if (ncoords < 0) return false;
      // Recursively extract from sub-geometries
      int ngeoms = GEOSGetNumGeometries_r(ctx, geom.get());
      for (int i = 0; i < ngeoms; ++i) {
        const GEOSGeometry *sub = GEOSGetGeometryN_r(ctx, geom.get(), i);
        if (!sub) continue;
        auto sub_wkb = geos_to_mysql(sub, 0);
        if (!sub_wkb.empty()) {
          // Overwrite SRID with 0 (already done)
          collect_points_wkb(sub_wkb.data(), sub_wkb.size(), pts);
        }
      }
      return true;
    }
    default:
      return false;
  }
}

static bool stx_points_init(UDF_INIT *initid, UDF_ARGS *args, char *msg) {
  if (args->arg_count != 1) {
    strcpy(msg, "stx_points() requires 1 argument");
    return true;
  }
  args->arg_type[0] = STRING_RESULT;
  initid->maybe_null = 1;
  initid->ptr = nullptr;
  return false;
}

static char *stx_points(UDF_INIT *initid, UDF_ARGS *args, char *result,
                         unsigned long *length, char *is_null, char *error) {
  if (initid->ptr) { free(initid->ptr); initid->ptr = nullptr; }
  if (!args->args[0]) { *is_null = 1; return nullptr; }

  uint32_t srid = extract_srid(args->args[0], args->lengths[0]);

  std::vector<std::pair<double, double>> pts;
  if (!collect_points_wkb(args->args[0], args->lengths[0], pts)) {
    *error = 1; return nullptr;
  }

  // Build MultiPoint WKB
  std::string buf;
  buf.reserve(4 + 5 + 4 + pts.size() * 21);
  detail::append_uint32(buf, srid);
  detail::append_wkb_header(buf, GeometryType::MultiPoint);
  detail::append_uint32(buf, static_cast<uint32_t>(pts.size()));
  for (const auto &[x, y] : pts) {
    detail::append_wkb_header(buf, GeometryType::Point);
    detail::append_double(buf, x);
    detail::append_double(buf, y);
  }

  return return_wkb(initid, buf, result, length);
}

static void stx_points_deinit(UDF_INIT *initid) {
  if (initid->ptr) free(initid->ptr);
}

// ----- stx_isring ------------------------------------------------------------
// Returns 1 if the LineString is a ring (closed and simple), 0 otherwise.
// Uses GEOS GEOSisRing().

static bool stx_isring_init(UDF_INIT *initid, UDF_ARGS *args, char *msg) {
  if (args->arg_count != 1) {
    strcpy(msg, "stx_isring() requires 1 argument (linestring)");
    return true;
  }
  args->arg_type[0] = STRING_RESULT;
  initid->maybe_null = 1;
  return false;
}

static long long stx_isring(UDF_INIT *, UDF_ARGS *args, char *is_null,
                             char *error) {
  if (!args->args[0]) { *is_null = 1; return 0; }

  // GEOSisRing only works on LineString — check type from WKB header
  uint32_t wkb_type = 0;
  if (args->lengths[0] >= 9) std::memcpy(&wkb_type, args->args[0] + 5, 4);
  GeometryType gt = static_cast<GeometryType>(wkb_type);
  if (gt != GeometryType::LineString) {
    my_error(ER_UNEXPECTED_GEOMETRY_TYPE, MYF(0),
             "LINESTRING", geometry_type_name(gt), "stx_isring");
    *error = 1; return 0;
  }

  auto geom = mysql_to_geos(args->args[0], args->lengths[0]);
  if (!geom) { *error = 1; return 0; }

  auto ctx = get_geos_context();
  char result = GEOSisRing_r(ctx, geom.get());
  if (result == 2) { *error = 1; return 0; }  // GEOS error
  return result ? 1 : 0;
}

static void stx_isring_deinit(UDF_INIT *) {}

// ----- stx_shortestline ------------------------------------------------------
// Returns the shortest line (LineString) between two geometries.
// Uses GEOS GEOSNearestPoints().

static bool stx_shortestline_init(UDF_INIT *initid, UDF_ARGS *args,
                                   char *msg) {
  if (args->arg_count != 2) {
    strcpy(msg, "stx_shortestline() requires 2 arguments");
    return true;
  }
  args->arg_type[0] = STRING_RESULT;
  args->arg_type[1] = STRING_RESULT;
  initid->maybe_null = 1;
  initid->ptr = nullptr;
  return false;
}

static char *stx_shortestline(UDF_INIT *initid, UDF_ARGS *args, char *result,
                               unsigned long *length, char *is_null,
                               char *error) {
  if (initid->ptr) { free(initid->ptr); initid->ptr = nullptr; }
  if (!args->args[0] || !args->args[1]) { *is_null = 1; return nullptr; }

  uint32_t srid = extract_srid(args->args[0], args->lengths[0]);

  auto geom1 = mysql_to_geos(args->args[0], args->lengths[0]);
  auto geom2 = mysql_to_geos(args->args[1], args->lengths[1]);
  if (!geom1 || !geom2) { *error = 1; return nullptr; }

  auto ctx = get_geos_context();
  GEOSCoordSequence *cs = GEOSNearestPoints_r(ctx, geom1.get(), geom2.get());
  if (!cs) { *error = 1; return nullptr; }

  double x1, y1, x2, y2;
  GEOSCoordSeq_getX_r(ctx, cs, 0, &x1);
  GEOSCoordSeq_getY_r(ctx, cs, 0, &y1);
  GEOSCoordSeq_getX_r(ctx, cs, 1, &x2);
  GEOSCoordSeq_getY_r(ctx, cs, 1, &y2);
  GEOSCoordSeq_destroy_r(ctx, cs);

  // Build a LineString WKB: SRID + header + 2 points
  std::string buf;
  buf.reserve(4 + 5 + 4 + 32);
  detail::append_uint32(buf, srid);
  detail::append_wkb_header(buf, GeometryType::LineString);
  detail::append_uint32(buf, 2);
  detail::append_double(buf, x1);
  detail::append_double(buf, y1);
  detail::append_double(buf, x2);
  detail::append_double(buf, y2);

  return return_wkb(initid, buf, result, length);
}

static void stx_shortestline_deinit(UDF_INIT *initid) {
  if (initid->ptr) free(initid->ptr);
}

// =============================================================================
// STX_dms2deg — DMS to decimal degrees
// =============================================================================

static bool stx_dms2deg_init(UDF_INIT *initid, UDF_ARGS *args,
                             char *message) {
  if (args->arg_count < 2 || args->arg_count > 3) {
    snprintf(message, MYSQL_ERRMSG_SIZE,
             "STX_dms2deg() requires 2 or 3 arguments: (degrees, minutes [, seconds])");
    return true;
  }
  for (unsigned i = 0; i < args->arg_count; i++)
    args->arg_type[i] = REAL_RESULT;
  initid->decimals = 31;
  initid->maybe_null = true;
  return false;
}

static double stx_dms2deg(UDF_INIT * /*initid*/, UDF_ARGS *args,
                           unsigned char *is_null,
                           unsigned char * /*error*/) {
  if (!args->args[0] || !args->args[1]) {
    *is_null = 1;
    return 0;
  }
  double d = *reinterpret_cast<double *>(args->args[0]);
  double m = *reinterpret_cast<double *>(args->args[1]);
  double s = 0.0;
  if (args->arg_count >= 3 && args->args[2])
    s = *reinterpret_cast<double *>(args->args[2]);

  double sign = (d < 0) ? -1.0 : 1.0;
  double result = sign * (std::fabs(d) + m / 60.0 + s / 3600.0);
  return result;
}

static void stx_dms2deg_deinit(UDF_INIT * /*initid*/) {}

// =============================================================================
// STX_deg2dms_deg — decimal degrees to DMS (degree part)
// =============================================================================

static bool stx_deg2dms_deg_init(UDF_INIT *initid, UDF_ARGS *args,
                                 char *message) {
  if (args->arg_count != 1) {
    snprintf(message, MYSQL_ERRMSG_SIZE,
             "STX_deg2dms_deg() requires exactly 1 argument");
    return true;
  }
  args->arg_type[0] = REAL_RESULT;
  initid->maybe_null = true;
  return false;
}

static long long stx_deg2dms_deg(UDF_INIT * /*initid*/, UDF_ARGS *args,
                                  unsigned char *is_null,
                                  unsigned char * /*error*/) {
  if (!args->args[0]) {
    *is_null = 1;
    return 0;
  }
  double d = *reinterpret_cast<double *>(args->args[0]);
  double sign = (d < 0) ? -1.0 : 1.0;
  long long deg = static_cast<long long>(std::floor(std::fabs(d)));
  return static_cast<long long>(sign) * deg;
}

static void stx_deg2dms_deg_deinit(UDF_INIT * /*initid*/) {}

// =============================================================================
// STX_deg2dms_min — decimal degrees to DMS (minute part)
// =============================================================================

static bool stx_deg2dms_min_init(UDF_INIT *initid, UDF_ARGS *args,
                                 char *message) {
  if (args->arg_count != 1) {
    snprintf(message, MYSQL_ERRMSG_SIZE,
             "STX_deg2dms_min() requires exactly 1 argument");
    return true;
  }
  args->arg_type[0] = REAL_RESULT;
  initid->maybe_null = true;
  return false;
}

static long long stx_deg2dms_min(UDF_INIT * /*initid*/, UDF_ARGS *args,
                                  unsigned char *is_null,
                                  unsigned char * /*error*/) {
  if (!args->args[0]) {
    *is_null = 1;
    return 0;
  }
  double d = *reinterpret_cast<double *>(args->args[0]);
  double frac = std::fabs(d) - std::floor(std::fabs(d));
  double min_total = frac * 60.0;
  return static_cast<long long>(std::floor(min_total));
}

static void stx_deg2dms_min_deinit(UDF_INIT * /*initid*/) {}

// =============================================================================
// STX_deg2dms_sec — decimal degrees to DMS (second part)
// =============================================================================

static bool stx_deg2dms_sec_init(UDF_INIT *initid, UDF_ARGS *args,
                                 char *message) {
  if (args->arg_count != 1) {
    snprintf(message, MYSQL_ERRMSG_SIZE,
             "STX_deg2dms_sec() requires exactly 1 argument");
    return true;
  }
  args->arg_type[0] = REAL_RESULT;
  initid->decimals = 31;
  initid->maybe_null = true;
  return false;
}

static double stx_deg2dms_sec(UDF_INIT * /*initid*/, UDF_ARGS *args,
                               unsigned char *is_null,
                               unsigned char * /*error*/) {
  if (!args->args[0]) {
    *is_null = 1;
    return 0;
  }
  double d = *reinterpret_cast<double *>(args->args[0]);
  double frac = std::fabs(d) - std::floor(std::fabs(d));
  double min_total = frac * 60.0;
  double sec = (min_total - std::floor(min_total)) * 60.0;
  return sec;
}

static void stx_deg2dms_sec_deinit(UDF_INIT * /*initid*/) {}

}  // extern "C"

// =============================================================================
// UDF registration table
// =============================================================================

struct udf_entry {
  const char *name;
  Item_result return_type;
  Udf_func_any func;
  Udf_func_init init;
  Udf_func_deinit deinit;
};

static const udf_entry udf_table[] = {
    {"stx_perimeter", REAL_RESULT, (Udf_func_any)stx_perimeter,
     stx_perimeter_init, stx_perimeter_deinit},
    {"stx_coveredby", INT_RESULT, (Udf_func_any)stx_coveredby,
     stx_coveredby_init, stx_coveredby_deinit},
    {"stx_covers", INT_RESULT, (Udf_func_any)stx_covers, stx_covers_init,
     stx_covers_deinit},
    {"stx_dwithin", INT_RESULT, (Udf_func_any)stx_dwithin, stx_dwithin_init,
     stx_dwithin_deinit},
    {"stx_azimuth", REAL_RESULT, (Udf_func_any)stx_azimuth, stx_azimuth_init,
     stx_azimuth_deinit},
    {"stx_project", STRING_RESULT, (Udf_func_any)stx_project,
     stx_project_init, stx_project_deinit},
    {"stx_linelocatepoint", REAL_RESULT, (Udf_func_any)stx_linelocatepoint,
     stx_linelocatepoint_init, stx_linelocatepoint_deinit},
    {"stx_linesubstring", STRING_RESULT, (Udf_func_any)stx_linesubstring,
     stx_linesubstring_init, stx_linesubstring_deinit},
    {"stx_angle", REAL_RESULT, (Udf_func_any)stx_angle, stx_angle_init,
     stx_angle_deinit},
    {"stx_translate", STRING_RESULT, (Udf_func_any)stx_translate,
     stx_translate_init, stx_translate_deinit},
    {"stx_translate_latlon", STRING_RESULT,
     (Udf_func_any)stx_translate_latlon, stx_translate_latlon_init,
     stx_translate_latlon_deinit},
    {"stx_scale", STRING_RESULT, (Udf_func_any)stx_scale, stx_scale_init,
     stx_scale_deinit},
    {"stx_rotate", STRING_RESULT, (Udf_func_any)stx_rotate, stx_rotate_init,
     stx_rotate_deinit},
    {"stx_reverse", STRING_RESULT, (Udf_func_any)stx_reverse,
     stx_reverse_init, stx_reverse_deinit},
    {"stx_pointonsurface", STRING_RESULT, (Udf_func_any)stx_pointonsurface,
     stx_pointonsurface_init, stx_pointonsurface_deinit},
    {"stx_closestpoint", STRING_RESULT, (Udf_func_any)stx_closestpoint,
     stx_closestpoint_init, stx_closestpoint_deinit},
    {"stx_relate", STRING_RESULT, (Udf_func_any)stx_relate, stx_relate_init,
     stx_relate_deinit},
    {"stx_relatematch", INT_RESULT, (Udf_func_any)stx_relatematch,
     stx_relatematch_init, stx_relatematch_deinit},
    {"stx_makepoint", STRING_RESULT, (Udf_func_any)stx_makepoint,
     stx_makepoint_init, stx_makepoint_deinit},
    {"stx_affine", STRING_RESULT, (Udf_func_any)stx_affine, stx_affine_init,
     stx_affine_deinit},
    {"stx_snaptogrid", STRING_RESULT, (Udf_func_any)stx_snaptogrid,
     stx_snaptogrid_init, stx_snaptogrid_deinit},
    {"stx_removerepeatedpoints", STRING_RESULT,
     (Udf_func_any)stx_removerepeatedpoints,
     stx_removerepeatedpoints_init, stx_removerepeatedpoints_deinit},
    {"stx_segmentize", STRING_RESULT, (Udf_func_any)stx_segmentize,
     stx_segmentize_init, stx_segmentize_deinit},
    {"stx_generatepoints", STRING_RESULT, (Udf_func_any)stx_generatepoints,
     stx_generatepoints_init, stx_generatepoints_deinit},
    {"stx_asencodedpolyline", STRING_RESULT,
     (Udf_func_any)stx_asencodedpolyline, stx_asencodedpolyline_init,
     stx_asencodedpolyline_deinit},
    {"stx_linefromencodedpolyline", STRING_RESULT,
     (Udf_func_any)stx_linefromencodedpolyline,
     stx_linefromencodedpolyline_init, stx_linefromencodedpolyline_deinit},
    {"stx_assvg", STRING_RESULT, (Udf_func_any)stx_assvg, stx_assvg_init,
     stx_assvg_deinit},
    {"stx_askml", STRING_RESULT, (Udf_func_any)stx_askml, stx_askml_init,
     stx_askml_deinit},
    {"stx_asewkt", STRING_RESULT, (Udf_func_any)stx_asewkt, stx_asewkt_init,
     stx_asewkt_deinit},
    {"stx_geomfromewkt", STRING_RESULT, (Udf_func_any)stx_geomfromewkt,
     stx_geomfromewkt_init, stx_geomfromewkt_deinit},
    {"stx_minimumboundingcircle", STRING_RESULT,
     (Udf_func_any)stx_minimumboundingcircle,
     stx_minimumboundingcircle_init, stx_minimumboundingcircle_deinit},
    {"stx_squaregrid", STRING_RESULT, (Udf_func_any)stx_squaregrid,
     stx_squaregrid_init, stx_squaregrid_deinit},
    {"stx_hexgrid", STRING_RESULT, (Udf_func_any)stx_hexgrid,
     stx_hexgrid_init, stx_hexgrid_deinit},
    // GEOS-based functions
    {"stx_makevalid", STRING_RESULT, (Udf_func_any)stx_makevalid,
     stx_makevalid_init, stx_makevalid_deinit},
    {"stx_linemerge", STRING_RESULT, (Udf_func_any)stx_linemerge,
     stx_linemerge_init, stx_linemerge_deinit},
    {"stx_voronoi", STRING_RESULT, (Udf_func_any)stx_voronoi,
     stx_voronoi_init, stx_voronoi_deinit},
    {"stx_delaunay", STRING_RESULT, (Udf_func_any)stx_delaunay,
     stx_delaunay_init, stx_delaunay_deinit},
    {"stx_offsetcurve", STRING_RESULT, (Udf_func_any)stx_offsetcurve,
     stx_offsetcurve_init, stx_offsetcurve_deinit},
    {"stx_concavehull", STRING_RESULT, (Udf_func_any)stx_concavehull,
     stx_concavehull_init, stx_concavehull_deinit},
    {"stx_snap", STRING_RESULT, (Udf_func_any)stx_snap, stx_snap_init,
     stx_snap_deinit},
    {"stx_polygonize", STRING_RESULT, (Udf_func_any)stx_polygonize,
     stx_polygonize_init, stx_polygonize_deinit},
    {"stx_buildarea", STRING_RESULT, (Udf_func_any)stx_buildarea,
     stx_buildarea_init, stx_buildarea_deinit},
    {"stx_sharedpaths", STRING_RESULT, (Udf_func_any)stx_sharedpaths,
     stx_sharedpaths_init, stx_sharedpaths_deinit},
    {"stx_node", STRING_RESULT, (Udf_func_any)stx_node, stx_node_init,
     stx_node_deinit},
    {"stx_simplifypreservetopology", STRING_RESULT,
     (Udf_func_any)stx_simplifypreservetopology,
     stx_simplifypreservetopology_init, stx_simplifypreservetopology_deinit},
    {"stx_unaryunion", STRING_RESULT, (Udf_func_any)stx_unaryunion,
     stx_unaryunion_init, stx_unaryunion_deinit},
    {"stx_clipbyrect", STRING_RESULT, (Udf_func_any)stx_clipbyrect,
     stx_clipbyrect_init, stx_clipbyrect_deinit},
    {"stx_reduceprecision", STRING_RESULT, (Udf_func_any)stx_reduceprecision,
     stx_reduceprecision_init, stx_reduceprecision_deinit},
    {"stx_maximuminscribedcircle", STRING_RESULT,
     (Udf_func_any)stx_maximuminscribedcircle,
     stx_maximuminscribedcircle_init, stx_maximuminscribedcircle_deinit},
    {"stx_minimumwidth", STRING_RESULT, (Udf_func_any)stx_minimumwidth,
     stx_minimumwidth_init, stx_minimumwidth_deinit},
    {"stx_simplifypolygonhull", STRING_RESULT,
     (Udf_func_any)stx_simplifypolygonhull,
     stx_simplifypolygonhull_init, stx_simplifypolygonhull_deinit},
    {"stx_concavehullofpolygons", STRING_RESULT,
     (Udf_func_any)stx_concavehullofpolygons,
     stx_concavehullofpolygons_init, stx_concavehullofpolygons_deinit},
    // Phase 5: constructor / property / distance functions
    {"stx_npoints", INT_RESULT, (Udf_func_any)stx_npoints,
     stx_npoints_init, stx_npoints_deinit},
    {"stx_makeline", STRING_RESULT, (Udf_func_any)stx_makeline,
     stx_makeline_init, stx_makeline_deinit},
    {"stx_makepolygon", STRING_RESULT, (Udf_func_any)stx_makepolygon,
     stx_makepolygon_init, stx_makepolygon_deinit},
    {"stx_points", STRING_RESULT, (Udf_func_any)stx_points,
     stx_points_init, stx_points_deinit},
    {"stx_isring", INT_RESULT, (Udf_func_any)stx_isring,
     stx_isring_init, stx_isring_deinit},
    {"stx_shortestline", STRING_RESULT, (Udf_func_any)stx_shortestline,
     stx_shortestline_init, stx_shortestline_deinit},
    // DMS conversion functions
    {"stx_dms2deg", REAL_RESULT, (Udf_func_any)stx_dms2deg,
     stx_dms2deg_init, stx_dms2deg_deinit},
    {"stx_deg2dms_deg", INT_RESULT, (Udf_func_any)stx_deg2dms_deg,
     stx_deg2dms_deg_init, stx_deg2dms_deg_deinit},
    {"stx_deg2dms_min", INT_RESULT, (Udf_func_any)stx_deg2dms_min,
     stx_deg2dms_min_init, stx_deg2dms_min_deinit},
    {"stx_deg2dms_sec", REAL_RESULT, (Udf_func_any)stx_deg2dms_sec,
     stx_deg2dms_sec_init, stx_deg2dms_sec_deinit},
    {nullptr, INVALID_RESULT, nullptr, nullptr, nullptr},
};

// =============================================================================
// Plugin init / deinit
// =============================================================================

static void build_status_info();  // forward declaration

static int spatial_plugin_init(void *p [[maybe_unused]]) {
  build_status_info();

  SERVICE_TYPE(registry) *reg = mysql_plugin_registry_acquire();
  if (!reg) return 1;

  // Acquire udf_metadata service for setting result charset
  my_h_service h_meta = nullptr;
  if (!reg->acquire("mysql_udf_metadata", &h_meta) && h_meta)
    udf_metadata_svc =
        reinterpret_cast<SERVICE_TYPE(mysql_udf_metadata) *>(h_meta);

  bool err = false;
  {
    my_service<SERVICE_TYPE(udf_registration)> svc("udf_registration", reg);
    if (!svc.is_valid()) { mysql_plugin_registry_release(reg); return 1; }

    for (const udf_entry *e = udf_table; e->name; ++e) {
      if (svc->udf_register(e->name, e->return_type, e->func, e->init,
                             e->deinit)) {
        err = true;
        break;
      }
    }
    if (err) {
      int wp;
      for (const udf_entry *e = udf_table; e->name; ++e)
        svc->udf_unregister(e->name, &wp);
    }
  }
  if (err && h_meta) {
    reg->release(h_meta);
    udf_metadata_svc = nullptr;
  }
  mysql_plugin_registry_release(reg);
  return err ? 1 : 0;
}

static int spatial_plugin_deinit(void *p [[maybe_unused]]) {
  SERVICE_TYPE(registry) *reg = mysql_plugin_registry_acquire();
  if (!reg) return 1;
  {
    my_service<SERVICE_TYPE(udf_registration)> svc("udf_registration", reg);
    if (svc.is_valid()) {
      int wp;
      for (const udf_entry *e = udf_table; e->name; ++e)
        svc->udf_unregister(e->name, &wp);
    }
  }
  // Release udf_metadata service
  if (udf_metadata_svc) {
    reg->release(reinterpret_cast<my_h_service>(
        const_cast<mysql_service_mysql_udf_metadata_t *>(udf_metadata_svc)));
    udf_metadata_svc = nullptr;
  }
  mysql_plugin_registry_release(reg);
  return 0;
}

// =============================================================================
// Status variables (SHOW STATUS LIKE 'spatial_plugin_%')
// =============================================================================

static char stx_function_list[2048];
static long stx_function_count;

static void build_status_info() {
  stx_function_list[0] = '\0';
  stx_function_count = 0;
  size_t pos = 0;
  for (const udf_entry *e = udf_table; e->name; ++e) {
    stx_function_count++;
    if (pos > 0 && pos < sizeof(stx_function_list) - 1)
      stx_function_list[pos++] = ',';
    size_t nlen = strlen(e->name);
    if (pos + nlen < sizeof(stx_function_list) - 1) {
      memcpy(stx_function_list + pos, e->name, nlen);
      pos += nlen;
    }
  }
  stx_function_list[pos] = '\0';
}

static SHOW_VAR spatial_status_vars[] = {
    {"spatial_plugin_version", const_cast<char *>(STX_PLUGIN_VERSION),
     SHOW_CHAR, SHOW_SCOPE_GLOBAL},
    {"spatial_plugin_requires", const_cast<char *>(STX_PLUGIN_REQUIRES),
     SHOW_CHAR, SHOW_SCOPE_GLOBAL},
    {"spatial_plugin_built_for", const_cast<char *>(MYSQL_SERVER_VERSION),
     SHOW_CHAR, SHOW_SCOPE_GLOBAL},
    {"spatial_plugin_function_count", (char *)&stx_function_count, SHOW_LONG,
     SHOW_SCOPE_GLOBAL},
    {"spatial_plugin_functions", stx_function_list, SHOW_CHAR,
     SHOW_SCOPE_GLOBAL},
    {nullptr, nullptr, SHOW_UNDEF, SHOW_SCOPE_UNDEF},
};

// =============================================================================
// DAEMON plugin declaration
// =============================================================================

static struct st_mysql_daemon spatial_daemon_handler = {
    MYSQL_DAEMON_INTERFACE_VERSION};

mysql_declare_plugin(spatial_plugin){
    MYSQL_DAEMON_PLUGIN,
    &spatial_daemon_handler,
    "spatial_plugin",
    STX_PLUGIN_AUTHOR,
    STX_PLUGIN_DESCRIPTION,
    PLUGIN_LICENSE_GPL,
    spatial_plugin_init,
    nullptr,
    spatial_plugin_deinit,
    STX_PLUGIN_VERSION_HEX,
    spatial_status_vars,
    nullptr,
    nullptr,
    0,
} mysql_declare_plugin_end;
