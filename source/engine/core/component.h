#pragma once

#include "object.h"

class Actor;

class Component : public Object
{
public:
    Component() = default;
    explicit Component(const std::string& name) : Object(name) {}
    ~Component() override = default;

    const char* get_class_name() const override { return "Component"; }

    virtual void begin_play() {}
    virtual void tick(float delta_time) {}

    Actor* get_owner() const { return owner; }
    void set_owner(Actor* new_owner) { owner = new_owner; }

    bool get_active() const { return is_active; }
    void set_active(bool active) { is_active = active; }

protected:
    Actor* owner = nullptr;
    bool is_active = true;
};
