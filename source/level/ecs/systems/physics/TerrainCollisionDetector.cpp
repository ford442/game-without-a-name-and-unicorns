
#include "TerrainCollisionDetector.h"
#include "../../components/physics/Physics.h"
#include "../../components/physics/PolyPlatform.h"
#include "../../components/Polyline.h"

TerrainCollisions TerrainCollisionDetector::detect(const AABB &aabb, bool ignorePlatforms, bool ignorePolyPlatforms)
{
    TerrainCollisions collisions;

    collisions.polyPlatform = ignorePolyPlatforms ? false : onPolyPlatform(

            aabb, collisions.polyPlatformEntity,
            collisions.polyPlatformDeltaLeft, collisions.polyPlatformDeltaRight,
            ignorePlatforms
    );

    collisions.halfSlopeDown = halfSlopeDownIntersection(aabb);
    collisions.halfSlopeUp = halfSlopeUpIntersection(aabb);
    collisions.slopeDown = collisions.halfSlopeDown || slopeDownIntersection(aabb);
    collisions.slopeUp = collisions.halfSlopeUp || slopeUpIntersection(aabb);
    collisions.flatFloor = floorIntersection(aabb, ignorePlatforms);
    collisions.floor = collisions.polyPlatform || collisions.slopeUp || collisions.slopeDown || collisions.flatFloor;
    collisions.slopedCeilingDown = slopedCeilingDownIntersection(aabb);
    collisions.slopedCeilingUp = slopedCeilingUpIntersection(aabb);
    collisions.ceiling = collisions.slopedCeilingUp || collisions.slopedCeilingDown || ceilingIntersection(aabb);
    collisions.leftWall = collisions.slopedCeilingUp || leftWallIntersection(aabb);
    collisions.rightWall = collisions.slopedCeilingDown || rightWallIntersection(aabb);

    collisions.anything = collisions.floor || collisions.leftWall || collisions.rightWall || collisions.ceiling;

    return collisions;
}

/**
 * Checks if the line from p0 to p1 intersects with Tile::full
 * @param p0    Should have smallest x and y
 * @param p1    Should have biggest x and y
 */
bool lineIntersectsWithFullBlock(const ivec2 &p0, const ivec2 &p1, const TileMap *map)
{
    bool col = false;
    map->loopThroughTiles(p0, p1, [&](ivec2 t, Tile tile) {
       if (tile == Tile::full) col = true;
    });
    return col;
}

/**
 * Checks if point p overlaps a slope
 * @param slopeType     Type of slope to check for
 */
bool pointOnSlope(const ivec2 &p, const TileMap *map, Tile slopeType)
{
    int
        tileX = p.x / TileMap::PIXELS_PER_TILE,
        tileY = p.y / TileMap::PIXELS_PER_TILE;

    Tile tile = map->getTile(tileX, tileY);

    if (tile != slopeType)
        return false;

    int
        tilePixelX = p.x % TileMap::PIXELS_PER_TILE,
        tilePixelY = p.y % TileMap::PIXELS_PER_TILE;

    switch (tile)
    {
        case Tile::slope_down:
            return tilePixelY <= TileMap::PIXELS_PER_TILE - 1 - tilePixelX;

        case Tile::slope_up:
            return tilePixelY <= tilePixelX;

        case Tile::sloped_ceil_down:
            return tilePixelY >= TileMap::PIXELS_PER_TILE - 1 - tilePixelX;

        case Tile::sloped_ceil_up:
            return tilePixelY >= tilePixelX;

        case Tile::slope_down_half0:
            return tilePixelY <= TileMap::PIXELS_PER_TILE - 1 - (tilePixelX * .5);

        case Tile::slope_down_half1:
            return tilePixelY <= TileMap::PIXELS_PER_TILE - 1 - (tilePixelX * .5) - 8;

        case Tile::slope_up_half0:
            return tilePixelY <= tilePixelX * .5;

        case Tile::slope_up_half1:
            return tilePixelY <= tilePixelX * .5 + 8;

        default: break;
    }
    return false;
}

bool TerrainCollisionDetector::ceilingIntersection(const AABB &aabb)
{
    return lineIntersectsWithFullBlock(
            aabb.center + aabb.halfSize * ivec2(-1, 1) + ivec2(1, 0),
            aabb.center + aabb.halfSize                - ivec2(1, 0),
            map
    );
}

bool TerrainCollisionDetector::slopeDownIntersection(const AABB &aabb)
{
    return
        pointOnSlope(aabb.bottomLeft() + ivec2(1, 0), map, Tile::slope_down)
        ||
        pointOnSlope(aabb.bottomLeft() + ivec2(0, 1), map, Tile::slope_down);
}

bool TerrainCollisionDetector::slopeUpIntersection(const AABB &aabb)
{
    return
        pointOnSlope(aabb.bottomRight() - ivec2(1, 0), map, Tile::slope_up)
        ||
        pointOnSlope(aabb.bottomRight() + ivec2(0, 1), map, Tile::slope_up);
}

bool TerrainCollisionDetector::slopedCeilingDownIntersection(const AABB &aabb)
{
    return
        pointOnSlope(aabb.topRight() - ivec2(1, 0), map, Tile::sloped_ceil_down)
        ||
        pointOnSlope(aabb.topRight() - ivec2(0, 1), map, Tile::sloped_ceil_down);
}

bool TerrainCollisionDetector::slopedCeilingUpIntersection(const AABB &aabb)
{
    return
        pointOnSlope(aabb.topLeft() + ivec2(1, 0), map, Tile::sloped_ceil_up)
        ||
        pointOnSlope(aabb.topLeft() - ivec2(0, 1), map, Tile::sloped_ceil_up);
}

bool TerrainCollisionDetector::floorIntersection(const AABB &aabb, bool ignorePlatforms)
{
    ivec2
        p0 = aabb.bottomLeft()  + ivec2(1, 0),
        p1 = aabb.bottomRight() - ivec2(1, 0);

    bool col = false;
    map->loopThroughTiles(p0, p1, [&](ivec2 t, Tile tile) {
        if (tile == Tile::full) col = true;
        else if (
                !ignorePlatforms && tile == Tile::platform
                && (p0.y % TileMap::PIXELS_PER_TILE) == TileMap::PIXELS_PER_TILE - 1
            )
            col = true;
    });
    return col;
}

bool TerrainCollisionDetector::leftWallIntersection(const AABB &aabb)
{
    return lineIntersectsWithFullBlock(
            aabb.bottomLeft()  + ivec2(0, 1),
            aabb.topLeft()     - ivec2(0, 1),
            map
    );
}

bool TerrainCollisionDetector::rightWallIntersection(const AABB &aabb)
{
    return lineIntersectsWithFullBlock(
            aabb.bottomRight() + ivec2(0, 1),
            aabb.topRight()    - ivec2(0, 1),
            map
    );
}

bool TerrainCollisionDetector::halfSlopeDownIntersection(const AABB &aabb)
{
    return
            pointOnSlope(aabb.bottomLeft() + ivec2(1, 0), map, Tile::slope_down_half0)
            ||
            pointOnSlope(aabb.bottomLeft() + ivec2(0, 1), map, Tile::slope_down_half0)
            ||
            pointOnSlope(aabb.bottomLeft() + ivec2(1, 0), map, Tile::slope_down_half1)
            ||
            pointOnSlope(aabb.bottomLeft() + ivec2(0, 1), map, Tile::slope_down_half1);
}

bool TerrainCollisionDetector::halfSlopeUpIntersection(const AABB &aabb)
{
    return
            pointOnSlope(aabb.bottomRight() - ivec2(1, 0), map, Tile::slope_up_half0)
            ||
            pointOnSlope(aabb.bottomRight() + ivec2(0, 1), map, Tile::slope_up_half0)
            ||
            pointOnSlope(aabb.bottomRight() - ivec2(1, 0), map, Tile::slope_up_half1)
            ||
            pointOnSlope(aabb.bottomRight() + ivec2(0, 1), map, Tile::slope_up_half1);
}

float polyPlatformHeightAtX(int x, const vec2 &p0, const vec2 &p1)
{
    vec2 delta = p1 - p0;
    return p0.y + delta.y * ((x - p0.x) / delta.x);
}

bool TerrainCollisionDetector::onPolyPlatform(const AABB &aabb, entt::entity &platformEntity, int8 &deltaLeft,
                                              int8 &deltaRight, bool fallThrough)
{
    if (!reg) return false;

    ivec2 point = aabb.bottomCenter();
    int xLeft = point.x - 1, xRight = point.x + 1;

    auto test = [&](
            AABB &platformAABB, PolyPlatform &platform, Polyline &line
    ) -> bool {

        if ((fallThrough && platform.allowFallThrough) || !platformAABB.contains(point))
            return false;

        auto it = line.points.begin();

        int heightLeft = -99, height = 0, heightRight = 0;

        for (int i = 0; i < line.points.size() - 1; i++)
        {
            const vec2 p0 = *it + vec2(platformAABB.center);
            const vec2 p1 = *(++it) + vec2(platformAABB.center);

            int xMin = p0.x, xMax = p1.x - 1;
            if (xMax >= xLeft && xMin <= xLeft)
                heightLeft = polyPlatformHeightAtX(xLeft, p0, p1);
            if (xMax >= point.x && xMin <= point.x)
                height = polyPlatformHeightAtX(point.x, p0, p1);
            if (xMax >= xRight && xMin <= xRight)
            {
                if (height == point.y)
                {
                    if (heightLeft == -99)
                        heightLeft = height;

                    heightRight = polyPlatformHeightAtX(xRight, p0, p1);

                    deltaLeft = heightLeft - height;
                    deltaRight = heightRight - height;
                    return true;
                }
            }
        }
        return false;
    };

    // first check if still on previous platform:
    if (platformEntity != entt::null)
    {
        AABB *platformAABB = reg->try_get<AABB>(platformEntity);
        PolyPlatform *platform = reg->try_get<PolyPlatform>(platformEntity);
        Polyline *line = reg->try_get<Polyline>(platformEntity);

        if (platformAABB && platform && line && test(*platformAABB, *platform, *line))
            return true;
    }

    bool foundPlatform = false;

    // check all other platforms:
    reg->view<AABB, PolyPlatform, Polyline>()
    .each([&](auto e, AABB &platformAABB, PolyPlatform &platform, Polyline &line){

        if (foundPlatform || e == platformEntity)
            return;

        if (test(platformAABB, platform, line))
        {
            foundPlatform = true;
            platformEntity = e;
        }
    });
    if (foundPlatform)
        return true;

    platformEntity = entt::null;
    deltaRight = deltaLeft = 0;
    return false;
}
