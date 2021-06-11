/****************************************************************************
* MeshLab                                                           o o     *
* A versatile mesh processing toolbox                             o     o   *
*                                                                _   O  _   *
* Copyright(C) 2005-2021                                           \/)\/    *
* Visual Computing Lab                                            /\/|      *
* ISTI - Italian National Research Council                           |      *
*                                                                    \      *
* All rights reserved.                                                      *
*                                                                           *
* This program is free software; you can redistribute it and/or modify      *
* it under the terms of the GNU General Public License as published by      *
* the Free Software Foundation; either version 2 of the License, or         *
* (at your option) any later version.                                       *
*                                                                           *
* This program is distributed in the hope that it will be useful,           *
* but WITHOUT ANY WARRANTY; without even the implied warranty of            *
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the             *
* GNU General Public License (http://www.gnu.org/licenses/gpl.txt)          *
* for more details.                                                         *
*                                                                           *
****************************************************************************/

#ifndef MESHLAB_EIGEN_MESH_CONVERSIONS_H
#define MESHLAB_EIGEN_MESH_CONVERSIONS_H

#include <Eigen/Core>
#include "../ml_document/cmesh.h"

typedef Eigen::Matrix<Scalarm, Eigen::Dynamic, 3> EigenMatrixX3m;
typedef Eigen::Matrix<Scalarm, Eigen::Dynamic, 2> EigenMatrixX2m;
typedef Eigen::Matrix<Scalarm, Eigen::Dynamic, 1> EigenVectorXm;

namespace meshlab {

CMeshO meshFromMatrices(
		const EigenMatrixX3m& vertices,
		const Eigen::MatrixX3i& faces = Eigen::MatrixX3i(),
		const EigenMatrixX3m& vertexNormals = EigenMatrixX3m(),
		const EigenMatrixX3m& faceNormals = EigenMatrixX3m(),
		const EigenVectorXm& vertexQuality = EigenVectorXm(),
		const EigenVectorXm& faceQuality = EigenVectorXm());

EigenMatrixX3m vertexMatrix(const CMeshO& mesh);
Eigen::MatrixX3i faceMatrix(const CMeshO& mesh);
EigenMatrixX3m vertexNormalMatrix(const CMeshO& mesh);
EigenMatrixX3m faceNormalMatrix(const CMeshO& mesh);
EigenMatrixX3m vertexColorMatrix(const CMeshO& mesh);
EigenMatrixX3m faceColorMatrix(const CMeshO& mesh);
Eigen::Matrix<unsigned int, Eigen::Dynamic, 1> vertexColorArray(const CMeshO& mesh);
Eigen::Matrix<unsigned int, Eigen::Dynamic, 1> faceColorArray(const CMeshO& mesh);
EigenVectorXm vertexQualityArray(const CMeshO& mesh);
EigenVectorXm faceQualityArray(const CMeshO& mesh);
EigenMatrixX2m vertexTexCoordMatrix(const CMeshO& mesh);
EigenMatrixX2m wedgeTexCoordMatrix(const CMeshO& mesh);
}

#endif // MESHLAB_EIGEN_MESH_CONVERSIONS_H
