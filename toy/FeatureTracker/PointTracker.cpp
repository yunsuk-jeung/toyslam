#include <opencv2/opencv.hpp>
#include "config.h"
#include "ToyLogger.h"
#include "Camera.h"
#include "ImagePyramid.h"
#include "Frame.h"
#include "Feature.h"
#include "PointTracker.h"

namespace toy {
PointTracker::PointTracker(std::string type)
  : mType{type}
  , prevFrame{nullptr} {
  if (mType == "Fast.OpticalflowLK") {
    mFeature2D = cv::FastFeatureDetector::create();
    std::cout << std::endl;
  }
}

PointTracker::~PointTracker() {}

void PointTracker::process(db::Frame* frame) {

  int trackedPtSize = track(frame);

  if (trackedPtSize > 20) return;
  auto newKptSize = extract(frame);

  if (trackedPtSize + newKptSize == 0) return;
  if (frame->getImagePyramid(1)->type() != 1) return;

  db::Feature* feature0 = frame->getFeature(0);
  db::Feature* feature1 = frame->getFeature(1);
  trackStereo(frame);
}

size_t PointTracker::extract(db::Frame* frame) {
  cv::Mat&     origin  = frame->getImagePyramid(0)->getOrigin();
  db::Feature* feature = frame->getFeature(0);
  Camera*      cam     = frame->getCamera(0);

  std::vector<cv::Mat>                    subImages;
  std::vector<std::vector<cv::KeyPoint> > keyPointsPerSubImage;
  std::vector<cv::Point2i>                offset;

  devideImage(origin, subImages, offset);
  auto subSize = subImages.size();
  keyPointsPerSubImage.resize(subSize);

  int subId = 0;
  for (auto& sub : subImages) {
    auto& kpts = keyPointsPerSubImage[subId++];
    mFeature2D->detect(sub, kpts);
  }

  std::vector<cv::KeyPoint> keyPoints;
  keyPoints.reserve(subSize);

  for (size_t i = 0; i < subSize; ++i) {
    auto& kpts = keyPointsPerSubImage[i];
    if (kpts.empty()) { continue; }
    std::nth_element(kpts.begin(),
                     kpts.begin(),
                     kpts.end(),
                     [](cv::KeyPoint& kpt1, cv::KeyPoint& kpt2) {
                       return kpt1.response > kpt2.response;
                     });
    auto& kpt = kpts.front();
    kpt.pt.x  = kpt.pt.x + offset[i].x;
    kpt.pt.y  = kpt.pt.y + offset[i].y;
    keyPoints.push_back(kpts.front());
  }

  convertCVKeyPointsToFeature(cam, keyPoints, feature);

  cv::Mat image = origin.clone();
  cv::cvtColor(image, image, CV_GRAY2BGR);
  for (const auto& kpt : keyPoints) { cv::circle(image, kpt.pt, 3, {255, 0, 0}, -1); }
  cv::imshow("keyPoint", image);

  return keyPoints.size();
}

void PointTracker::devideImage(cv::Mat&                  src,
                               cv::Mat&                  mask,
                               std::vector<cv::Mat>&     subs,
                               std::vector<cv::Point2i>& offsets) {

  const int rowGridCount   = Config::Vio::rowGridCount;
  const int colGridCount   = Config::Vio::colGridCount;
  const int totalGridCount = rowGridCount * colGridCount;
  subs.reserve(totalGridCount);
  offsets.reserve(totalGridCount);

  const int startCol = (src.cols % colGridCount) >> 1;
  const int startRow = (src.rows % rowGridCount) >> 1;

  const int gridCols = src.cols / colGridCount;
  const int gridRows = src.rows / rowGridCount;

  for (int r = 0; r < rowGridCount; ++r) {
    for (int c = 0; c < colGridCount; ++c) {
      auto offset = cv::Point2i({startCol + gridCols * c}, {startRow + gridRows * r});

      cv::Rect roi(offset.x, offset.y, gridCols, gridRows);
      cv::Mat  crop = src(roi);
      subs.push_back(crop);
      offsets.push_back(offset);
    }
  }
}

size_t PointTracker::track(db::Frame* frame) {
  return size_t();
}

size_t PointTracker::trackStereo(db::Frame* frame) {

  auto& pyramid0    = frame->getImagePyramid(0)->getPyramids();
  auto& keyPoints0  = frame->getFeature(0)->getKeypoints();
  auto& ids0        = keyPoints0.mIds;
  auto& levels0     = keyPoints0.mLevels;
  auto& uvs0        = keyPoints0.mUvs;
  auto& trackCount0 = keyPoints0.mTrackCounts;
  auto& undists0    = keyPoints0.mUndists;

  auto& pyramid1    = frame->getImagePyramid(1)->getPyramids();
  auto& keyPoints1  = frame->getFeature(1)->getKeypoints();
  auto& ids1        = keyPoints1.mIds;
  auto& levels1     = keyPoints1.mLevels;
  auto& uvs1        = keyPoints1.mUvs;
  auto& trackCount1 = keyPoints1.mTrackCounts;
  auto& undists1    = keyPoints1.mUndists;

  const auto patch = cv::Size2i(Config::Vio::patchSize, Config::Vio::patchSize);

  std::vector<uchar> status;

  cv::calcOpticalFlowPyrLK(pyramid0,
                           pyramid1,
                           uvs0,
                           uvs1,
                           status,
                           cv::noArray(),
                           patch,
                           Config::Vio::pyramidLevel);

  auto* cam1 = frame->getCamera(1);
  cam1->undistortPoints(uvs1, undists1);

  cv::Mat image0 = pyramid0[0].clone();
  cv::cvtColor(image0, image0, cv::COLOR_GRAY2BGR);

  cv::Mat image1 = pyramid1[0].clone();
  cv::cvtColor(image1, image1, cv::COLOR_GRAY2BGR);

  for (int i = 0; i < uvs0.size(); i++) {
    if (status[i] == 0) {
      cv::line(image1, uvs0[i], uvs1[i], {0.0, 0.0, 255.0}, 1);
      cv::circle(image1, uvs1[i], 4, {0.0, 0.0, 255.0}, -1);
    }
    else {
      cv::line(image1, uvs0[i], uvs1[i], {0.0, 255.0, 0.0}, 1);
      cv::circle(image1, uvs1[i], 4, {0.0, 255.0, 0.0}, -1);
    }
  }

  Sophus::SE3d    Sbc0    = Sophus::SE3d::exp(frame->getLbc(0));
  Sophus::SE3d    Sbc1    = Sophus::SE3d::exp(frame->getLbc(1));
  Sophus::SE3d    Sc1c0   = Sbc1.inverse() * Sbc0;
  Eigen::Matrix3d Rc1c0   = Sc1c0.so3().matrix();
  Eigen::Matrix3d Tc1c0_x = Sophus::SO3d::hat(Sc1c0.translation());
  Eigen::Matrix3d E       = Rc1c0 * Tc1c0_x;

  constexpr double epipolarThreashold = 0.0015625;  //1.0 / 640;

  auto trackedSize = uvs0.size();
  for (int i = 0; i < trackedSize; i++) {
    if (status[i] == 0) continue;
    const auto& undist0 = undists0[i];
    const auto& undist1 = undists1[i];

    Eigen::Vector3d nuv0 = Eigen::Vector3d(undist0.x, undist0.y, 1.0);
    Eigen::Vector3d nuv1 = Eigen::Vector3d(undist0.x, undist0.y, 1.0);

    double err = nuv1.transpose() * E * nuv0;
    if (err > epipolarThreashold) status[i] = 0;
  }

  for (int i = 0; i < uvs0.size(); i++) {
    if (status[i] == 0) {
      cv::line(image1, uvs0[i], uvs1[i], {255.0, 0.0, 0.0}, 1);
      cv::circle(image1, uvs1[i], 2, {255.0, 0.0, 0.0}, -1);
    }
  }

  cv::imshow("remove outlier 0", image1);
  cv::waitKey(1);
  return size_t();
}

void PointTracker::convertCVKeyPointsToFeature(Camera*                    cam,
                                               std::vector<cv::KeyPoint>& kpts,
                                               db::Feature*               feature) {

  db::Feature newFeat;
  auto&       newKpts = feature->getKeypoints();
  newKpts.reserve(kpts.size());

  auto& ids        = newKpts.mIds;
  auto& levels     = newKpts.mLevels;
  auto& points     = newKpts.mUvs;
  auto& trackCount = newKpts.mTrackCounts;
  auto& undists    = newKpts.mUndists;

  for (const auto& kpt : kpts) {
    ids.push_back(mFeatureId++);
    levels.push_back(kpt.octave);
    points.push_back(kpt.pt);
    trackCount.push_back(0);
  }
  //asdfasdf
  cam->undistortPoints(points, undists);

  auto& keypoints = feature->getKeypoints();
  auto  size      = newKpts.size() + keypoints.size();

  keypoints.reserve(size);
  keypoints.push_back(newKpts);
}

cv::Mat PointTracker::createMask() {
  return cv::Mat();
}

}  //namespace toy