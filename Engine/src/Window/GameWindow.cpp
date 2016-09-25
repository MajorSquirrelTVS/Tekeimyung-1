#include <iostream>

/**
	Please, include the extension loader library before GLFW3.
	See : http://www.glfw.org/docs/latest/quick.html#quick_include
*/
#include <GL/glew.h>

#include "Window/GameWindow.hpp"

GameWindow::GameWindow(int width, int height, const char *title) :
	_screenWidth(width), _screenHeight(height),
	_bufferWidth(0), _bufferHeight(0),
	_title(title), _window(nullptr) {}

GameWindow::~GameWindow()
{
	if (_window != nullptr)
	{
		//delete (this->_window);
		// deleting the pointer causes an error ????
	}
}

int     GameWindow::initialize()
{
	const GLFWvidmode *vidmode;
	int xPos, yPos;

	// Initializing GLFW.
	if (glfwInit() == GLFW_FALSE)
	{
		std::cerr << "Could not initialize GLFW." << std::endl;
		return (1);
	}

	// Creating a GLFW window.
	_window = glfwCreateWindow(_screenWidth, _screenHeight, _title.c_str(), nullptr, nullptr);
	if (_window == nullptr)
	{
		std::cerr << "Could not initialize the window properly." << std::endl;
		glfwTerminate();
		return (1);
	}
	glfwMakeContextCurrent(_window);
	glfwGetFramebufferSize(_window, &_bufferWidth, &_bufferHeight);
	glfwSwapInterval(1);

	// Placing the game window at the center of the screen.
	vidmode = glfwGetVideoMode(glfwGetPrimaryMonitor());
	xPos = (vidmode->width - WINDOW_DEFAULT_WIDTH) / 2;
	yPos = (vidmode->height - WINDOW_DEFAULT_HEIGHT) / 2;
	glfwSetWindowPos(_window, xPos, yPos);

	return (0);
}

int GameWindow::width() const
{
	return (_screenWidth);
}

int GameWindow::height() const
{
	return (_screenHeight);
}

std::string GameWindow::name() const
{
	return (_title);
}

void GameWindow::setDecorated(bool decorated)
{
	glfwWindowHint(GLFW_DECORATED, decorated ? GLFW_TRUE : GLFW_FALSE);
}

void GameWindow::setMaximized(bool maximized)
{
	glfwWindowHint(GLFW_MAXIMIZED, maximized ? GLFW_TRUE : GLFW_FALSE);
}

void GameWindow::setResizable(bool resizable)
{
	glfwWindowHint(GLFW_RESIZABLE, resizable ? GLFW_TRUE : GLFW_FALSE);
}

bool GameWindow::isRunning() const
{
	return (glfwWindowShouldClose(_window) == GLFW_FALSE);
}

void GameWindow::display()
{
	glViewport(0, 0, _bufferWidth, _bufferHeight);
	glClear(GL_COLOR_BUFFER_BIT);
	glfwSwapBuffers(_window);
}

void GameWindow::pollEvents()
{
	glfwPollEvents();
}

void GameWindow::close() {
	glfwDestroyWindow(_window);
	glfwTerminate();
}