#ifndef MYSQL_DYNAMIC_PLUGIN
#define MYSQL_DYNAMIC_PLUGIN
#endif

#include <mysql/plugin.h>
#include <mysql.h>
#include <mysql/service_plugin_registry.h>
#include <mysql/components/my_service.h>
#include <mysql/components/services/udf_registration.h>
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

static double perimeter_cartesian(const CartesianVariant &geom) {
  if (auto *p = std::get_if<Polygon>(&geom)) return bg::perimeter(*p);
  if (auto *p = std::get_if<MultiPolygon>(&geom)) return bg::perimeter(*p);
  return 0.0;
}

static double perimeter_geographic(const GeographicVariant &geom) {
  if (auto *p = std::get_if<GeoPolygon>(&geom)) return bg::perimeter(*p);
  if (auto *p = std::get_if<GeoMultiPolygon>(&geom)) return bg::perimeter(*p);
  return 0.0;
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
  initid->decimals = 15;
  return false;
}

static double stx_perimeter(UDF_INIT *, UDF_ARGS *args, char *is_null,
                            char *error) {
  if (!args->args[0]) { *is_null = 1; return 0.0; }
  auto r = parse_geometry(args->args[0], args->lengths[0]);
  if (!r) { *error = 1; return 0.0; }
  if (auto *c = std::get_if<CartesianVariant>(&r->geometry))
    return perimeter_cartesian(*c);
  if (auto *g = std::get_if<GeographicVariant>(&r->geometry))
    return perimeter_geographic(*g);
  return 0.0;
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
  initid->decimals = 15;
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
  initid->decimals = 15;
  return false;
}

static double stx_linelocatepoint(UDF_INIT *, UDF_ARGS *args, char *is_null,
                                  char *error) {
  if (!args->args[0] || !args->args[1]) { *is_null = 1; return 0.0; }
  auto r1 = parse_geometry(args->args[0], args->lengths[0]);
  auto r2 = parse_geometry(args->args[1], args->lengths[1]);
  if (!r1 || !r2) { *error = 1; return 0.0; }

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
  double sf = *reinterpret_cast<double *>(args->args[1]);
  double ef = *reinterpret_cast<double *>(args->args[2]);
  sf = std::max(0.0, std::min(1.0, sf));
  ef = std::max(0.0, std::min(1.0, ef));

  std::string wkb;

  if (auto *cv = std::get_if<CartesianVariant>(&r->geometry)) {
    auto *ls = std::get_if<Linestring>(cv);
    if (!ls) { *error = 1; return nullptr; }
    auto sub = line_substring_impl<Point, Linestring>(*ls, sf, ef);
    wkb = write_linestring(r->srid, sub);
  } else if (auto *gv = std::get_if<GeographicVariant>(&r->geometry)) {
    auto *ls = std::get_if<GeoLinestring>(gv);
    if (!ls) { *error = 1; return nullptr; }
    auto sub = line_substring_impl<GeoPoint, GeoLinestring>(*ls, sf, ef);
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
  initid->decimals = 15;
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

// ----- stx_scale -------------------------------------------------------------
// Scales a geometry by (sx, sy) relative to origin.

static bool stx_scale_init(UDF_INIT *initid, UDF_ARGS *args, char *msg) {
  if (args->arg_count != 3) {
    strcpy(msg, "stx_scale() requires 3 arguments (geometry, sx, sy)");
    return true;
  }
  args->arg_type[0] = STRING_RESULT;
  args->arg_type[1] = REAL_RESULT;
  args->arg_type[2] = REAL_RESULT;
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

  auto wkb = transform_and_write(*r, [sx, sy](double &x, double &y) {
    x *= sx;
    y *= sy;
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

// ----- stx_pointonsurface helpers (outside extern "C" for templates) ---------

// Compute coordinate-average centroid (works for both Cartesian and Geographic).
template <typename PointT, typename PolygonT>
static PointT coord_avg_centroid(const PolygonT &poly) {
  const auto &outer = poly.outer();
  double sx = 0, sy = 0;
  // Exclude last point if it duplicates the first (closed ring).
  size_t n = outer.size();
  if (n > 1 && bg::get<0>(outer[0]) == bg::get<0>(outer[n - 1]) &&
      bg::get<1>(outer[0]) == bg::get<1>(outer[n - 1]))
    --n;
  if (n == 0) { PointT p; bg::set<0>(p, 0); bg::set<1>(p, 0); return p; }
  for (size_t i = 0; i < n; ++i) {
    sx += bg::get<0>(outer[i]);
    sy += bg::get<1>(outer[i]);
  }
  PointT c;
  bg::set<0>(c, sx / n);
  bg::set<1>(c, sy / n);
  return c;
}

// Helper: find a point guaranteed to be inside the polygon.
template <typename PointT, typename PolygonT>
static PointT point_on_surface_impl(const PolygonT &poly) {
  PointT centroid = coord_avg_centroid<PointT, PolygonT>(poly);
  if (bg::within(centroid, poly)) return centroid;

  // Fallback: scan along y = centroid.y across polygon exterior ring
  double cy = bg::get<1>(centroid);
  const auto &outer = poly.outer();
  std::vector<double> xs;
  for (size_t i = 0; i + 1 < outer.size(); ++i) {
    double y0 = bg::get<1>(outer[i]);
    double y1 = bg::get<1>(outer[i + 1]);
    if ((y0 <= cy && cy < y1) || (y1 <= cy && cy < y0)) {
      double x0 = bg::get<0>(outer[i]);
      double x1 = bg::get<0>(outer[i + 1]);
      double t = (cy - y0) / (y1 - y0);
      xs.push_back(x0 + t * (x1 - x0));
    }
  }
  std::sort(xs.begin(), xs.end());
  if (xs.size() >= 2) {
    PointT p;
    bg::set<0>(p, (xs[0] + xs[1]) / 2.0);
    bg::set<1>(p, cy);
    return p;
  }
  return centroid;
}

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

static bool stx_pointonsurface_init(UDF_INIT *initid, UDF_ARGS *args,
                                     char *msg) {
  if (args->arg_count != 1) {
    strcpy(msg, "stx_pointonsurface() requires 1 argument (polygon)");
    return true;
  }
  args->arg_type[0] = STRING_RESULT;
  initid->maybe_null = 1;
  initid->max_length = 25;
  return false;
}

static char *stx_pointonsurface(UDF_INIT *initid, UDF_ARGS *args, char *result,
                                 unsigned long *length, char *is_null,
                                 char *error) {
  if (!args->args[0]) { *is_null = 1; return nullptr; }
  auto r = parse_geometry(args->args[0], args->lengths[0]);
  if (!r) { *error = 1; return nullptr; }

  std::string wkb;

  if (auto *cv = std::get_if<CartesianVariant>(&r->geometry)) {
    if (auto *poly = std::get_if<Polygon>(cv)) {
      auto pt = point_on_surface_impl<Point, Polygon>(*poly);
      wkb = write_point(r->srid, pt);
    } else if (auto *mpoly = std::get_if<MultiPolygon>(cv)) {
      if (mpoly->empty()) { *error = 1; return nullptr; }
      const Polygon *largest = &(*mpoly)[0];
      double max_area = std::abs(bg::area(*largest));
      for (size_t i = 1; i < mpoly->size(); ++i) {
        double a = std::abs(bg::area((*mpoly)[i]));
        if (a > max_area) { max_area = a; largest = &(*mpoly)[i]; }
      }
      auto pt = point_on_surface_impl<Point, Polygon>(*largest);
      wkb = write_point(r->srid, pt);
    } else {
      *error = 1;
      return nullptr;
    }
  } else if (auto *gv = std::get_if<GeographicVariant>(&r->geometry)) {
    if (auto *poly = std::get_if<GeoPolygon>(gv)) {
      auto pt = point_on_surface_impl<GeoPoint, GeoPolygon>(*poly);
      wkb = write_point(r->srid, pt);
    } else if (auto *mpoly = std::get_if<GeoMultiPolygon>(gv)) {
      if (mpoly->empty()) { *error = 1; return nullptr; }
      const GeoPolygon *largest = &(*mpoly)[0];
      double max_area = std::abs(bg::area(*largest));
      for (size_t i = 1; i < mpoly->size(); ++i) {
        double a = std::abs(bg::area((*mpoly)[i]));
        if (a > max_area) { max_area = a; largest = &(*mpoly)[i]; }
      }
      auto pt = point_on_surface_impl<GeoPoint, GeoPolygon>(*largest);
      wkb = write_point(r->srid, pt);
    } else {
      *error = 1;
      return nullptr;
    }
  } else {
    *error = 1;
    return nullptr;
  }

  *length = static_cast<unsigned long>(wkb.size());
  std::memcpy(result, wkb.data(), wkb.size());
  return result;
}

static void stx_pointonsurface_deinit(UDF_INIT *) {}

}  // extern "C" (pause for closestpoint/relate templates)

// ----- stx_closestpoint helpers (outside extern "C") -------------------------
// Finds closest point on geometry2 to a reference point using segment projection.

template <typename PointT>
static PointT closest_point_on_segment(const PointT &p, const PointT &a,
                                        const PointT &b) {
  double ax = bg::get<0>(a), ay = bg::get<1>(a);
  double bx = bg::get<0>(b), by = bg::get<1>(b);
  double px = bg::get<0>(p), py = bg::get<1>(p);
  double dx = bx - ax, dy = by - ay;
  double len_sq = dx * dx + dy * dy;
  double t = (len_sq > 0.0) ? ((px - ax) * dx + (py - ay) * dy) / len_sq : 0.0;
  t = std::max(0.0, std::min(1.0, t));
  PointT result;
  bg::set<0>(result, ax + t * dx);
  bg::set<1>(result, ay + t * dy);
  return result;
}

template <typename PointT, typename LinestringT>
static PointT closest_point_on_linestring(const PointT &p,
                                           const LinestringT &ls) {
  PointT best;
  double best_dist_sq = std::numeric_limits<double>::max();
  for (size_t i = 0; i + 1 < ls.size(); ++i) {
    PointT cp = closest_point_on_segment(p, ls[i], ls[i + 1]);
    double dx = bg::get<0>(cp) - bg::get<0>(p);
    double dy = bg::get<1>(cp) - bg::get<1>(p);
    double d2 = dx * dx + dy * dy;
    if (d2 < best_dist_sq) { best_dist_sq = d2; best = cp; }
  }
  return best;
}

template <typename PointT, typename RingT>
static PointT closest_point_on_ring(const PointT &p, const RingT &ring) {
  PointT best;
  double best_dist_sq = std::numeric_limits<double>::max();
  for (size_t i = 0; i + 1 < ring.size(); ++i) {
    PointT cp = closest_point_on_segment(p, ring[i], ring[i + 1]);
    double dx = bg::get<0>(cp) - bg::get<0>(p);
    double dy = bg::get<1>(cp) - bg::get<1>(p);
    double d2 = dx * dx + dy * dy;
    if (d2 < best_dist_sq) { best_dist_sq = d2; best = cp; }
  }
  return best;
}

template <typename PointT, typename PolygonT>
static PointT closest_point_on_polygon(const PointT &p,
                                        const PolygonT &poly) {
  // If point is inside polygon, closest point is the point itself
  if (bg::within(p, poly) || bg::covered_by(p, poly)) return p;
  // Otherwise find closest point on boundary
  PointT best = closest_point_on_ring<PointT>(p, poly.outer());
  double best_dist_sq = [&]() {
    double dx = bg::get<0>(best) - bg::get<0>(p);
    double dy = bg::get<1>(best) - bg::get<1>(p);
    return dx * dx + dy * dy;
  }();
  for (const auto &inner : poly.inners()) {
    PointT cp = closest_point_on_ring<PointT>(p, inner);
    double dx = bg::get<0>(cp) - bg::get<0>(p);
    double dy = bg::get<1>(cp) - bg::get<1>(p);
    double d2 = dx * dx + dy * dy;
    if (d2 < best_dist_sq) { best_dist_sq = d2; best = cp; }
  }
  return best;
}

// Dispatches closest-point-on-g2 for a known source point type.
template <typename PointT, typename Variant>
static std::optional<PointT> closest_point_dispatch(const PointT &p,
                                                     const Variant &g2) {
  using LS = std::conditional_t<std::is_same_v<PointT, Point>, Linestring,
                                 GeoLinestring>;
  using Poly = std::conditional_t<std::is_same_v<PointT, Point>, Polygon,
                                   GeoPolygon>;
  using MPoly = std::conditional_t<std::is_same_v<PointT, Point>, MultiPolygon,
                                    GeoMultiPolygon>;

  if (auto *pt2 = std::get_if<PointT>(&g2)) return *pt2;
  if (auto *ls = std::get_if<LS>(&g2))
    return closest_point_on_linestring(p, *ls);
  if (auto *poly = std::get_if<Poly>(&g2))
    return closest_point_on_polygon(p, *poly);
  if (auto *mpoly = std::get_if<MPoly>(&g2)) {
    PointT best;
    double best_d2 = std::numeric_limits<double>::max();
    for (const auto &poly : *mpoly) {
      PointT cp = closest_point_on_polygon(p, poly);
      double dx = bg::get<0>(cp) - bg::get<0>(p);
      double dy = bg::get<1>(cp) - bg::get<1>(p);
      double d2 = dx * dx + dy * dy;
      if (d2 < best_d2) { best_d2 = d2; best = cp; }
    }
    return best;
  }
  return std::nullopt;
}

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

extern "C" {  // resume extern "C" for closestpoint/relate UDFs

// ----- stx_closestpoint ------------------------------------------------------

static bool stx_closestpoint_init(UDF_INIT *initid, UDF_ARGS *args,
                                   char *msg) {
  if (args->arg_count != 2) {
    strcpy(msg, "stx_closestpoint() requires 2 arguments");
    return true;
  }
  args->arg_type[0] = STRING_RESULT;
  args->arg_type[1] = STRING_RESULT;
  initid->maybe_null = 1;
  initid->max_length = 25;
  return false;
}

static char *stx_closestpoint(UDF_INIT *initid, UDF_ARGS *args, char *result,
                               unsigned long *length, char *is_null,
                               char *error) {
  if (!args->args[0] || !args->args[1]) { *is_null = 1; return nullptr; }
  auto two = parse_two_geoms(args);
  if (!two) { *error = 1; return nullptr; }

  std::string wkb;

  if (auto *c1 = std::get_if<CartesianVariant>(&two->r1.geometry)) {
    auto *c2 = std::get_if<CartesianVariant>(&two->r2.geometry);
    if (!c2) { *error = 1; return nullptr; }
    auto *pt1 = std::get_if<Point>(c1);
    if (!pt1) { *error = 1; return nullptr; }  // g1 must be Point
    auto cp = closest_point_dispatch<Point, CartesianVariant>(*pt1, *c2);
    if (!cp) { *error = 1; return nullptr; }
    wkb = write_point(two->r2.srid, *cp);
  } else if (auto *g1 = std::get_if<GeographicVariant>(&two->r1.geometry)) {
    auto *g2 = std::get_if<GeographicVariant>(&two->r2.geometry);
    if (!g2) { *error = 1; return nullptr; }
    auto *pt1 = std::get_if<GeoPoint>(g1);
    if (!pt1) { *error = 1; return nullptr; }  // g1 must be Point
    auto cp = closest_point_dispatch<GeoPoint, GeographicVariant>(*pt1, *g2);
    if (!cp) { *error = 1; return nullptr; }
    wkb = write_point(two->r2.srid, *cp);
  } else {
    *error = 1;
    return nullptr;
  }

  *length = static_cast<unsigned long>(wkb.size());
  std::memcpy(result, wkb.data(), wkb.size());
  return result;
}

static void stx_closestpoint_deinit(UDF_INIT *) {}

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
                            unsigned long *length, char *is_null, char *) {
  if (!args->args[0] || !args->args[1]) { *is_null = 1; return nullptr; }
  if (args->arg_count == 3 && !args->args[2]) { *is_null = 1; return nullptr; }
  double x = *reinterpret_cast<double *>(args->args[0]);
  double y = *reinterpret_cast<double *>(args->args[1]);
  uint32_t srid = 0;
  if (args->arg_count == 3)
    srid = static_cast<uint32_t>(*reinterpret_cast<long long *>(args->args[2]));
  auto wkb = write_point_wkb(srid, x, y);
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
        // Point/MultiPoint: no repeated points to remove
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
    // Exact duplicate removal: use bg::unique
    if (auto *cv = std::get_if<CartesianVariant>(&r->geometry)) {
      std::visit([](auto &g) { bg::unique(g); }, *cv);
      wkb = write_geometry(r->srid, *cv);
    } else if (auto *gv = std::get_if<GeographicVariant>(&r->geometry)) {
      std::visit([](auto &g) { bg::unique(g); }, *gv);
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

  int col_start = static_cast<int>(std::floor(minx / dx)) - 1;
  int col_end = static_cast<int>(std::ceil(maxx / dx)) + 1;

  for (int col = col_start; col <= col_end; ++col) {
    double cx = col * dx;
    if (cx + size < minx || cx - size > maxx) continue;

    double y_off = (std::abs(col) % 2 == 1) ? dy / 2.0 : 0.0;
    int row_start = static_cast<int>(std::floor((miny - y_off) / dy)) - 1;
    int row_end = static_cast<int>(std::ceil((maxy - y_off) / dy)) + 1;

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

// ----- stx_linefromenccodedpolyline ------------------------------------------
// Decodes a Google Encoded Polyline string to a LineString.
// stx_linefromenccodedpolyline(text [, srid [, precision]])

static bool stx_linefromenccodedpolyline_init(UDF_INIT *initid, UDF_ARGS *args,
                                               char *msg) {
  if (args->arg_count < 1 || args->arg_count > 3) {
    strcpy(msg,
           "stx_linefromenccodedpolyline() requires 1-3 arguments "
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

static char *stx_linefromenccodedpolyline(UDF_INIT *initid, UDF_ARGS *args,
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

static void stx_linefromenccodedpolyline_deinit(UDF_INIT *initid) {
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
    {"stx_linefromenccodedpolyline", STRING_RESULT,
     (Udf_func_any)stx_linefromenccodedpolyline,
     stx_linefromenccodedpolyline_init, stx_linefromenccodedpolyline_deinit},
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
  mysql_plugin_registry_release(reg);
  return 0;
}

// =============================================================================
// Status variables (SHOW STATUS LIKE 'spatial_plugin_%')
// =============================================================================

static char stx_function_list[1024];
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
