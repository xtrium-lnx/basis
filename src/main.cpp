#define TS_VFS_IMPLEMENTATION
#include <ts/ts_ecs.h>
#include <ts/ts_vfs.h>

//#define TS_VFS_ZIP_IMPLEMENTATION
//#include <ts/ts_vfs_zip.h>

#include <basis/window.h>
#include <basis/device.h>
#include <basis/image.h>
#include <basis/mesh.h>
#include <basis/cache.h>
#include <basis/basicrenderer.h>

#include <chrono>
#include <memory>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/gtc/matrix_transform.hpp>

struct Transform
{
	glm::vec3 position    = glm::vec3(0.0f);
	glm::quat orientation = glm::identity<glm::quat>();
	glm::vec3 scale       = glm::vec3(1.0f);

	glm::mat4 ComputeMatrix() const
	{
		return glm::translate(glm::mat4(1.0f), position)
			 * glm::mat4_cast(orientation)
			 * glm::scale(glm::mat4(1.0f), scale);
	}
};

struct MeshComponent
{
	basis::Mesh* mesh;
};

struct CameraComponent
{
	glm::vec3 target      = glm::vec3(0.0f);
	glm::vec3 up          = glm::vec3(0.0f, 1.0f, 0.0f);

	float     fov         = 45.0f;
	glm::vec2 depthBounds = glm::vec2(0.1f, 100.0f);

	glm::mat4 ComputeViewMatrix(const Transform& transform) const
	{
		return glm::lookAt(transform.position, target, up);
	}

	glm::mat4 ComputeProjectionMatrix(float aspectRatio) const
	{
		return glm::perspective(glm::radians(fov), aspectRatio, depthBounds.x, depthBounds.y);
	}
};

int main(int argc, char** argv)
{
	auto window = std::make_unique<basis::Window>(basis::WindowDesc { { 1600, 900 }, "Basis example" });
	auto device = std::make_unique<basis::Device>(*window);
	auto vfs    = std::make_unique<ts::Vfs>();

	auto renderer = std::make_unique<basis::BasicRenderer>(*window, *device, *vfs);

	ts::Scene scene;
	basis::Cache<basis::Mesh> meshes([&](std::string_view path) {
		return vfs->Open(path)
			.and_then([](ts::VfsFile f) { return f.ReadText(); })
			.transform([&device](const std::string& text) { return basis::Mesh::FromObj(*device, text); })
			.value();
	});

	auto monkey = scene.Spawn(
		Transform {},
		MeshComponent { .mesh = meshes.GetOrLoad("/meshes/suzanne.obj") }
	);

	auto camera = scene.Spawn(
		Transform {},
		CameraComponent {}
	);

	auto tPrev = std::chrono::high_resolution_clock::now();
	float t = 0.0f;

	do
	{
		window->PollEvents();
		auto tNow = std::chrono::high_resolution_clock::now();
		auto dt = std::chrono::duration<float>(tNow - tPrev);
		tPrev = tNow;
		t += dt.count();

		auto [image, imageAvailable, renderFinished] = device->AcquireNextFrame();

		scene.RequireComponent<Transform>(camera).position = glm::vec3(6.0f * glm::cos(t), 2.0f, 6.0f * glm::sin(t));
		
		scene.Query<Transform, CameraComponent>().Single([&](ts::Entity, Transform& transform, CameraComponent& camera) {
			renderer->SetCamera(
				camera.ComputeViewMatrix(transform),
				camera.ComputeProjectionMatrix(float(window->Size().x) / float(window->Size().y))
			);
		});

		scene.Query<Transform, MeshComponent>().Each([&](ts::Entity, Transform& transform, MeshComponent& meshComponent) {
			renderer->PushGeometry(transform.ComputeMatrix(), meshComponent.mesh);
		});

		renderer->Render(image, { imageAvailable }, renderFinished);
		device->Present({ renderFinished });
	} while (!window->ShouldClose());

	device->WaitForIdle();

	scene.Clear();
	renderer.reset();
	vfs.reset();

	meshes.ReleaseAll();
	device->FlushDeferred();
	device.reset();
	window.reset();
	return 0;
}
