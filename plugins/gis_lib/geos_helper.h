#ifndef GIS_LIB_GEOS_HELPER_H
#define GIS_LIB_GEOS_HELPER_H

// GEOS helper: MySQL internal geometry (SRID + WKB) <-> GEOSGeometry conversion
// Uses GEOS C API (reentrant _r functions) for thread safety.

#define GEOS_USE_ONLY_R_API
#include <geos_c.h>

#include <cstdint>
#include <cstring>
#include <memory>
#include <string>

namespace gis_lib {

// RAII wrapper for GEOSContextHandle
class GEOSContextWrapper {
 public:
  GEOSContextWrapper() : ctx_(GEOS_init_r()) {}
  ~GEOSContextWrapper() {
    if (ctx_) GEOS_finish_r(ctx_);
  }
  GEOSContextWrapper(const GEOSContextWrapper &) = delete;
  GEOSContextWrapper &operator=(const GEOSContextWrapper &) = delete;

  GEOSContextHandle_t get() const { return ctx_; }

 private:
  GEOSContextHandle_t ctx_;
};

// Thread-local GEOS context (one per thread for MySQL UDF thread safety)
inline GEOSContextHandle_t get_geos_context() {
  static thread_local GEOSContextWrapper wrapper;
  return wrapper.get();
}

// Custom deleter for GEOSGeometry smart pointer
struct GEOSGeomDeleter {
  void operator()(GEOSGeometry *g) const {
    if (g) GEOSGeom_destroy_r(get_geos_context(), g);
  }
};
using GEOSGeomPtr = std::unique_ptr<GEOSGeometry, GEOSGeomDeleter>;

// Convert MySQL internal geometry (SRID 4 bytes + WKB) to GEOSGeometry.
// Returns nullptr on failure.
inline GEOSGeomPtr mysql_to_geos(const char *data, size_t length) {
  if (!data || length < 4) return nullptr;

  auto ctx = get_geos_context();
  const unsigned char *wkb = reinterpret_cast<const unsigned char *>(data + 4);
  size_t wkb_len = length - 4;

  GEOSWKBReader *reader = GEOSWKBReader_create_r(ctx);
  if (!reader) return nullptr;

  GEOSGeometry *geom = GEOSWKBReader_read_r(ctx, reader, wkb, wkb_len);
  GEOSWKBReader_destroy_r(ctx, reader);

  return GEOSGeomPtr(geom);
}

// Convert GEOSGeometry to MySQL internal geometry format (SRID + WKB).
// Returns empty string on failure.
inline std::string geos_to_mysql(const GEOSGeometry *geom, uint32_t srid) {
  if (!geom) return {};

  auto ctx = get_geos_context();
  GEOSWKBWriter *writer = GEOSWKBWriter_create_r(ctx);
  if (!writer) return {};

  GEOSWKBWriter_setByteOrder_r(ctx, writer, GEOS_WKB_NDR);  // little-endian
  GEOSWKBWriter_setOutputDimension_r(ctx, writer, 2);

  size_t wkb_size = 0;
  unsigned char *wkb = GEOSWKBWriter_write_r(ctx, writer, geom, &wkb_size);
  GEOSWKBWriter_destroy_r(ctx, writer);

  if (!wkb) return {};

  // Build MySQL geometry: 4-byte SRID (little-endian) + WKB
  std::string result(4 + wkb_size, '\0');
  std::memcpy(&result[0], &srid, 4);
  std::memcpy(&result[4], wkb, wkb_size);

  GEOSFree_r(ctx, wkb);
  return result;
}

// Extract SRID from MySQL internal geometry format.
inline uint32_t extract_srid(const char *data, size_t length) {
  if (!data || length < 4) return 0;
  uint32_t srid = 0;
  std::memcpy(&srid, data, 4);
  return srid;
}

}  // namespace gis_lib

#endif  // GIS_LIB_GEOS_HELPER_H
