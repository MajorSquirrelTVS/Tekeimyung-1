/**
* @Author   Simon Ambroise
*/

#include <algorithm>
#include <cmath>
#include <limits>

#include <Engine/Core/Components/BoxColliderComponent.hh>
#include <Engine/Core/Components/RenderComponent.hh>
#include <Engine/Core/Components/SphereColliderComponent.hh>
#include <Engine/Core/Components/TransformComponent.hh>
#include <Engine/Graphics/Geometries/Geometry.hpp>
#include <Engine/Physics/Collisions.hpp>

Collisions::Collisions()
{}

Collisions::~Collisions()
{}

bool    Collisions::sphereVSsphere(const glm::vec3& pos1, float radius1, const glm::vec3& pos2, float radius2)
{
    float distance = pow((pos1.x - pos2.x), 2) + pow((pos1.y - pos2.y), 2) + pow((pos1.z - pos2.z), 2);

    float sumRadius = pow(radius2 + radius1, 2);
    return (distance <= sumRadius);
}

bool    Collisions::pointVSAABB(const glm::vec3& point, const glm::vec3& boxPosition, const glm::vec3& boxSize)
{
    return  (point.x >= boxPosition.x && point.x <= boxPosition.x + boxSize.x) &&
        (point.y >= boxPosition.y && point.y <= boxPosition.y + boxSize.y) &&
        (point.z >= boxPosition.z && point.z <= boxPosition.z + boxSize.z);
}

bool    Collisions::pointVSAABB2D(const glm::vec2& point, const glm::vec2& boxPosition, const glm::vec2& boxSize)
{
    return  (point.x >= boxPosition.x && point.x <= boxPosition.x + boxSize.x) &&
        (point.y >= boxPosition.y && point.y <= boxPosition.y + boxSize.y);
}

double SquaredDistPointAABB(const glm::vec3& p, const glm::vec3& min, const glm::vec3& max)
{
    auto check = [&](
        const double pn,
        const double bmin,
        const double bmax) -> double
    {
        double out = 0;
        double v = pn;

        if (v < bmin)
        {
            double val = (bmin - v);
            out += val * val;
        }

        if (v > bmax)
        {
            double val = (v - bmax);
            out += val * val;
        }

        return out;
    };

    double sq = 0.0;

    sq += check(p.x, min.x, max.x);
    sq += check(p.y, min.y, max.y);
    sq += check(p.z, min.z, max.z);

    return sq;
}

bool    Collisions::AABBVSAABB(const glm::vec3& boxPos1, const glm::vec3& boxSize1, const glm::vec3& boxPos2, const glm::vec3& boxSize2)
{
    glm::vec3 boxMin1 = glm::vec3(
            boxPos1.x - (boxSize1.x / 2.0f),
            boxPos1.y - (boxSize1.y / 2.0f),
            boxPos1.z - (boxSize1.z / 2.0f)
        );
    glm::vec3 boxMax1 = glm::vec3(
            boxPos1.x + (boxSize1.x / 2.0f),
            boxPos1.y + (boxSize1.y / 2.0f),
            boxPos1.z + (boxSize1.z / 2.0f)
        );
    glm::vec3 boxMin2 = glm::vec3(
            boxPos2.x - (boxSize2.x / 2.0f),
            boxPos2.y - (boxSize2.y / 2.0f),
            boxPos2.z - (boxSize2.z / 2.0f)
        );
    glm::vec3 boxMax2 = glm::vec3(
            boxPos2.x + (boxSize2.x / 2.0f),
            boxPos2.y + (boxSize2.y / 2.0f),
            boxPos2.z + (boxSize2.z / 2.0f)
        );

    return (boxMin1.x <= boxMax2.x && boxMax1.x >= boxMin2.x) &&
        (boxMin1.y <= boxMax2.y && boxMax1.y >= boxMin2.y) &&
        (boxMin1.z <= boxMax2.z && boxMax1.z >= boxMin2.z);
}

bool    Collisions::sphereVSAABB(const glm::vec3& spherePosition, float sphereRadius, const glm::vec3& boxPosition, const glm::vec3& boxSize)
{
    glm::vec3 min;
    min.x = boxPosition.x - (boxSize.x / 2.0f);
    min.y = boxPosition.y - (boxSize.y / 2.0f);
    min.z = boxPosition.z - (boxSize.z / 2.0f);
    double squaredDistance = SquaredDistPointAABB(spherePosition, min, min + boxSize);

    return squaredDistance < (sphereRadius * sphereRadius);
}



float Collisions::rayVSAABB(const Ray& ray, const glm::vec3& boxMin, const glm::vec3& boxMax)
{
    float tNear = FLT_MIN;
    float tFar = FLT_MAX;

    // X planes
    {
        if (ray.direction.x == 0) // Ray parallel to the plane
        {
            if (ray.origin.x < boxMin.x || ray.origin.x > boxMax.x) // Ray not beteween the two planes
            {
                return (0);
            }
        }
        float t1 = (boxMin.x - ray.origin.x) / ray.direction.x;
        float t2 = (boxMax.x - ray.origin.x) / ray.direction.x;
        if (t1 > t2)
        {
            float tmp = t1;
            t1 = t2;
            t2 = tmp;
        }
        if (t1 > tNear)
            tNear = t1;
        if (t2 < tFar)
            tFar = t2;
        if (tNear > tFar)
            return (0);
        if (tFar < 0)
            return (0);

    }
    // Y planes
    {
        if (ray.direction.y == 0) // Ray parallel to the plane
        {
            if (ray.origin.y < boxMin.y || ray.origin.y > boxMax.y) // Ray not beteween the two planes
            {
                return (0);
            }
        }
        float t1 = (boxMin.y - ray.origin.y) / ray.direction.y;
        float t2 = (boxMax.y - ray.origin.y) / ray.direction.y;
        if (t1 > t2)
        {
            float tmp = t1;
            t1 = t2;
            t2 = tmp;
        }
        if (t1 > tNear)
            tNear = t1;
        if (t2 < tFar)
            tFar = t2;
        if (tNear > tFar)
            return (0);
        if (tFar < 0)
            return (0);

    }
    // Z planes
    {
        if (ray.direction.z == 0) // Ray parallel to the plane
        {
            if (ray.origin.z < boxMin.z || ray.origin.z > boxMax.z) // Ray not beteween the two planes
            {
                return (0);
            }
        }
        float t1 = (boxMin.z - ray.origin.z) / ray.direction.z;
        float t2 = (boxMax.z - ray.origin.z) / ray.direction.z;
        if (t1 > t2)
        {
            float tmp = t1;
            t1 = t2;
            t2 = tmp;
        }
        if (t1 > tNear)
            tNear = t1;
        if (t2 < tFar)
            tFar = t2;
        if (tNear > tFar)
            return (0);
        if (tFar < 0)
            return (0);

    }
    return (tNear);
}

bool    Collisions::isCollidingSphereAndEntity(glm::vec3 spherePos, float sphereRadius, Entity* entity)
{
    sTransformComponent* entityTransform = entity->getComponent<sTransformComponent>();

    if (entity->getComponent<sSphereColliderComponent>() != nullptr)
    {
        sSphereColliderComponent* entitySphereCollider = entity->getComponent<sSphereColliderComponent>();

        return (Collisions::sphereVSsphere(
            spherePos,
            sphereRadius * (SIZE_UNIT / 2.0f),
            entitySphereCollider->pos + entityTransform->getPos(),
            entitySphereCollider->radius * std::max({ entityTransform->getScale().x, entityTransform->getScale().y, entityTransform->getScale().z }) * (SIZE_UNIT / 2.0f)
        ));
    }

    if (entity->getComponent<sBoxColliderComponent>() != nullptr)
    {
        sBoxColliderComponent* entityBoxCollider = entity->getComponent<sBoxColliderComponent>();

        return (Collisions::sphereVSAABB(
            spherePos,
            sphereRadius * (SIZE_UNIT / 2.0f),
            entityBoxCollider->pos + entityTransform->getPos(),
            glm::vec3(SIZE_UNIT * entityBoxCollider->size.x * entityTransform->getScale().x,
                SIZE_UNIT * entityBoxCollider->size.y * entityTransform->getScale().y,
                SIZE_UNIT * entityBoxCollider->size.z * entityTransform->getScale().z)
        ));
    }

    return false;
}

bool    Collisions::isColliding(Entity *firstEntity, Entity *secondEntity)
{
    sTransformComponent* firstTransform = firstEntity->getComponent<sTransformComponent>();
    sSphereColliderComponent* firstSphereCollider = firstEntity->getComponent<sSphereColliderComponent>();
    sBoxColliderComponent* firstBoxCollider = firstEntity->getComponent<sBoxColliderComponent>();
    sTransformComponent* secondTransform = secondEntity->getComponent<sTransformComponent>();
    sSphereColliderComponent* secondSphereCollider = secondEntity->getComponent<sSphereColliderComponent>();
    sBoxColliderComponent* secondBoxCollider = secondEntity->getComponent<sBoxColliderComponent>();

    if (firstSphereCollider != nullptr && secondSphereCollider != nullptr)
    {
        return (Collisions::sphereVSsphere(
            firstSphereCollider->pos + firstTransform->getPos(),
            firstSphereCollider->radius * std::max({ firstTransform->getScale().x, firstTransform->getScale().y, firstTransform->getScale().z }) * (SIZE_UNIT / 2.0f),
            secondSphereCollider->pos + secondTransform->getPos(),
            secondSphereCollider->radius * std::max({ secondTransform->getScale().x, secondTransform->getScale().y, secondTransform->getScale().z }) * (SIZE_UNIT / 2.0f)
        ));
    }

    if (firstBoxCollider != nullptr && secondBoxCollider != nullptr)
    {
        sRenderComponent* firstRender = firstEntity->getComponent<sRenderComponent>();
        sRenderComponent* secondRender = secondEntity->getComponent<sRenderComponent>();
        const glm::vec3& firstModelPivot = firstRender->getModel()->getPivot();
        const glm::vec3& secondModelPivot = secondRender->getModel()->getPivot();

        return (Collisions::AABBVSAABB(
                firstBoxCollider->pos + firstTransform->getPos(),
                glm::vec3(SIZE_UNIT * firstBoxCollider->size.x * firstTransform->getScale().x,
                    SIZE_UNIT * firstBoxCollider->size.y * firstTransform->getScale().y,
                    SIZE_UNIT * firstBoxCollider->size.z * firstTransform->getScale().z),
                secondBoxCollider->pos + secondTransform->getPos(),
                glm::vec3(SIZE_UNIT * secondBoxCollider->size.x * secondTransform->getScale().x,
                    SIZE_UNIT * secondBoxCollider->size.y * secondTransform->getScale().y,
                    SIZE_UNIT * secondBoxCollider->size.z * secondTransform->getScale().z)
            ));
    }

    if (firstBoxCollider != nullptr && secondSphereCollider != nullptr)
    {
        return (Collisions::sphereVSAABB(
            secondSphereCollider->pos + secondTransform->getPos(),
            secondSphereCollider->radius * std::max({ secondTransform->getScale().x, secondTransform->getScale().y, secondTransform->getScale().z }) * (SIZE_UNIT / 2.0f),
            firstBoxCollider->pos + firstTransform->getPos(),
            glm::vec3(SIZE_UNIT * firstBoxCollider->size.x * firstTransform->getScale().x,
                SIZE_UNIT * firstBoxCollider->size.y * firstTransform->getScale().y,
                SIZE_UNIT * firstBoxCollider->size.z * firstTransform->getScale().z)
        ));
    }

    if (secondBoxCollider != nullptr && firstSphereCollider != nullptr)
    {
        return (Collisions::sphereVSAABB(
            firstSphereCollider->pos + firstTransform->getPos(),
            firstSphereCollider->radius * std::max({ firstTransform->getScale().x, firstTransform->getScale().y, firstTransform->getScale().z }) * (SIZE_UNIT / 2.0f),
            secondBoxCollider->pos + secondTransform->getPos(),
            glm::vec3(SIZE_UNIT * secondBoxCollider->size.x * secondTransform->getScale().x,
                SIZE_UNIT * secondBoxCollider->size.y * secondTransform->getScale().y,
                SIZE_UNIT * secondBoxCollider->size.z * secondTransform->getScale().z)
        ));
    }

    return (false);
}
