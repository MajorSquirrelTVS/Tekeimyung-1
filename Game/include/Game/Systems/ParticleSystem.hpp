#pragma once

#include <unordered_map>
#include <vector>

#include <ECS/System.hpp>

#include <Engine/Utils/MonitoringDebugWindow.hpp>
#include <Engine/Graphics/Model.hpp>
#include <Engine/Graphics/Geometries/Geometry.hpp>

#include <Game/Components.hh>

#define PARTICLE_SYSTEM_NAME "Particle system"

#define MAX_PARTICLES   5000

struct sParticle
{
    glm::vec3 pos;
    glm::vec3 velocity;
    float speed;

    // How long a particle live, in frames
    int life;

    // Particle color
    glm::vec4 color;
    // Increment color by colorStep each frame
    glm::vec4 colorStep;

    // Particle scale
    glm::vec3 size;
    // Increment size by sizeStep each frame
    glm::vec3 sizeStep;
};

struct sEmitter
{
    std::vector<sParticle>  particles;
    unsigned int            particlesNb;
    float                   life;
    float                   elapsedTime;
};

START_SYSTEM(ParticleSystem)
public:
    ParticleSystem();
    virtual ~ParticleSystem();

    virtual void    update(EntityManager &em, float elapsedTime);
    std::unordered_map<uint32_t, sEmitter*>* getEmitters();

private:
    void            initEmitter(Entity* entity);
    void            updateEmitter(EntityManager &em, Entity* entity, float elapsedTime);
    void            removeEmitter(uint32_t id);

private:
    std::unordered_map<uint32_t, sEmitter*>     _emitters;

    ADD_MONITORING_VAR
END_SYSTEM(ParticleSystem)
