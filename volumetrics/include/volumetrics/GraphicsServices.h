#pragma once

#include <glm/glm.hpp>
#include <string>

// Host-owned renderer services. Their implementations stay in the host, so the
// DLL neither links RendererLib nor compiles its ShaderProgram/FullscreenQuad.
// All components must use the same compiler, C++ runtime, GLM and build settings.
class VolumetricShader
{
public:
    virtual ~VolumetricShader() = default;
    virtual bool IsValid() const = 0;
    virtual void Bind() const = 0;
    virtual void SetUniform(const std::string&, int) const = 0;
    virtual void SetUniform(const std::string&, bool) const = 0;
    virtual void SetUniform(const std::string&, float) const = 0;
    virtual void SetUniform(const std::string&, const glm::vec3&) const = 0;
    virtual void SetUniform(const std::string&, const glm::mat4&) const = 0;
};

class VolumetricGraphicsServices
{
public:
    virtual ~VolumetricGraphicsServices() = default;
    virtual VolumetricShader* CreateShader(const char* vertex, const char* fragment) = 0;
    virtual void DrawFullscreen() const = 0;
};
