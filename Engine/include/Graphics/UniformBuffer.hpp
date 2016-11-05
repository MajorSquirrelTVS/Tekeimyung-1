#pragma once

#include <GL/glew.h>
#include <cstdint>

#include "Graphics/ShaderProgram.hpp"

class UniformBuffer
{
public:
    UniformBuffer();
    ~UniformBuffer();

    void                    update(void* data, uint32_t size);

    // Bind UBO with uniform buffer block in shader
    void                    bind(const ShaderProgram& shaderProgram, const char* blockName) const;

    void                    setBindingPoint(uint16_t bindingPoint);

private:
    // Uniform buffer object
    GLuint                  _UBO;

    bool                    _firstUpdate;

    GLuint                  _bindingPoint;
};
