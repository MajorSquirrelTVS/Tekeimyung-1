#pragma once

#include <memory>
#include <string>
#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include <Window/Keyboard.hpp>
#include <Window/Mouse.hpp>

#include <imgui.h>
#include "imgui_impl_glfw_gl3.h"

#define WINDOW_DEFAULT_WIDTH    1280
#define WINDOW_DEFAULT_HEIGHT   720
#define WINDOW_DEFAULT_TITLE    "Window default title - Powered by "

class GameWindow
{
public:
    explicit                            GameWindow(int width = WINDOW_DEFAULT_WIDTH, int height = WINDOW_DEFAULT_HEIGHT, const char *title = WINDOW_DEFAULT_TITLE);
    virtual                             ~GameWindow();
    
    bool                                initialize();
	void								registerEvents();
    
    int                                 getWidth() const;
    int	                                getHeight() const;
    std::string	                        getTitle() const;
    static std::shared_ptr<GameWindow>  getInstance();
	Keyboard&							getKeyboard();
    Mouse&                              getMouse();
    
    void                                setDecorated(bool decorated);
    void                                setMaximized(bool fullscreen);
    void                                setResizable(bool resizable);
    static void                         setInstance(std::shared_ptr<GameWindow> instance);

    bool                                isRunning() const;
    void                                display();
    void                                pollEvents();
    void                                close();

	static void							keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods);
    static void                         buttonCallback(GLFWwindow* window, int button, int action, int mods);
    static void                         cursorEnterCallback(GLFWwindow* window, int entered);
    static void                         cursorPositionCallback(GLFWwindow* window, double xPos, double yPos);
private:
    GLFWwindow*                         _window;

    int                                 _screenWidth;
    int                                 _screenHeight;
    int                                 _bufferWidth;
    int                                 _bufferHeight;
    std::string                         _title;

    static std::shared_ptr<GameWindow>  _instance;
	Keyboard							_keyboard;
    Mouse                               _mouse;
};
