#include <iostream>
#include <algorithm>

#include "render_base/exception.hpp"
#include "render_base/window.hpp"
#include "render_base/context3d.hpp"

using namespace Render3D;

std::atomic<GLuint> Window::clusterCount(0);

Window::Window(int w, int h, const char* title, Window* parent) {
    SDL_Init(SDL_INIT_VIDEO);

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
    SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 1);
    SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, 4);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24); // default is 16

    window = SDL_CreateWindow(title, SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, w, h, SDL_WINDOW_RESIZABLE | SDL_WINDOW_OPENGL);
    if (!window) {
        SDL_Quit();

        throw Exception(std::string("Exception occurred with initializing window: ") + SDL_GetError());
    }
    if (parent != nullptr && !parent->destroyed && parent->context != nullptr) {
        SDL_GL_MakeCurrent(parent->window, parent->glContext);
        SDL_GL_SetAttribute(SDL_GL_SHARE_WITH_CURRENT_CONTEXT, 1);
        parentWindow = parent;
        clusterID = parent->clusterID;
    } else {
        SDL_GL_SetAttribute(SDL_GL_SHARE_WITH_CURRENT_CONTEXT, 0);
        parentWindow = nullptr;
        clusterID = ++clusterCount;
    }
    glContext = SDL_GL_CreateContext(window);
    if (glContext == nullptr) {
        SDL_GL_DeleteContext(glContext);
        SDL_DestroyWindow(window);
        SDL_Quit();

        throw Exception(std::string("Exception occurred with initializing opengl context: ") + SDL_GetError());
    }

    SDL_GL_MakeCurrent(window, glContext);

    GLenum glewError = glewInit();
    if (glewError != GLEW_OK) {
        SDL_DestroyWindow(window);
        SDL_Quit();
        // throw exception
        throw Exception(std::string("Exception occurred with initializing glew: ") + reinterpret_cast<const char*>(glewGetErrorString(glewError)));
    }

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_MULTISAMPLE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);

    width = w;
    height = h;
    active = true;
    destroyed = false;
    vsyncEnabled = false;
    fullscreenEnabled = false;
    mouseLockEnabled = false;
    mouseDownCallback = mouseUpCallback = nullptr;
    mouseMoveCallback = nullptr;
    windowResizeCallback = nullptr;
    mouseX = mouseY = 0;

    SDL_AddEventWatch(eventWatcher, this);

    std::cout << "Window created and binds all set" << std::endl;

    context = new Context3D(this);
}

Window::~Window() {
    close();
}

void Window::makeCurrent(bool isCurrent) {
    if (isCurrent) {
        if (SDL_GL_GetCurrentContext() != glContext) {
            if (SDL_GL_MakeCurrent(window, glContext) != 0) {
                throw Exception(std::string("Exception occurred with making window current: ") + SDL_GetError());
            }
            if (SDL_GL_SetSwapInterval(vsyncEnabled) != 0) {
                throw Exception(std::string("Error setting window swap interval: ") + SDL_GetError());
            }
        }
    } else {
        if (SDL_GL_MakeCurrent(window, nullptr) != 0) {
            throw Exception(std::string("Exception occurred with making window not current: ") + SDL_GetError());
        }
    }
}

void Window::updateViewport() {
    glViewport(0, 0, width, height);
}

void Window::close() {
    if (!destroyed) {
        destroyed = true;
        context->clearObjects();
        context->clearShaders();
        context->clearTextures();
        delete context;
        SDL_GL_DeleteContext(glContext);
        SDL_DestroyWindow(window);
        context = nullptr;
    }
}

bool Window::isActive() const {
    return active;
}

void Window::pollEvents() {
    SDL_PumpEvents();
}

void Window::waitEvents() {
    SDL_Event event;
    SDL_WaitEvent(&event);
    SDL_PumpEvents();
}

void Window::clear() {
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

void Window::drawTriangle(GLfloat vertices[]) {
    
}

int Window::getWidth() const {
    return width;
}

int Window::getHeight() const {
    return height;
}

void Window::getSize(int& w, int& h) const {
    w = width;
    h = height;
}

void Window::setWidth(int w) {
    width = w;
    SDL_SetWindowSize(window, width, height);
}

void Window::setHeight(int h) {
    height = h;
    SDL_SetWindowSize(window, width, height);
}

void Window::setSize(int w, int h) {
    width = w;
    height = h;
    SDL_SetWindowSize(window, width, height);
}

void Window::applyResize(int w, int h) {
    width = w;
    height = h;
}

void Window::update() {
    SDL_GL_SwapWindow(window);
}

void Window::setVSyncEnabled(bool enabled) {
    vsyncEnabled = enabled;
    if (SDL_GL_SetSwapInterval(vsyncEnabled) != 0) {
        throw Exception(std::string("Error setting window swap interval: ") + SDL_GetError());
    }
}

void Window::setFullscreenEnabled(bool enabled) {
    if (enabled && !fullscreenEnabled) {
        windowedWidth = width;
        windowedHeight = height;

        SDL_DisplayMode mode;
        SDL_GetCurrentDisplayMode(SDL_GetWindowDisplayIndex(window), &mode);
        setSize(mode.w, mode.h);
    }
    SDL_SetWindowFullscreen(window, (enabled ? SDL_WINDOW_FULLSCREEN : 0));
    if (!enabled && fullscreenEnabled) {
        setSize(windowedWidth, windowedHeight);
    }
    fullscreenEnabled = enabled;
}

void Window::setMouseLockEnabled(bool enabled) {
    mouseLockEnabled = enabled;
    SDL_SetRelativeMouseMode((enabled ? SDL_TRUE : SDL_FALSE));
    if (!enabled) {
        SDL_WarpMouseInWindow(window, mouseX, mouseY);
    }
}

bool Window::isVSyncEnabled() const {
    return vsyncEnabled;
}

bool Window::isFullscreenEnabled() const {
    return fullscreenEnabled;
}

bool Window::isMouseLockEnabled() const {
    return mouseLockEnabled;
}

void Window::toggleFullscreen() {
    setFullscreenEnabled(!fullscreenEnabled);
}

int Window::eventWatcher(void* data, SDL_Event* event) {
    Window* window = static_cast<Window*>(data);
    if (window == nullptr) {
        return 0;
    }
    if (event->type == SDL_QUIT) {
        window->active = false;
    } else if (event->type == SDL_MOUSEBUTTONDOWN) {
        if (event->button.windowID == SDL_GetWindowID(window->window)) {
            for (unsigned int i = 0; i < window->mouseState.size(); ++i) {
                if (window->mouseState[i] == event->button.button) {
                    return 0;
                }
            }
            window->mouseState.push_back(event->button.button);
            if (!window->mouseLockEnabled) {
                window->mouseX = event->button.x;
                window->mouseY = event->button.y;
            }
            if (window->mouseDownCallback != nullptr) {
                window->mouseDownCallback(static_cast<MOUSE_BUTTON>(event->button.button), event->button.x, event->button.y);
            }
        }
    } else if (event->type == SDL_MOUSEBUTTONUP) {
        if (event->button.windowID == SDL_GetWindowID(window->window)) {
            for (unsigned int i = 0; i < window->mouseState.size(); ++i) {
                if (window->mouseState[i] == event->button.button) {
                    std::swap(window->mouseState[i], window->mouseState.back());
                    window->mouseState.pop_back();
                    break;
                }
            }
            if (!window->mouseLockEnabled) {
                window->mouseX = event->button.x;
                window->mouseY = event->button.y;
            }
            if (window->mouseUpCallback != nullptr) {
                window->mouseUpCallback(static_cast<MOUSE_BUTTON>(event->button.button), event->button.x, event->button.y);
            }
        }
    } else if (event->type == SDL_MOUSEMOTION) {
        if (event->button.windowID == SDL_GetWindowID(window->window)) {
            int tempX = window->mouseX;
            int tempY = window->mouseY;
            if (!window->mouseLockEnabled) {
                window->mouseX = event->motion.x;
                window->mouseY = event->motion.y;
            }
            if ((window->mouseMoveCallback != nullptr) &&
                (window->mouseLockEnabled || event->motion.x != tempX || event->motion.y != tempY)) {
                window->mouseMoveCallback(tempX, tempY, event->motion.xrel, event->motion.yrel);
            }
        }
    } else if (event->type == SDL_KEYDOWN) {
        if (event->key.windowID == SDL_GetWindowID(window->window)) {
            for (unsigned int i = 0; i < window->keyboardState.size(); ++i) {
                if (window->keyboardState[i] == event->key.keysym.scancode) {
                    return 0;
                }
            }
            window->keyboardState.push_back(event->key.keysym.scancode);
            if (window->keyDownCallback != nullptr) {
                window->keyDownCallback(static_cast<KEYCODE>(event->key.keysym.scancode));
            }
        }
    } else if (event->type == SDL_KEYUP) {
        if (event->key.windowID == SDL_GetWindowID(window->window)) {
            for (unsigned int i = 0; i < window->keyboardState.size(); ++i) {
                if (window->keyboardState[i] == event->key.keysym.scancode) {
                    std::swap(window->keyboardState[i], window->keyboardState.back());
                    window->keyboardState.pop_back();
                    break;
                }
            }
            if (window->keyUpCallback != nullptr) {
                window->keyUpCallback(static_cast<KEYCODE>(event->key.keysym.scancode));
            }
        }
    } else if (event->type == SDL_WINDOWEVENT) {
        if (event->window.windowID == SDL_GetWindowID(window->window)) {
            switch (event->window.event) {
                case SDL_WINDOWEVENT_SIZE_CHANGED:
                    if (window->windowResizeCallback != nullptr) {
                        window->windowResizeCallback(event->window.data1, event->window.data2);
                    } else {
                        window->applyResize(event->window.data1, event->window.data2);
                    }
                    break;
                case SDL_WINDOWEVENT_CLOSE:
                    window->active = false;
                default:
                    break;
            }
        }
    }
    
    return 0;
}

KEYCODE Window::getKeycodeFromName(const std::string& name) {
    return static_cast<KEYCODE>(SDL_GetScancodeFromName(name.c_str()));
}

std::string Window::getKeyNameFromCode(KEYCODE key) {
    return std::string(SDL_GetScancodeName(SDL_Scancode(static_cast<int>(key))));
}

bool Window::isKeyPressed(KEYCODE key) {
    for (unsigned int i = 0; i < keyboardState.size(); ++i) {
        if (keyboardState[i] == static_cast<int>(key)) {
            return true;
        }
    }

    return false;
}

bool Window::isMouseDown(MOUSE_BUTTON button) {
    for (unsigned int i = 0; i < mouseState.size(); ++i) {
        if (mouseState[i] == static_cast<int>(button)) {
            return true;
        }
    }

    return false;
}

void Window::getMousePosition(double& x, double& y) {
    x = mouseX;
    y = mouseY;
}

void Window::setMouseDownCallback(const MouseButtonCallback& callback) {
    mouseDownCallback = callback;
}

void Window::setMouseUpCallback(const MouseButtonCallback& callback) {
    mouseUpCallback = callback;
}

void Window::setKeyDownCallback(const KeyCallback& callback) {
    keyDownCallback = callback;
}

void Window::setKeyUpCallback(const KeyCallback& callback) {
    keyUpCallback = callback;
}

void Window::setMouseMoveCallback(const MouseMoveCallback& callback) {
    mouseMoveCallback = callback;
}

void Window::setResizeCallback(const WindowResizeCallback& callback) {
    windowResizeCallback = callback;
}

bool Window::isChild() const {
    return parentWindow != nullptr;
}

GLuint Window::getClusterID() const {
    return clusterID;
}

Window* Window::getParent() const {
    return parentWindow;
}

Context3D* Window::getContext() const {
    return context;
}

bool Window::isShaderActive(const Shader& shader) const {
    std::thread::id thisThread = std::this_thread::get_id();
    for (unsigned int i = 0; i < activeShaders.size(); ++i) {
        if (activeShaders[i].first == thisThread) {
            return activeShaders[i].second == &shader;
        }
    }

    return false;
}

void Window::setShaderActive(const Shader& shader, bool active) {
    std::thread::id thisThread = std::this_thread::get_id();
    for (unsigned int i = 0; i < activeShaders.size(); ++i) {
        if (activeShaders[i].first == thisThread) {
            activeShaders[i].second = active ? &shader : nullptr;
            return;
        }
    }

    if (active) {
        activeShaders.push_back(std::pair<std::thread::id, const Shader*>(thisThread, &shader));
    }
}
