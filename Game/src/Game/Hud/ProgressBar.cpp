/*
** @Author : Simon AMBROISE
*/

#include <Engine/Core/Components/Components.hh>
#include <Engine/EntityFactory.hpp>

#include <Game/Hud/ProgressBar.hpp>

ProgressBar::~ProgressBar()
{
    auto em = EntityFactory::getBindedEntityManager();

    if (empty)
    {
        em->destroyEntityRegister(this->empty->handle);
    }
    if (filled)
    {
        em->destroyEntityRegister(this->filled->handle);
    }
}

void    ProgressBar::init(const std::string& entityProgressBarEmpty, const std::string& entityProgressBar)
{
    this->filled = EntityFactory::createEntity(entityProgressBar);
    this->filledTransform = filled->getComponent<sTransformComponent>();
    this->filledRender = filled->getComponent<sRenderComponent>();

    this->empty = EntityFactory::createEntity(entityProgressBarEmpty);
    this->emptyTransform = empty->getComponent<sTransformComponent>();
    this->emptyRender = empty->getComponent<sRenderComponent>();
}

void ProgressBar::update()
{
    this->filledTransform->setScale(glm::vec3((float)currentProgress / (float)maxProgress * emptyTransform->getScale().x,
                                                    this->filledTransform->getScale().y,
                                                    this->filledTransform->getScale().z));

    // We take arbitrarily the size of empty progress bar
    // (empty progress bar is assumed to have the same size)
    float barSize = emptyRender->getModel()->getMax().x - emptyRender->getModel()->getMin().x;
    float emptyBarSize = barSize * emptyTransform->getScale().x / 2.0f;
    filledTransform->setPos({emptyTransform->getPos().x,
                            emptyTransform->getPos().y,
                            filledTransform->getPos().z});
    filledTransform->translate(-glm::vec3(emptyBarSize - ((filledTransform->getScale().x * barSize) / 2.0f),
                                                    0.0f, 0.0f));
}

void    ProgressBar::display(bool display)
{
    this->filledRender->display = display;
    this->emptyRender->display = display;
}

bool    ProgressBar::isDisplayed()
{
    return (this->filledRender->display && this->emptyRender->display);
}