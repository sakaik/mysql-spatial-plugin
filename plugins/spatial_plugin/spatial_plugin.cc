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
#include <string>

#include <boost/geometry/algorithms/covered_by.hpp>
#include <boost/geometry/algorithms/distance.hpp>
#include <boost/geometry/algorithms/length.hpp>
#include <boost/geometry/algorithms/perimeter.hpp>
#include <boost/geometry/formulas/vincenty_direct.hpp>
#include <boost/geometry/formulas/vincenty_inverse.hpp>
#include <boost/geometry/srs/spheroid.hpp>

#include "gis_lib/geometry_types.h"
#include "gis_lib/wkb_parser.h"
#include "gis_lib/wkb_writer.h"

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
// Rotates a geometry by angle (radians) around the origin.

static bool stx_rotate_init(UDF_INIT *initid, UDF_ARGS *args, char *msg) {
  if (args->arg_count != 2) {
    strcpy(msg, "stx_rotate() requires 2 arguments (geometry, angle)");
    return true;
  }
  args->arg_type[0] = STRING_RESULT;
  args->arg_type[1] = REAL_RESULT;
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

  auto wkb =
      transform_and_write(*r, [cos_a, sin_a](double &x, double &y) {
        double nx = x * cos_a - y * sin_a;
        double ny = x * sin_a + y * cos_a;
        x = nx;
        y = ny;
      });
  if (wkb.empty()) { *error = 1; return nullptr; }
  return return_wkb(initid, wkb, result, length);
}

static void stx_rotate_deinit(UDF_INIT *initid) {
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
    {nullptr, INVALID_RESULT, nullptr, nullptr, nullptr},
};

// =============================================================================
// Plugin init / deinit
// =============================================================================

static int spatial_plugin_init(void *p [[maybe_unused]]) {
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
// DAEMON plugin declaration
// =============================================================================

static struct st_mysql_daemon spatial_daemon_handler = {
    MYSQL_DAEMON_INTERFACE_VERSION};

mysql_declare_plugin(spatial_plugin){
    MYSQL_DAEMON_PLUGIN,
    &spatial_daemon_handler,
    "spatial_plugin",
    "STX Spatial Extensions",
    "Spatial functions (stx_*) via boost::geometry",
    PLUGIN_LICENSE_GPL,
    spatial_plugin_init,
    nullptr,
    spatial_plugin_deinit,
    0x0300,  // version 3.0
    nullptr,
    nullptr,
    nullptr,
    0,
} mysql_declare_plugin_end;
