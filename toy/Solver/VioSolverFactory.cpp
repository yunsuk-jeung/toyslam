#include <iostream>
#include "Logger.h"
#include "config.h"
#include "SqrtLocalSolver.h"
#include "VioSolverFactory.h"

namespace toy {
VioSolver* VioSolverFactory::createVioSolver() {

  if (Config::Vio::solverType == "SqrtLocalSolver") {
    return new SqrtLocalSolver();
  }

  LOGE("Check your json for vio solver");
  throw std::runtime_error("no vio solver type");
}
}  //namespace toy