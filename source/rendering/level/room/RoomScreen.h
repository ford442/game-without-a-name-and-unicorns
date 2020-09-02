
#ifndef GAME_ROOMSCREEN_H
#define GAME_ROOMSCREEN_H

#include <gu/screen.h>
#include <gl_includes.h>
#include <graphics/3d/debug_line_renderer.h>
#include <graphics/orthographic_camera.h>
#include <graphics/frame_buffer.h>
#include <graphics/shader_asset.h>
#include <utils/quad_renderer.h>
#include <imgui.h>

#include "../../../level/room/Room.h"
#include "RoomEditor.h"
#include "DebugTileRenderer.h"
#include "../../../ecs/components/physics/Physics.h"
#include "../../../ecs/components/PlatformerMovement.h"
#include "../../../ecs/components/Networked.h"
#include "../../../ecs/EntityInspector.h"
#include "CameraMovement.h"
#include "tile_map/TileMapRenderer.h"
#include "../../PaletteEditor.h"
#include "../../../ecs/components/graphics/Light.h"
#include "light/ShadowCaster.h"
#include "light/LightMapRenderer.h"
#include "../../../ecs/components/physics/VerletRope.h"
#include "../../sprites/MegaSpriteSheet.h"
#include "../../sprites/SpriteRenderer.h"
#include "../../PolylineRenderer.h"
#include "../../../ecs/components/body_parts/Leg.h"
#include "../../../ecs/components/body_parts/LimbJoint.h"
#include "../../../ecs/components/combat/Aiming.h"
#include "../../../ecs/components/Polyline.h"
#include "../../../ecs/components/physics/PolyPlatform.h"
#include "../../../ecs/entity_templates/LuaEntityTemplate.h"
#include "../../../game/Game.h"
#include "../../../ecs/components/graphics/PaletteSetter.h"
#include "blood_splatter/BloodSplatterRenderer.h"
#include "FluidRenderer.h"

class RoomScreen : public Screen
{
    bool showRoomEditor = false;

    EntityInspector inspector;

    DebugLineRenderer lineRenderer;

    OrthographicCamera cam;

    FrameBuffer
        *indexedFbo = NULL,
        *rgbAndBloomFbo = NULL,
        *horizontalBlurFbo = NULL;

    TileMapRenderer *tileMapRenderer = NULL;
    BloodSplatterRenderer *bloodSplatterRenderer = NULL;

    ShaderAsset applyPaletteShader, horizontalGaussianShader, postProcessingShader;

    ShadowCaster shadowCaster;
    LightMapRenderer lightMapRenderer;

    SpriteRenderer spriteRenderer;

    PolylineRenderer polylineRenderer;
    FluidRenderer fluidRenderer;

    TileMap *tileMap = NULL;

  public:

    static inline uint currentPaletteEffect = 0, prevPaletteEffect = 0;
    float timeSinceNewPaletteEffect = 0;
    CameraMovement camMovement;

    Room *room;

    RoomScreen(Room *room, bool showRoomEditor=false);

    void render(double deltaTime) override;

    void setPaletteEffect(float deltaTime);

    void onResize() override;

    void renderDebugStuff();

    void renderDebugBackground();

    void renderDebugTiles();

    void renderWindArrows();

    ~RoomScreen();
};


#endif