#!/bin/bash
# Generate geographic_srids.h from MySQL's information_schema.
# Usage: ./generate_srids.sh "mysql_command"
# Example: ./generate_srids.sh "../../mysql960/bin/mysql --socket=../../mysql960/mysql.sock -u root -p"

set -e

MYSQL_CMD="${1:?Usage: $0 \"mysql_command\"}"
OUTPUT="$(dirname "$0")/geographic_srids.h"

GEOGRAPHIC_SRIDS=$($MYSQL_CMD -N -B -e \
  "SELECT SRS_ID FROM information_schema.ST_SPATIAL_REFERENCE_SYSTEMS WHERE DEFINITION LIKE 'GEOGCS%' ORDER BY SRS_ID;")

# SRIDs where the first axis direction is NORTH (lat/northing first).
# These require coordinate swapping because WKB stores (x=easting/lon, y=northing/lat).
# Extracted from the AXIS[] entries in the SRS DEFINITION column.
AXIS_SWAP_SRIDS=$($MYSQL_CMD -N -B -e \
  "SELECT SRS_ID FROM information_schema.ST_SPATIAL_REFERENCE_SYSTEMS
   WHERE DEFINITION != ''
     AND SUBSTRING_INDEX(SUBSTRING_INDEX(DEFINITION, 'AXIS[', -2), ']', 1) LIKE '%NORTH%'
   ORDER BY SRS_ID;")

{
  echo "// Auto-generated from information_schema.ST_SPATIAL_REFERENCE_SYSTEMS"
  echo "// Regenerate with: make generate-srids MYSQL=\"...\""
  echo "#ifndef GIS_LIB_GEOGRAPHIC_SRIDS_H"
  echo "#define GIS_LIB_GEOGRAPHIC_SRIDS_H"
  echo ""
  echo "// Geographic SRIDs (GEOGCS) — used for lat/lon range validation."
  echo "static const std::unordered_set<uint32_t> GEOGRAPHIC_SRIDS = {"
  for id in $GEOGRAPHIC_SRIDS; do
    echo "    ${id},"
  done
  echo "};"
  echo ""
  echo "// SRIDs where the first axis is NORTH (lat or northing)."
  echo "// For these SRIDs, user-facing arguments are (lat/northing, lon/easting)"
  echo "// but internal WKB stores (lon/easting, lat/northing), so coordinates"
  echo "// must be swapped. Covers both geographic and projected SRIDs."
  echo "static const std::unordered_set<uint32_t> AXIS_SWAP_SRIDS = {"
  for id in $AXIS_SWAP_SRIDS; do
    echo "    ${id},"
  done
  echo "};"
  echo ""
  echo "#endif"
} > "$OUTPUT"

GEO_COUNT=$(echo "$GEOGRAPHIC_SRIDS" | wc -w)
SWAP_COUNT=$(echo "$AXIS_SWAP_SRIDS" | wc -w)
echo "Generated $OUTPUT with $GEO_COUNT geographic SRIDs and $SWAP_COUNT axis-swap SRIDs."
