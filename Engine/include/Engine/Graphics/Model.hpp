/**
* @Author   Guillaume Labey
*/

#pragma once

# include <cstdint>
# include <vector>
# include <memory>
# include <glm/gtx/quaternion.hpp>
# include <assimp/Importer.hpp>

# include <Engine/Graphics/Mesh.hpp>
# include <Engine/Graphics/Buffer.hpp>
# include <Engine/Graphics/ShaderProgram.hpp>
# include <Engine/Utils/Resource.hpp>

#define BUFFER_OFFSET(bytes) ((GLubyte*) NULL + (bytes))

class Model: public Resource
{
public:
    Model();
    ~Model();

    bool                        loadFromFile(const std::string &fileName) override;

    uint32_t                    getVertexsSize() const;
    uint32_t                    getIndicesSize() const;
    const std::vector<std::unique_ptr<Mesh> > &getMeshs() const;
    const glm::vec3&            getSize() const;
    const glm::vec3&            getMin() const;
    const glm::vec3&            getMax() const;
    const glm::vec3&            getPivot() const;

    static Resource::eType      getResourceType() { return Resource::eType::MODEL; }

    const Buffer&               getBuffer() const;
    GLuint                      getPrimitiveType() const;

    virtual bool                isGeometry() const;

protected:
    void                        initVertexData();
    void                        initIndexData();

    void                        calculateSize();

private:
    void                        transformVertices(aiScene* scene, aiNode* node);
    void                        computeSceneNodeAbsoluteTransform(aiNode* node);

protected:
    std::vector<std::unique_ptr<Mesh> >   _meshs;

    // Vertices raw buffers
    Vertex*                             _vertexData;
    GLuint*                             _indexData;

    // OpenGL Buffers
    Buffer                              _buffer;

    aiScene*                            _scene;
    Assimp::Importer                    _importer;
    float                               _anim;

    glm::vec3                           _size;
    glm::vec3                           _min;
    glm::vec3                           _max;
    glm::vec3                           _pivot;

    GLuint                              _primitiveType;
};
