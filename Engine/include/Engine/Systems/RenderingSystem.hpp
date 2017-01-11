/**
* @Author   Guillaume Labey
*/

#pragma once

#include <unordered_map>
#include <memory>

#include <ECS/System.hpp>

#include <Engine/Utils/MonitoringDebugWindow.hpp>
#include <Engine/Graphics/ShaderProgram.hpp>
#include <Engine/Graphics/Model.hpp>
#include <Engine/Graphics/Camera.hpp>

#include <Engine/Components.hh>
#include <Engine/Systems/ParticleSystem.hpp>

#define RENDERING_SYSTEM_NAME "Rendering system"

START_SYSTEM(RenderingSystem)
public:
    RenderingSystem(Camera* camera, std::unordered_map<uint32_t, sEmitter*>* particleEmitters);
    virtual ~RenderingSystem();

    virtual void update(EntityManager& em, float elapsedTime);
    virtual bool                            init();
    void                                    renderEntity(sRenderComponent *render, Entity* entity, float elapsedTime);
    void                                    renderCollider(Entity* entity);
    void                                    renderColliders(EntityManager& em);
    void                                    renderParticles(EntityManager& em, float elapsedTime);

    virtual bool                            onEntityNewComponent(Entity* entity, sComponent* component);
    virtual bool                            onEntityRemovedComponent(Entity* entity, sComponent* component);
    virtual bool                            onEntityDeleted(Entity* entity);

private:
    bool                                    isTransparent(sRenderComponent *render) const;
    std::shared_ptr<Model>                  getModel(sRenderComponent *render);

private:
    std::unordered_map<uint32_t, sEmitter*>*    _particleEmitters;

    ADD_MONITORING_VAR

    Camera*                                     _camera;

    std::unordered_map<uint32_t, Entity*>       _transparentEntities;

    std::vector<uint32_t>                       _collidableEntities;
END_SYSTEM(RenderingSystem)