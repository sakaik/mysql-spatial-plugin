-- STX Spatial Plugin - Stored Procedures / Utility Functions
--
-- Run this script after INSTALL PLUGIN to set up helper routines.
--
-- Usage:
--   mysql -u root -p < stored_procedures.sql

DELIMITER //

-- ---------------------------------------------------------------------------
-- util.to_geom(g BLOB) RETURNS GEOMETRY
--
-- Converts BLOB returned by STX_* functions to GEOMETRY type.
-- STX_* UDFs return STRING_RESULT (BLOB) due to MySQL UDF framework
-- limitations, but the binary content is valid geometry (SRID + WKB).
-- This function performs a zero-cost type cast so that GUI tools like
-- DBeaver can recognize the result as spatial data for map display.
--
-- Example:
--   SELECT util.to_geom(STX_Rotate(geom, PI()/4)) FROM t;
-- ---------------------------------------------------------------------------

CREATE DATABASE IF NOT EXISTS util//

DROP FUNCTION IF EXISTS util.to_geom//

CREATE FUNCTION util.to_geom(g BLOB)
RETURNS GEOMETRY DETERMINISTIC
RETURN g//

-- ---------------------------------------------------------------------------
-- util.stx_generatepoints_table(poly GEOMETRY, n INT)
--
-- Generates n random points inside the given polygon and stores them
-- in a temporary table `_stx_points`.  The table is created fresh on
-- each call (previous contents are dropped).
--
-- The temporary table has the following schema:
--   id   INT AUTO_INCREMENT PRIMARY KEY
--   geom GEOMETRY NOT NULL
--
-- Example:
--   CALL util.stx_generatepoints_table(
--       ST_GeomFromText('POLYGON((0 0,10 0,10 10,0 10,0 0))'), 100);
--   SELECT * FROM _stx_points;
-- ---------------------------------------------------------------------------

DROP PROCEDURE IF EXISTS util.stx_generatepoints_table//

CREATE PROCEDURE util.stx_generatepoints_table(
    IN poly GEOMETRY,
    IN n INT
)
BEGIN
    DECLARE i INT DEFAULT 1;
    DECLARE mp GEOMETRY;
    DECLARE total INT;

    DROP TEMPORARY TABLE IF EXISTS _stx_points;
    CREATE TEMPORARY TABLE _stx_points (
        id   INT AUTO_INCREMENT PRIMARY KEY,
        geom GEOMETRY NOT NULL
    );

    SET mp = util.to_geom(STX_Generatepoints(poly, n));
    SET total = ST_NumGeometries(mp);

    WHILE i <= total DO
        INSERT INTO _stx_points (geom) VALUES (ST_GeometryN(mp, i));
        SET i = i + 1;
    END WHILE;
END//

DELIMITER ;
