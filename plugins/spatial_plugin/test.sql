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

DROP PROCEDURE IF EXISTS assert_error//
CREATE PROCEDURE assert_error(
  IN test_name VARCHAR(255),
  IN stmt TEXT,
  IN expected_errno INT
)
BEGIN
  DECLARE actual_errno INT DEFAULT 0;
  DECLARE CONTINUE HANDLER FOR SQLEXCEPTION
    GET DIAGNOSTICS CONDITION 1 actual_errno = MYSQL_ERRNO;
  SET @stx_dummy = NULL;
  SET @stx_stmt = CONCAT('SELECT ', stmt, ' INTO @stx_dummy');
  PREPARE stx_s FROM @stx_stmt;
  EXECUTE stx_s;
  DEALLOCATE PREPARE stx_s;
  SET @total = @total + 1;
  IF actual_errno = expected_errno THEN
    SET @pass = @pass + 1;
    SELECT CONCAT('[PASS] ', test_name) AS result;
  ELSE
    SET @fail = @fail + 1;
    SELECT CONCAT('[FAIL] ', test_name,
                  ' -- expected errno: ', expected_errno,
                  ', got: ', actual_errno) AS result;
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

-- non-polygon type should raise error ER_UNEXPECTED_GEOMETRY_TYPE (3516)
CALL assert_error(
  'perimeter: POINT raises ER_UNEXPECTED_GEOMETRY_TYPE',
  'stx_perimeter(ST_GeomFromText(''POINT(1 1)''))',
  3516);

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
  ST_SRID(stx_makepoint(35.6, 139.7, 4326)),
  4326);

-- Geographic SRID: args follow SRS axis order (lat, lon)
CALL assert_eq_double(
  'makepoint: SRID 4326 latitude (arg1=35.6)',
  ST_Latitude(stx_makepoint(35.6, 139.7, 4326)),
  35.6, 0.0001);

CALL assert_eq_double(
  'makepoint: SRID 4326 longitude (arg2=139.7)',
  ST_Longitude(stx_makepoint(35.6, 139.7, 4326)),
  139.7, 0.0001);

-- Cartesian SRID 0: args are (x, y) as-is
CALL assert_eq_text(
  'makepoint: SRID 0 preserves (x, y) order',
  ST_AsText(stx_makepoint(10, 20)),
  'POINT(10 20)');

-- Boundary values: lat [-90, 90], lon [-180, 180]
CALL assert_eq_double(
  'makepoint: lat=90 accepted',
  ST_Latitude(stx_makepoint(90, 0, 4326)),
  90, 0.0001);

CALL assert_eq_double(
  'makepoint: lat=-90 accepted',
  ST_Latitude(stx_makepoint(-90, 0, 4326)),
  -90, 0.0001);

CALL assert_eq_double(
  'makepoint: lon=180 accepted',
  ST_Longitude(stx_makepoint(0, 180, 4326)),
  180, 0.0001);

CALL assert_eq_double(
  'makepoint: lon=-180 accepted',
  ST_Longitude(stx_makepoint(0, -180, 4326)),
  -180, 0.0001);

-- Out of range: latitude
CALL assert_error(
  'makepoint: lat=90.1 raises ER_LATITUDE_OUT_OF_RANGE',
  'stx_makepoint(90.1, 0, 4326)',
  3617);

CALL assert_error(
  'makepoint: lat=-90.1 raises ER_LATITUDE_OUT_OF_RANGE',
  'stx_makepoint(-90.1, 0, 4326)',
  3617);

-- Out of range: longitude
CALL assert_error(
  'makepoint: lon=180.1 raises ER_LONGITUDE_OUT_OF_RANGE',
  'stx_makepoint(0, 180.1, 4326)',
  3616);

CALL assert_error(
  'makepoint: lon=-180.1 raises ER_LONGITUDE_OUT_OF_RANGE',
  'stx_makepoint(0, -180.1, 4326)',
  3616);

-- Cartesian SRID: no range check (any values allowed)
CALL assert_eq_text(
  'makepoint: SRID 0 allows any values',
  ST_AsText(stx_makepoint(999, -999)),
  'POINT(999 -999)');

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
-- stx_minimumboundingcircle
-- =============================================================================

-- Square polygon: MBC should be circumscribed circle
-- Diagonal of 10x10 square = 10*sqrt(2), radius = 5*sqrt(2) ≈ 7.071
-- Area ≈ π * (5√2)² = 50π ≈ 157.08
CALL assert_eq_double(
  'minimumboundingcircle: square polygon area',
  ROUND(ST_Area(stx_minimumboundingcircle(
    ST_GeomFromText('POLYGON((0 0, 10 0, 10 10, 0 10, 0 0))'))), 1),
  157.1, 0.1);

-- Triangle: circumradius R = abc/(4K) = 10*√125*√125/(4*50) = 6.25
-- Area = π * 6.25² ≈ 122.72
CALL assert_eq_double(
  'minimumboundingcircle: triangle area',
  ROUND(ST_Area(stx_minimumboundingcircle(
    ST_GeomFromText('POLYGON((0 0, 10 0, 5 10, 0 0))'))), 0),
  123, 1);

-- Point → degenerate circle (radius 0, area 0)
CALL assert_eq_double(
  'minimumboundingcircle: single point',
  ST_Area(stx_minimumboundingcircle(ST_GeomFromText('POINT(5 5)'))),
  0, 0.001);

-- LineString: center at midpoint
CALL assert_eq_double(
  'minimumboundingcircle: linestring area',
  ROUND(ST_Area(stx_minimumboundingcircle(
    ST_GeomFromText('LINESTRING(0 0, 10 0)'))), 1),
  78.5, 0.2);

-- Custom segments per quarter (4 = 16 total segments)
CALL assert_eq_double(
  'minimumboundingcircle: custom segments',
  ROUND(ST_Area(stx_minimumboundingcircle(
    ST_GeomFromText('POLYGON((0 0, 10 0, 10 10, 0 10, 0 0))'), 4)), 0),
  150, 10);

-- NULL input
CALL assert_eq_text(
  'minimumboundingcircle: NULL returns NULL',
  ST_AsText(stx_minimumboundingcircle(NULL)),
  NULL);

-- MultiPoint
CALL assert_eq_double(
  'minimumboundingcircle: multipoint area',
  ROUND(ST_Area(stx_minimumboundingcircle(
    ST_GeomFromText('MULTIPOINT((0 0),(10 0),(10 10),(0 10))'))), 1),
  157.1, 0.1);

-- =============================================================================
-- stx_squaregrid
-- =============================================================================

-- 10x10 area with size 5 → 4 cells
CALL assert_eq_int(
  'squaregrid: 4 cells for 10x10 area, size 5',
  ST_NumGeometries(stx_squaregrid(5,
    ST_GeomFromText('POLYGON((0 0, 10 0, 10 10, 0 10, 0 0))'))),
  4);

-- Grid text check: first cell is (0,0)→(5,5)
CALL assert_eq_text(
  'squaregrid: correct cell geometry',
  ST_AsText(stx_squaregrid(5,
    ST_GeomFromText('POLYGON((0 0, 10 0, 10 10, 0 10, 0 0))'))),
  'GEOMETRYCOLLECTION(POLYGON((0 0,5 0,5 5,0 5,0 0)),POLYGON((5 0,10 0,10 5,5 5,5 0)),POLYGON((0 5,5 5,5 10,0 10,0 5)),POLYGON((5 5,10 5,10 10,5 10,5 5)))');

-- 10x10 with size 10 → 1 cell
CALL assert_eq_int(
  'squaregrid: 1 cell for 10x10 area, size 10',
  ST_NumGeometries(stx_squaregrid(10,
    ST_GeomFromText('POLYGON((0 0, 10 0, 10 10, 0 10, 0 0))'))),
  1);

-- Non-aligned bounds: 0.5-9.5 with size 5 → snaps to origin grid
CALL assert_eq_int(
  'squaregrid: non-aligned bounds snap to origin',
  ST_NumGeometries(stx_squaregrid(5,
    ST_GeomFromText('POLYGON((0.5 0.5, 9.5 0.5, 9.5 9.5, 0.5 9.5, 0.5 0.5))'))),
  4);

-- NULL input
CALL assert_eq_text(
  'squaregrid: NULL geom returns NULL',
  ST_AsText(stx_squaregrid(5, NULL)),
  NULL);

-- LineString bounds
CALL assert_eq_int(
  'squaregrid: linestring bounds',
  ST_NumGeometries(stx_squaregrid(5,
    ST_GeomFromText('LINESTRING(0 0, 10 10)'))),
  4);

-- =============================================================================
-- stx_hexgrid
-- =============================================================================

-- Basic hex grid
CALL assert_eq_int(
  'hexgrid: produces cells',
  ST_NumGeometries(stx_hexgrid(5,
    ST_GeomFromText('POLYGON((0 0, 20 0, 20 20, 0 20, 0 0))'))) > 0,
  1);

-- Each cell is a polygon with 7 ring points (6 vertices + closing)
CALL assert_eq_int(
  'hexgrid: each cell has 7 ring points',
  ST_NumPoints(ST_ExteriorRing(ST_GeometryN(stx_hexgrid(5,
    ST_GeomFromText('POLYGON((0 0, 20 0, 20 20, 0 20, 0 0))')), 1))),
  7);

-- Small area with large hex → at least 1 cell
CALL assert_eq_int(
  'hexgrid: small area gets at least 1 cell',
  ST_NumGeometries(stx_hexgrid(100,
    ST_GeomFromText('POLYGON((0 0, 10 0, 10 10, 0 10, 0 0))'))) >= 1,
  1);

-- NULL input
CALL assert_eq_text(
  'hexgrid: NULL geom returns NULL',
  ST_AsText(stx_hexgrid(5, NULL)),
  NULL);

-- Result is a GEOMETRYCOLLECTION
CALL assert_eq_text(
  'hexgrid: result is GEOMETRYCOLLECTION',
  ST_GeometryType(stx_hexgrid(5,
    ST_GeomFromText('POLYGON((0 0, 10 0, 10 10, 0 10, 0 0))'))),
  'GEOMCOLLECTION');

-- =============================================================================
-- GEOS: stx_makevalid
-- =============================================================================

-- Bowtie polygon (self-intersecting) -> repaired as two triangles
CALL assert_eq_text(
  'makevalid: bowtie polygon repaired to MULTIPOLYGON',
  ST_GeometryType(STX_Makevalid(ST_GeomFromText('POLYGON((0 0, 10 10, 10 0, 0 10, 0 0))'))),
  'MULTIPOLYGON');

-- Already valid geometry passes through unchanged
CALL assert_eq_text(
  'makevalid: valid polygon unchanged',
  ST_AsText(STX_Makevalid(ST_GeomFromText('POLYGON((0 0,10 0,10 10,0 10,0 0))'))),
  'POLYGON((0 0,10 0,10 10,0 10,0 0))');

-- NULL input
CALL assert_eq_text(
  'makevalid: NULL input returns NULL',
  STX_Makevalid(NULL),
  NULL);

-- =============================================================================
-- GEOS: stx_linemerge
-- =============================================================================

-- Three connected lines merged into one
CALL assert_eq_text(
  'linemerge: connected lines merged',
  ST_AsText(STX_Linemerge(ST_GeomFromText('MULTILINESTRING((0 0,1 1),(1 1,2 2),(2 2,3 3))'))),
  'LINESTRING(0 0,1 1,2 2,3 3)');

-- Reverse direction also merges
CALL assert_eq_text(
  'linemerge: reverse direction merged',
  ST_AsText(STX_Linemerge(ST_GeomFromText('MULTILINESTRING((0 0,1 1),(1 1,2 2),(3 3,2 2))'))),
  'LINESTRING(0 0,1 1,2 2,3 3)');

-- Disconnected lines stay as MultiLineString
CALL assert_eq_text(
  'linemerge: disconnected lines stay multi',
  ST_GeometryType(STX_Linemerge(ST_GeomFromText('MULTILINESTRING((0 0,1 1),(5 5,6 6))'))),
  'MULTILINESTRING');

-- NULL input
CALL assert_eq_text(
  'linemerge: NULL input returns NULL',
  STX_Linemerge(NULL),
  NULL);

-- =============================================================================
-- GEOS: stx_voronoi
-- =============================================================================

-- 3 points -> 3 Voronoi cells
CALL assert_eq_int(
  'voronoi: 3 points produce 3 cells',
  ST_NumGeometries(STX_Voronoi(ST_GeomFromText('MULTIPOINT(0 0, 10 0, 5 10)'))),
  3);

-- Result is GEOMETRYCOLLECTION of POLYGONs
CALL assert_eq_text(
  'voronoi: result is GEOMCOLLECTION',
  ST_GeometryType(STX_Voronoi(ST_GeomFromText('MULTIPOINT(0 0, 10 0, 5 10)'))),
  'GEOMCOLLECTION');

-- NULL input
CALL assert_eq_text(
  'voronoi: NULL input returns NULL',
  STX_Voronoi(NULL),
  NULL);

-- =============================================================================
-- GEOS: stx_delaunay
-- =============================================================================

-- 4 corner points -> triangulation
CALL assert_eq_text(
  'delaunay: result is GEOMCOLLECTION of triangles',
  ST_GeometryType(STX_Delaunay(ST_GeomFromText('MULTIPOINT(0 0, 10 0, 10 10, 0 10)'))),
  'GEOMCOLLECTION');

-- 4 points -> 2 triangles
CALL assert_eq_int(
  'delaunay: 4 points produce 2 triangles',
  ST_NumGeometries(STX_Delaunay(ST_GeomFromText('MULTIPOINT(0 0, 10 0, 10 10, 0 10)'))),
  2);

-- edges_only mode returns MULTILINESTRING
CALL assert_eq_text(
  'delaunay: edges_only returns MULTILINESTRING',
  ST_GeometryType(STX_Delaunay(ST_GeomFromText('MULTIPOINT(0 0, 10 0, 10 10, 0 10)'), 0, 1)),
  'MULTILINESTRING');

-- NULL input
CALL assert_eq_text(
  'delaunay: NULL input returns NULL',
  STX_Delaunay(NULL),
  NULL);

-- =============================================================================
-- STX_Offsetcurve (Phase 4b)
-- =============================================================================

-- Basic offset left (positive distance)
CALL assert_eq_text(
  'offsetcurve: left offset returns LINESTRING',
  ST_GeometryType(STX_Offsetcurve(ST_GeomFromText('LINESTRING(0 0, 10 0)'), 1)),
  'LINESTRING');

-- Left offset: Y should be positive
CALL assert_eq_double(
  'offsetcurve: left offset Y = 1',
  ST_Y(ST_PointN(STX_Offsetcurve(ST_GeomFromText('LINESTRING(0 0, 10 0)'), 1), 1)),
  1.0, 0.001);

-- Right offset (negative distance): Y should be negative
CALL assert_eq_double(
  'offsetcurve: right offset Y = -2',
  ST_Y(ST_PointN(STX_Offsetcurve(ST_GeomFromText('LINESTRING(0 0, 10 0)'), -2), 1)),
  -2.0, 0.001);

-- With custom quad_segs
CALL assert_eq_text(
  'offsetcurve: custom quad_segs returns LINESTRING',
  ST_GeometryType(STX_Offsetcurve(ST_GeomFromText('LINESTRING(0 0, 10 0, 10 10)'), 1, 4)),
  'LINESTRING');

-- With join_style=2 (mitre)
CALL assert_eq_text(
  'offsetcurve: mitre join returns LINESTRING',
  ST_GeometryType(STX_Offsetcurve(ST_GeomFromText('LINESTRING(0 0, 10 0, 10 10)'), 1, 8, 2)),
  'LINESTRING');

-- NULL input
CALL assert_eq_text(
  'offsetcurve: NULL geom returns NULL',
  STX_Offsetcurve(NULL, 1),
  NULL);

-- NULL distance
CALL assert_eq_text(
  'offsetcurve: NULL distance returns NULL',
  STX_Offsetcurve(ST_GeomFromText('LINESTRING(0 0, 10 0)'), NULL),
  NULL);

-- =============================================================================
-- STX_Concavehull (Phase 4b)
-- =============================================================================

-- ratio=1 should give convex hull
CALL assert_eq_text(
  'concavehull: ratio=1 returns POLYGON (convex hull)',
  ST_GeometryType(STX_Concavehull(
    ST_GeomFromText('MULTIPOINT(0 0, 10 0, 10 10, 0 10, 5 5)'), 1.0)),
  'POLYGON');

-- ratio=1 with 4 points = convex hull = 4-corner polygon (5 ring points)
CALL assert_eq_int(
  'concavehull: ratio=1 convex hull has 5 ring points',
  ST_NumPoints(ST_ExteriorRing(STX_Concavehull(
    ST_GeomFromText('MULTIPOINT(0 0, 10 0, 10 10, 0 10)'), 1.0))),
  5);

-- ratio=0 should give maximum concavity (more vertices in result)
CALL assert_eq_text(
  'concavehull: ratio=0 returns POLYGON',
  ST_GeometryType(STX_Concavehull(
    ST_GeomFromText('MULTIPOINT(0 0, 5 0, 10 0, 10 5, 10 10, 5 10, 0 10, 0 5, 5 1)'), 0.0)),
  'POLYGON');

-- allow_holes parameter
CALL assert_eq_text(
  'concavehull: allow_holes returns POLYGON',
  ST_GeometryType(STX_Concavehull(
    ST_GeomFromText('MULTIPOINT(0 0, 10 0, 10 10, 0 10, 5 5)'), 0.5, 1)),
  'POLYGON');

-- Collinear points -> LINESTRING
CALL assert_eq_text(
  'concavehull: collinear points return LINESTRING',
  ST_GeometryType(STX_Concavehull(
    ST_GeomFromText('MULTIPOINT(0 0, 5 0, 10 0)'), 1.0)),
  'LINESTRING');

-- Single point -> POINT
CALL assert_eq_text(
  'concavehull: single point returns POINT',
  ST_GeometryType(STX_Concavehull(
    ST_GeomFromText('POINT(5 5)'), 1.0)),
  'POINT');

-- NULL input
CALL assert_eq_text(
  'concavehull: NULL returns NULL',
  STX_Concavehull(NULL, 0.5),
  NULL);

-- =============================================================================
-- STX_Snap (Phase 4b)
-- =============================================================================

-- Snap a point to a nearby line
CALL assert_eq_text(
  'snap: result is POINT',
  ST_GeometryType(STX_Snap(
    ST_GeomFromText('POINT(0.5 0.5)'),
    ST_GeomFromText('LINESTRING(0 0, 10 0)'),
    1.0)),
  'POINT');

-- Snap polygon vertices to grid points
CALL assert_eq_text(
  'snap: snapped polygon is POLYGON',
  ST_GeometryType(STX_Snap(
    ST_GeomFromText('POLYGON((0.1 0.1, 9.9 0.1, 9.9 9.9, 0.1 9.9, 0.1 0.1))'),
    ST_GeomFromText('MULTIPOINT(0 0, 10 0, 10 10, 0 10)'),
    0.5)),
  'POLYGON');

-- Snap with tolerance=0 should return original
CALL assert_eq_text(
  'snap: tolerance=0 returns original WKT',
  ST_AsText(STX_Snap(
    ST_GeomFromText('POINT(1 1)'),
    ST_GeomFromText('POINT(2 2)'),
    0)),
  'POINT(1 1)');

-- Verify snapped coordinates
CALL assert_eq_double(
  'snap: vertex snaps to reference point X',
  ST_X(STX_Snap(
    ST_GeomFromText('POINT(0.1 0)'),
    ST_GeomFromText('POINT(0 0)'),
    0.5)),
  0.0, 0.001);

-- NULL input
CALL assert_eq_text(
  'snap: NULL geom1 returns NULL',
  STX_Snap(NULL, ST_GeomFromText('POINT(0 0)'), 1.0),
  NULL);

CALL assert_eq_text(
  'snap: NULL geom2 returns NULL',
  STX_Snap(ST_GeomFromText('POINT(0 0)'), NULL, 1.0),
  NULL);

-- =============================================================================
-- STX_Polygonize (Phase 4c)
-- =============================================================================

-- Square ring -> 1 polygon
CALL assert_eq_int(
  'polygonize: square ring produces 1 polygon',
  ST_NumGeometries(STX_Polygonize(ST_GeomFromText(
    'MULTILINESTRING((0 0,10 0),(10 0,10 10),(10 10,0 10),(0 10,0 0))'))),
  1);

-- Result type is GEOMCOLLECTION
CALL assert_eq_text(
  'polygonize: result is GEOMCOLLECTION',
  ST_GeometryType(STX_Polygonize(ST_GeomFromText(
    'MULTILINESTRING((0 0,10 0),(10 0,10 10),(10 10,0 10),(0 10,0 0))'))),
  'GEOMCOLLECTION');

-- Inner polygon is correct area
CALL assert_eq_double(
  'polygonize: polygon area is 100',
  ST_Area(ST_GeometryN(STX_Polygonize(ST_GeomFromText(
    'MULTILINESTRING((0 0,10 0),(10 0,10 10),(10 10,0 10),(0 10,0 0))')), 1)),
  100.0, 0.001);

-- Two adjacent squares -> 2 polygons
CALL assert_eq_int(
  'polygonize: two adjacent squares produce 2 polygons',
  ST_NumGeometries(STX_Polygonize(ST_GeomFromText(
    'MULTILINESTRING((0 0,10 0),(10 0,10 10),(10 10,0 10),(0 10,0 0),(10 0,20 0),(20 0,20 10),(20 10,10 10))'))),
  2);

-- NULL input
CALL assert_eq_text(
  'polygonize: NULL returns NULL',
  STX_Polygonize(NULL),
  NULL);

-- =============================================================================
-- STX_Buildarea (Phase 4c)
-- =============================================================================

-- Square ring -> POLYGON
CALL assert_eq_text(
  'buildarea: square ring produces POLYGON',
  ST_GeometryType(STX_Buildarea(ST_GeomFromText(
    'MULTILINESTRING((0 0,10 0),(10 0,10 10),(10 10,0 10),(0 10,0 0))'))),
  'POLYGON');

-- Area is correct
CALL assert_eq_double(
  'buildarea: area is 100',
  ST_Area(STX_Buildarea(ST_GeomFromText(
    'MULTILINESTRING((0 0,10 0),(10 0,10 10),(10 10,0 10),(0 10,0 0))'))),
  100.0, 0.001);

-- Outer + inner ring -> polygon with hole
CALL assert_eq_double(
  'buildarea: outer+inner ring area = outer - inner',
  ST_Area(STX_Buildarea(ST_GeomFromText(
    'MULTILINESTRING((0 0,10 0,10 10,0 10,0 0),(2 2,8 2,8 8,2 8,2 2))'))),
  64.0, 0.001);

-- NULL input
CALL assert_eq_text(
  'buildarea: NULL returns NULL',
  STX_Buildarea(NULL),
  NULL);

-- =============================================================================
-- STX_Sharedpaths (Phase 4c)
-- =============================================================================

-- Same direction shared segment
CALL assert_eq_text(
  'sharedpaths: result is GEOMCOLLECTION',
  ST_GeometryType(STX_Sharedpaths(
    ST_GeomFromText('LINESTRING(0 0, 10 0, 10 10)'),
    ST_GeomFromText('LINESTRING(0 0, 10 0)'))),
  'GEOMCOLLECTION');

-- Has 2 sub-collections (same-direction, opposite-direction)
CALL assert_eq_int(
  'sharedpaths: result has 2 sub-collections',
  ST_NumGeometries(STX_Sharedpaths(
    ST_GeomFromText('LINESTRING(0 0, 10 0, 10 10)'),
    ST_GeomFromText('LINESTRING(0 0, 10 0)'))),
  2);

-- Same direction: shared segment should be non-empty
CALL assert_eq_int(
  'sharedpaths: same-direction path count',
  ST_NumGeometries(ST_GeometryN(STX_Sharedpaths(
    ST_GeomFromText('LINESTRING(0 0, 10 0, 10 10)'),
    ST_GeomFromText('LINESTRING(0 0, 10 0)')), 1)),
  1);

-- No shared paths -> empty sub-collections
CALL assert_eq_int(
  'sharedpaths: disjoint lines have 0 same-direction paths',
  ST_NumGeometries(ST_GeometryN(STX_Sharedpaths(
    ST_GeomFromText('LINESTRING(0 0, 10 0)'),
    ST_GeomFromText('LINESTRING(0 5, 10 5)')), 1)),
  0);

-- Opposite direction shared path
CALL assert_eq_int(
  'sharedpaths: opposite-direction shared path found',
  ST_NumGeometries(ST_GeometryN(STX_Sharedpaths(
    ST_GeomFromText('LINESTRING(0 0, 10 0)'),
    ST_GeomFromText('LINESTRING(10 0, 0 0)')), 2)),
  1);

-- NULL input
CALL assert_eq_text(
  'sharedpaths: NULL returns NULL',
  STX_Sharedpaths(NULL, ST_GeomFromText('LINESTRING(0 0, 10 0)')),
  NULL);

-- =============================================================================
-- STX_Node (Phase 4c)
-- =============================================================================

-- Two crossing lines -> 5 segments (X shape split at intersection)
CALL assert_eq_text(
  'node: result is MULTILINESTRING',
  ST_GeometryType(STX_Node(ST_GeomFromText(
    'MULTILINESTRING((0 0, 10 10), (0 10, 10 0))'))),
  'MULTILINESTRING');

-- Crossing at midpoint creates 4 segments
CALL assert_eq_int(
  'node: X crossing produces 4 segments',
  ST_NumGeometries(STX_Node(ST_GeomFromText(
    'MULTILINESTRING((0 0, 10 10), (0 10, 10 0))'))),
  4);

-- Non-crossing lines remain unchanged
CALL assert_eq_int(
  'node: parallel lines stay as 2',
  ST_NumGeometries(STX_Node(ST_GeomFromText(
    'MULTILINESTRING((0 0, 10 0), (0 5, 10 5))'))),
  2);

-- NULL input
CALL assert_eq_text(
  'node: NULL returns NULL',
  STX_Node(NULL),
  NULL);

-- =============================================================================
-- STX_Simplifypreservetopology (Phase 4d)
-- =============================================================================

-- Basic simplification preserves polygon type
CALL assert_eq_text(
  'simplifypreservetopology: result is POLYGON',
  ST_GeometryType(STX_Simplifypreservetopology(
    ST_GeomFromText('POLYGON((0 0, 5 1, 10 0, 10 10, 5 9, 0 10, 0 0))'), 2)),
  'POLYGON');

-- High tolerance simplifies to fewer points
CALL assert_eq_int(
  'simplifypreservetopology: high tolerance reduces points',
  ST_NumPoints(ST_ExteriorRing(STX_Simplifypreservetopology(
    ST_GeomFromText('POLYGON((0 0, 5 1, 10 0, 10 10, 5 9, 0 10, 0 0))'), 2))),
  5);

-- Zero tolerance returns original
CALL assert_eq_int(
  'simplifypreservetopology: tolerance=0 preserves all points',
  ST_NumPoints(ST_ExteriorRing(STX_Simplifypreservetopology(
    ST_GeomFromText('POLYGON((0 0, 5 1, 10 0, 10 10, 5 9, 0 10, 0 0))'), 0))),
  7);

-- Linestring simplification
CALL assert_eq_text(
  'simplifypreservetopology: linestring stays LINESTRING',
  ST_GeometryType(STX_Simplifypreservetopology(
    ST_GeomFromText('LINESTRING(0 0, 1 0.1, 2 0, 3 0.1, 4 0)'), 0.5)),
  'LINESTRING');

-- NULL input
CALL assert_eq_text(
  'simplifypreservetopology: NULL returns NULL',
  STX_Simplifypreservetopology(NULL, 1),
  NULL);

-- =============================================================================
-- STX_Unaryunion (Phase 4d)
-- =============================================================================

-- Overlapping multipolygon dissolved into single polygon
CALL assert_eq_text(
  'unaryunion: overlapping multipolygon becomes POLYGON',
  ST_GeometryType(STX_Unaryunion(ST_GeomFromText(
    'MULTIPOLYGON(((0 0,10 0,10 10,0 10,0 0)),((5 5,15 5,15 15,5 15,5 5)))'))),
  'POLYGON');

-- Area of union is less than sum of parts (due to overlap)
CALL assert_eq_double(
  'unaryunion: area accounts for overlap',
  ST_Area(STX_Unaryunion(ST_GeomFromText(
    'MULTIPOLYGON(((0 0,10 0,10 10,0 10,0 0)),((5 5,15 5,15 15,5 15,5 5)))'))),
  175.0, 0.001);

-- Non-overlapping multipolygon stays as multipolygon
CALL assert_eq_text(
  'unaryunion: disjoint multipolygon stays MULTIPOLYGON',
  ST_GeometryType(STX_Unaryunion(ST_GeomFromText(
    'MULTIPOLYGON(((0 0,5 0,5 5,0 5,0 0)),((10 10,15 10,15 15,10 15,10 10)))'))),
  'MULTIPOLYGON');

-- Single polygon unchanged
CALL assert_eq_double(
  'unaryunion: single polygon area unchanged',
  ST_Area(STX_Unaryunion(ST_GeomFromText(
    'POLYGON((0 0,10 0,10 10,0 10,0 0))'))),
  100.0, 0.001);

-- NULL input
CALL assert_eq_text(
  'unaryunion: NULL returns NULL',
  STX_Unaryunion(NULL),
  NULL);

-- =============================================================================
-- STX_Clipbyrect (Phase 4d)
-- =============================================================================

-- Clip polygon by overlapping rectangle
CALL assert_eq_text(
  'clipbyrect: clipped polygon is POLYGON',
  ST_GeometryType(STX_Clipbyrect(
    ST_GeomFromText('POLYGON((0 0, 10 0, 10 10, 0 10, 0 0))'),
    0, 0, 5, 5)),
  'POLYGON');

-- Clipped area is correct
CALL assert_eq_double(
  'clipbyrect: clipped area is 25',
  ST_Area(STX_Clipbyrect(
    ST_GeomFromText('POLYGON((0 0, 10 0, 10 10, 0 10, 0 0))'),
    0, 0, 5, 5)),
  25.0, 0.001);

-- Polygon fully inside rect returns unchanged
CALL assert_eq_double(
  'clipbyrect: fully inside rect returns full area',
  ST_Area(STX_Clipbyrect(
    ST_GeomFromText('POLYGON((2 2, 8 2, 8 8, 2 8, 2 2))'),
    0, 0, 10, 10)),
  36.0, 0.001);

-- Clip linestring
CALL assert_eq_text(
  'clipbyrect: linestring clip returns LINESTRING',
  ST_GeometryType(STX_Clipbyrect(
    ST_GeomFromText('LINESTRING(0 5, 10 5)'),
    2, 0, 8, 10)),
  'LINESTRING');

-- Disjoint geometry returns empty
CALL assert_eq_int(
  'clipbyrect: disjoint returns empty (0 geometries)',
  ST_NumGeometries(STX_Clipbyrect(
    ST_GeomFromText('POLYGON((0 0, 5 0, 5 5, 0 5, 0 0))'),
    10, 10, 20, 20)),
  0);

-- NULL input
CALL assert_eq_text(
  'clipbyrect: NULL returns NULL',
  STX_Clipbyrect(NULL, 0, 0, 10, 10),
  NULL);

-- =============================================================================
-- STX_Reduceprecision (Phase 4e)
-- =============================================================================

-- Basic precision reduction
CALL assert_eq_text(
  'reduceprecision: point rounded to 0.01',
  ST_AsText(STX_Reduceprecision(ST_GeomFromText('POINT(1.23456 2.34567)'), 0.01)),
  'POINT(1.23 2.35)');

-- Gridsize=1 rounds to integer
CALL assert_eq_text(
  'reduceprecision: gridsize=1 rounds to integer',
  ST_AsText(STX_Reduceprecision(ST_GeomFromText('POINT(1.7 2.3)'), 1)),
  'POINT(2 2)');

-- Polygon stays valid after reduction
CALL assert_eq_text(
  'reduceprecision: polygon stays POLYGON',
  ST_GeometryType(STX_Reduceprecision(
    ST_GeomFromText('POLYGON((0.1 0.1, 9.9 0.1, 9.9 9.9, 0.1 9.9, 0.1 0.1))'), 1)),
  'POLYGON');

-- NULL input
CALL assert_eq_text(
  'reduceprecision: NULL returns NULL',
  STX_Reduceprecision(NULL, 1),
  NULL);

-- =============================================================================
-- STX_Maximuminscribedcircle (Phase 4e)
-- =============================================================================

-- Returns a LINESTRING (center to nearest boundary)
CALL assert_eq_text(
  'maximuminscribedcircle: returns LINESTRING',
  ST_GeometryType(STX_Maximuminscribedcircle(
    ST_GeomFromText('POLYGON((0 0, 10 0, 10 10, 0 10, 0 0))'), 0.1)),
  'LINESTRING');

-- 2 points in result (center and nearest boundary)
CALL assert_eq_int(
  'maximuminscribedcircle: result has 2 points',
  ST_NumPoints(STX_Maximuminscribedcircle(
    ST_GeomFromText('POLYGON((0 0, 10 0, 10 10, 0 10, 0 0))'), 0.1)),
  2);

-- Center of a square should be near (5,5), radius ~5
CALL assert_eq_double(
  'maximuminscribedcircle: square center X near 5',
  ST_X(ST_StartPoint(STX_Maximuminscribedcircle(
    ST_GeomFromText('POLYGON((0 0, 10 0, 10 10, 0 10, 0 0))'), 0.01))),
  5.0, 0.1);

-- Length of result = radius of inscribed circle
CALL assert_eq_double(
  'maximuminscribedcircle: square radius near 5',
  ST_Length(STX_Maximuminscribedcircle(
    ST_GeomFromText('POLYGON((0 0, 10 0, 10 10, 0 10, 0 0))'), 0.01)),
  5.0, 0.1);

-- NULL input
CALL assert_eq_text(
  'maximuminscribedcircle: NULL returns NULL',
  STX_Maximuminscribedcircle(NULL, 1),
  NULL);

-- =============================================================================
-- STX_Minimumwidth (Phase 4e)
-- =============================================================================

-- Returns a LINESTRING
CALL assert_eq_text(
  'minimumwidth: returns LINESTRING',
  ST_GeometryType(STX_Minimumwidth(
    ST_GeomFromText('POLYGON((0 0, 10 0, 10 5, 0 5, 0 0))'))),
  'LINESTRING');

-- Minimum width of 10x5 rectangle = 5
CALL assert_eq_double(
  'minimumwidth: 10x5 rectangle width is 5',
  ST_Length(STX_Minimumwidth(
    ST_GeomFromText('POLYGON((0 0, 10 0, 10 5, 0 5, 0 0))'))),
  5.0, 0.001);

-- Minimum width of a square = side length
CALL assert_eq_double(
  'minimumwidth: 10x10 square width is 10',
  ST_Length(STX_Minimumwidth(
    ST_GeomFromText('POLYGON((0 0, 10 0, 10 10, 0 10, 0 0))'))),
  10.0, 0.001);

-- NULL input
CALL assert_eq_text(
  'minimumwidth: NULL returns NULL',
  STX_Minimumwidth(NULL),
  NULL);

-- =============================================================================
-- STX_Simplifypolygonhull (Phase 4e)
-- =============================================================================

-- vertex_fraction=1.0 returns original (same point count)
CALL assert_eq_int(
  'simplifypolygonhull: fraction=1.0 preserves points',
  ST_NumPoints(ST_ExteriorRing(STX_Simplifypolygonhull(
    ST_GeomFromText('POLYGON((0 0, 5 1, 10 0, 10 10, 5 9, 0 10, 0 0))'), 1.0))),
  7);

-- vertex_fraction=0.0 outer hull = convex hull (fewer points)
CALL assert_eq_int(
  'simplifypolygonhull: fraction=0 outer hull = convex hull',
  ST_NumPoints(ST_ExteriorRing(STX_Simplifypolygonhull(
    ST_GeomFromText('POLYGON((0 0, 5 1, 10 0, 10 10, 5 9, 0 10, 0 0))'), 0.0))),
  5);

-- Inner hull (is_outer=0) has fewer or equal points
CALL assert_eq_text(
  'simplifypolygonhull: inner hull is POLYGON',
  ST_GeometryType(STX_Simplifypolygonhull(
    ST_GeomFromText('POLYGON((0 0, 5 1, 10 0, 10 10, 5 9, 0 10, 0 0))'), 0.5, 0)),
  'POLYGON');

-- NULL input
CALL assert_eq_text(
  'simplifypolygonhull: NULL returns NULL',
  STX_Simplifypolygonhull(NULL, 0.5),
  NULL);

-- =============================================================================
-- STX_Concavehullofpolygons (Phase 4e)
-- =============================================================================

-- Two disjoint polygons -> enclosing polygon
CALL assert_eq_text(
  'concavehullofpolygons: result is POLYGON',
  ST_GeometryType(STX_Concavehullofpolygons(ST_GeomFromText(
    'MULTIPOLYGON(((0 0,5 0,5 5,0 5,0 0)),((10 10,15 10,15 15,10 15,10 10)))'), 1.0)),
  'POLYGON');

-- Hull area >= sum of input areas
CALL assert_eq_int(
  'concavehullofpolygons: hull area >= input area',
  ST_Area(STX_Concavehullofpolygons(ST_GeomFromText(
    'MULTIPOLYGON(((0 0,5 0,5 5,0 5,0 0)),((10 10,15 10,15 15,10 15,10 10)))'), 1.0)) >= 50,
  1);

-- allow_holes parameter
CALL assert_eq_text(
  'concavehullofpolygons: allow_holes returns POLYGON',
  ST_GeometryType(STX_Concavehullofpolygons(ST_GeomFromText(
    'MULTIPOLYGON(((0 0,10 0,10 10,0 10,0 0)),((20 0,30 0,30 10,20 10,20 0)))'), 0.5, 1)),
  'POLYGON');

-- NULL input
CALL assert_eq_text(
  'concavehullofpolygons: NULL returns NULL',
  STX_Concavehullofpolygons(NULL, 0.5),
  NULL);

-- =============================================================================
-- stx_npoints
-- =============================================================================

CALL assert_eq_int(
  'npoints: Point',
  stx_npoints(ST_GeomFromText('POINT(1 2)')),
  1);

CALL assert_eq_int(
  'npoints: LineString 3 points',
  stx_npoints(ST_GeomFromText('LINESTRING(0 0, 1 1, 2 2)')),
  3);

CALL assert_eq_int(
  'npoints: Polygon (5 vertices including closing)',
  stx_npoints(ST_GeomFromText('POLYGON((0 0, 10 0, 10 10, 0 10, 0 0))')),
  5);

CALL assert_eq_int(
  'npoints: Polygon with hole (outer 5 + inner 5)',
  stx_npoints(ST_GeomFromText('POLYGON((0 0, 10 0, 10 10, 0 10, 0 0),(2 2, 4 2, 4 4, 2 4, 2 2))')),
  10);

CALL assert_eq_int(
  'npoints: MultiPoint 3 points',
  stx_npoints(ST_GeomFromText('MULTIPOINT((0 0),(1 1),(2 2))')),
  3);

CALL assert_eq_int(
  'npoints: MultiLineString (2 + 3 = 5)',
  stx_npoints(ST_GeomFromText('MULTILINESTRING((0 0, 1 1),(2 2, 3 3, 4 4))')),
  5);

CALL assert_eq_int(
  'npoints: MultiPolygon (5 + 5 = 10)',
  stx_npoints(ST_GeomFromText('MULTIPOLYGON(((0 0, 10 0, 10 10, 0 10, 0 0)),((20 20, 30 20, 30 30, 20 30, 20 20)))')),
  10);

CALL assert_eq_int(
  'npoints: NULL returns NULL',
  stx_npoints(NULL),
  NULL);

-- Geographic (SRID 6668 JGD2011)
CALL assert_eq_int(
  'npoints: Geographic Point (SRID 6668)',
  stx_npoints(ST_GeomFromText('POINT(35 135)', 6668)),
  1);

CALL assert_eq_int(
  'npoints: Geographic LineString (SRID 6668)',
  stx_npoints(ST_GeomFromText('LINESTRING(35 135, 35.5 135, 35.3 135.5, 35 135.5)', 6668)),
  4);

CALL assert_eq_int(
  'npoints: Geographic Polygon (SRID 6668)',
  stx_npoints(ST_GeomFromText('POLYGON((35 135, 35.5 135, 35.3 135.5, 35 135.5, 35 135))', 6668)),
  5);

-- =============================================================================
-- stx_makeline
-- =============================================================================

CALL assert_eq_text(
  'makeline: two points',
  ST_AsText(stx_makeline(
    ST_GeomFromText('POINT(0 0)'),
    ST_GeomFromText('POINT(1 1)'))),
  'LINESTRING(0 0,1 1)');

CALL assert_eq_text(
  'makeline: from MultiPoint',
  ST_AsText(stx_makeline(ST_GeomFromText('MULTIPOINT((0 0),(1 1),(2 2))'))),
  'LINESTRING(0 0,1 1,2 2)');

CALL assert_eq_text(
  'makeline: two points with SRID',
  ST_AsText(stx_makeline(
    ST_GeomFromText('POINT(0 0)', 4326),
    ST_GeomFromText('POINT(1 1)', 4326))),
  'LINESTRING(0 0,1 1)');

-- Verify SRID preserved
CALL assert_eq_int(
  'makeline: SRID preserved',
  ST_SRID(stx_makeline(
    ST_GeomFromText('POINT(0 0)', 4326),
    ST_GeomFromText('POINT(1 1)', 4326))),
  4326);

CALL assert_eq_text(
  'makeline: NULL returns NULL',
  ST_AsText(stx_makeline(NULL, ST_GeomFromText('POINT(1 1)'))),
  NULL);

-- =============================================================================
-- stx_makepolygon
-- =============================================================================

CALL assert_eq_text(
  'makepolygon: simple polygon from linestring',
  ST_GeometryType(stx_makepolygon(ST_GeomFromText('LINESTRING(0 0, 10 0, 10 10, 0 10, 0 0)'))),
  'POLYGON');

CALL assert_eq_int(
  'makepolygon: result has correct number of points',
  stx_npoints(stx_makepolygon(ST_GeomFromText('LINESTRING(0 0, 10 0, 10 10, 0 10, 0 0)'))),
  5);

CALL assert_eq_text(
  'makepolygon: with hole',
  ST_GeometryType(stx_makepolygon(
    ST_GeomFromText('LINESTRING(0 0, 10 0, 10 10, 0 10, 0 0)'),
    ST_GeomFromText('MULTILINESTRING((2 2, 4 2, 4 4, 2 4, 2 2))'))),
  'POLYGON');

CALL assert_eq_int(
  'makepolygon: with hole has correct number of points',
  stx_npoints(stx_makepolygon(
    ST_GeomFromText('LINESTRING(0 0, 10 0, 10 10, 0 10, 0 0)'),
    ST_GeomFromText('MULTILINESTRING((2 2, 4 2, 4 4, 2 4, 2 2))'))),
  10);

CALL assert_eq_int(
  'makepolygon: SRID preserved',
  ST_SRID(stx_makepolygon(ST_GeomFromText('LINESTRING(0 0, 10 0, 10 10, 0 10, 0 0)', 4326))),
  4326);

CALL assert_eq_text(
  'makepolygon: NULL returns NULL',
  ST_AsText(stx_makepolygon(NULL)),
  NULL);

-- =============================================================================
-- stx_points
-- =============================================================================

CALL assert_eq_text(
  'points: LineString vertices as MultiPoint',
  ST_AsText(stx_points(ST_GeomFromText('LINESTRING(0 0, 1 1, 2 2)'))),
  'MULTIPOINT((0 0),(1 1),(2 2))');

CALL assert_eq_text(
  'points: Point returns single MultiPoint',
  ST_AsText(stx_points(ST_GeomFromText('POINT(5 10)'))),
  'MULTIPOINT((5 10))');

CALL assert_eq_int(
  'points: Polygon vertices count matches npoints',
  stx_npoints(stx_points(ST_GeomFromText('POLYGON((0 0, 10 0, 10 10, 0 10, 0 0))'))),
  5);

CALL assert_eq_text(
  'points: result type is MULTIPOINT',
  ST_GeometryType(stx_points(ST_GeomFromText('POLYGON((0 0, 10 0, 10 10, 0 10, 0 0))'))),
  'MULTIPOINT');

CALL assert_eq_int(
  'points: SRID preserved',
  ST_SRID(stx_points(ST_GeomFromText('LINESTRING(0 0, 1 1)', 4326))),
  4326);

CALL assert_eq_text(
  'points: NULL returns NULL',
  ST_AsText(stx_points(NULL)),
  NULL);

-- =============================================================================
-- stx_isring
-- =============================================================================

CALL assert_eq_int(
  'isring: closed simple ring returns 1',
  stx_isring(ST_GeomFromText('LINESTRING(0 0, 10 0, 10 10, 0 10, 0 0)')),
  1);

CALL assert_eq_int(
  'isring: open linestring returns 0',
  stx_isring(ST_GeomFromText('LINESTRING(0 0, 1 1, 2 2)')),
  0);

CALL assert_eq_int(
  'isring: self-crossing closed linestring returns 0',
  stx_isring(ST_GeomFromText('LINESTRING(0 0, 2 0, 0 2, 2 2, 0 0)')),
  0);

CALL assert_eq_int(
  'isring: NULL returns NULL',
  stx_isring(NULL),
  NULL);

-- =============================================================================
-- stx_shortestline
-- =============================================================================

CALL assert_eq_text(
  'shortestline: point to linestring',
  ST_AsText(stx_shortestline(
    ST_GeomFromText('POINT(0 0)'),
    ST_GeomFromText('LINESTRING(1 1, 2 2)'))),
  'LINESTRING(0 0,1 1)');

CALL assert_eq_text(
  'shortestline: result type is LINESTRING',
  ST_GeometryType(stx_shortestline(
    ST_GeomFromText('POINT(0 0)'),
    ST_GeomFromText('POINT(3 4)'))),
  'LINESTRING');

CALL assert_eq_double(
  'shortestline: length matches ST_Distance',
  ST_Distance(
    ST_GeomFromText('POINT(0 0)'),
    ST_GeomFromText('POINT(3 4)')),
  ST_Length(stx_shortestline(
    ST_GeomFromText('POINT(0 0)'),
    ST_GeomFromText('POINT(3 4)'))),
  0.0001);

CALL assert_eq_int(
  'shortestline: SRID preserved',
  ST_SRID(stx_shortestline(
    ST_GeomFromText('POINT(0 0)', 4326),
    ST_GeomFromText('POINT(1 1)', 4326))),
  4326);

CALL assert_eq_text(
  'shortestline: NULL returns NULL',
  ST_AsText(stx_shortestline(NULL, ST_GeomFromText('POINT(1 1)'))),
  NULL);

-- =============================================================================
-- Type error tests (ER_UNEXPECTED_GEOMETRY_TYPE = 3516)
-- =============================================================================

-- isring: only accepts LineString
CALL assert_error(
  'isring: POINT raises ER_UNEXPECTED_GEOMETRY_TYPE',
  'stx_isring(ST_GeomFromText(''POINT(1 1)''))',
  3516);

CALL assert_error(
  'isring: POLYGON raises ER_UNEXPECTED_GEOMETRY_TYPE',
  'stx_isring(ST_GeomFromText(''POLYGON((0 0,1 0,1 1,0 1,0 0))''))',
  3516);

-- linelocatepoint: requires (LineString, Point)
CALL assert_error(
  'linelocatepoint: POINT as 1st arg raises error',
  'stx_linelocatepoint(ST_GeomFromText(''POINT(1 1)''), ST_GeomFromText(''POINT(0 0)''))',
  3516);

CALL assert_error(
  'linelocatepoint: LINESTRING as 2nd arg raises error',
  'stx_linelocatepoint(ST_GeomFromText(''LINESTRING(0 0,1 1)''), ST_GeomFromText(''LINESTRING(0 0,1 1)''))',
  3516);

-- linesubstring: only accepts LineString
CALL assert_error(
  'linesubstring: POLYGON raises ER_UNEXPECTED_GEOMETRY_TYPE',
  'stx_linesubstring(ST_GeomFromText(''POLYGON((0 0,1 0,1 1,0 1,0 0))''), 0.0, 0.5)',
  3516);

-- offsetcurve: only accepts LineString
CALL assert_error(
  'offsetcurve: POINT raises ER_UNEXPECTED_GEOMETRY_TYPE',
  'stx_offsetcurve(ST_GeomFromText(''POINT(1 1)''), 1.0)',
  3516);

-- makeline 2-arg: requires Point + Point
CALL assert_error(
  'makeline: LINESTRING arg raises ER_UNEXPECTED_GEOMETRY_TYPE',
  'ST_AsText(stx_makeline(ST_GeomFromText(''LINESTRING(0 0,1 1)''), ST_GeomFromText(''POINT(2 2)'')))',
  3516);

-- makeline 1-arg: requires MultiPoint
CALL assert_error(
  'makeline: LINESTRING (single arg) raises ER_UNEXPECTED_GEOMETRY_TYPE',
  'ST_AsText(stx_makeline(ST_GeomFromText(''LINESTRING(0 0,1 1,2 2)'')))',
  3516);

-- makepolygon: outer ring must be LineString
CALL assert_error(
  'makepolygon: POINT raises ER_UNEXPECTED_GEOMETRY_TYPE',
  'ST_AsText(stx_makepolygon(ST_GeomFromText(''POINT(1 1)'')))',
  3516);

-- makepolygon: inner rings must be MultiLineString
CALL assert_error(
  'makepolygon: inner ring POINT raises ER_UNEXPECTED_GEOMETRY_TYPE',
  'ST_AsText(stx_makepolygon(ST_GeomFromText(''LINESTRING(0 0,10 0,10 10,0 10,0 0)''), ST_GeomFromText(''POINT(5 5)'')))',
  3516);

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
DROP PROCEDURE IF EXISTS assert_error;
DROP DATABASE IF EXISTS stx_test;
