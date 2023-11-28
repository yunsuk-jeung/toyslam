#pragma once

#include <Volk/volk.h>
#include <unordered_map>
#include "core/Instance.h"

namespace vkl {
class App;
class WindowInfo {
public:

  enum class Mode {
    Headless,
    Fullscreen,
    FullscreenBorderless,
    FullscreenStretch,
    Default
  };

  enum class Vsync { OFF, ON, Default };

  class Extent {
  public:

    uint32_t width;
    uint32_t height;
  };

  WindowInfo(std::string title, Mode mode, bool resizable, Vsync vsync, Extent extent)
      : title(title), mode(mode), resizable(resizable), vsync(vsync), extent(extent) {}

  WindowInfo(const WindowInfo& info) {

    this->title     = info.title;
    this->mode      = info.mode;
    this->resizable = info.resizable;
    this->vsync     = info.vsync;
    this->extent    = info.extent;
  }

  std::string title     = "Default";
  Mode        mode      = Mode::Default;
  bool        resizable = true;
  Vsync       vsync     = Vsync::Default;
  Extent      extent    = {1280, 720};
};

class Window {
public:

  Window() = delete;
  //Window(WindowInfo& info);
  Window(WindowInfo& info, App* app);
  virtual ~Window() = default;

  // clang-format off
  virtual VkSurfaceKHR createSurface(Instance* instance) = 0;
  virtual VkSurfaceKHR createSurface(VkInstance       instance,
                                     VkPhysicalDevice physical_device)= 0;
  // clang-format on

  virtual void updateExtent(uint32_t w, uint32_t h);

  virtual std::vector<const char*> getRequiredSurfaceExtension() = 0;

  virtual void pollEvents();
  virtual void prepareGUI()  = 0;
  virtual void newGUIFrame() = 0;
  virtual void endGUIFrame() = 0;
  virtual void endGUI()      = 0;

protected:

  virtual void createWindow() = 0;

protected:

  WindowInfo windowInfo;
  App*       app = nullptr;

public:

  WindowInfo& getWindowInfo() { return windowInfo; }
  void        setApplication(App* _app) { app = _app; }
};
}  //namespace vkl