#include <iostream>
#include <functional>
#include <filesystem>

#include "DataReader.h"
#include "Simulator.h"
#include "SensorFactory.h"
#include "Slam.h"
#include "SlamApp.h"
#include "GlfwWindow.h"

vkl::SlamApp*   app              = nullptr;
io::Sensor*     sensor           = nullptr;
io::DataReader* dataReader       = nullptr;
std::string     dataPath         = "D:/dataset/EUROC/MH_01_easy";
std::string     configPath       = "D:/workspaceD/toy_vio/configs/";
std::string     sensorConfigFile = "euroc_sensor.json";
std::string     slamConfigFile   = "VioOnly.json";

void setupSensor() {
  sensor = io::SensorFactory::createSensor(io::SensorFactory::SensorType::SIMULATOR);

  if (sensor->isSimulator()) {
    dataReader = io::DataReader::createDataReader(io::DataReader::Type::EUROC);
    dataReader->openDirectory(sensorConfigFile, dataPath);
    ((io::Simulator*)sensor)->registerDataReader(dataReader);
  }
  sensor->prepare();
}

void registerCallbacks() {
  auto imageCallback = [](ImageData& imageData0, ImageData& imageData1) {
    toy::SLAM::getInstance()->setNewImage(imageData0, imageData1);
  };

  auto accCallback = [](const uint64_t& ns, float* acc) {
    toy::SLAM::getInstance()->setAcc(ns, acc);
  };

  auto gyrCallback = [](const uint64_t& ns, float* gyr) {
    toy::SLAM::getInstance()->setGyr(ns, gyr);
  };

  sensor->registerImageCallback(imageCallback);
  sensor->registerAccCallback(accCallback);
  sensor->registerGyrCallback(gyrCallback);
}

void prepareSensor() {
  setupSensor();
  registerCallbacks();
}

void prepareSLAM() {
  CameraInfo info0;
  CameraInfo info1;
  sensor->getInfo(&info0, &info1);

  toy::SLAM::getInstance()->setSensorInfo(&info0, &info1);
  toy::SLAM::getInstance()->prepare(slamConfigFile);
}

void prepareGUI() {
  vkl::WindowInfo winInfo{
    "Sample window",
    vkl::WindowInfo::Mode::Default,
    true,
    vkl::WindowInfo::Vsync::ON,
    vkl::WindowInfo::Orientation::Landscape,
    {1280, 1280}
  };

  app = new vkl::SlamApp();
  std::unique_ptr<vkl::Window> glfwWindow(new vkl::GlfwWindow(winInfo, app));
  app->registerWindow(glfwWindow);
  app->addShaderPath("F:/transfer/toy_slam/libvulkanlight/shaders");
  app->addResourcePath("F:/transfer/toy_slam/libvulkanlight/shaders");
  app->addShaderPath("D:/workspaceD/toy_vio/libvulkanlight/shaders");
  app->addResourcePath("D:/workspaceD/toy_vio/libvulkanlight/shaders");
  app->registerSensor(sensor);
  app->prepare();
}

int main() {
  if (!std::filesystem::exists(configPath + sensorConfigFile)) {
    configPath = "F:/transfer/toy_slam/configs/";
  }
  sensorConfigFile = configPath + sensorConfigFile;
  slamConfigFile   = configPath + slamConfigFile;

  prepareSensor();
  prepareSLAM();
  prepareGUI();

  //while (true) {
  //  ((io::Simulator*)sensor)->spinOnce();
  //  int key = cv::waitKey();
  //  if (key == 27)
  //    break;
  //}

  app->run();

  toy::SLAM::deleteInstance();

  delete dataReader;
  delete sensor;
  delete app;

  return 0;
}