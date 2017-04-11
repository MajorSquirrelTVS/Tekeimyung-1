/**
* @Author   Simon AMBROISE
*/

#pragma once

#include <Game/Weapons/IWeapon.hpp>

class LaserWeapon final : public IWeapon
{
public:
    LaserWeapon();
    ~LaserWeapon() = default;

public:
    virtual void fire(sTransformComponent* playerTransform, sRenderComponent* playerRender, glm::vec3 playerDirection);
    virtual void reload();
};
