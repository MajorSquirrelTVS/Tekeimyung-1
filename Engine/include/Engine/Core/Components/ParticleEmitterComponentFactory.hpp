/**
* @Author   Guillaume Labey
*/

#pragma once

#include <Engine/Core/Components/IComponentFactory.hpp>
#include <Engine/Core/Components/ParticleEmitterComponent.hh>

template <>
class ComponentFactory<sParticleEmitterComponent> final: public BaseComponentFactory<sParticleEmitterComponent>
{
public:
    const char* getTypeName() override final { return "sParticleEmitterComponent"; }
    sComponent* loadFromJson(const std::string& entityType, const JsonValue& json) override final;
    JsonValue& saveToJson(const std::string& entityType, const sComponent* savedComponent = nullptr, JsonValue* toJson = nullptr) override final;

    bool    updateEditor(const std::string& entityType, sComponent** savedComponent, sComponent* entityComponent, Entity* entity) override final;
};
