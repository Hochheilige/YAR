#pragma once

#include "component.h"
#include "../math/Transform.h"

#include <vector>
#include <algorithm>

class SceneComponent : public Component
{
public:
    SceneComponent() = default;
    explicit SceneComponent(const std::string& name) : Component(name) {}
    ~SceneComponent() override = default;

    const char* get_class_name() const override { return "SceneComponent"; }

    // Transform accessors
    const Transform& get_relative_transform() const { return relative_transform; }
    void set_relative_transform(const Transform& transform) { relative_transform = transform; }

    Transform get_world_transform() const;

    // Position convenience
    Vector3 get_relative_position() const { return relative_transform.position; }
    void set_relative_position(const Vector3& position) { relative_transform.position = position; }

    Vector3 get_world_position() const { return get_world_transform().position; }

    // Rotation convenience
    Quaternion get_relative_rotation() const { return relative_transform.rotation; }
    void set_relative_rotation(const Quaternion& rotation) { relative_transform.rotation = rotation; }

    // Scale convenience
    Vector3 get_relative_scale() const { return relative_transform.scale; }
    void set_relative_scale(const Vector3& scale) { relative_transform.scale = scale; }

    // Hierarchy
    void attach_to(SceneComponent* new_parent);
    void detach();

    SceneComponent* get_parent() const { return parent; }
    const std::vector<SceneComponent*>& get_children() const { return children; }

protected:
    Transform relative_transform;
    SceneComponent* parent = nullptr;
    std::vector<SceneComponent*> children;
};
