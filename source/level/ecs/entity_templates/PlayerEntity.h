
#ifndef GAME_PLAYERENTITY_H
#define GAME_PLAYERENTITY_H


#include "EntityTemplate.h"
#include "../components/physics/Physics.h"
#include "../components/PlatformerMovement.h"
#include "../components/Networked.h"
#include "../components/PlayerControlled.h"
#include "../components/graphics/Light.h"
#include "../components/body_parts/Leg.h"
#include "../components/graphics/SpriteBobbing.h"
#include "../components/graphics/AsepriteView.h"
#include "../components/graphics/SpriteAnchor.h"
#include "../components/body_parts/LimbJoint.h"
#include "../components/Polyline.h"
#include "../components/graphics/DrawPolyline.h"
#include "../components/body_parts/Arm.h"
#include "../components/combat/Aiming.h"
#include "../components/combat/Bow.h"
#include "../components/body_parts/Head.h"
#include "../components/combat/Health.h"
#include "../components/combat/KnockBack.h"

class PlayerEntity : public EntityTemplate
{
  public:
    entt::entity create() override
    {
        entt::entity e = room->entities.create();

        room->entities.assign<Physics>(e).ignorePolyPlatforms = false;
        room->entities.assign<AABB>(e, ivec2(3, 13), ivec2(64, 64));
        room->entities.assign<StaticCollider>(e);
        room->entities.assign<PlatformerMovement>(e);
        room->entities.assign<Flip>(e);
        room->entities.assign<LightPoint>(e); // todo, remove
        room->entities.assign<Health>(e, 100, 100);
        room->entities.assign<KnockBack>(e);

        // LEGS: -----------------------------------

        float legLength = 15;

        std::vector<entt::entity> legEntities{createChild(e), createChild(e)};

        entt::entity legAnchors[2] = {createChild(e), createChild(e)};
        entt::entity knees[2] = {createChild(e), createChild(e)};

        for (int i = 0; i < 2; i++)
        {
            auto opposite = legEntities[i == 0 ? 1 : 0];

            room->entities.assign<Leg>(legEntities[i], legLength, e, ivec2(i == 0 ? -3 : 3, 0), i == 0 ? -2 : 2, opposite, 15.f);
            room->entities.assign<AABB>(legEntities[i], ivec2(1));


            room->entities.assign<AABB>(legAnchors[i], ivec2(1));
            room->entities.assign<SpriteAnchor>(legAnchors[i], e, i == 0 ? "leg_left" : "leg_right", true);

            room->entities.assign<AABB>(knees[i], ivec2(1));
            room->entities.assign<LimbJoint>(knees[i], legAnchors[i], legEntities[i]);

            // bezier curve:
            room->entities.assign<BezierCurve>(legEntities[i], std::vector<entt::entity>{

                legEntities[i], knees[i], legAnchors[i]

            });
            room->entities.assign<DrawPolyline>(legEntities[i], std::vector<uint8>{5});
        }

        room->entities.assign<SpriteBobbing>(e, legEntities);
        room->entities.assign<AsepriteView>(e, asset<aseprite::Sprite>("sprites/player_body"));

        // ARMS: ----------------------

        entt::entity arms[2] = {createChild(e), createChild(e)};
        entt::entity elbows[2] = {createChild(e), createChild(e)};
        entt::entity shoulders[2] = {createChild(e), createChild(e)};
        float armLength = 15;

        for (int i = 0; i < 2; i++)
        {
            // arm/hand:
            room->entities.assign<AABB>(arms[i], ivec2(1));
            Arm &arm = room->entities.get_or_assign<Arm>(arms[i], e, ivec2(i == 0 ? -3 : 3, 4), armLength);

            // shoulders:
            room->entities.assign<AABB>(shoulders[i], ivec2(1));
            room->entities.assign<SpriteAnchor>(shoulders[i], e, i == 0 ? "arm_left" : "arm_right", true);

            // elbow:
            room->entities.assign<AABB>(elbows[i], ivec2(1));
            room->entities.assign<LimbJoint>(elbows[i], shoulders[i], arms[i]);

            // bezier curve:
            room->entities.assign<BezierCurve>(arms[i], std::vector<entt::entity>{

                    arms[i], elbows[i], shoulders[i]

            });
            room->entities.assign<DrawPolyline>(arms[i], std::vector<uint8>{5});
        }

        // WEAPON: -------------------
        room->entities.assign<Aiming>(e);

        auto bowEntity = room->getTemplate("Bow")->create();
        setParent(bowEntity, e, "bow");
        Bow &bow = room->entities.get<Bow>(bowEntity);
        bow.archer = e;
        bow.archerLeftArm = arms[0];
        bow.archerRightArm = arms[1];
        bow.rotatePivot.y = 7;

        // HEAD: -----------
        auto headEntity = createChild(e, "head");
        room->entities.assign<Head>(headEntity, e);
        room->entities.assign<AABB>(headEntity, ivec2(2, 4));
        room->entities.assign<SpriteAnchor>(headEntity, e, "head");
        room->entities.assign<AsepriteView>(headEntity, asset<aseprite::Sprite>("sprites/player_head"));

        return e;
    }

    void makeNetworkedServerSide(Networked &networked) override
    {
        networked.toSend.components<PlayerControlled, PlatformerMovement>();

        networked.toSend.group()
                .component<Physics>()
                .interpolatedComponent<AABB>()
                .endGroup();

        networked.toReceive.group()
                .component<Physics>()
                .interpolatedComponent<AABB>()
                .endGroup();

        networked.toSend.component<Aiming>();
        networked.toReceive.component<Aiming>();
    }

    void makeNetworkedClientSide(Networked &networked) override
    {
        networked.toReceive.components<PlayerControlled, PlatformerMovement>();

        networked.sendIfLocalPlayerReceiveOtherwise.group()
                .component<Physics>()
                .interpolatedComponent<AABB>()
                .endGroup();
        networked.sendIfLocalPlayerReceiveOtherwise.component<Aiming>();
    }
};


#endif
