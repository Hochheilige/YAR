#pragma once

#include "object.h"
#include "scene_component.h"

#include <vector>
#include <type_traits>

class Actor : public Object
{
public:
    Actor();
    explicit Actor(const std::string& name);
    ~Actor() override;

    const char* get_class_name() const override { return "Actor"; }

    virtual void begin_play();
    virtual void tick(float delta_time);

    // Component management
    template <typename T, typename... Args>
    T* add_component(Args&&... args);

    template <typename T>
    T* get_component() const;

    const std::vector<Component*>& get_components() const { return components; }

    // Transform convenience (delegates to root component)
    SceneComponent* get_root_component() const { return root_component; }

    Transform get_transform() const { return root_component->get_relative_transform(); }
    void set_transform(const Transform& transform) { root_component->set_relative_transform(transform); }

    Vector3 get_position() const { return root_component->get_relative_position(); }
    void set_position(const Vector3& position) { root_component->set_relative_position(position); }

    Quaternion get_rotation() const { return root_component->get_relative_rotation(); }
    void set_rotation(const Quaternion& rotation) { root_component->set_relative_rotation(rotation); }

    Vector3 get_scale() const { return root_component->get_relative_scale(); }
    void set_scale(const Vector3& scale) { root_component->set_relative_scale(scale); }

protected:
    SceneComponent* root_component = nullptr;
    std::vector<Component*> components;
};

// ======================================= //
//        Template Implementations         //
// ======================================= //

template <typename T, typename... Args>
T* Actor::add_component(Args&&... args)
{
    static_assert(std::is_base_of_v<Component, T>, "T must derive from Component");

    T* component = new T(std::forward<Args>(args)...);
    component->set_owner(this);
    components.push_back(component);

    // If it's a SceneComponent, attach to root by default
    if constexpr (std::is_base_of_v<SceneComponent, T>)
    {
        if (component != root_component)
            component->attach_to(root_component);
    }

    return component;
}

template <typename T>
T* Actor::get_component() const
{
    static_assert(std::is_base_of_v<Component, T>, "T must derive from Component");

    for (Component* comp : components)
    {
        T* casted = dynamic_cast<T*>(comp);
        if (casted)
            return casted;
    }
    return nullptr;
}
