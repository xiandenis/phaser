#include "packlo/backend/registration/sph-registration.h"
#include "packlo/backend/alignment/phase-aligner.h"
#include "packlo/backend/alignment/range-based-aligner.h"
#include "packlo/backend/correlation/gaussian-peak-based-eval.h"
#include "packlo/backend/correlation/gmm-peak-based-eval.h"
#include "packlo/common/rotation-utils.h"
#include "packlo/common/statistic-utils.h"
#include "packlo/common/translation-utils.h"

#include <glog/logging.h>

DEFINE_int32(
    spherical_bandwith, 70,
    "Defines the bandwith used for the spherical registration.");
DEFINE_string(alignment_algorithm, "phase",
    "Sets the algorithm used for the translational alignment.");
DEFINE_string(
    evaluation_algorithm, "gaussian",
    "Defines the algorithm used for the evaluation of the correlations.");

namespace registration {

SphRegistration::SphRegistration()
    : BaseRegistration("SphRegistration"),
      sampler_(FLAGS_spherical_bandwith),
      alignment_algorithm_(FLAGS_alignment_algorithm),
      evaluation_algorithm_(FLAGS_evaluation_algorithm) {
  initializeAlgorithms();
}

void SphRegistration::initializeAlgorithms() {
  // Initialize the translational alignment.
  if (alignment_algorithm_ == "phase")
    aligner_ = std::make_unique<alignment::PhaseAligner>();
  else if (alignment_algorithm_ == "averaging")
    aligner_ = std::make_unique<alignment::RangeBasedAligner>();
  else
    LOG(FATAL) << "Unknown alignment algorithm specificed.";

  if (evaluation_algorithm_ == "gaussian")
    eval_ = std::make_unique<correlation::GaussianPeakBasedEval>();
  else if (evaluation_algorithm_ == "gmm")
    eval_ = std::make_unique<correlation::GmmPeakBasedEval>();
  else
    LOG(FATAL) << "Unknown evaluation algorithm specificed.";
}

model::RegistrationResult SphRegistration::registerPointCloud(
    model::PointCloudPtr cloud_prev, model::PointCloudPtr cloud_cur) {
  CHECK(cloud_prev);
  CHECK(cloud_cur);
  cloud_prev->initialize_kd_tree();

  // Register the point cloud.
  model::RegistrationResult result = estimateRotation(cloud_prev, cloud_cur);
  result.combine(estimateTranslation(cloud_prev, result.getRegisteredCloud()));

  // Evaluate the resul.
  common::BaseDistributionPtr uncertainty =
      eval_->evaluateCorrelation(*aligner_, sph_corr_);
  result.setUncertaintyEstimate(uncertainty);

  return result;
}

model::RegistrationResult SphRegistration::estimateRotation(
    model::PointCloudPtr cloud_prev, model::PointCloudPtr cloud_cur) {
  VLOG(1) << "[SphRegistration] Estimating rotation...";
  cloud_cur->initialize_kd_tree();

  std::array<double, 3> zyz;
  correlatePointcloud(*cloud_prev, *cloud_cur, &zyz);
  model::PointCloud rot_cloud = common::RotationUtils::RotateAroundZYZCopy(
      *cloud_cur, zyz[2], zyz[1], zyz[0]);

  return model::RegistrationResult (std::move(rot_cloud), std::move(zyz));
}

model::RegistrationResult SphRegistration::estimateTranslation(
    model::PointCloudPtr cloud_prev, model::PointCloudPtr rot_cloud) {
  VLOG(1) << "[SphRegistration] Estimating translation...";

  common::Vector_t xyz;
  const double duration_translation_f_ms = common::executeTimedFunction(
      &alignment::BaseAligner::alignRegistered, &(*aligner_), *cloud_prev,
      f_values_, *rot_cloud, h_values_, &xyz);
  CHECK_EQ(xyz.rows(), 3);
  statistics_manager_.emplaceValue(
      kTranslationDurationKey, duration_translation_f_ms);

  VLOG(1) << "Found translation: " << xyz.transpose();
  VLOG(1) << "Translational alignment took: " << duration_translation_f_ms
    << "ms.";
  model::PointCloud reg_cloud = common::TranslationUtils::TranslateXYZCopy(
      *rot_cloud, xyz(0), xyz(1), xyz(2));

  return model::RegistrationResult(std::move(reg_cloud), std::move(xyz));
}

void SphRegistration::getStatistics(
    common::StatisticsManager* manager) const noexcept {
  BaseRegistration::getStatistics(manager);
  sph_corr_.getStatistics(manager);
}

void SphRegistration::correlatePointcloud(
    const model::PointCloud& source, const model::PointCloud& target,
    std::array<double, 3>* const zyz) {
  CHECK(zyz);

  const double duration_sample_f_ms = common::executeTimedFunction(
      &common::SphericalSampler::sampleUniformly, &sampler_, source,
      &f_values_);
  const double duration_sample_h_ms = common::executeTimedFunction(
      &common::SphericalSampler::sampleUniformly, &sampler_, target,
      &h_values_);
  CHECK(f_values_.size() == h_values_.size());

  const double duration_correlation_ms = common::executeTimedFunction(
      &backend::SphericalCorrelation::correlateSignals, &sph_corr_, f_values_,
      h_values_, sampler_.getInitializedBandwith(), zyz);

  VLOG(1) << "Registered point cloud.\n"
          << "Sampling took for f and h: [" << duration_sample_f_ms << "ms,"
          << duration_sample_h_ms << "ms]. \n"
          << "Correlation took: " << duration_correlation_ms << "ms.";

  statistics_manager_.emplaceValue(kSampleDurationKey, duration_sample_f_ms);
  statistics_manager_.emplaceValue(kSampleDurationKey, duration_sample_h_ms);
  statistics_manager_.emplaceValue(
      kCorrelationDurationKey, duration_correlation_ms);
}

void SphRegistration::setBandwith(const int bandwith) {
  sampler_.initialize(bandwith);
}

correlation::BaseEval& SphRegistration::getEvaluation() {
  return *eval_;
}

}  // namespace registration
