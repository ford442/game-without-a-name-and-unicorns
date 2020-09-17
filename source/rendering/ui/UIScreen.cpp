
#include <imgui.h>
#include <utils/code_editor/CodeEditor.h>
#include <utils/quad_renderer.h>
#include "UIScreen.h"
#include "../../game/Game.h"
#include "../../game/session/SingleplayerSession.h"
#include "../../ecs/systems/graphics/SpriteSystem.h"
#include "../../ecs/systems/LuaScriptsSystem.h"
#include "../../ecs/systems/AudioSystem.h"
#include "../level/room/RoomScreen.h"
#include "../../ecs/systems/ChildrenSystem.h"

UIScreen::UIScreen(const asset<luau::Script> &s)
    :
    script(s),
    cam(.1, 1000, 0, 0),
    inspector(*this, "UI"),
    applyPaletteUIShader("Apply palette UI shader", "shaders/fullscreen_quad", "shaders/ui/apply_palette")
{

    addSystem(new ChildrenSystem("children"));
    addSystem(new SpriteSystem("(animated) sprites"));
    addSystem(new AudioSystem("audio"));
    addSystem(new LuaScriptsSystem("lua functions"));

    initialize();
    UIScreen::onResize();

    try
    {
        luau::getLuaState().unsafe_script(script->getByteCode().as_string_view(), luaEnvironment);
    }
    catch (std::exception &e)
    {
        std::cerr << "Error while running UIScreen script " << script.getLoadedAsset().fullPath << ":" << std::endl;
        std::cerr << e.what() << std::endl;
    }

    cam.position = mu::Z;
    cam.lookAt(mu::ZERO_3);

    inspector.createEntity_showSubFolder = "ui";
    inspector.createEntity_persistentOption = false;
}

void UIScreen::initializeLuaEnvironment()
{
    EntityEngine::initializeLuaEnvironment();

    luaEnvironment["endCurrentSession"] = [] {
        Game::setCurrentSession(NULL);
    };

    luaEnvironment["startSinglePlayerSession"] = [] (const char *saveGamePath) {
        Game::setCurrentSession(new SingleplayerSession(saveGamePath));
    };
    // todo: startMultiplayerServerSession and startMultiplayerClientsession

    luaEnvironment["joinSession"] = [] (const char *username, const sol::function& onJoinRequestDeclined) {

        auto &session = Game::getCurrentSession();

        session.onJoinRequestDeclined = [onJoinRequestDeclined] (auto reason) {

            sol::protected_function_result result = onJoinRequestDeclined(reason);
            if (!result.valid())
                throw gu_err(result.get<sol::error>().what());
        };
        session.join(username);
    };

    luaEnvironment["loadOrCreateLevel"] = [](const char *path)
    {
        auto &session = Game::getCurrentSession();
        auto singleplayerSession = dynamic_cast<SingleplayerSession *>(&session);
        if (singleplayerSession)
            singleplayerSession->setLevel(new Level(path));
    };
}

void UIScreen::render(double deltaTime)
{
    assert(indexedFbo != NULL);
    gu::profiler::Zone z("UI");

    lineRenderer.projection = cam.combined;

    update(deltaTime); // todo: move this? Update ALL UIScreens? or only the active one?

    cursorPosition = cam.getCursorRayDirection() + cam.position;

    {   // starting points of the render tree (UIElements that are NOT a Child):

        UIContainer uiContainer;
        uiContainer.fixedHeight = cam.viewportHeight;
        uiContainer.fixedWidth = cam.viewportWidth;
        uiContainer.innerTopLeft = ivec2(cam.viewportWidth * -.5, cam.viewportHeight * .5);
        uiContainer.innerHeight = cam.viewportHeight;
        uiContainer.minX = uiContainer.innerTopLeft.x;
        uiContainer.maxX = uiContainer.minX + cam.viewportWidth;
        uiContainer.textCursor = uiContainer.innerTopLeft;

        entities.view<UIElement>(entt::exclude<Child>).each([&] (auto e, UIElement &el) {
            renderUIElement(e, el, uiContainer, deltaTime);
        });
    }

    {   // indexed image:
        indexedFbo->bind();

        uint zero = 0;
        glClearBufferuiv(GL_COLOR, 0, &zero);

        glClear(GL_DEPTH_BUFFER_BIT);
        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LESS);

        textRenderer.render(cam);
        spriteRenderer.render(cam);
        nineSliceRenderer.render(cam);

        glDisable(GL_DEPTH_TEST);
        indexedFbo->unbind();
    }

    {   // indexed fbo -> rgb
        applyPaletteUIShader.use();


        glUniform2i(applyPaletteUIShader.location("realResolution"), gu::widthPixels, gu::heightPixels);

        auto palettesTexture = Game::palettes->get3DTexture();
        palettesTexture->bind(0);
        glUniform1i(applyPaletteUIShader.location("palettes"), 0);
        glUniform1ui(applyPaletteUIShader.location("paletteEffect"), RoomScreen::currentPaletteEffect);
        glUniform1ui(applyPaletteUIShader.location("prevPaletteEffect"), RoomScreen::prevPaletteEffect);
        glUniform1f(applyPaletteUIShader.location("timeSinceNewPaletteEffect"), RoomScreen::timeSinceNewPaletteEffect);

        indexedFbo->colorTexture->bind(1, applyPaletteUIShader, "indexedImage");

        Mesh::getQuad()->render();
    }

    renderDebugStuff();
}

void UIScreen::onResize()
{
    cam.viewportWidth = ceil(gu::widthPixels / 3.);
    cam.viewportHeight = ceil(gu::heightPixels / 3.);
    cam.update();

    // create a new framebuffer to render the pixelated UIScreen to:
    delete indexedFbo;
    indexedFbo = new FrameBuffer(cam.viewportWidth, cam.viewportHeight, 0);
    indexedFbo->addColorTexture(GL_R8UI, GL_RED_INTEGER, GL_NEAREST, GL_NEAREST);
    indexedFbo->addDepthTexture(GL_NEAREST, GL_NEAREST);
}

void UIScreen::renderDebugStuff()
{
    if (!Game::settings.showDeveloperOptions)
        return;

    lineRenderer.projection = cam.combined;

    inspector.drawGUI(&cam, lineRenderer);

    ImGui::BeginMainMenuBar();
    if (ImGui::BeginMenu("UI"))
    {
        ImGui::Separator();

        ImGui::TextDisabled("Active UIScreen:");
        ImGui::Text("%s", script.getLoadedAsset().fullPath.c_str());

        ImGui::EndMenu();
    }
    ImGui::EndMainMenuBar();
}

void UIScreen::renderUIElement(entt::entity e, UIElement &el, UIContainer &container, double deltaTime)
{
    if (el.startOnNewLine)
        container.goToNewLine(el.lineSpacing);

    if (auto *textView = entities.try_get<TextView>(e))
        textRenderer.add(*textView, container, el);

    else if (auto *spriteView = entities.try_get<AsepriteView>(e))
    {
        if (!spriteView->sprite.isSet())
            return;

        int width = spriteView->sprite->width, height = spriteView->sprite->height;

        if (el.absolutePositioning)
        {
            ivec2 pos = el.getAbsolutePosition(container, width, height);
            pos.y -= height;
            spriteRenderer.add(*spriteView, pos + el.renderOffset);
        }
        else
        {
            container.resizeOrNewLine(width, el.lineSpacing);

            if (container.centerAlign)
                container.textCursor.x -= width / 2;

            spriteRenderer.add(*spriteView, container.textCursor - ivec2(0, height) + el.renderOffset);
            container.textCursor.x += width;

            container.resizeLineHeight(height);
        }
    }

    else if (auto *childContainer = entities.try_get<UIContainer>(e))
        renderUIContainer(e, el, *childContainer, container, deltaTime);
}

void UIScreen::renderUIContainer(entt::entity e, UIElement &el, UIContainer &cont, UIContainer &parentCont, double deltaTime)
{
    ivec2 outerTopLeft;
    if (el.absolutePositioning)
        outerTopLeft = el.getAbsolutePosition(parentCont, cont.fixedWidth, cont.fixedHeight);
    else
        outerTopLeft = parentCont.textCursor + ivec2(el.margin.x, -el.margin.y);
    outerTopLeft += el.renderOffset;

    if (parentCont.centerAlign)
        outerTopLeft.x -= cont.fixedWidth / 2;

    cont.innerTopLeft = outerTopLeft + cont.padding * ivec2(1, -1);

    if (cont.nineSliceSprite.isSet())
    {
        cont.spriteSlice = &cont.nineSliceSprite->getSliceByName("9slice", 0);
        cont.nineSlice = cont.spriteSlice->nineSlice.has_value() ? &cont.spriteSlice->nineSlice.value() : NULL;
    }

    if (cont.nineSlice)
        cont.innerTopLeft += cont.nineSlice->topLeftOffset * ivec2(1, -1);

    ivec2 size(cont.fixedWidth, cont.fixedHeight);

    if (cont.fillRemainingParentHeight)
    {
        int maxY = parentCont.innerTopLeft.y - parentCont.innerHeight;
        size.y = outerTopLeft.y - maxY;
    }
    if (cont.fillRemainingParentWidth)
        size.x = parentCont.maxX - parentCont.textCursor.x;

    cont.innerHeight = size.y - (outerTopLeft.y - cont.innerTopLeft.y) * 2;

    cont.minX = cont.innerTopLeft.x;
    cont.maxX = cont.minX + size.x - cont.padding.x * 2;
    cont.textCursor = cont.innerTopLeft;
    cont.resetCursorX();

    if (cont.nineSlice)
        cont.maxX -= (cont.spriteSlice->width - cont.nineSlice->innerSize.x - cont.nineSlice->topLeftOffset.x) * 2;

    int originalMaxX = cont.maxX;

    cont.currentLineHeight = 0;
    if (auto *parent = entities.try_get<Parent>(e))
        for (auto child : parent->children)
            if (auto *childEl = entities.try_get<UIElement>(child))
                renderUIElement(child, *childEl, cont, deltaTime);

    if (cont.autoHeight)
    {
        size.y = -(cont.textCursor.y - outerTopLeft.y) + cont.currentLineHeight + cont.padding.y;

        if (cont.nineSlice)
            size.y += cont.spriteSlice->height - cont.nineSlice->innerSize.y - cont.nineSlice->topLeftOffset.y;
    }
    if (cont.autoWidth)
        size.x += cont.maxX - originalMaxX;

    if (cont.nineSlice)
        nineSliceRenderer.add(cont.nineSliceSprite.get(), ivec3(outerTopLeft, 0), size);

    parentCont.textCursor.x += size.x + el.margin.x * 2;
    parentCont.resizeLineHeight(size.y + el.margin.y * 2);
}
