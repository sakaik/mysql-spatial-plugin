-- =============================================================================
-- spatial_plugin test suite
-- Usage: make test
-- =============================================================================

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

CALL assert_eq_text(
  'project: east by 10 cartesian',
  ST_AsText(stx_project(ST_GeomFromText('POINT(0 0)'), 10.0, PI() / 2)),
  'POINT(10 0)');

-- Geographic: project from Tokyo north by 1000m
-- Lat should increase, lon should stay ~same
SET @proj_tokyo = stx_project(ST_GeomFromText('POINT(35.6762 139.6503)', 4326), 1000.0, 0.0);
CALL assert_eq_double(
  'project: Tokyo north 1km, lat increases',
  ST_Y(@proj_tokyo),
  139.6503, 0.01);
CALL assert_eq_double(
  'project: Tokyo north 1km, lon ~same',
  ST_X(@proj_tokyo),
  35.6852, 0.002);

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
