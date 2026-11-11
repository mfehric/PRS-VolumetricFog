#pragma once
#include "scene/Scene.h"
#include <memory>
#include <vector>

class Mesh;
class Material;
class MaterialInstance;

class VolumetricDemoScene : public Scene
{
public:
    bool Setup();
    void OnUpdate(float deltaTime, IInputProvider& input) override;
private:
    std::shared_ptr<Mesh> m_cube;
    std::shared_ptr<Material> m_material;
    std::vector<std::unique_ptr<MaterialInstance>> m_materials;
    glm::vec3 m_player{0.0f};
    void Box(glm::vec3 center, glm::vec3 scale, glm::vec3 color);
};
