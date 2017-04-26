/**
* @Author   Guillaume Labey
*/

#include <iostream>
#include <imgui.h>
#include <imgui_impl_glfw_gl3.h>
#include <ImGuizmo.h>
#include <algorithm>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <Engine/Components.hh>
#include <Engine/Graphics/UI/Font.hpp>
#include <Engine/Graphics/Material.hpp>
#include <Engine/Utils/Exception.hpp>
#include <Engine/Utils/Logger.hpp>
#include <Engine/Utils/ResourceManager.hpp>
#include <Engine/Window/GameWindow.hpp>

#include <Engine/Graphics/Renderer.hpp>

std::shared_ptr<Renderer>   Renderer::_instance = nullptr;

Renderer::Renderer(): _currentCamera(nullptr)
{
    _defaultLight.setDirection({-1.0f, -0.3f, 0.3f});

    _UILight.setDirection({0.0f, 0.0f, 1.0f});
    _UILight.setAmbient({1.0f, 1.0f, 1.0f});
    _UILight.setDiffuse({0.0f, 0.0f, 0.0f});

    _UICamera.setProjType(Camera::eProj::ORTHOGRAPHIC_2D);
    _UICamera.updateViewport();
    _UICamera.updateUBO();
}

Renderer::~Renderer() {}

std::shared_ptr<Renderer>   Renderer::getInstance()
{
    if (!_instance)
        _instance = std::make_shared<Renderer>();

    return (_instance);
}

bool    Renderer::initialize()
{
    try
    {
        initTextRendering();
        setupShaderPrograms();
        setup2DTextureGeneration();
        onWindowResize();
    }
    catch(const Exception& e)
    {
        std::cerr << e.what() << std::endl;
        return (false);
    }


    // Enable depth buffer
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);

    // Activate back culling
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);

    //Enable scissor
    glEnable(GL_SCISSOR_TEST);
    return (true);
}

void    Renderer::onWindowResize()
{
    _UICamera.updateViewport();
    _UICamera.updateUBO();

    if (!setupMainFrameBuffers())
    {
        EXCEPT(InternalErrorException, "Failed to setup main frame buffer");
    }

    // Update scissor
    glScissor(0,
            0,
            (uint32_t)GameWindow::getInstance()->getBufferWidth(),
            (uint32_t)GameWindow::getInstance()->getBufferHeight());
}

Camera* Renderer::getCurrentCamera()
{
    return (_currentCamera);
}

void    Renderer::setCurrentCamera(Camera* camera)
{
    _currentCamera = camera;
}

void    Renderer::beginFrame()
{
    // Clear window screen
    glClear(GL_COLOR_BUFFER_BIT);

    _frameBuffer.use(GL_FRAMEBUFFER);

    // Clear frame buffer
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    _frameBuffer.unBind(GL_FRAMEBUFFER);

    _transparencyFrameBuffer.use(GL_FRAMEBUFFER);

    // Clear frame buffer
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    _transparencyFrameBuffer.unBind(GL_FRAMEBUFFER);

    for (auto& blurFramebuffer: _blurFramebuffers)
    {
        for (auto& framebuffer: blurFramebuffer)
        {
            framebuffer.use(GL_FRAMEBUFFER);
            glClear(GL_COLOR_BUFFER_BIT);
        }
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    ImGui_ImplGlfwGL3_NewFrame();
    ImGuizmo::BeginFrame();
}

void    Renderer::endFrame()
{
    // Apply bloom and then blend the scene with bloom texture
    {
        glDisable(GL_DEPTH_TEST);
        bloomPass(_frameBuffer.getColorAttachments()[1].get(), _blurFramebuffers);
        finalBlendingPass();
        glEnable(GL_DEPTH_TEST);
    }

    // Display imgui windows
    ImGui::Render();

    // Display screen
    GameWindow::getInstance()->display();
}

void    Renderer::render(Camera* camera, RenderQueue& renderQueue)
{
    if (camera)
    {
        _currentCamera = camera;
    }

    // All the renders will use the color attachments and the depth buffer of the framebuffer
    _frameBuffer.use(GL_FRAMEBUFFER);
    sceneRenderPass(camera, renderQueue);
    _transparencyFrameBuffer.use(GL_FRAMEBUFFER);
    transparencyPass(camera, renderQueue);
    _transparencyFrameBuffer.unBind(GL_FRAMEBUFFER);
}

std::unique_ptr<Texture>    Renderer::generateTextureFromModel(sRenderComponent* renderComponent, uint32_t width, uint32_t height)
{
    float windowBufferWidth = (float)GameWindow::getInstance()->getBufferWidth();
    float windowBufferHeight = (float)GameWindow::getInstance()->getBufferHeight();

    ModelInstance* model = renderComponent->getModelInstance();

    // Init framebuffer
    if (!setupFrameBuffers(_2DFrameBuffer, _2DBlurFrameBuffers, width, height))
    {
        LOG_INFO("Failed to setup 2D render frame buffers");
        return (nullptr);
    }

    // Set camera viewport
    {
        Camera::sViewport viewportRect;
        viewportRect.offset.x = 0.0f;
        viewportRect.offset.y = 0.0f;
        viewportRect.extent.width = (float)width / windowBufferWidth;
        viewportRect.extent.height = (float)height / windowBufferHeight;
        _2DRenderCamera.setViewportRect(viewportRect);
    }


    // Calculate camera projection size so the model fit all the texture
    // The size is the distance between the front left bottom and back right top corners of the model
    // We don't take the real position of the corners, but create a virtual 3D box with the size of the model
    {
        glm::vec3 frontLeftBottom = glm::vec3(_2DRenderCamera.getView() * glm::vec4(0.0f, 0.0f, 0.0f, 1.0f));
        glm::vec3 backRightTop = glm::vec3(_2DRenderCamera.getView() * glm::vec4(model->getModel()->getSize().x, model->getModel()->getSize().y, -model->getModel()->getSize().z, 1.0f));
        float projSize = glm::distance(frontLeftBottom, backRightTop);
        // Inverse the projection so the texture otherwise the texture will be reversed
        _2DRenderCamera.setProjSize(-projSize);
    }

    // Place the object in front of the camera and center it
    {
        glm::vec3 cameraDir = glm::normalize(_2DRenderCamera.getDirection());

        // Calculate the translation the model need to have his pivot point centered
        glm::vec3 modelPivot = glm::vec3(0.0f, -(model->getModel()->getSize().y / 2.0f + model->getModel()->getMin().y), 0.0f);
        modelPivot += glm::vec3(-(model->getModel()->getSize().x / 2.0f + model->getModel()->getMin().x), 0.0f, 0.0f);
        modelPivot += glm::vec3(0.0f, 0.0f, -(model->getModel()->getSize().z / 2.0f + model->getModel()->getMin().z));

        // We place the object at a distance of 200 in front of the camera and center it
        glm::mat4 transform = glm::translate(glm::mat4(1.0f), cameraDir * -200.0f + modelPivot);

        // Update uniform buffer
        _2DRenderBuffer.update((void*)glm::value_ptr(transform), sizeof(glm::mat4));
        _2DRenderBuffer.update((void*)glm::value_ptr(renderComponent->color), sizeof(glm::vec4), sizeof(glm::mat4));
    }

    // The light have the same direction as the camera
    _2DRenderLight.setDirection(-glm::normalize(_2DRenderCamera.getDirection()));

    // Update render queue
    _2DRenderQueue.clear();
    _2DRenderQueue.addModel(model, &_2DRenderBuffer, 0, _2DRenderBuffer.getSize());
    _2DRenderQueue.addLight(&_2DRenderLight);

    // Render the model in our frame buffer
    _2DFrameBuffer.use(GL_FRAMEBUFFER);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    sceneRenderPass(&_2DRenderCamera, _2DRenderQueue);

    // Bloom pass
    {
        bloomPass(_2DFrameBuffer.getColorAttachments()[1].get(), _2DBlurFrameBuffers);
        glViewport(0,
                    0,
                    (uint32_t)width,
                    (uint32_t)height);

        _finalBlendingShaderProgram.use();
        glUniform1i(_finalBlendingShaderProgram.getUniformLocation("image"), 0);

        _2DFrameBuffer.use(GL_FRAMEBUFFER);
        glEnable(GL_BLEND);
        glBlendFunc(GL_ONE, GL_ONE);
        for (uint32_t i = 0; i < _2DBlurFrameBuffers.size(); ++i)
        {
            _2DBlurFrameBuffers[i][1].getColorAttachments()[0]->bind();
            _screenPlane.bind();

            glDrawElements(GL_TRIANGLES,
                        6,
                        GL_UNSIGNED_INT,
                        0);
        }
        _2DFrameBuffer.getColorAttachments()[0]->bind();

        glDrawElements(GL_TRIANGLES,
                    6,
                    GL_UNSIGNED_INT,
                    0);

        glDisable(GL_BLEND);
    }

    _2DFrameBuffer.unBind(GL_FRAMEBUFFER);

    return (std::move(_2DFrameBuffer.getColorAttachments()[0]));
}

void    Renderer::sceneRenderPass(Camera* camera, RenderQueue& renderQueue)
{
    _currentShaderProgram = nullptr;

    // Scene objects
    {
        if (camera)
        {
            auto& lights = renderQueue.getLights();
            uint32_t lightsNb = renderQueue.getLightsNb();

            // Set default light
            if (lightsNb == 0)
            {
                lights[0] = &_defaultLight;
                lightsNb = 1;
            }

            camera->updateViewport();
            camera->updateUBO();
            camera->getUBO().bind();

            glViewport((uint32_t)camera->getViewport().offset.x,
                        (uint32_t)camera->getViewport().offset.y,
                        (uint32_t)camera->getViewport().extent.width,
                        (uint32_t)camera->getViewport().extent.height);

            renderOpaqueObjects(renderQueue.getOpaqueMeshs(), renderQueue.getOpaqueMeshsNb(), lights, lightsNb);

            // Enable blend to blend transparent ojects and particles
            glEnable(GL_BLEND);
            // Disable write to the depth buffer so that the depth of transparent objects is not written
            // because we don't want a transparent object to hide an other transparent object
            glDepthMask(GL_FALSE);
            renderTransparentObjects(renderQueue.getTransparentMeshs(), renderQueue.getTransparentMeshsNb(), lights, lightsNb);
        }
        else if (renderQueue.getOpaqueMeshsNb() + renderQueue.getTransparentMeshsNb() != 0)
        {
            LOG_WARN("Attempt to render non-UI objects with no camera");
        }
    }

    // Disable write to the depth buffer and depth test because UI is sort with layers
    glDepthMask(GL_FALSE);
    glDisable(GL_DEPTH_TEST);

    // Disable blending for opaque objects
    glDisable(GL_BLEND);

    // Render UI objects
    {
        // Use this light for UI
        std::vector<Light*> lights = {&_UILight};

        // Set viewport
        glViewport((uint32_t)_UICamera.getViewport().offset.x,
                    (uint32_t)_UICamera.getViewport().offset.y,
                    (uint32_t)_UICamera.getViewport().extent.width,
                    (uint32_t)_UICamera.getViewport().extent.height);
        _UICamera.getUBO().bind();


        renderOpaqueObjects(renderQueue.getUIOpaqueMeshs(), renderQueue.getUIOpaqueMeshsNb(), lights, 1);


        // Enable blend to blend transparent ojects and particles
        glEnable(GL_BLEND);
        renderTransparentObjects(renderQueue.getUITransparentMeshs(), renderQueue.getUITransparentMeshsNb(), lights, 1);
    }

    // Render texts
    {
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        _textShaderProgram.use();
        glUniform1i(_textShaderProgram.getUniformLocation("textImage"), 0);
        renderTexts(renderQueue.getTexts(), renderQueue.getTextsNb());
    }

    // Enable depth buffer write and depth test for non-UI objects
    glDepthMask(GL_TRUE);
    glEnable(GL_DEPTH_TEST);

    // Disable blending for opaque objects
    glDisable(GL_BLEND);
}

// The transparency is dynamic objects transparency when behind static objects
void    Renderer::transparencyPass(Camera* camera, RenderQueue& renderQueue)
{
    if (!camera)
        return;

    glm::vec4 blackColor;
    glm::vec4 transparencyColor(1.0, 0.647, 0.0, 1.0);
    _transparencyShaderProgram.use();
    _currentShaderProgram = nullptr;

    camera->getUBO().bind();

    glViewport((uint32_t)camera->getViewport().offset.x,
                (uint32_t)camera->getViewport().offset.y,
                (uint32_t)camera->getViewport().extent.width,
                (uint32_t)camera->getViewport().extent.height);

    glUniform4fv(_transparencyShaderProgram.getUniformLocation("color"), 1, glm::value_ptr(blackColor));

    // First pass
    // We render all the static objects in the transparency frame buffer
    // We set the color to black because we only want to fill the depth buffer
    {
        // Opaque objects
        {
            auto& meshs = renderQueue.getOpaqueMeshs();
            uint32_t meshsNb = renderQueue.getOpaqueMeshsNb();
            for (uint32_t i = 0; i < meshsNb; ++i)
            {
                auto& renderableMesh = meshs[i];
                Mesh* mesh = renderableMesh.meshInstance->getMesh();

                // Render only static objects
                if (renderableMesh.dynamic || renderableMesh.hideDynamic)
                    continue;

                // Bind buffer
                mesh->getModel()->getBuffer().bind();

                renderableMesh.ubo->bind(renderableMesh.uboOffset, renderableMesh.uboSize);

                GLuint primitive = mesh->getModel()->getPrimitiveType();

                // Draw to screen
                if (renderableMesh.instancesNb > 0)
                {
                    glDrawElementsInstanced(primitive,
                                    (GLuint)mesh->indices.size(),
                                    GL_UNSIGNED_INT,
                                    BUFFER_OFFSET((GLuint)mesh->idxOffset * sizeof(GLuint)),
                                    renderableMesh.instancesNb);
                }
                else
                {
                    glDrawElements(primitive,
                                    (GLuint)mesh->indices.size(),
                                    GL_UNSIGNED_INT,
                                    BUFFER_OFFSET((GLuint)mesh->idxOffset * sizeof(GLuint)));
                }
            }
        }

        // Transparent objects
        {
            auto& meshs = renderQueue.getTransparentMeshs();
            uint32_t meshsNb = renderQueue.getTransparentMeshsNb();
            for (uint32_t i = 0; i < meshsNb; ++i)
            {
                auto& renderableMesh = meshs[i];
                Mesh* mesh = renderableMesh.meshInstance->getMesh();

                // Render only static objects
                if (renderableMesh.dynamic || renderableMesh.hideDynamic)
                    continue;

                // Bind buffer
                mesh->getModel()->getBuffer().bind();

                renderableMesh.ubo->bind(renderableMesh.uboOffset, renderableMesh.uboSize);

                GLuint primitive = mesh->getModel()->getPrimitiveType();

                // Draw to screen
                if (renderableMesh.instancesNb > 0)
                {
                    glDrawElementsInstanced(primitive,
                                    (GLuint)mesh->indices.size(),
                                    GL_UNSIGNED_INT,
                                    BUFFER_OFFSET((GLuint)mesh->idxOffset * sizeof(GLuint)),
                                    renderableMesh.instancesNb);
                }
                else
                {
                    glDrawElements(primitive,
                                    (GLuint)mesh->indices.size(),
                                    GL_UNSIGNED_INT,
                                    BUFFER_OFFSET((GLuint)mesh->idxOffset * sizeof(GLuint)));
                }
            }
        }

    }

    // Second pass
    // We render only dynamic objects that are behind the static objects into the transparency buffer
    {
        // Disable depth write and change depth function to render only objects behind actual depth
        glDepthFunc(GL_GREATER);
        glDepthMask(GL_FALSE);

        glUniform4fv(_transparencyShaderProgram.getUniformLocation("color"), 1, glm::value_ptr(transparencyColor));

        // Opaque objects
        {
            auto& meshs = renderQueue.getOpaqueMeshs();
            uint32_t meshsNb = renderQueue.getOpaqueMeshsNb();
            for (uint32_t i = 0; i < meshsNb; ++i)
            {
                auto& renderableMesh = meshs[i];
                Mesh* mesh = renderableMesh.meshInstance->getMesh();

                // Render only dynamic objects
                if (!renderableMesh.dynamic)
                    continue;

                // Bind buffer
                mesh->getModel()->getBuffer().bind();

                renderableMesh.ubo->bind(renderableMesh.uboOffset, renderableMesh.uboSize);

                GLuint primitive = mesh->getModel()->getPrimitiveType();

                // Draw to screen
                if (renderableMesh.instancesNb > 0)
                {
                    glDrawElementsInstanced(primitive,
                                    (GLuint)mesh->indices.size(),
                                    GL_UNSIGNED_INT,
                                    BUFFER_OFFSET((GLuint)mesh->idxOffset * sizeof(GLuint)),
                                    renderableMesh.instancesNb);
                }
                else
                {
                    glDrawElements(primitive,
                                    (GLuint)mesh->indices.size(),
                                    GL_UNSIGNED_INT,
                                    BUFFER_OFFSET((GLuint)mesh->idxOffset * sizeof(GLuint)));
                }
            }
        }

        // Transparent objects
        {
            auto& meshs = renderQueue.getTransparentMeshs();
            uint32_t meshsNb = renderQueue.getTransparentMeshsNb();
            for (uint32_t i = 0; i < meshsNb; ++i)
            {
                auto& renderableMesh = meshs[i];
                Mesh* mesh = renderableMesh.meshInstance->getMesh();

                // Render only dynamic objects
                if (!renderableMesh.dynamic)
                    continue;

                // Bind buffer
                mesh->getModel()->getBuffer().bind();

                renderableMesh.ubo->bind(renderableMesh.uboOffset, renderableMesh.uboSize);

                GLuint primitive = mesh->getModel()->getPrimitiveType();

                // Draw to screen
                if (renderableMesh.instancesNb > 0)
                {
                    glDrawElementsInstanced(primitive,
                                    (GLuint)mesh->indices.size(),
                                    GL_UNSIGNED_INT,
                                    BUFFER_OFFSET((GLuint)mesh->idxOffset * sizeof(GLuint)),
                                    renderableMesh.instancesNb);
                }
                else
                {
                    glDrawElements(primitive,
                                    (GLuint)mesh->indices.size(),
                                    GL_UNSIGNED_INT,
                                    BUFFER_OFFSET((GLuint)mesh->idxOffset * sizeof(GLuint)));
                }
            }
        }

        // Reset depth buffer function/write
        glDepthFunc(GL_LEQUAL);
        glDepthMask(GL_TRUE);
    }

}

void    Renderer::bloomPass(Texture* sceneColorAttachment,
                            const std::vector<std::array<Framebuffer, 2>>& blurFrameBuffers)
{
    for (uint32_t i = 0; i < blurFrameBuffers.size(); ++i)
    {
        auto& blurFramebuffer = blurFrameBuffers[i];
        auto& horizontalColorAttachment = blurFramebuffer[0].getColorAttachments()[0];
        auto& verticalColorAttachment = blurFramebuffer[1].getColorAttachments()[0];

        glViewport(0,
                0,
                (uint32_t)horizontalColorAttachment->getWidth(),
                (uint32_t)horizontalColorAttachment->getHeight());


        _finalBlendingShaderProgram.use();
        glUniform1i(_finalBlendingShaderProgram.getUniformLocation("image"), 0);

        // Downsample bright image into texture
        {
            auto& frameBuffer = blurFramebuffer[1];
            frameBuffer.use(GL_FRAMEBUFFER);

            sceneColorAttachment->bind();
            _screenPlane.bind();

            glDrawElements(GL_TRIANGLES,
                        6,
                        GL_UNSIGNED_INT,
                        0);
        }

        _blurShaderProgram.use();
        glUniform1i(_blurShaderProgram.getUniformLocation("image"), 0);
        glUniform2f(_blurShaderProgram.getUniformLocation("targetSize"),
            (float)horizontalColorAttachment->getWidth(),
            (float)horizontalColorAttachment->getHeight());

        for (uint32_t j = 0; j < 1; ++j)
        {
            // Horizontal pass
            {
                auto& frameBuffer = blurFramebuffer[0];

                frameBuffer.use(GL_FRAMEBUFFER);
                glUniform1i(_blurShaderProgram.getUniformLocation("horizontal"), true);


                verticalColorAttachment->bind();
                //sceneBrightColorAttachment->bind();
                _screenPlane.bind();

                glDrawElements(GL_TRIANGLES,
                            6,
                            GL_UNSIGNED_INT,
                            0);
            }


            // Vertical pass
            {
                auto& frameBuffer = blurFramebuffer[1];

                frameBuffer.use(GL_FRAMEBUFFER);
                glUniform1i(_blurShaderProgram.getUniformLocation("horizontal"), false);


                horizontalColorAttachment->bind();
                _screenPlane.bind();

                glDrawElements(GL_TRIANGLES,
                            6,
                            GL_UNSIGNED_INT,
                            0);
            }
        }
        sceneColorAttachment = blurFramebuffer[1].getColorAttachments()[0].get();
    }

    // Unbind framebuffer
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void    Renderer::finalBlendingPass()
{
    GLsizei windowBufferWidth = (GLsizei)GameWindow::getInstance()->getBufferWidth();
    GLsizei windowBufferHeight = (GLsizei)GameWindow::getInstance()->getBufferHeight();

    glViewport(0,
                0,
                (uint32_t)windowBufferWidth,
                (uint32_t)windowBufferHeight);

    _finalBlendingShaderProgram.use();
    glUniform1i(_finalBlendingShaderProgram.getUniformLocation("image"), 0);

    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE);
    for (uint32_t i = 0; i < _blurFramebuffers.size(); ++i)
    {
        _blurFramebuffers[i][1].getColorAttachments()[0]->bind();
        _screenPlane.bind();

        glDrawElements(GL_TRIANGLES,
                    6,
                    GL_UNSIGNED_INT,
                    0);
    }
    _frameBuffer.getColorAttachments()[0]->bind();

    glDrawElements(GL_TRIANGLES,
                6,
                GL_UNSIGNED_INT,
                0);

    _transparencyFrameBuffer.getColorAttachments()[0]->bind();
    glDrawElements(GL_TRIANGLES,
                6,
                GL_UNSIGNED_INT,
                0);


    glDisable(GL_BLEND);
}

bool sortOpaque(const sRenderableMesh& lhs, const sRenderableMesh& rhs)
{
    Material* lhsMaterial = lhs.meshInstance->getMaterial();
    Material* rhsMaterial = rhs.meshInstance->getMaterial();

    // Sort by layer and shaderProgram
    return (lhs.layer > rhs.layer ||
            lhs.layer > rhs.layer && lhsMaterial->getOptions() > rhsMaterial->getOptions());
}

void    Renderer::renderOpaqueObjects(std::vector<sRenderableMesh>& meshs,
                                    uint32_t meshsNb,
                                    std::vector<Light*>& lights,
                                    uint32_t lightsNb)
{
    if (meshsNb == 0)
        return;

    std::sort(meshs.begin(), meshs.begin() + meshsNb, sortOpaque);

    for (uint32_t j = 0; j < lightsNb; ++j)
    {
        lights[j]->bind();
        for (uint32_t i = 0; i < meshsNb; ++i)
        {
            auto& renderableMesh = meshs[i];
            Mesh* mesh = renderableMesh.meshInstance->getMesh();
            Material* material = renderableMesh.meshInstance->getMaterial();

            // Bind new shader
            if (!_currentShaderProgram || _currentShaderProgram->getOptions() != material->getOptions())
            {
                _currentShaderProgram = &_shaderPrograms.at(material->getOptions());
                _currentShaderProgram->use();

                // Set texture location unit
                // Must be the same unit as material textures. See Material::loadFromAssimp
                glUniform1i(_currentShaderProgram->getUniformLocation("AmbientTexture"), 0);
                glUniform1i(_currentShaderProgram->getUniformLocation("DiffuseTexture"), 1);
                glUniform1i(_currentShaderProgram->getUniformLocation("BloomTexture"), 2);
                glUniform1i(_currentShaderProgram->getUniformLocation("BloomTextureAlpha"), 3);
            }

            // Bind buffer
            mesh->getModel()->getBuffer().bind();

            material->bind();
            renderableMesh.ubo->bind(renderableMesh.uboOffset, renderableMesh.uboSize);

            GLuint primitive = mesh->getModel()->getPrimitiveType();
            if (material->wireframe)
                primitive = GL_LINE_STRIP;

            // Draw to screen
            if (renderableMesh.instancesNb > 0)
            {
                glDrawElementsInstanced(primitive,
                                (GLuint)mesh->indices.size(),
                                GL_UNSIGNED_INT,
                                BUFFER_OFFSET((GLuint)mesh->idxOffset * sizeof(GLuint)),
                                renderableMesh.instancesNb);
            }
            else
            {
                glDrawElements(primitive,
                                (GLuint)mesh->indices.size(),
                                GL_UNSIGNED_INT,
                                BUFFER_OFFSET((GLuint)mesh->idxOffset * sizeof(GLuint)));
            }
        }
    }
}

bool sortTransparent(const sRenderableMesh& lhs, const sRenderableMesh& rhs)
{
    Material* lhsMaterial = lhs.meshInstance->getMaterial();
    Material* rhsMaterial = rhs.meshInstance->getMaterial();

    // Sort by layer, shaderProgram, src blend and dst blend
    return (lhs.layer > rhs.layer ||
        lhs.layer > rhs.layer && lhsMaterial->getOptions() > rhsMaterial->getOptions() ||
        lhs.layer > rhs.layer && lhsMaterial->getOptions() == rhsMaterial->getOptions() && lhsMaterial->srcBlend < rhsMaterial->srcBlend ||
        lhs.layer > rhs.layer && lhsMaterial->getOptions() > rhsMaterial->getOptions() && lhsMaterial->srcBlend == rhsMaterial->srcBlend && lhsMaterial->dstBlend < rhsMaterial->dstBlend);
}

void    Renderer::renderTransparentObjects(std::vector<sRenderableMesh>& meshs,
                                            uint32_t meshsNb,
                                            std::vector<Light*>& lights,
                                            uint32_t lightsNb)
{
    if (meshsNb == 0)
        return;

    std::sort(meshs.begin(), meshs.begin() + meshsNb, sortTransparent);

    GLenum lastSrcBlend = meshs[0].meshInstance->getMaterial()->srcBlend;
    GLenum lastDstBlend = meshs[0].meshInstance->getMaterial()->dstBlend;
    glBlendFunc(lastSrcBlend, lastDstBlend);

    for (uint32_t j = 0; j < lightsNb; ++j)
    {
        lights[j]->bind();
        for (uint32_t i = 0; i < meshsNb; ++i)
        {
            auto& renderableMesh = meshs[i];
            Mesh* mesh = renderableMesh.meshInstance->getMesh();
            Material* material = renderableMesh.meshInstance->getMaterial();

            // Bind new shader
            if (!_currentShaderProgram || _currentShaderProgram->getOptions() != material->getOptions())
            {
                _currentShaderProgram = &_shaderPrograms.at(material->getOptions());
                _currentShaderProgram->use();

                // Set texture location unit
                // Must be the same unit as material textures. See Material::loadFromAssimp
                glUniform1i(_currentShaderProgram->getUniformLocation("AmbientTexture"), 0);
                glUniform1i(_currentShaderProgram->getUniformLocation("DiffuseTexture"), 1);
                glUniform1i(_currentShaderProgram->getUniformLocation("BloomTexture"), 2);
                glUniform1i(_currentShaderProgram->getUniformLocation("BloomTextureAlpha"), 3);
            }

            // Change blend mode
            if (lastSrcBlend != material->srcBlend ||
                lastDstBlend != material->dstBlend)
            {
                glBlendFunc(material->srcBlend, material->dstBlend);
                lastSrcBlend = material->srcBlend;
                lastDstBlend = material->dstBlend;
            }

            // Bind buffer
            mesh->getModel()->getBuffer().bind();

            material->bind();
            renderableMesh.ubo->bind(renderableMesh.uboOffset, renderableMesh.uboSize);

            GLuint primitive = mesh->getModel()->getPrimitiveType();
            if (material->wireframe)
                primitive = GL_LINE_STRIP;

            // Draw to screen
            if (renderableMesh.instancesNb > 0)
            {
                glDrawElementsInstanced(primitive,
                                (GLuint)mesh->indices.size(),
                                GL_UNSIGNED_INT,
                                BUFFER_OFFSET((GLuint)mesh->idxOffset * sizeof(GLuint)),
                                renderableMesh.instancesNb);
            }
            else
            {
                glDrawElements(primitive,
                                (GLuint)mesh->indices.size(),
                                GL_UNSIGNED_INT,
                                BUFFER_OFFSET((GLuint)mesh->idxOffset * sizeof(GLuint)));
            }
        }
    }
}

void    Renderer::renderTexts(std::vector<sRenderableText>& texts,
                                uint32_t textsNb)
{
    if (textsNb == 0)
        return;

    _UILight.bind();

    for (uint32_t i = 0; i < textsNb; ++i)
    {
        auto& renderText = texts[i];
        auto& text = renderText.text;

        const Font* font = text.getFont();
        float fontScale = (float)text.getFontSize() / 200.0f;
        ASSERT(font != nullptr, "Text should have font")
        glm::vec2 pos = renderText.pos;
        pos.y -= text.getFontSize();
        _textUBO.update((void*)&text.getColor(), sizeof(glm::vec4), sizeof(glm::mat4));

        for (uint32_t j = 0; j < text.getContent().size(); ++j)
        {
            char c = text.getContent()[j];
            if (c == '\n')
            {
                pos.x = renderText.pos.x;
                pos.y -= text.getFontSize();
                continue;
            }

            auto char_ = font->getChar(c);
            if (!char_)
                continue;

            glm::mat4 translate = glm::translate(glm::mat4(1.0f), {
                pos.x + (char_->bearing.x * fontScale),
                pos.y + (char_->bearing.y) * fontScale,
                0.0f
            });
            glm::mat4 scale = glm::scale(glm::mat4(1.0f), {
                char_->texture.getWidth() * fontScale,
                char_->texture.getHeight() * fontScale,
                1.0f
            });

            //glm::mat4 transform = glm::mat4(1.0f);
            glm::mat4 transform = translate * scale;
            _textUBO.update(&transform, sizeof(glm::mat4), 0);
            _textUBO.bind();
            _textPlane.bind();
            char_->texture.bind();

            glDrawElements(GL_TRIANGLES,
                            6,
                            GL_UNSIGNED_INT,
                            0);
            pos.x += (char_->advance >> 6) * fontScale;
        }

    }
}

bool    Renderer::setupFrameBuffers(Framebuffer& framebuffer,
                                    std::vector<std::array<Framebuffer, 2>>& blurFrameBuffers,
                                    uint32_t width,
                                    uint32_t height)
{
    // Check if the framebuffer has the same size as the given width and height
    // We don't setup again the framebuffer (and blur framebuffers) if the size is the same
    if (framebuffer.getColorAttachments().size() != 0)
    {
        auto& sceneColorAttachment = framebuffer.getColorAttachments()[0];
        if (sceneColorAttachment->getWidth() == width &&
            sceneColorAttachment->getHeight() == height)
        {
            return (true);
        }
    }
    // Setup scene framebuffer
    {
        framebuffer.bind(GL_FRAMEBUFFER);
        framebuffer.removeColorAttachments();
        // Scene color attachment
        framebuffer.addColorAttachment(Texture::create(width,
                                                height,
                                                GL_RGBA16F,
                                                GL_RGBA,
                                                GL_FLOAT));
        // Bright scene color attachment (used for glow)
        framebuffer.addColorAttachment(Texture::create(width,
                                                height,
                                                GL_RGBA16F,
                                                GL_RGBA,
                                                GL_FLOAT));
        framebuffer.setDepthAttachment(GL_DEPTH_COMPONENT,
                                        width,
                                        height);

        if (!framebuffer.isComplete())
        {
            framebuffer.unBind(GL_FRAMEBUFFER);
            return (false);
        }
        framebuffer.unBind(GL_FRAMEBUFFER);
    }

    // Setup blur framebuffers
    {
        blurFrameBuffers.resize(3);

        for (auto& blurFramebuffer: blurFrameBuffers)
        {
            for (auto& framebuffer: blurFramebuffer)
            {
                framebuffer.removeColorAttachments();
            }
        }

        // Full texture blur
        blurFrameBuffers[0][0].addColorAttachment(Texture::create(static_cast<GLsizei>(width),
                                                    static_cast<GLsizei>(height),
                                                    GL_RGBA16F,
                                                    GL_RGBA,
                                                    GL_FLOAT));
        blurFrameBuffers[0][1].addColorAttachment(Texture::create(static_cast<GLsizei>(width),
                                                    static_cast<GLsizei>(height),
                                                    GL_RGBA16F,
                                                    GL_RGBA,
                                                    GL_FLOAT));
        // 1/2 Texture blur
        blurFrameBuffers[1][0].addColorAttachment(Texture::create(static_cast<GLsizei>(width / 2.0f),
                                                    static_cast<GLsizei>(height / 2.0f),
                                                    GL_RGBA16F,
                                                    GL_RGBA,
                                                    GL_FLOAT));
        blurFrameBuffers[1][1].addColorAttachment(Texture::create(static_cast<GLsizei>(width / 2.0f),
                                                    static_cast<GLsizei>(height / 2.0f),
                                                    GL_RGBA16F,
                                                    GL_RGBA,
                                                    GL_FLOAT));

        // 1/4 Texture blur
        blurFrameBuffers[2][0].addColorAttachment(Texture::create(static_cast<GLsizei>(width / 4.0f),
                                                    static_cast<GLsizei>(height / 4.0f),
                                                    GL_RGBA16F,
                                                    GL_RGBA,
                                                    GL_FLOAT));
        blurFrameBuffers[2][1].addColorAttachment(Texture::create(static_cast<GLsizei>(width / 4.0f),
                                                    static_cast<GLsizei>(height / 4.0f),
                                                    GL_RGBA16F,
                                                    GL_RGBA,
                                                    GL_FLOAT));

        for (auto& blurFramebuffer: blurFrameBuffers)
        {
            for (auto& framebuffer: blurFramebuffer)
            {
                framebuffer.bind(GL_FRAMEBUFFER);
                if (!framebuffer.isComplete())
                {
                    return (false);
                }
            }
        }
    }
    return (true);
}

bool    Renderer::setupMainFrameBuffers()
{
    GLsizei windowBufferWidth = (GLsizei)GameWindow::getInstance()->getBufferWidth();
    GLsizei windowBufferHeight = (GLsizei)GameWindow::getInstance()->getBufferHeight();

    if (!windowBufferWidth ||
        !windowBufferHeight ||
        !setupFrameBuffers(_frameBuffer, _blurFramebuffers, windowBufferWidth, windowBufferHeight))
    {
        return (false);
    }

    // Setup transparency frame buffer
    {
        _transparencyFrameBuffer.bind(GL_FRAMEBUFFER);
        _transparencyFrameBuffer.removeColorAttachments();
        // Scene color attachment
        _transparencyFrameBuffer.addColorAttachment(Texture::create(windowBufferWidth,
                                                windowBufferHeight,
                                                GL_RGBA16F,
                                                GL_RGBA,
                                                GL_FLOAT));
        _transparencyFrameBuffer.setDepthAttachment(GL_DEPTH_COMPONENT,
                                        windowBufferWidth,
                                        windowBufferHeight);

        if (!_transparencyFrameBuffer.isComplete())
        {
            return (false);
        }
        _transparencyFrameBuffer.unBind(GL_FRAMEBUFFER);
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    return (true);
}

void    Renderer::setupShaderPrograms()
{
    // Init scene render programs
    // We create shader permutations for each material option
    // It will activate/deactivate preprocessor conditions in the shaders
    {
        std::vector<std::vector<Material::eOption> > permutations = {
            {  },
            { Material::eOption::TEXTURE_AMBIENT },
            { Material::eOption::TEXTURE_DIFFUSE },
            { Material::eOption::FACE_CAMERA },
            { Material::eOption::BLOOM },
            { Material::eOption::TEXTURE_BLOOM },
            { Material::eOption::TEXTURE_BLOOM_ALPHA },
            { Material::eOption::TEXTURE_AMBIENT, Material::eOption::TEXTURE_DIFFUSE },
            { Material::eOption::TEXTURE_AMBIENT, Material::eOption::FACE_CAMERA },
            { Material::eOption::TEXTURE_AMBIENT, Material::eOption::BLOOM },
            { Material::eOption::TEXTURE_AMBIENT, Material::eOption::TEXTURE_BLOOM },
            { Material::eOption::TEXTURE_AMBIENT, Material::eOption::TEXTURE_BLOOM_ALPHA },
            { Material::eOption::TEXTURE_DIFFUSE, Material::eOption::FACE_CAMERA },
            { Material::eOption::TEXTURE_DIFFUSE, Material::eOption::BLOOM },
            { Material::eOption::TEXTURE_DIFFUSE, Material::eOption::TEXTURE_BLOOM },
            { Material::eOption::TEXTURE_DIFFUSE, Material::eOption::TEXTURE_BLOOM_ALPHA },
            { Material::eOption::FACE_CAMERA, Material::eOption::BLOOM },
            { Material::eOption::FACE_CAMERA, Material::eOption::TEXTURE_BLOOM },
            { Material::eOption::FACE_CAMERA, Material::eOption::TEXTURE_BLOOM_ALPHA },
            { Material::eOption::BLOOM, Material::eOption::TEXTURE_BLOOM },
            { Material::eOption::BLOOM, Material::eOption::TEXTURE_BLOOM_ALPHA },
            { Material::eOption::TEXTURE_BLOOM, Material::eOption::TEXTURE_BLOOM_ALPHA },
            { Material::eOption::TEXTURE_AMBIENT, Material::eOption::TEXTURE_DIFFUSE, Material::eOption::FACE_CAMERA },
            { Material::eOption::TEXTURE_AMBIENT, Material::eOption::TEXTURE_DIFFUSE, Material::eOption::BLOOM },
            { Material::eOption::TEXTURE_AMBIENT, Material::eOption::TEXTURE_DIFFUSE, Material::eOption::TEXTURE_BLOOM },
            { Material::eOption::TEXTURE_AMBIENT, Material::eOption::TEXTURE_DIFFUSE, Material::eOption::TEXTURE_BLOOM_ALPHA },
            { Material::eOption::TEXTURE_AMBIENT, Material::eOption::FACE_CAMERA, Material::eOption::BLOOM },
            { Material::eOption::TEXTURE_AMBIENT, Material::eOption::FACE_CAMERA, Material::eOption::TEXTURE_BLOOM },
            { Material::eOption::TEXTURE_AMBIENT, Material::eOption::FACE_CAMERA, Material::eOption::TEXTURE_BLOOM_ALPHA },
            { Material::eOption::TEXTURE_AMBIENT, Material::eOption::BLOOM, Material::eOption::TEXTURE_BLOOM },
            { Material::eOption::TEXTURE_AMBIENT, Material::eOption::BLOOM, Material::eOption::TEXTURE_BLOOM_ALPHA },
            { Material::eOption::TEXTURE_AMBIENT, Material::eOption::TEXTURE_BLOOM, Material::eOption::TEXTURE_BLOOM_ALPHA },
            { Material::eOption::TEXTURE_DIFFUSE, Material::eOption::FACE_CAMERA, Material::eOption::BLOOM },
            { Material::eOption::TEXTURE_DIFFUSE, Material::eOption::FACE_CAMERA, Material::eOption::TEXTURE_BLOOM },
            { Material::eOption::TEXTURE_DIFFUSE, Material::eOption::FACE_CAMERA, Material::eOption::TEXTURE_BLOOM_ALPHA },
            { Material::eOption::TEXTURE_DIFFUSE, Material::eOption::BLOOM, Material::eOption::TEXTURE_BLOOM },
            { Material::eOption::TEXTURE_DIFFUSE, Material::eOption::BLOOM, Material::eOption::TEXTURE_BLOOM_ALPHA },
            { Material::eOption::TEXTURE_DIFFUSE, Material::eOption::TEXTURE_BLOOM, Material::eOption::TEXTURE_BLOOM_ALPHA },
            { Material::eOption::FACE_CAMERA, Material::eOption::BLOOM, Material::eOption::TEXTURE_BLOOM },
            { Material::eOption::FACE_CAMERA, Material::eOption::BLOOM, Material::eOption::TEXTURE_BLOOM_ALPHA },
            { Material::eOption::FACE_CAMERA, Material::eOption::TEXTURE_BLOOM, Material::eOption::TEXTURE_BLOOM_ALPHA },
            { Material::eOption::BLOOM, Material::eOption::TEXTURE_BLOOM, Material::eOption::TEXTURE_BLOOM_ALPHA },
            { Material::eOption::TEXTURE_AMBIENT, Material::eOption::TEXTURE_DIFFUSE, Material::eOption::FACE_CAMERA, Material::eOption::BLOOM },
            { Material::eOption::TEXTURE_AMBIENT, Material::eOption::TEXTURE_DIFFUSE, Material::eOption::FACE_CAMERA, Material::eOption::TEXTURE_BLOOM },
            { Material::eOption::TEXTURE_AMBIENT, Material::eOption::TEXTURE_DIFFUSE, Material::eOption::FACE_CAMERA, Material::eOption::TEXTURE_BLOOM_ALPHA },
            { Material::eOption::TEXTURE_AMBIENT, Material::eOption::TEXTURE_DIFFUSE, Material::eOption::BLOOM, Material::eOption::TEXTURE_BLOOM },
            { Material::eOption::TEXTURE_AMBIENT, Material::eOption::TEXTURE_DIFFUSE, Material::eOption::BLOOM, Material::eOption::TEXTURE_BLOOM_ALPHA },
            { Material::eOption::TEXTURE_AMBIENT, Material::eOption::TEXTURE_DIFFUSE, Material::eOption::TEXTURE_BLOOM, Material::eOption::TEXTURE_BLOOM_ALPHA },
            { Material::eOption::TEXTURE_AMBIENT, Material::eOption::FACE_CAMERA, Material::eOption::BLOOM, Material::eOption::TEXTURE_BLOOM },
            { Material::eOption::TEXTURE_AMBIENT, Material::eOption::FACE_CAMERA, Material::eOption::BLOOM, Material::eOption::TEXTURE_BLOOM_ALPHA },
            { Material::eOption::TEXTURE_AMBIENT, Material::eOption::FACE_CAMERA, Material::eOption::TEXTURE_BLOOM, Material::eOption::TEXTURE_BLOOM_ALPHA },
            { Material::eOption::TEXTURE_AMBIENT, Material::eOption::BLOOM, Material::eOption::TEXTURE_BLOOM, Material::eOption::TEXTURE_BLOOM_ALPHA },
            { Material::eOption::TEXTURE_DIFFUSE, Material::eOption::FACE_CAMERA, Material::eOption::BLOOM, Material::eOption::TEXTURE_BLOOM },
            { Material::eOption::TEXTURE_DIFFUSE, Material::eOption::FACE_CAMERA, Material::eOption::BLOOM, Material::eOption::TEXTURE_BLOOM_ALPHA },
            { Material::eOption::TEXTURE_DIFFUSE, Material::eOption::FACE_CAMERA, Material::eOption::TEXTURE_BLOOM, Material::eOption::TEXTURE_BLOOM_ALPHA },
            { Material::eOption::TEXTURE_DIFFUSE, Material::eOption::BLOOM, Material::eOption::TEXTURE_BLOOM, Material::eOption::TEXTURE_BLOOM_ALPHA },
            { Material::eOption::FACE_CAMERA, Material::eOption::BLOOM, Material::eOption::TEXTURE_BLOOM, Material::eOption::TEXTURE_BLOOM_ALPHA },
            { Material::eOption::TEXTURE_AMBIENT, Material::eOption::TEXTURE_DIFFUSE, Material::eOption::FACE_CAMERA, Material::eOption::BLOOM, Material::eOption::TEXTURE_BLOOM },
            { Material::eOption::TEXTURE_AMBIENT, Material::eOption::TEXTURE_DIFFUSE, Material::eOption::FACE_CAMERA, Material::eOption::BLOOM, Material::eOption::TEXTURE_BLOOM_ALPHA },
            { Material::eOption::TEXTURE_AMBIENT, Material::eOption::TEXTURE_DIFFUSE, Material::eOption::FACE_CAMERA, Material::eOption::TEXTURE_BLOOM, Material::eOption::TEXTURE_BLOOM_ALPHA },
            { Material::eOption::TEXTURE_AMBIENT, Material::eOption::TEXTURE_DIFFUSE, Material::eOption::BLOOM, Material::eOption::TEXTURE_BLOOM, Material::eOption::TEXTURE_BLOOM_ALPHA },
            { Material::eOption::TEXTURE_AMBIENT, Material::eOption::FACE_CAMERA, Material::eOption::BLOOM, Material::eOption::TEXTURE_BLOOM, Material::eOption::TEXTURE_BLOOM_ALPHA },
            { Material::eOption::TEXTURE_DIFFUSE, Material::eOption::FACE_CAMERA, Material::eOption::BLOOM, Material::eOption::TEXTURE_BLOOM, Material::eOption::TEXTURE_BLOOM_ALPHA },
            { Material::eOption::TEXTURE_AMBIENT, Material::eOption::TEXTURE_DIFFUSE, Material::eOption::FACE_CAMERA, Material::eOption::BLOOM, Material::eOption::TEXTURE_BLOOM, Material::eOption::TEXTURE_BLOOM_ALPHA },
        };


        for (const auto& options: permutations)
        {
            int optionFlag = 0;
            for (auto option: options)
            {
                optionFlag |= option;
            }

            auto& shaderProgram = _shaderPrograms[optionFlag];
            shaderProgram.setOptions(optionFlag);
            shaderProgram.attachShader(GL_VERTEX_SHADER, "resources/shaders/shader.vert", options);
            shaderProgram.attachShader(GL_FRAGMENT_SHADER, "resources/shaders/shader.frag", options);
            shaderProgram.link();
        }
    }

    // Init shader program of blur
    {
        _blurShaderProgram.attachShader(GL_VERTEX_SHADER, "resources/shaders/shader-single-plane.vert");
        _blurShaderProgram.attachShader(GL_FRAGMENT_SHADER, "resources/shaders/shader-blur.frag");
        _blurShaderProgram.link();
    }

    // Init shader program of final blending
    {
        _finalBlendingShaderProgram.attachShader(GL_VERTEX_SHADER, "resources/shaders/shader-single-plane.vert");
        _finalBlendingShaderProgram.attachShader(GL_FRAGMENT_SHADER, "resources/shaders/shader-simple.frag");
        _finalBlendingShaderProgram.link();
    }

    // Init shader program of final blending
    {
        _hdrShaderProgram.attachShader(GL_VERTEX_SHADER, "resources/shaders/shader-single-plane.vert");
        _hdrShaderProgram.attachShader(GL_FRAGMENT_SHADER, "resources/shaders/shader-hdr.frag");
        _hdrShaderProgram.link();
    }

    // Init shader program of transparency (When dynamic objects are behind static objects)
    {
        _transparencyShaderProgram.attachShader(GL_VERTEX_SHADER, "resources/shaders/shader.vert");
        _transparencyShaderProgram.attachShader(GL_FRAGMENT_SHADER, "resources/shaders/shader-transparency.frag");
        _transparencyShaderProgram.link();
    }

    // Init buffer containing the plane vertices used for final blending
    {
       Vertex vertexs[] {
            //1. pos
            //2. color
            //3. normal
            //4. texture uv
            {glm::vec3(-1.0f, 1.0f, 0.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f), glm::vec2(0.0f, 1.0f)},  // Top Left
            {glm::vec3(1.0f, 1.0f, 0.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f), glm::vec2(1.0f, 1.0f)},  // Top Right
            {glm::vec3(-1.0f, -1.0f, 0.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f), glm::vec2(0.0f, 0.0f)},  // Bottom Left
            {glm::vec3(1.0f, -1.0f, 0.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f), glm::vec2(1.0f, 0.0f)}  // Bottom Right
        };

        GLuint indices[] = {
            0, 2, 3,
            3, 1, 0
        };

        _screenPlane.updateData(vertexs, 4, indices, 6);
    }
}

void    Renderer::setup2DTextureGeneration()
{
    // Setup camera
    // We rotate the camera to have an isometric view
    _2DRenderCamera.rotate(-35.0f, glm::vec3(1.0f, 0.0f, 0.0f));
    _2DRenderCamera.rotate(47.0f, glm::vec3(0.0f, 1.0f, 0.0f));
    _2DRenderCamera.setNear(0.1f);
    _2DRenderCamera.setFar(5000.0f);
    _2DRenderCamera.setProjType(Camera::eProj::ORTHOGRAPHIC_3D);

    // Setup uniform buffer to pass model transformation and color
    _2DRenderBuffer.init(sizeof(glm::mat4) + sizeof(glm::vec4), GL_SHADER_STORAGE_BUFFER);
    _2DRenderBuffer.setBindingPoint(3);


    // Setup light
    _2DRenderLight.setAmbient({1.0f, 1.0f, 1.0f});
    _2DRenderLight.setDiffuse({1.0f, 1.0f, 1.0f});
}

void    Renderer::initTextRendering()
{
    // Set alignment to 1 because fonts textures pixels only used 1 byte
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

    _textShaderProgram.attachShader(GL_VERTEX_SHADER, "resources/shaders/shader.vert", {});
    _textShaderProgram.attachShader(GL_FRAGMENT_SHADER, "resources/shaders/shader-text.frag", {});
    _textShaderProgram.link();

    // Init text plane buffer
    {
       Vertex vertexs[] {
            //1. pos
            //2. color
            //3. normal
            //4. texture uv
            {glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f), glm::vec2(0.0f, 0.0f)},  // Top Left
            {glm::vec3(1.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f), glm::vec2(1.0f, 0.0f)},  // Top Right
            {glm::vec3(0.0f, -1.0f, 0.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f), glm::vec2(0.0f, 1.0f)},  // Bottom Left
            {glm::vec3(1.0f, -1.0f, 0.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f), glm::vec2(1.0f, 1.0f)}  // Bottom Right
        };

        GLuint indices[] = {
            0, 2, 3,
            3, 1, 0
        };

        _textPlane.updateData(vertexs, 4, indices, 6);
    }

    _textUBO.init((uint32_t)(sizeof(glm::mat4) + sizeof(glm::vec4)),
                    GL_SHADER_STORAGE_BUFFER);
    _textUBO.setBindingPoint(3);
}
