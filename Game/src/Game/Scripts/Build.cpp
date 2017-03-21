/*
** @Author : Simon AMBROISE
*/

#include <Engine/Components.hh>

#include <Engine/Physics/Collisions.hpp>
#include <Engine/EntityFactory.hpp>

#include <Game/Scripts/Build.hpp>
#include <Game/Scripts/Tile.hpp>
#include <Game/Scripts/Teleport.hpp>

void Build::start()
{
    // BlockBrown
    this->_buildableItems["BlockBrown"].push_back("TRAP_NEEDLE");
    this->_buildableItems["BlockBrown"].push_back("TRAP_CUTTER");
    this->_buildableItems["BlockBrown"].push_back("TRAP_FIRE");
    this->_buildableItems["BlockBrown"].push_back("BLOCK_GREEN");

    // BlockGreen
    this->_buildableItems["BlockGreen"].push_back("TOWER_FIRE");

    //this->_buildableItems.push_back("TP_FIRST");
    //this->_buildableItems.push_back("TP_SECOND");

    this->_buildEnabled = false;
    this->_buildableRadius = 7.7f;
    this->_currentIdx = 0;
    this->_tile = nullptr;
    this->_preview = nullptr;

    this->_transform = this->getComponent<sTransformComponent>();
    this->_render = this->getComponent<sRenderComponent>();
}

void Build::update(float dt)
{
    this->buildInput();
    this->checkBuildableZone();
}

void Build::buildInput()
{
    if (this->_buildEnabled && this->mouse.getStateMap()[Mouse::eButton::MOUSE_BUTTON_1] == Mouse::eButtonState::CLICK_PRESSED)
    {
        if (this->_tile != nullptr)
        {
            auto& position = this->_tile->getComponent<sTransformComponent>()->getPos();

            auto entity = this->Instantiate(this->_buildableItems[this->_tile->getTag()][this->_currentIdx], glm::vec3(position.x, position.y + 12.5f, position.z));
            auto previewRenderer = entity->getComponent<sRenderComponent>();
            previewRenderer->ignoreRaycast = true;

            if (this->_buildableItems[this->_tile->getTag()][this->_currentIdx] == "TP_FIRST")
            {
                this->_buildSecondTP = true;
                this->_currentIdx = static_cast<int>(this->_buildableItems.size() - 1);
                this->_firstTpPos = glm::vec3(position.x, position.y + 12.5f, position.z);
                this->firstTp = entity;
            }
            else if (this->_buildableItems[this->_tile->getTag()][this->_currentIdx] == "TP_SECOND")
            {
                this->_buildSecondTP = false;
                this->_currentIdx = 0;

                entity->getComponent<sScriptComponent>()->getScript<Teleport>("Teleport")->setTPPos(this->_firstTpPos);
                entity->getComponent<sScriptComponent>()->getScript<Teleport>("Teleport")->setActivity(false);
                this->firstTp->getComponent<sScriptComponent>()->getScript<Teleport>("Teleport")->setTPPos(glm::vec3(position.x, position.y + 12.5f, position.z));
                this->firstTp->getComponent<sScriptComponent>()->getScript<Teleport>("Teleport")->setActivity(true);
            }
        }
    }

    if (this->_buildEnabled && this->mouse.getStateMap()[Mouse::eButton::MOUSE_BUTTON_2] == Mouse::eButtonState::CLICK_PRESSED && !this->_buildSecondTP)
    {
        this->_currentIdx++;

        if (this->_currentIdx >= this->_buildableItems[this->_tile->getTag()].size())
            this->_currentIdx = 0;

        if (this->_tile != nullptr && this->_preview != nullptr)
        {
            this->Destroy(this->_preview);

            auto& position = this->_tile->getComponent<sTransformComponent>()->getPos();
            this->_preview = this->Instantiate(this->_buildableItems[this->_tile->getTag()][this->_currentIdx], glm::vec3(position.x, position.y + 12.5f, position.z));
            auto previewRenderer = this->_preview->getComponent<sRenderComponent>();
            previewRenderer->ignoreRaycast = true;
        }
    }

    if (this->keyboard[Keyboard::eKey::B] == Keyboard::eKeyState::KEY_PRESSED && !this->_buildSecondTP)
    {
        this->_buildEnabled = !this->_buildEnabled;
        if (!this->_buildEnabled && this->_preview)
        {
            this->Destroy(this->_preview);
            this->_preview = nullptr;
        }
    }
}

void Build::setTile(const Entity* tile)
{
    if (this->_tile != tile)
    {
        if (this->_preview != nullptr)
        {
            this->Destroy(this->_preview);
            this->_preview = nullptr;
        }

        this->_tile = tile;

        if (this->_tile != nullptr && this->_tile->getComponent<sScriptComponent>()->getScript<Tile>("Tile")->isBuildable())
        {
            auto& position = this->_tile->getComponent<sTransformComponent>()->getPos();
            this->_preview = this->Instantiate(this->_buildableItems[this->_tile->getTag()][this->_currentIdx], glm::vec3(position.x, position.y + 12.5f, position.z));
            auto previewRenderer = this->_preview->getComponent<sRenderComponent>();
            previewRenderer->ignoreRaycast = true;
        }
    }
}

void Build::checkBuildableZone()
{
    auto em = EntityFactory::getBindedEntityManager();

    const auto& tilesBrown = em->getEntitiesByTag("BlockBrown");
    const auto& tilesGreen = em->getEntitiesByTag("BlockGreen");

    for (auto &tile : tilesBrown)
    {
        auto box = tile->getComponent<sBoxColliderComponent>();
        if (box != nullptr)
        {
            auto pos = tile->getComponent<sTransformComponent>()->getPos();
            auto scriptComponent = tile->getComponent<sScriptComponent>();

            if (!scriptComponent)
                continue;

            Tile* tile = scriptComponent->getScript<Tile>("Tile");
            if (!tile)
                continue;

            tile->setBuildable(false);

            if (!_buildEnabled)
                continue;

            if (Collisions::sphereVSAABB(_transform->getPos(), this->_buildableRadius * SIZE_UNIT, box->pos + pos, glm::vec3(box->size.x * SIZE_UNIT, box->size.y * SIZE_UNIT, box->size.z * SIZE_UNIT)))
            {
                tile->setBuildable(true);
            }
        }
    }

    for (auto &tile : tilesGreen)
    {
        auto box = tile->getComponent<sBoxColliderComponent>();
        if (box != nullptr)
        {
            auto pos = tile->getComponent<sTransformComponent>()->getPos();
            auto scriptComponent = tile->getComponent<sScriptComponent>();

            if (!scriptComponent)
                continue;

            Tile* tile = scriptComponent->getScript<Tile>("Tile");
            if (!tile)
                continue;

            tile->setBuildable(false);

            if (!_buildEnabled)
                continue;

            if (Collisions::sphereVSAABB(_transform->getPos(), this->_buildableRadius * SIZE_UNIT, box->pos + pos, glm::vec3(box->size.x * SIZE_UNIT, box->size.y * SIZE_UNIT, box->size.z * SIZE_UNIT)))
            {
                tile->setBuildable(true);
            }
        }
    }
}
