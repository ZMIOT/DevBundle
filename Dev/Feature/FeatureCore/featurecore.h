#ifndef FEATURECORE_H
#define FEATURECORE_H
#include "MeshType.h"
#include "featurecore_global.h"
#include <armadillo>
#include "pointnormal.h"
#include "normalhistogram.h"
#include "colorhistogram.h"
#include "blockbasedfeature.h"
#include "agd.h"
template class FEATURECORESHARED_EXPORT Feature::NormalHistogram<DefaultMesh>;
template class FEATURECORESHARED_EXPORT Feature::ColorHistogramLab<DefaultMesh>;
template class FEATURECORESHARED_EXPORT Feature::ColorHistogramRGB<DefaultMesh>;
template class FEATURECORESHARED_EXPORT Feature::BlockBasedFeature<DefaultMesh>;
template class FEATURECORESHARED_EXPORT Feature::AGD<DefaultMesh>; //Average Geodesic Distance
void FEATURECORESHARED_EXPORT extract_patch_feature(DefaultMesh&, arma::vec&, Config::Ptr);
#endif // FEATURECORE_H
