#include "scene_component.h"

Transform SceneComponent::get_world_transform() const
{
    if (parent)
        return parent->get_world_transform() * relative_transform;

    return relative_transform;
}

void SceneComponent::attach_to(SceneComponent* new_parent)
{
    if (parent == new_parent)
        return;

    detach();

    parent = new_parent;
    if (parent)
        parent->children.push_back(this);
}

void SceneComponent::detach()
{
    if (!parent)
        return;

    auto& siblings = parent->children;
    siblings.erase(std::remove(siblings.begin(), siblings.end(), this), siblings.end());
    parent = nullptr;
}
