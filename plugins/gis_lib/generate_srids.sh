#!/bin/bash
# Generate geographic_srids.h from MySQL's information_schema.
# Usage: ./generate_srids.sh "mysql_command"
# Example: ./generate_srids.sh "../../mysql960/bin/mysql --socket=../../mysql960/mysql.sock -u root -p"

set -e

MYSQL_CMD="${1:?Usage: $0 \"mysql_command\"}"
OUTPUT="$(dirname "$0")/geographic_srids.h"

SRIDS=$($MYSQL_CMD -N -B -e \
  "SELECT SRS_ID FROM information_schema.ST_SPATIAL_REFERENCE_SYSTEMS WHERE DEFINITION LIKE 'GEOGCS%' ORDER BY SRS_ID;")

{
  echo "// Auto-generated from information_schema.ST_SPATIAL_REFERENCE_SYSTEMS"
  echo "// Regenerate with: make generate-srids MYSQL=\"...\""
  echo "#ifndef GIS_LIB_GEOGRAPHIC_SRIDS_H"
  echo "#define GIS_LIB_GEOGRAPHIC_SRIDS_H"
  echo "static const std::unordered_set<uint32_t> GEOGRAPHIC_SRIDS = {"
  for id in $SRIDS; do
    echo "    ${id},"
  done
  echo "};"
  echo "#endif"
} > "$OUTPUT"

COUNT=$(echo "$SRIDS" | wc -w)
echo "Generated $OUTPUT with $COUNT geographic SRIDs."
