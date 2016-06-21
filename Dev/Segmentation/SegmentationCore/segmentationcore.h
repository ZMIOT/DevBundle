#ifndef SEGMENTATIONCORE_H
#define SEGMENTATIONCORE_H
#include "supervoxelclustering.h"
#include "regiongrowing.h"
#include "regiongrowingrgb.h"
#include <armadillo>
#include <stdint.h>
#include "segmentationcore_global.h"
#include "sac_segmentation.h"
#include "graphcut.h"
namespace Segmentation
{
template class SEGMENTATIONCORESHARED_EXPORT  Voxel<DefaultMesh>;
template class SEGMENTATIONCORESHARED_EXPORT  SuperVoxel<DefaultMesh>;
template class SEGMENTATIONCORESHARED_EXPORT  DefaultVoxelDistFunctor<DefaultMesh>;
template class SEGMENTATIONCORESHARED_EXPORT  SuperVoxelClustering<DefaultMesh>;
template class SEGMENTATIONCORESHARED_EXPORT  RegionGrowing<DefaultMesh>;
template class SEGMENTATIONCORESHARED_EXPORT  RegionGrowingRGB<DefaultMesh>;
}

#endif // SEGMENTATIONCORE_H
