#include <basis/window.h>

#define GLFW_INCLUDE_VULKAN
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>

#ifndef NDEBUG
# pragma comment(lib, "glfw3-s-d.lib")
#else /* NDEBUG */
# pragma comment(lib, "glfw3-s.lib")
#endif /* NDEBUG */

using namespace basis;

Window::Window(const WindowDesc& desc)
{
	glfwInit();

	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
	m_window = glfwCreateWindow(desc.size.x, desc.size.y, desc.title.c_str(), desc.fullscreen ? glfwGetPrimaryMonitor() : nullptr, nullptr);
}

Window::~Window()
{
	glfwDestroyWindow(m_window);
	glfwTerminate();
}

GLFWwindow* Window::NativeHandle() const
{
	return m_window;
}

glm::uvec2 Window::Size() const
{
	int w, h;
	glfwGetFramebufferSize(m_window, &w, &h);
	return { w, h };
}

bool Window::ShouldClose() const
{
	return glfwWindowShouldClose(m_window);
}

void Window::PollEvents()
{
	glfwPollEvents();
}
