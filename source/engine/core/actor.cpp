#include "actor.h"

Actor::Actor()
{
    root_component = new SceneComponent("DefaultSceneRoot");
    root_component->set_owner(this);
    components.push_back(root_component);
}

Actor::Actor(const std::string& name)
    : Object(name)
{
    root_component = new SceneComponent("DefaultSceneRoot");
    root_component->set_owner(this);
    components.push_back(root_component);
}

Actor::~Actor()
{
    for (Component* comp : components)
        delete comp;

    components.clear();
    root_component = nullptr;
}

void Actor::begin_play()
{
    for (Component* comp : components)
    {
        if (comp->get_active())
            comp->begin_play();
    }
}

void Actor::tick(float delta_time)
{
    for (Component* comp : components)
    {
        if (comp->get_active())
            comp->tick(delta_time);
    }
}
