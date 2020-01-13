#include "packlo/backend/correlation/z-score-eval.h"
#include "packlo/backend/registration/sph-registration.h"
#include "packlo/common/data/datasource-ply.h"
#include "packlo/common/metric-utils.h"
#include "packlo/common/test/testing-entrypoint.h"
#include "packlo/common/test/testing-predicates.h"
#include "packlo/distribution/bingham-mixture.h"

#include <gtest/gtest.h>

namespace uncertainty {

class RotBinghamMixtureTest : public ::testing::Test {
 protected:
  virtual void SetUp() {
    ds_ = std::make_unique<data::DatasourcePly>();
    CHECK_NOTNULL(ds_);
    ds_->setDatasetFolder("./test_clouds/arche/");
    registrator_ = std::make_unique<registration::SphRegistration>(
        "phase", "bmm", "gaussian");
    z_score_eval_ = dynamic_cast<correlation::ZScoreEval*>(
        &registrator_->getRotEvaluation());
  }

  data::DatasourcePlyPtr ds_;
  registration::SphRegistrationPtr registrator_;
  correlation::ZScoreEval* z_score_eval_;
};

TEST_F(RotBinghamMixtureTest, LowUncertainty) {
  CHECK(ds_);
  z_score_eval_->getPeakExtraction().getScoreThreshold() = 5.45;

  model::RegistrationResult result;
  model::PointCloudPtr prev_cloud = nullptr;
  ds_->subscribeToPointClouds([&](const model::PointCloudPtr& cloud) {
    CHECK(cloud);
    if (prev_cloud == nullptr) {
      prev_cloud = cloud;
      prev_cloud->initialize_kd_tree();
      return;
    }

    // Register the point clouds.
    cloud->initialize_kd_tree();
    result = registrator_->registerPointCloud(prev_cloud, cloud);
    prev_cloud = cloud;
    EXPECT_TRUE(result.foundSolutionForRotation());
    EXPECT_TRUE(result.foundSolutionForTranslation());

    common::BinghamMixturePtr uncertainty =
        std::dynamic_pointer_cast<common::BinghamMixture>(
            result.getRotUncertaintyEstimate());
    CHECK_NOTNULL(uncertainty);
    const Eigen::MatrixXd& cov = uncertainty->getMixtureS2();
    VLOG(1) << "------------------------- Uncertainty bingham:\n" << cov;
    EXPECT_LE(cov.trace(), 5);
  });
  ds_->startStreaming(0);
}

}  // namespace uncertainty

MAPLAB_UNITTEST_ENTRYPOINT