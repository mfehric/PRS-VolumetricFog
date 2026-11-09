#include "VolumetricDemoScene.h"
#include "assets/AssetImporter.h"
#include "core/Material.h"
#include "core/Mesh.h"
#include "core/MeshData.h"
#include "core/ShaderProgram.h"
#include "scene/LightBuilder.h"
#include <array>

namespace
{
    MeshData Cube()
    {
        MeshData mesh;
        mesh.name = "VolumetricDemoCube";
        // A single reusable cube with explicit flat normals and UV tangents.
        const std::array<glm::vec3, 6> normals = {{{1,0,0},{-1,0,0},{0,1,0},{0,-1,0},{0,0,1},{0,0,-1}}};
        for (const glm::vec3 n : normals)
        {
            const glm::vec3 u = std::abs(n.y) > 0.5f ? glm::vec3(1,0,0) : glm::normalize(glm::cross(glm::vec3(0,1,0), n));
            const glm::vec3 v = glm::cross(n, u);
            const uint32_t base = static_cast<uint32_t>(mesh.vertices.size());
            mesh.vertices.push_back({(n-u-v)*0.5f, n, {0,0}, glm::vec4(u,1)});
            mesh.vertices.push_back({(n+u-v)*0.5f, n, {1,0}, glm::vec4(u,1)});
            mesh.vertices.push_back({(n+u+v)*0.5f, n, {1,1}, glm::vec4(u,1)});
            mesh.vertices.push_back({(n-u+v)*0.5f, n, {0,1}, glm::vec4(u,1)});
            mesh.indices.insert(mesh.indices.end(), {base,base+1,base+2,base,base+2,base+3});
        }
        SubMesh sub;
        sub.name = "Cube"; sub.indexCount = static_cast<uint32_t>(mesh.indices.size()); sub.hasTangents = true;
        mesh.submeshes.push_back(sub);
        return mesh;
    }
}

void VolumetricDemoScene::Box(glm::vec3 center, glm::vec3 scale, glm::vec3 color)
{
    auto material = std::make_unique<MaterialInstance>(m_material);
    material->SetVec3("u_AlbedoColor", color);
    material->SetFloat("u_Metallic", 0.0f); material->SetFloat("u_Roughness", 0.85f);
    RenderItem item;
    item.meshMulti = m_cube.get(); item.material = material.get();
    item.transform.SetTranslation(center); item.transform.SetScale(scale);
    item.flags.castShadow = true; item.flags.receiveShadow = true;
    AddObject(item); m_materials.push_back(std::move(material));
}

bool VolumetricDemoScene::Setup()
{
    SetSceneName("Volumetric: Window shafts");
    SetClearColor({0.018f, 0.023f, 0.035f, 1.0f});
    SetSkyboxVisible(false);
    auto shader = AssetImporter::LoadShader("assets/shaders/mesh.vert", "assets/shaders/mesh.frag");
    if (!shader || !shader->IsValid()) return false;
    m_cube = std::make_shared<Mesh>(Cube());
    m_material = std::make_shared<Material>(shader);
    m_material->SetVec4("u_TintColor", {1,1,1,1});
    Camera camera;
    camera.SetPosition({5.4f, 2.8f, 3.7f});
    camera.SetTarget({-1.5f, 2.0f, -4.0f});
    camera.SetFOV(52.0f); camera.SetClipPlanes(0.1f, 60.0f);
    SetCamera(camera);
    GetLights().Clear();
    SetAmbientLight({0.18f,0.22f,0.30f}, 0.15f);
    GetLights().SetDirectionalLight(DirectionalLightBuilder()
        .Direction({0.45f,-0.48f,1.0f}).Color({1.0f,0.86f,0.64f}).Intensity(10.0f)
        .CastShadow(true).ShadowResolution(1024,1024).ShadowBias(0.0005f,0.012f)
        .PCFRadius(1).Name("Window sun").Build());

    const glm::vec3 plaster{0.30f,0.34f,0.39f};
    Box({0,-0.2f,0}, {18,0.4f,18}, {0.22f,0.25f,0.29f});
    // Rear wall at z=-5: three narrow vertical openings between the four piers.
    Box({0,0.65f,-5}, {14,1.3f,0.5f}, plaster);
    Box({0,6.0f,-5}, {14,2.0f,0.5f}, plaster);
    Box({-5.6f,3.15f,-5}, {2.8f,3.7f,0.5f}, plaster);
    Box({5.6f,3.15f,-5}, {2.8f,3.7f,0.5f}, plaster);
    Box({-1.65f,3.15f,-5}, {1.3f,3.7f,0.5f}, plaster);
    Box({1.65f,3.15f,-5}, {1.3f,3.7f,0.5f}, plaster);
    // One side wall and a roof keep most of the room dark; the front is open.
    Box({-7,3.5f,0}, {0.4f,7,10}, plaster);
    Box({0,7.1f,0}, {14,0.3f,10}, plaster);
    // An obstacle intercepts one shaft and casts a visible shadow through the fog.
    Box({1.4f,1.2f,-0.5f}, {1.3f,2.4f,1.3f}, {0.45f,0.19f,0.08f});
    Box({-2.0f,0.5f,1.5f}, {1.4f,1.0f,1.4f}, {0.12f,0.24f,0.32f});
    return true;
}

void VolumetricDemoScene::OnUpdate(float deltaTime, IInputProvider& input)
{
    glm::vec3 movement;
    UpdateStandardCameraAndPlayer(deltaTime, input, m_player, movement);
}
