#include "ToyLogger.h"
#include "Feature.h"
#include "ImagePyramid.h"
#include "Frame.h"

namespace toy {
namespace db {

Frame::Frame(ImagePyramid* imagePyramid)
    : mId{-1}
    , mImagePyramids{&imagePyramid[0], &imagePyramid[1]}
    , mFeatures{
          new Feature(),
          new Feature(),
      } ,
       mLbcs{Eigen::Vector6d(), Eigen::Vector6d()} {}

Frame::~Frame() {
  delete[] mImagePyramids[0];
  mImagePyramids.fill(nullptr);

  for (auto* ptr : mCameras) { delete ptr; }
  mCameras.fill(nullptr);

  for (auto* ptr : mFeatures) { delete ptr; }
  mFeatures.fill(nullptr);
}

void Frame::setLbc(float* pfbc0, float* pfbc1) {
  Eigen::Matrix4f Mbc0(pfbc0);
  Eigen::Matrix4f Mbc1(pfbc1);

  Eigen::Matrix3d Rbc0 = Mbc0.block<3, 3>(0, 0).cast<double>();
  Eigen::Vector3d Tbc0 = Mbc0.block<3, 1>(0, 3).cast<double>();

  Eigen::Matrix3d Rbc1 = Mbc1.block<3, 3>(0, 0).cast<double>();
  Eigen::Vector3d Tbc1 = Mbc1.block<3, 1>(0, 3).cast<double>();

  Eigen::Quaterniond Qbc0(Rbc0);
  Eigen::Quaterniond Qbc1(Rbc1);

  Sophus::SO3d SObc0 = Sophus::SO3d(Qbc0);
  mLbcs[0].head(3)   = SObc0.log();
  mLbcs[0].tail(3)   = Tbc0;

  Sophus::SO3d SObc1 = Sophus::SO3d(Qbc1);
  mLbcs[1].head(3)   = SObc1.log();
  mLbcs[1].tail(3)   = Tbc1;
}

}  //namespace db
}  //namespace toy
