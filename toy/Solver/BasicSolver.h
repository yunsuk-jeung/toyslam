#pragma once
#include <Eigen/Dense>

#include <opencv2/opencv.hpp>
#include "ImagePyramid.h"
#include "Camera.h"

#include <sophus/se3.hpp>
#include <tbb/parallel_reduce.h>
#include <tbb/blocked_range.h>
#include "ToyLogger.h"
#include "config.h"
#include "usings.h"
#include "Frame.h"
#include "MapPoint.h"
#include "CostFunction.h"

namespace toy {
class BasicSolver {
public:
  static bool triangulate(const Eigen::Vector3d undist0,
                          const Eigen::Vector3d undist1,
                          const Sophus::SE3d&   Sc1c0,
                          Eigen::Vector3d&      out) {
    Eigen::Matrix<double, 3, 4> P0 = Eigen::Matrix<double, 3, 4>::Identity();
    Eigen::Matrix<double, 3, 4> P1 = Sc1c0.matrix3x4();
    Eigen::Matrix4d             A;
    A.row(0) = undist0[0] * P0.row(2) - undist0[2] * P0.row(0);
    A.row(1) = undist0[1] * P0.row(2) - undist0[2] * P0.row(1);
    A.row(2) = undist1[0] * P1.row(2) - undist1[2] * P1.row(0);
    A.row(3) = undist1[1] * P1.row(2) - undist1[2] * P1.row(1);

    Eigen::Vector4d homoPoint;
    homoPoint = A.jacobiSvd(Eigen::ComputeFullV).matrixV().rightCols<1>();
    out       = homoPoint.head(3) / homoPoint.w();

    auto& min = Config::Solver::basicMinDepth;
    auto& max = Config::Solver::basicMaxDepth;

    if (out.z() < min || out.z() > max)
      return false;

    return true;
  }

  static bool solveFramePose(db::Frame::Ptr curr) {
    auto&            mapPointFactorMap = curr->getMapPointFactorMap();
    Eigen::Matrix66d H                 = Eigen::Matrix66d::Zero();
    Eigen::Vector6d  b                 = Eigen::Vector6d::Zero();

    MEstimator::Ptr huber = std::shared_ptr<Huber>(new Huber(1.0));
    //MEstimator::Ptr huber = nullptr;

    std::vector<PoseOnlyReprojectionCost> costs;
    costs.reserve(mapPointFactorMap.size());

    Sophus::SE3d Twc = curr->getTwc(0);

    for (auto& [mpWeak, factor] : mapPointFactorMap) {
      auto mp = mpWeak.lock();
      costs.emplace_back(&Twc, mp, factor.undist0().head(2), huber);
    }

    /*
    //single thread is faster for the small problem
    struct Reductor {
      Reductor(std::vector<PoseOnlyReprojectionCost>& costs)
        : mCosts{costs} {
        mH.setZero();
        mb.setZero();
      }
      void operator()(const tbb::blocked_range<size_t>& range) {
        for (size_t r = range.begin(); r != range.end(); ++r) {
          auto& cost = mCosts[r];
          cost.linearlize();
          cost.addToHessian(mH, mb);
        }
      }
      Reductor(Reductor& src, tbb::split)
        : mCosts(src.mCosts) {
        mH.setZero();
        mb.setZero();
      };
      inline void join(Reductor& b) {
        mH += b.mH;
        mb += b.mb;
      }

    std::vector<PoseOnlyReprojectionCost>& mCosts;
    Eigen::Matrix6d                        mH;
    Eigen::Vector6d                        mb;
    };

    Reductor                   reductor(costs);
    tbb::blocked_range<size_t> range(0, costs.size());
    tbb::parallel_reduce(range, reductor);
    */

    constexpr int    maxIter = 2;
    constexpr double lambda  = 1e-6;

    for (int iter = 0; iter < maxIter; iter++) {
      Eigen::Matrix66d JtJ = Eigen::Matrix66d::Zero();
      Eigen::Vector6d  JtC = Eigen::Vector6d::Zero();

      double err = 0.0;
      for (auto& cost : costs) {
        err += cost.linearlize();
        cost.addToHessian(JtJ, JtC);
      }

      Eigen::Vector6d D = JtJ.diagonal();
      D *= lambda;
      JtJ.diagonal().array() += D.array().max(lambda);
      Eigen::Vector6d delX = -JtJ.ldlt().solve(JtC);

      Twc.translation() += delX.head(3);
      Twc.so3() *= Sophus::SO3d::exp(delX.tail(3));
    }

    auto&        Tbc0 = curr->getTbc(0);
    Sophus::SE3d Twb  = Twc * Tbc0.inverse();

    curr->setTwb(Twb);

    return true;
  }
};
}  //namespace toy
