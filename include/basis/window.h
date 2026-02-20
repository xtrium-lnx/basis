#pragma once

#include <string>

#include <glm/glm.hpp>

struct GLFWwindow;

namespace basis
{
	struct WindowDesc
	{
		glm::uvec2  size       = { 1280, 720 };
		std::string title      = "Basis Window";
		bool        fullscreen = false;
	};

	class Window
	{
		GLFWwindow* m_window = nullptr;

	public:
		explicit Window(const WindowDesc& desc);
		~Window();

		GLFWwindow* NativeHandle() const;
		glm::uvec2   Size() const;

		bool ShouldClose() const;
		void PollEvents();
	};
}
