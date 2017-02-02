#include <Engine/Components.hh>

#include <Game/Scripts/Castle.hpp>

void Castle::Start()
{
    this->health = 500;
}

void Castle::Update(float dt)
{
}

void Castle::OnCollisionEnter(Entity* entity)
{
    if (entity->getComponent<sNameComponent>()->value == "PLAYER")
    {
        LOG_DEBUG("Damage done");
        this->TakeDamage(25);
    }
}

void Castle::Death()
{
    this->Destroy();
    LOG_DEBUG("GAME OVER");
}