#pragma once

#include <string>

class Object
{
public:
    Object() = default;
    explicit Object(const std::string& name) : name(name) {}
    virtual ~Object() = default;

    virtual const char* get_class_name() const { return "Object"; }

    void set_name(const std::string& new_name) { name = new_name; }
    const std::string& get_name() const { return name; }

protected:
    std::string name;
};
