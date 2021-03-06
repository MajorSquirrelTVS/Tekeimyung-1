/*
** @Author : Simon AMBROISE
*/

#include <Engine/Core/Components/Components.hh>

#include <Engine/EntityFactory.hpp>
#include <Engine/Utils/ResourceManager.hpp>

#include <Game/Building/Tile.hpp>
#include <Game/Character/NewBuild.hpp>

REGISTER_SCRIPT(Tile);

void Tile::start()
{
    _render = this->getComponent<sRenderComponent>();
    _renderMaterial = *_render->getModelInstance()->getMeshsInstances()[0]->getMaterial();
    _buildMaterial = ResourceManager::getInstance()->getResource<Material>("build.mat");
}

void Tile::update(float dt)
{
}

void Tile::onHoverEnter()
{
    auto    em = EntityFactory::getBindedEntityManager();
    auto    buildScript = em->getEntityByTag("Player")->getComponent<sScriptComponent>()->getScript<NewBuild>("NewBuild");

    buildScript->setTileHovered(this->getEntity());
}

void Tile::onHoverExit()
{
    auto    em = EntityFactory::getBindedEntityManager();
    auto    buildScript = em->getEntityByTag("Player")->getComponent<sScriptComponent>()->getScript<NewBuild>("NewBuild");

    buildScript->setTileHovered(nullptr);
}

void Tile::setBuildable(bool buildable)
{
    Material*   displayedMaterial;

    this->_buildable = buildable;
    displayedMaterial = (buildable == true ? this->_buildMaterial : &this->_renderMaterial);
    this->_render->getModelInstance()->setMaterial(displayedMaterial);
}

bool Tile::isBuildable()
{
    return this->_buildable;
}
