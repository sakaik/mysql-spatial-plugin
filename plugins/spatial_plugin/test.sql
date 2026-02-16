-- =============================================================================
-- spatial_plugin test suite
-- Usage: make test
-- =============================================================================

CREATE DATABASE IF NOT EXISTS stx_test;
USE stx_test;

-- Ensure plugin is loaded
SELECT 'SETUP: plugin check' AS info;
SELECT IF(COUNT(*) > 0, 'OK', 'FAIL: plugin not installed')
  FROM information_schema.PLUGINS WHERE PLUGIN_NAME = 'spatial_plugin';

SET @pass = 0, @fail = 0, @total = 0;

DELIMITER //
DROP PROCEDURE IF EXISTS assert_eq_double//
CREATE PROCEDURE assert_eq_double(
  IN test_name VARCHAR(255),
  IN actual DOUBLE,
  IN expected DOUBLE,
  IN tolerance DOUBLE
)
BEGIN
  SET @total = @total + 1;
  IF actual IS NULL AND expected IS NULL THEN
    SET @pass = @pass + 1;
    SELECT CONCAT('[PASS] ', test_name) AS result;
  ELSEIF actual IS NOT NULL AND expected IS NOT NULL
        AND ABS(actual - expected) <= tolerance THEN
    SET @pass = @pass + 1;
    SELECT CONCAT('[PASS] ', test_name) AS result;
  ELSE
    SET @fail = @fail + 1;
    SELECT CONCAT('[FAIL] ', test_name,
                  ' -- expected: ', IFNULL(expected, 'NULL'),
                  ', got: ', IFNULL(actual, 'NULL')) AS result;
  END IF;
END//

DROP PROCEDURE IF EXISTS assert_eq_int//
CREATE PROCEDURE assert_eq_int(
  IN test_name VARCHAR(255),
  IN actual BIGINT,
  IN expected BIGINT
)
BEGIN
  SET @total = @total + 1;
  IF actual IS NULL AND expected IS NULL THEN
    SET @pass = @pass + 1;
    SELECT CONCAT('[PASS] ', test_name) AS result;
  ELSEIF actual <=> expected THEN
    SET @pass = @pass + 1;
    SELECT CONCAT('[PASS] ', test_name) AS result;
  ELSE
    SET @fail = @fail + 1;
    SELECT CONCAT('[FAIL] ', test_name,
                  ' -- expected: ', IFNULL(expected, 'NULL'),
                  ', got: ', IFNULL(actual, 'NULL')) AS result;
  END IF;
END//

DROP PROCEDURE IF EXISTS assert_eq_text//
CREATE PROCEDURE assert_eq_text(
  IN test_name VARCHAR(255),
  IN actual TEXT,
  IN expected TEXT
)
BEGIN
  SET @total = @total + 1;
  IF actual IS NULL AND expected IS NULL THEN
    SET @pass = @pass + 1;
    SELECT CONCAT('[PASS] ', test_name) AS result;
  ELSEIF actual <=> expected THEN
    SET @pass = @pass + 1;
    SELECT CONCAT('[PASS] ', test_name) AS result;
  ELSE
    SET @fail = @fail + 1;
    SELECT CONCAT('[FAIL] ', test_name,
                  ' -- expected: ', IFNULL(expected, 'NULL'),
                  ', got: ', IFNULL(actual, 'NULL')) AS result;
  END IF;
END//

DELIMITER ;

-- =============================================================================
-- stx_perimeter
-- =============================================================================

CALL assert_eq_double(
  'perimeter: square 10x10 cartesian',
  stx_perimeter(ST_GeomFromText('POLYGON((0 0, 10 0, 10 10, 0 10, 0 0))')),
  40.0, 0.001);

CALL assert_eq_double(
  'perimeter: multipolygon cartesian (two 10x10 squares)',
  stx_perimeter(ST_GeomFromText('MULTIPOLYGON(((0 0, 10 0, 10 10, 0 10, 0 0)),((20 20, 30 20, 30 30, 20 30, 20 20)))')),
  80.0, 0.001);

CALL assert_eq_double(
  'perimeter: SRID 4326 geographic (10x10 deg square)',
  stx_perimeter(ST_GeomFromText('POLYGON((0 0, 10 0, 10 10, 0 10, 0 0))', 4326)),
  4421233.0, 5000.0);

CALL assert_eq_double(
  'perimeter: SRID 6668 geographic (same as 4326)',
  stx_perimeter(ST_GeomFromText('POLYGON((0 0, 10 0, 10 10, 0 10, 0 0))', 6668)),
  4421233.0, 5000.0);

CALL assert_eq_double(
  'perimeter: non-polygon returns 0',
  stx_perimeter(ST_GeomFromText('POINT(1 1)')),
  0.0, 0.001);

-- NULL input
CALL assert_eq_double(
  'perimeter: NULL input returns NULL',
  stx_perimeter(NULL),
  NULL, 0);

-- =============================================================================
-- stx_coveredby
-- =============================================================================

CALL assert_eq_int(
  'coveredby: point inside polygon',
  stx_coveredby(
    ST_GeomFromText('POINT(5 5)'),
    ST_GeomFromText('POLYGON((0 0, 10 0, 10 10, 0 10, 0 0))')),
  1);

CALL assert_eq_int(
  'coveredby: point outside polygon',
  stx_coveredby(
    ST_GeomFromText('POINT(15 15)'),
    ST_GeomFromText('POLYGON((0 0, 10 0, 10 10, 0 10, 0 0))')),
  0);

CALL assert_eq_int(
  'coveredby: point on boundary',
  stx_coveredby(
    ST_GeomFromText('POINT(0 5)'),
    ST_GeomFromText('POLYGON((0 0, 10 0, 10 10, 0 10, 0 0))')),
  1);

CALL assert_eq_int(
  'coveredby: point on linestring',
  stx_coveredby(
    ST_GeomFromText('POINT(5 0)'),
    ST_GeomFromText('LINESTRING(0 0, 10 0)')),
  1);

CALL assert_eq_int(
  'coveredby: point off linestring',
  stx_coveredby(
    ST_GeomFromText('POINT(5 1)'),
    ST_GeomFromText('LINESTRING(0 0, 10 0)')),
  0);

CALL assert_eq_int(
  'coveredby: linestring inside polygon',
  stx_coveredby(
    ST_GeomFromText('LINESTRING(1 1, 9 9)'),
    ST_GeomFromText('POLYGON((0 0, 10 0, 10 10, 0 10, 0 0))')),
  1);

CALL assert_eq_int(
  'coveredby: polygon inside polygon',
  stx_coveredby(
    ST_GeomFromText('POLYGON((2 2, 8 2, 8 8, 2 8, 2 2))'),
    ST_GeomFromText('POLYGON((0 0, 10 0, 10 10, 0 10, 0 0))')),
  1);

CALL assert_eq_int(
  'coveredby: NULL args returns NULL',
  stx_coveredby(NULL, ST_GeomFromText('POLYGON((0 0, 10 0, 10 10, 0 10, 0 0))')),
  NULL);

-- =============================================================================
-- stx_covers
-- =============================================================================

CALL assert_eq_int(
  'covers: polygon covers inner point',
  stx_covers(
    ST_GeomFromText('POLYGON((0 0, 10 0, 10 10, 0 10, 0 0))'),
    ST_GeomFromText('POINT(5 5)')),
  1);

CALL assert_eq_int(
  'covers: polygon does not cover outer point',
  stx_covers(
    ST_GeomFromText('POLYGON((0 0, 10 0, 10 10, 0 10, 0 0))'),
    ST_GeomFromText('POINT(15 15)')),
  0);

CALL assert_eq_int(
  'covers: polygon covers boundary point',
  stx_covers(
    ST_GeomFromText('POLYGON((0 0, 10 0, 10 10, 0 10, 0 0))'),
    ST_GeomFromText('POINT(0 5)')),
  1);

-- =============================================================================
-- stx_dwithin
-- =============================================================================

CALL assert_eq_int(
  'dwithin: distance=5, threshold=5 -> 1',
  stx_dwithin(
    ST_GeomFromText('POINT(0 0)'),
    ST_GeomFromText('POINT(3 4)'),
    5.0),
  1);

CALL assert_eq_int(
  'dwithin: distance=5, threshold=4.9 -> 0',
  stx_dwithin(
    ST_GeomFromText('POINT(0 0)'),
    ST_GeomFromText('POINT(3 4)'),
    4.9),
  0);

CALL assert_eq_int(
  'dwithin: same point, threshold=0 -> 1',
  stx_dwithin(
    ST_GeomFromText('POINT(5 5)'),
    ST_GeomFromText('POINT(5 5)'),
    0.0),
  1);

CALL assert_eq_int(
  'dwithin: point to linestring',
  stx_dwithin(
    ST_GeomFromText('POINT(5 1)'),
    ST_GeomFromText('LINESTRING(0 0, 10 0)'),
    1.0),
  1);

CALL assert_eq_int(
  'dwithin: geographic SRID 4326, ~111km apart, threshold 200000m',
  stx_dwithin(
    ST_GeomFromText('POINT(0 0)', 4326),
    ST_GeomFromText('POINT(1 0)', 4326),
    200000.0),
  1);

CALL assert_eq_int(
  'dwithin: NULL args returns NULL',
  stx_dwithin(NULL, ST_GeomFromText('POINT(0 0)'), 1.0),
  NULL);

-- =============================================================================
-- stx_azimuth
-- =============================================================================

CALL assert_eq_double(
  'azimuth: east = pi/2',
  stx_azimuth(
    ST_GeomFromText('POINT(0 0)'),
    ST_GeomFromText('POINT(1 0)')),
  PI() / 2, 0.0001);

CALL assert_eq_double(
  'azimuth: north = 0',
  stx_azimuth(
    ST_GeomFromText('POINT(0 0)'),
    ST_GeomFromText('POINT(0 1)')),
  0.0, 0.0001);

CALL assert_eq_double(
  'azimuth: south = pi',
  stx_azimuth(
    ST_GeomFromText('POINT(0 0)'),
    ST_GeomFromText('POINT(0 -1)')),
  PI(), 0.0001);

CALL assert_eq_double(
  'azimuth: west = 3*pi/2',
  stx_azimuth(
    ST_GeomFromText('POINT(0 0)'),
    ST_GeomFromText('POINT(-1 0)')),
  3 * PI() / 2, 0.0001);

CALL assert_eq_double(
  'azimuth: northeast = pi/4',
  stx_azimuth(
    ST_GeomFromText('POINT(0 0)'),
    ST_GeomFromText('POINT(1 1)')),
  PI() / 4, 0.0001);

CALL assert_eq_double(
  'azimuth: geographic SRID 4326 east',
  stx_azimuth(
    ST_GeomFromText('POINT(35 139)', 4326),
    ST_GeomFromText('POINT(35 140)', 4326)),
  PI() / 2, 0.05);

-- =============================================================================
-- stx_project
-- =============================================================================

CALL assert_eq_text(
  'project: north by 10 cartesian',
  ST_AsText(stx_project(ST_GeomFromText('POINT(0 0)'), 10.0, 0.0)),
  'POINT(0 10)');

CALL assert_eq_double(
  'project: east by 10 cartesian, x',
  ST_X(stx_project(ST_GeomFromText('POINT(0 0)'), 10.0, PI() / 2)),
  10.0, 0.0001);
CALL assert_eq_double(
  'project: east by 10 cartesian, y',
  ST_Y(stx_project(ST_GeomFromText('POINT(0 0)'), 10.0, PI() / 2)),
  0.0, 0.0001);

-- Geographic: project from Tokyo north by 1000m
-- ST_X = latitude (should increase), ST_Y = longitude (should stay ~same)
SET @proj_tokyo = stx_project(ST_GeomFromText('POINT(35.6762 139.6503)', 4326), 1000.0, 0.0);
CALL assert_eq_double(
  'project: Tokyo north 1km, lat increases',
  ST_X(@proj_tokyo),
  35.6852, 0.002);
CALL assert_eq_double(
  'project: Tokyo north 1km, lon ~same',
  ST_Y(@proj_tokyo),
  139.6503, 0.01);

CALL assert_eq_text(
  'project: NULL input returns NULL',
  ST_AsText(stx_project(NULL, 10.0, 0.0)),
  NULL);

-- =============================================================================
-- stx_linelocatepoint
-- =============================================================================

CALL assert_eq_double(
  'linelocatepoint: midpoint of line',
  stx_linelocatepoint(
    ST_GeomFromText('LINESTRING(0 0, 10 0)'),
    ST_GeomFromText('POINT(5 0)')),
  0.5, 0.001);

CALL assert_eq_double(
  'linelocatepoint: start of line',
  stx_linelocatepoint(
    ST_GeomFromText('LINESTRING(0 0, 10 0)'),
    ST_GeomFromText('POINT(0 0)')),
  0.0, 0.001);

CALL assert_eq_double(
  'linelocatepoint: end of line',
  stx_linelocatepoint(
    ST_GeomFromText('LINESTRING(0 0, 10 0)'),
    ST_GeomFromText('POINT(10 0)')),
  1.0, 0.001);

CALL assert_eq_double(
  'linelocatepoint: point off line projects to 0.25',
  stx_linelocatepoint(
    ST_GeomFromText('LINESTRING(0 0, 10 0)'),
    ST_GeomFromText('POINT(2.5 5)')),
  0.25, 0.001);

CALL assert_eq_double(
  'linelocatepoint: multi-segment line',
  stx_linelocatepoint(
    ST_GeomFromText('LINESTRING(0 0, 5 0, 10 0)'),
    ST_GeomFromText('POINT(5 0)')),
  0.5, 0.001);

-- =============================================================================
-- stx_linesubstring
-- =============================================================================

CALL assert_eq_text(
  'linesubstring: middle half',
  ST_AsText(stx_linesubstring(
    ST_GeomFromText('LINESTRING(0 0, 10 0)'),
    0.25, 0.75)),
  'LINESTRING(2.5 0,7.5 0)');

CALL assert_eq_text(
  'linesubstring: full line',
  ST_AsText(stx_linesubstring(
    ST_GeomFromText('LINESTRING(0 0, 10 0)'),
    0.0, 1.0)),
  'LINESTRING(0 0,10 0)');

CALL assert_eq_text(
  'linesubstring: first half of multi-segment',
  ST_AsText(stx_linesubstring(
    ST_GeomFromText('LINESTRING(0 0, 5 0, 10 0)'),
    0.0, 0.5)),
  'LINESTRING(0 0,5 0)');

CALL assert_eq_text(
  'linesubstring: across segment boundary',
  ST_AsText(stx_linesubstring(
    ST_GeomFromText('LINESTRING(0 0, 10 0, 10 10)'),
    0.25, 0.75)),
  'LINESTRING(5 0,10 0,10 5)');

-- =============================================================================
-- stx_angle
-- =============================================================================

CALL assert_eq_double(
  'angle: right angle (pi/2)',
  stx_angle(
    ST_GeomFromText('POINT(1 0)'),
    ST_GeomFromText('POINT(0 0)'),
    ST_GeomFromText('POINT(0 1)')),
  PI() / 2, 0.0001);

CALL assert_eq_double(
  'angle: straight line (pi)',
  stx_angle(
    ST_GeomFromText('POINT(1 0)'),
    ST_GeomFromText('POINT(0 0)'),
    ST_GeomFromText('POINT(-1 0)')),
  PI(), 0.0001);

CALL assert_eq_double(
  'angle: full turn minus small = 3*pi/2',
  stx_angle(
    ST_GeomFromText('POINT(0 1)'),
    ST_GeomFromText('POINT(0 0)'),
    ST_GeomFromText('POINT(1 0)')),
  3 * PI() / 2, 0.0001);

CALL assert_eq_double(
  'angle: 45 degrees',
  stx_angle(
    ST_GeomFromText('POINT(1 0)'),
    ST_GeomFromText('POINT(0 0)'),
    ST_GeomFromText('POINT(1 1)')),
  PI() / 4, 0.0001);

CALL assert_eq_double(
  'angle: NULL input returns NULL',
  stx_angle(NULL, ST_GeomFromText('POINT(0 0)'), ST_GeomFromText('POINT(1 0)')),
  NULL, 0);

-- =============================================================================
-- stx_translate
-- =============================================================================

CALL assert_eq_text(
  'translate: point by (10, 20)',
  ST_AsText(stx_translate(ST_GeomFromText('POINT(1 2)'), 10, 20)),
  'POINT(11 22)');

CALL assert_eq_text(
  'translate: linestring by (5, -5)',
  ST_AsText(stx_translate(ST_GeomFromText('LINESTRING(0 0, 10 0)'), 5, -5)),
  'LINESTRING(5 -5,15 -5)');

CALL assert_eq_text(
  'translate: polygon by (1, 1)',
  ST_AsText(stx_translate(
    ST_GeomFromText('POLYGON((0 0, 10 0, 10 10, 0 10, 0 0))'), 1, 1)),
  'POLYGON((1 1,1 11,11 11,11 1,1 1))');

CALL assert_eq_text(
  'translate: NULL input returns NULL',
  ST_AsText(stx_translate(NULL, 1, 1)),
  NULL);

-- =============================================================================
-- stx_scale
-- =============================================================================

CALL assert_eq_text(
  'scale: point by (2, 3)',
  ST_AsText(stx_scale(ST_GeomFromText('POINT(3 4)'), 2, 3)),
  'POINT(6 12)');

CALL assert_eq_text(
  'scale: linestring by (0.5, 0.5)',
  ST_AsText(stx_scale(ST_GeomFromText('LINESTRING(0 0, 10 0, 10 10)'), 0.5, 0.5)),
  'LINESTRING(0 0,5 0,5 5)');

CALL assert_eq_text(
  'scale: polygon by (2, 2)',
  ST_AsText(stx_scale(
    ST_GeomFromText('POLYGON((0 0, 5 0, 5 5, 0 5, 0 0))'), 2, 2)),
  'POLYGON((0 0,0 10,10 10,10 0,0 0))');

CALL assert_eq_text(
  'scale: NULL input returns NULL',
  ST_AsText(stx_scale(NULL, 2, 2)),
  NULL);

-- =============================================================================
-- stx_rotate
-- =============================================================================

CALL assert_eq_double(
  'rotate: point (1,0) by pi/2, x component',
  ST_X(stx_rotate(ST_GeomFromText('POINT(1 0)'), PI() / 2)),
  0.0, 0.0001);

CALL assert_eq_double(
  'rotate: point (1,0) by pi/2, y component',
  ST_Y(stx_rotate(ST_GeomFromText('POINT(1 0)'), PI() / 2)),
  1.0, 0.0001);

CALL assert_eq_double(
  'rotate: point (1,0) by pi, x component',
  ST_X(stx_rotate(ST_GeomFromText('POINT(1 0)'), PI())),
  -1.0, 0.0001);

CALL assert_eq_double(
  'rotate: point (1,0) by pi, y component',
  ST_Y(stx_rotate(ST_GeomFromText('POINT(1 0)'), PI())),
  0.0, 0.0001);

CALL assert_eq_double(
  'rotate: point (0,1) by -pi/2, x component',
  ST_X(stx_rotate(ST_GeomFromText('POINT(0 1)'), -PI() / 2)),
  1.0, 0.0001);

CALL assert_eq_text(
  'rotate: NULL input returns NULL',
  ST_AsText(stx_rotate(NULL, PI())),
  NULL);

-- =============================================================================
-- stx_reverse
-- =============================================================================

CALL assert_eq_text(
  'reverse: linestring',
  ST_AsText(stx_reverse(ST_GeomFromText('LINESTRING(0 0, 1 1, 2 2)'))),
  'LINESTRING(2 2,1 1,0 0)');

CALL assert_eq_text(
  'reverse: multipoint (unordered, unchanged)',
  ST_AsText(stx_reverse(ST_GeomFromText('MULTIPOINT((0 0),(1 1),(2 2))'))),
  'MULTIPOINT((0 0),(1 1),(2 2))');

CALL assert_eq_text(
  'reverse: geographic linestring',
  ST_AsText(stx_reverse(ST_GeomFromText('LINESTRING(0 0, 1 0, 1 1)', 4326))),
  'LINESTRING(1 1,1 0,0 0)');

CALL assert_eq_text(
  'reverse: NULL input returns NULL',
  ST_AsText(stx_reverse(NULL)),
  NULL);

-- =============================================================================
-- stx_pointonsurface
-- =============================================================================

CALL assert_eq_text(
  'pointonsurface: square centroid',
  ST_AsText(stx_pointonsurface(ST_GeomFromText('POLYGON((0 0, 10 0, 10 10, 0 10, 0 0))'))),
  'POINT(5 5)');

-- L-shaped concave polygon: centroid is outside, should find interior point
CALL assert_eq_int(
  'pointonsurface: L-shape is inside',
  stx_coveredby(
    stx_pointonsurface(ST_GeomFromText('POLYGON((0 0, 10 0, 10 5, 5 5, 5 10, 0 10, 0 0))')),
    ST_GeomFromText('POLYGON((0 0, 10 0, 10 5, 5 5, 5 10, 0 10, 0 0))')),
  1);

CALL assert_eq_text(
  'pointonsurface: NULL input returns NULL',
  ST_AsText(stx_pointonsurface(NULL)),
  NULL);

-- =============================================================================
-- stx_closestpoint
-- =============================================================================

CALL assert_eq_text(
  'closestpoint: point to linestring',
  ST_AsText(stx_closestpoint(
    ST_GeomFromText('POINT(5 5)'),
    ST_GeomFromText('LINESTRING(0 0, 10 0)'))),
  'POINT(5 0)');

CALL assert_eq_text(
  'closestpoint: point to polygon boundary',
  ST_AsText(stx_closestpoint(
    ST_GeomFromText('POINT(15 5)'),
    ST_GeomFromText('POLYGON((0 0, 10 0, 10 10, 0 10, 0 0))'))),
  'POINT(10 5)');

CALL assert_eq_text(
  'closestpoint: point inside polygon returns self',
  ST_AsText(stx_closestpoint(
    ST_GeomFromText('POINT(5 5)'),
    ST_GeomFromText('POLYGON((0 0, 10 0, 10 10, 0 10, 0 0))'))),
  'POINT(5 5)');

CALL assert_eq_text(
  'closestpoint: point to point',
  ST_AsText(stx_closestpoint(
    ST_GeomFromText('POINT(0 0)'),
    ST_GeomFromText('POINT(3 4)'))),
  'POINT(3 4)');

CALL assert_eq_text(
  'closestpoint: NULL input returns NULL',
  ST_AsText(stx_closestpoint(NULL, ST_GeomFromText('POINT(0 0)'))),
  NULL);

-- =============================================================================
-- stx_relate
-- =============================================================================

CALL assert_eq_text(
  'relate: point inside polygon',
  stx_relate(
    ST_GeomFromText('POINT(5 5)'),
    ST_GeomFromText('POLYGON((0 0, 10 0, 10 10, 0 10, 0 0))')),
  '0FFFFF212');

CALL assert_eq_text(
  'relate: disjoint polygons',
  stx_relate(
    ST_GeomFromText('POLYGON((0 0, 1 0, 1 1, 0 1, 0 0))'),
    ST_GeomFromText('POLYGON((5 5, 6 5, 6 6, 5 6, 5 5))')),
  'FF2FF1212');

CALL assert_eq_text(
  'relate: equal points',
  stx_relate(
    ST_GeomFromText('POINT(1 1)'),
    ST_GeomFromText('POINT(1 1)')),
  '0FFFFFFF2');

CALL assert_eq_text(
  'relate: disjoint points',
  stx_relate(
    ST_GeomFromText('POINT(0 0)'),
    ST_GeomFromText('POINT(1 1)')),
  'FF0FFF0F2');

CALL assert_eq_text(
  'relate: NULL input returns NULL',
  stx_relate(NULL, ST_GeomFromText('POINT(0 0)')),
  NULL);

-- =============================================================================
-- stx_relatematch
-- =============================================================================

CALL assert_eq_int(
  'relatematch: within pattern (T*F**F***)',
  stx_relatematch(
    ST_GeomFromText('POINT(5 5)'),
    ST_GeomFromText('POLYGON((0 0, 10 0, 10 10, 0 10, 0 0))'),
    'T*F**F***'),
  1);

CALL assert_eq_int(
  'relatematch: intersects pattern (T********)',
  stx_relatematch(
    ST_GeomFromText('POINT(5 5)'),
    ST_GeomFromText('POLYGON((0 0, 10 0, 10 10, 0 10, 0 0))'),
    'T********'),
  1);

CALL assert_eq_int(
  'relatematch: disjoint pattern (FF*FF****)',
  stx_relatematch(
    ST_GeomFromText('POLYGON((0 0, 1 0, 1 1, 0 1, 0 0))'),
    ST_GeomFromText('POLYGON((5 5, 6 5, 6 6, 5 6, 5 5))'),
    'FF*FF****'),
  1);

CALL assert_eq_int(
  'relatematch: not within (negative)',
  stx_relatematch(
    ST_GeomFromText('POINT(15 15)'),
    ST_GeomFromText('POLYGON((0 0, 10 0, 10 10, 0 10, 0 0))'),
    'T*F**F***'),
  0);

CALL assert_eq_int(
  'relatematch: NULL input returns NULL',
  stx_relatematch(NULL, ST_GeomFromText('POINT(0 0)'), 'T*F**F***'),
  NULL);

-- =============================================================================
-- stx_makepoint
-- =============================================================================

CALL assert_eq_text(
  'makepoint: basic (x=1, y=2)',
  ST_AsText(stx_makepoint(1, 2)),
  'POINT(1 2)');

CALL assert_eq_int(
  'makepoint: default SRID is 0',
  ST_SRID(stx_makepoint(1, 2)),
  0);

CALL assert_eq_int(
  'makepoint: explicit SRID 4326',
  ST_SRID(stx_makepoint(139.7, 35.6, 4326)),
  4326);

CALL assert_eq_text(
  'makepoint: NULL x returns NULL',
  ST_AsText(stx_makepoint(NULL, 1)),
  NULL);

CALL assert_eq_text(
  'makepoint: NULL srid returns NULL',
  ST_AsText(stx_makepoint(1, 2, NULL)),
  NULL);

-- =============================================================================
-- stx_affine
-- =============================================================================

-- Identity: affine(geom, 1,0,0,1, 0,0) = geom
CALL assert_eq_text(
  'affine: identity transform',
  ST_AsText(stx_affine(ST_GeomFromText('POINT(3 4)'), 1, 0, 0, 1, 0, 0)),
  'POINT(3 4)');

-- Translation: affine(geom, 1,0,0,1, dx,dy)
CALL assert_eq_text(
  'affine: translation (+10, +20)',
  ST_AsText(stx_affine(ST_GeomFromText('POINT(1 2)'), 1, 0, 0, 1, 10, 20)),
  'POINT(11 22)');

-- Scale: affine(geom, sx,0,0,sy, 0,0)
CALL assert_eq_text(
  'affine: scale (2x, 3x)',
  ST_AsText(stx_affine(ST_GeomFromText('POINT(3 4)'), 2, 0, 0, 3, 0, 0)),
  'POINT(6 12)');

-- Reflection across X axis: affine(geom, 1,0,0,-1, 0,0)
CALL assert_eq_text(
  'affine: reflect across X axis',
  ST_AsText(stx_affine(ST_GeomFromText('POINT(3 4)'), 1, 0, 0, -1, 0, 0)),
  'POINT(3 -4)');

-- Shear: affine(geom, 1,2,0,1, 0,0) → x'=x+2y, y'=y
CALL assert_eq_text(
  'affine: shear (b=2)',
  ST_AsText(stx_affine(ST_GeomFromText('POINT(1 3)'), 1, 2, 0, 1, 0, 0)),
  'POINT(7 3)');

-- LineString
CALL assert_eq_text(
  'affine: linestring translate',
  ST_AsText(stx_affine(ST_GeomFromText('LINESTRING(0 0, 1 1)'), 1, 0, 0, 1, 5, 5)),
  'LINESTRING(5 5,6 6)');

-- NULL input
CALL assert_eq_text(
  'affine: NULL geometry returns NULL',
  ST_AsText(stx_affine(NULL, 1, 0, 0, 1, 0, 0)),
  NULL);

-- =============================================================================
-- stx_snaptogrid
-- =============================================================================

CALL assert_eq_text(
  'snaptogrid: point snap to 0.5',
  ST_AsText(stx_snaptogrid(ST_GeomFromText('POINT(1.23 4.56)'), 0.5)),
  'POINT(1 4.5)');

CALL assert_eq_text(
  'snaptogrid: point snap to 1',
  ST_AsText(stx_snaptogrid(ST_GeomFromText('POINT(1.6 2.4)'), 1)),
  'POINT(2 2)');

CALL assert_eq_text(
  'snaptogrid: linestring snap to 1',
  ST_AsText(stx_snaptogrid(ST_GeomFromText('LINESTRING(0.1 0.2, 1.6 2.7, 3.3 4.8)'), 1)),
  'LINESTRING(0 0,2 3,3 5)');

-- Separate X and Y grid sizes
CALL assert_eq_text(
  'snaptogrid: different x/y sizes',
  ST_AsText(stx_snaptogrid(ST_GeomFromText('POINT(1.7 2.3)'), 1, 0.5)),
  'POINT(2 2.5)');

-- size=0: no change
CALL assert_eq_text(
  'snaptogrid: size=0 no change',
  ST_AsText(stx_snaptogrid(ST_GeomFromText('POINT(1.23 4.56)'), 0)),
  'POINT(1.23 4.56)');

-- NULL input
CALL assert_eq_text(
  'snaptogrid: NULL returns NULL',
  ST_AsText(stx_snaptogrid(NULL, 1)),
  NULL);

-- =============================================================================
-- stx_removerepeatedpoints
-- =============================================================================

-- Exact duplicates in linestring
CALL assert_eq_text(
  'removerepeatedpoints: exact duplicates',
  ST_AsText(stx_removerepeatedpoints(ST_GeomFromText('LINESTRING(0 0, 0 0, 1 1, 1 1, 2 2)'))),
  'LINESTRING(0 0,1 1,2 2)');

-- No duplicates: unchanged
CALL assert_eq_text(
  'removerepeatedpoints: no duplicates',
  ST_AsText(stx_removerepeatedpoints(ST_GeomFromText('LINESTRING(0 0, 1 1, 2 2)'))),
  'LINESTRING(0 0,1 1,2 2)');

-- Tolerance-based removal
CALL assert_eq_text(
  'removerepeatedpoints: tolerance removes near points',
  ST_AsText(stx_removerepeatedpoints(
    ST_GeomFromText('LINESTRING(0 0, 0.001 0.001, 1 1, 1.001 1.001, 2 2)'), 0.01)),
  'LINESTRING(0 0,1 1,2 2)');

-- Polygon: duplicates removed, ring stays valid
CALL assert_eq_text(
  'removerepeatedpoints: polygon removes duplicates',
  ST_AsText(stx_removerepeatedpoints(
    ST_GeomFromText('POLYGON((0 0, 0 0, 1 0, 1 0, 1 1, 1 1, 0 0))'))),
  'POLYGON((0 0,1 1,1 0,0 0))');

-- NULL input
CALL assert_eq_text(
  'removerepeatedpoints: NULL returns NULL',
  ST_AsText(stx_removerepeatedpoints(NULL)),
  NULL);

-- =============================================================================
-- stx_segmentize
-- =============================================================================

-- Line from 0,0 to 10,0 with max_length=3 → 4 segments
CALL assert_eq_text(
  'segmentize: line 10 units, max 3',
  ST_AsText(stx_segmentize(ST_GeomFromText('LINESTRING(0 0, 10 0)'), 3)),
  'LINESTRING(0 0,2.5 0,5 0,7.5 0,10 0)');

-- Already short enough: no change
CALL assert_eq_text(
  'segmentize: already short, no change',
  ST_AsText(stx_segmentize(ST_GeomFromText('LINESTRING(0 0, 1 0)'), 5)),
  'LINESTRING(0 0,1 0)');

-- Point: unchanged
CALL assert_eq_text(
  'segmentize: point unchanged',
  ST_AsText(stx_segmentize(ST_GeomFromText('POINT(1 2)'), 1)),
  'POINT(1 2)');

-- Polygon: 10-unit sides split into 2 segments each (max_length=6)
CALL assert_eq_text(
  'segmentize: polygon densified',
  ST_AsText(stx_segmentize(ST_GeomFromText('POLYGON((0 0, 10 0, 10 10, 0 10, 0 0))'), 6)),
  'POLYGON((0 0,0 5,0 10,5 10,10 10,10 5,10 0,5 0,0 0))');

-- NULL input
CALL assert_eq_text(
  'segmentize: NULL returns NULL',
  ST_AsText(stx_segmentize(NULL, 1)),
  NULL);

-- =============================================================================
-- stx_generatepoints
-- =============================================================================

-- Generate 5 points with seed → always 5 points
CALL assert_eq_int(
  'generatepoints: returns correct count',
  ST_NumGeometries(stx_generatepoints(
    ST_GeomFromText('POLYGON((0 0, 10 0, 10 10, 0 10, 0 0))'), 5, 42)),
  5);

-- Reproducible with same seed
CALL assert_eq_text(
  'generatepoints: same seed same result',
  ST_AsText(stx_generatepoints(
    ST_GeomFromText('POLYGON((0 0, 10 0, 10 10, 0 10, 0 0))'), 3, 123)),
  ST_AsText(stx_generatepoints(
    ST_GeomFromText('POLYGON((0 0, 10 0, 10 10, 0 10, 0 0))'), 3, 123)));

-- All points are within the polygon
CALL assert_eq_int(
  'generatepoints: all points within polygon',
  (SELECT COUNT(*) = 10 FROM JSON_TABLE(
    ST_AsGeoJSON(stx_generatepoints(ST_GeomFromText('POLYGON((0 0, 10 0, 10 10, 0 10, 0 0))'), 10, 42)),
    '$.coordinates[*]' COLUMNS(
      x DOUBLE PATH '$[0]',
      y DOUBLE PATH '$[1]'
    )
  ) AS jt WHERE x BETWEEN 0 AND 10 AND y BETWEEN 0 AND 10),
  1);

-- NULL input
CALL assert_eq_text(
  'generatepoints: NULL returns NULL',
  ST_AsText(stx_generatepoints(NULL, 5)),
  NULL);

-- Geographic polygon (SRID 4326) — ST_GeomFromText uses (lat, lon) for 4326
CALL assert_eq_int(
  'generatepoints: geographic polygon (SRID 4326)',
  ST_NumGeometries(stx_generatepoints(
    ST_GeomFromText('POLYGON((35.6 139.7, 35.6 139.8, 35.7 139.8, 35.7 139.7, 35.6 139.7))', 4326), 5, 42)),
  5);

-- =============================================================================
-- stx_asencodedpolyline
-- =============================================================================

-- Google's example: (-120.2, 38.5), (-120.95, 40.7), (-126.453, 43.252)
CALL assert_eq_text(
  'asencodedpolyline: Google example',
  stx_asencodedpolyline(ST_GeomFromText('LINESTRING(-120.2 38.5, -120.95 40.7, -126.453 43.252)')),
  '_p~iF~ps|U_ulLnnqC_mqNvxq`@');

-- Simple line
CALL assert_eq_text(
  'asencodedpolyline: simple line',
  stx_asencodedpolyline(ST_GeomFromText('LINESTRING(0 0, 1 1)')),
  '??_ibE_ibE');

-- NULL input
CALL assert_eq_text(
  'asencodedpolyline: NULL returns NULL',
  stx_asencodedpolyline(NULL),
  NULL);

-- =============================================================================
-- stx_linefromenccodedpolyline
-- =============================================================================

-- Decode Google example (default SRID 4326)
CALL assert_eq_int(
  'linefromenccodedpolyline: default SRID is 4326',
  ST_SRID(stx_linefromenccodedpolyline('_p~iF~ps|U_ulLnnqC_mqNvxq`@')),
  4326);

-- Decode with explicit SRID 0
CALL assert_eq_int(
  'linefromenccodedpolyline: explicit SRID 0',
  ST_SRID(stx_linefromenccodedpolyline('_p~iF~ps|U_ulLnnqC_mqNvxq`@', 0)),
  0);

-- Round-trip encode/decode
CALL assert_eq_text(
  'linefromenccodedpolyline: round-trip',
  ST_AsText(stx_linefromenccodedpolyline(
    stx_asencodedpolyline(ST_GeomFromText('LINESTRING(-120.2 38.5, -120.95 40.7, -126.453 43.252)')),
    0)),
  'LINESTRING(-120.2 38.5,-120.95 40.7,-126.453 43.252)');

-- NULL input
CALL assert_eq_text(
  'linefromenccodedpolyline: NULL returns NULL',
  ST_AsText(stx_linefromenccodedpolyline(NULL)),
  NULL);

-- =============================================================================
-- stx_assvg
-- =============================================================================

-- Point
CALL assert_eq_text(
  'assvg: point',
  stx_assvg(ST_GeomFromText('POINT(1 2)')),
  'cx="1" cy="-2"');

-- LineString absolute (rel=0, default)
CALL assert_eq_text(
  'assvg: linestring absolute',
  stx_assvg(ST_GeomFromText('LINESTRING(0 0, 10 10, 20 0)')),
  'M 0 -0 L 10 -10 L 20 -0');

-- LineString relative (rel=1)
CALL assert_eq_text(
  'assvg: linestring relative',
  stx_assvg(ST_GeomFromText('LINESTRING(10 20, 30 40, 50 20)'), 1),
  'M 10 -20 l 20 -20 l 20 20');

-- Polygon
CALL assert_eq_text(
  'assvg: polygon',
  stx_assvg(ST_GeomFromText('POLYGON((0 0, 10 0, 10 10, 0 10, 0 0))')),
  'M 0 -0 L 0 -10 L 10 -10 L 10 -0 L 0 -0 Z');

-- Custom precision
CALL assert_eq_text(
  'assvg: custom precision',
  stx_assvg(ST_GeomFromText('POINT(1.5 2.3)'), 0, 2),
  'cx="1.5" cy="-2.3"');

-- NULL input
CALL assert_eq_text(
  'assvg: NULL returns NULL',
  stx_assvg(NULL),
  NULL);

-- =============================================================================
-- stx_askml
-- =============================================================================

-- Point
CALL assert_eq_text(
  'askml: point',
  stx_askml(ST_GeomFromText('POINT(10 20)')),
  '<Point><coordinates>10,20</coordinates></Point>');

-- LineString
CALL assert_eq_text(
  'askml: linestring',
  stx_askml(ST_GeomFromText('LINESTRING(0 0, 10 10)')),
  '<LineString><coordinates>0,0 10,10</coordinates></LineString>');

-- Polygon
CALL assert_eq_text(
  'askml: polygon',
  stx_askml(ST_GeomFromText('POLYGON((0 0, 10 0, 10 10, 0 10, 0 0))')),
  '<Polygon><outerBoundaryIs><LinearRing><coordinates>0,0 0,10 10,10 10,0 0,0</coordinates></LinearRing></outerBoundaryIs></Polygon>');

-- Geographic (SRID 4326) - KML uses lon,lat order from WKB
CALL assert_eq_text(
  'askml: geographic SRID 4326',
  stx_askml(ST_GeomFromText('POINT(35.6 139.7)', 4326)),
  '<Point><coordinates>139.7,35.6</coordinates></Point>');

-- Custom precision
CALL assert_eq_text(
  'askml: custom precision',
  stx_askml(ST_GeomFromText('POINT(1.23456789 9.87654321)'), 4),
  '<Point><coordinates>1.235,9.877</coordinates></Point>');

-- NULL input
CALL assert_eq_text(
  'askml: NULL returns NULL',
  stx_askml(NULL),
  NULL);

-- =============================================================================
-- stx_asewkt
-- =============================================================================

-- Point SRID 0
CALL assert_eq_text(
  'asewkt: point SRID 0',
  stx_asewkt(ST_GeomFromText('POINT(1 2)')),
  'SRID=0;POINT(1 2)');

-- Point SRID 4326 (EWKT uses lon,lat from WKB)
CALL assert_eq_text(
  'asewkt: point SRID 4326',
  stx_asewkt(ST_GeomFromText('POINT(35.6 139.7)', 4326)),
  'SRID=4326;POINT(139.7 35.6)');

-- LineString
CALL assert_eq_text(
  'asewkt: linestring',
  stx_asewkt(ST_GeomFromText('LINESTRING(0 0, 10 10)')),
  'SRID=0;LINESTRING(0 0,10 10)');

-- MultiPoint
CALL assert_eq_text(
  'asewkt: multipoint',
  stx_asewkt(ST_GeomFromText('MULTIPOINT((1 2),(3 4))')),
  'SRID=0;MULTIPOINT((1 2),(3 4))');

-- NULL input
CALL assert_eq_text(
  'asewkt: NULL returns NULL',
  stx_asewkt(NULL),
  NULL);

-- =============================================================================
-- stx_geomfromewkt
-- =============================================================================

-- Point with SRID
CALL assert_eq_text(
  'geomfromewkt: point with SRID',
  ST_AsText(stx_geomfromewkt('SRID=4326;POINT(139.7 35.6)')),
  'POINT(35.6 139.7)');

-- SRID preserved
CALL assert_eq_int(
  'geomfromewkt: SRID preserved',
  ST_SRID(stx_geomfromewkt('SRID=4326;POINT(139.7 35.6)')),
  4326);

-- Without SRID prefix (defaults to SRID 0)
CALL assert_eq_text(
  'geomfromewkt: no SRID prefix defaults to 0',
  ST_AsText(stx_geomfromewkt('POINT(5 10)')),
  'POINT(5 10)');

CALL assert_eq_int(
  'geomfromewkt: no SRID gives SRID 0',
  ST_SRID(stx_geomfromewkt('POINT(5 10)')),
  0);

-- Polygon round-trip
CALL assert_eq_text(
  'geomfromewkt: polygon round-trip',
  ST_AsText(stx_geomfromewkt('POLYGON((0 0, 10 0, 10 10, 0 10, 0 0))')),
  'POLYGON((0 0,10 0,10 10,0 10,0 0))');

-- EWKT round-trip via asewkt → geomfromewkt
CALL assert_eq_text(
  'geomfromewkt: EWKT round-trip',
  ST_AsText(stx_geomfromewkt(stx_asewkt(ST_GeomFromText('LINESTRING(0 0, 10 10)')))),
  'LINESTRING(0 0,10 10)');

-- NULL input
CALL assert_eq_text(
  'geomfromewkt: NULL returns NULL',
  ST_AsText(stx_geomfromewkt(NULL)),
  NULL);

-- =============================================================================
-- Summary
-- =============================================================================

SELECT CONCAT(
  '\n========================================\n',
  '  Total: ', @total, '  Pass: ', @pass, '  Fail: ', @fail, '\n',
  '  Result: ', IF(@fail = 0, 'ALL PASSED', 'SOME FAILED'), '\n',
  '========================================\n'
) AS summary;

-- Cleanup
DROP PROCEDURE IF EXISTS assert_eq_double;
DROP PROCEDURE IF EXISTS assert_eq_int;
DROP PROCEDURE IF EXISTS assert_eq_text;
DROP DATABASE IF EXISTS stx_test;
