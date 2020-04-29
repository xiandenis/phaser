#ifndef PHASER_BACKEND_CORRELATION_SPATIAL_CORRELATION_CUDA_H_
#define PHASER_BACKEND_CORRELATION_SPATIAL_CORRELATION_CUDA_H_

#include "phaser/backend/correlation/base-spatial-correlation.h"

#include <array>
#include <vector>

#include <cuda.h>
#include <cuda_runtime.h>
#include <cufft.h>

namespace alignment {

class SpatialCorrelationCuda : public BaseSpatialCorrelation {
 public:
  explicit SpatialCorrelationCuda(const uint32_t voxels_per_dim);
  ~SpatialCorrelationCuda();

  double* correlateSignals(double* const f, double* const g) override;

 private:
  cufftHandle f_plan_;
  cufftHandle c_plan_;
  cufftDoubleComplex* F_;
  cufftDoubleComplex* G_;
  cufftDoubleComplex* C_;
  double* c_;
  const uint32_t n_voxels_total_;
  const uint32_t n_voxels_per_dim_;
};

}  // namespace alignment

#endif  // PHASER_BACKEND_CORRELATION_SPATIAL_CORRELATION_CUDA_H_