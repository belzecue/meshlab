/****************************************************************************
* MeshLab                                                           o o     *
* A versatile mesh processing toolbox                             o     o   *
*                                                                _   O  _   *
* Copyright(C) 2005                                                \/)\/    *
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

#include "filter_plymc.h"
#include <wrap/io_trimesh/export_vmi.h>
#include <vcg/complex/algorithms/smooth.h>
#include <vcg/complex/algorithms/create/plymc/plymc.h>
#include <vcg/complex/algorithms/create/plymc/simplemeshprovider.h>
#include <QTemporaryFile>

using namespace vcg;

// Constructor usually performs only two simple tasks of filling the two lists
//  - typeList: with all the possible id of the filtering actions
//  - actionList with the corresponding actions. If you want to add icons to your filtering actions you can do here by construction the QActions accordingly

PlyMCPlugin::PlyMCPlugin()
{
	typeList = {
		FP_PLYMC,
		FP_MC_SIMPLIFY
	};
	
	for(ActionIDType tt: types())
		actionList.push_back(new QAction(filterName(tt), this));
}

QString PlyMCPlugin::pluginName() const
{
	return "FilterPlyMC";
}

// ST() must return the very short string describing each filtering action
// (this string is used also to define the menu entry)
QString PlyMCPlugin::filterName(ActionIDType filterId) const
{
	switch(filterId)
	{
	case FP_PLYMC :        return QString("Surface Reconstruction: VCG");
	case FP_MC_SIMPLIFY :  return QString("Simplification: Edge Collapse for Marching Cube meshes");
	default : assert(0);
	}
	return {};
}

// Info() must return the longer string describing each filtering action
// (this string is used in the About plugin dialog)
QString PlyMCPlugin::filterInfo(ActionIDType filterId) const
{
	switch(filterId)
	{
	case FP_PLYMC :  return QString( "The surface reconstrction algorithm that have been used for a long time inside the ISTI-Visual Computer Lab."
									 "It is mostly a variant of the Curless et al. e.g. a volumetric approach with some original weighting schemes,"
									 "a different expansion rule, and another approach to hole filling through volume dilation/relaxations.<br>"
									 "The filter is applied to <b>ALL</b> the visible layers. In practice, all the meshes/point clouds that are currently <i>visible</i> are used to build the volumetric distance field.");
	case FP_MC_SIMPLIFY :  return QString( "A simplification/cleaning algorithm that works ONLY on meshes generated by Marching Cubes algorithm." );
		
	default : assert(0);
	}
	return QString("Unknown Filter");
}

// The FilterClass describes in which generic class of filters it fits.
// This choice affect the submenu in which each filter will be placed
// More than a single class can be chosen.
PlyMCPlugin::FilterClass PlyMCPlugin::getClass(const QAction *a) const
{
	switch(ID(a))
	{
	case FP_PLYMC :  return FilterPlugin::Remeshing;
	case FP_MC_SIMPLIFY :  return FilterPlugin::Remeshing;
	default : assert(0);
	}
	return FilterPlugin::Generic;
}

// This function define the needed parameters for each filter. Return true if the filter has some parameters
// it is called every time, so you can set the default value of parameters according to the mesh
// For each parameter you need to define,
// - the name of the parameter,
// - the string shown in the dialog
// - the default value
// - a possibly long string describing the meaning of that parameter (shown as a popup help in the dialog)
RichParameterList PlyMCPlugin::initParameterList(const QAction *action,const MeshModel &m)
{
	RichParameterList parlst;
	switch(ID(action))
	{
	case FP_PLYMC :
		parlst.addParam(RichAbsPerc("voxSize",m.cm.bbox.Diag()/100.0,0,m.cm.bbox.Diag(),"Voxel Side", "VoxelSide"));
		parlst.addParam(    RichInt("subdiv",1,"SubVol Splitting","The level of recursive splitting of the subvolume reconstruction process. A value of '3' means that a 3x3x3 regular space subdivision is created and the reconstruction process generate 8 matching meshes. It is useful for reconsruction objects at a very high resolution. Default value (1) means no splitting."));
		parlst.addParam(  RichFloat("geodesic",2.0,"Geodesic Weighting","The influence of each range map is weighted with its geodesic distance from the borders. In this way when two (or more ) range maps overlaps their contribution blends smoothly hiding possible misalignments. "));
		parlst.addParam(   RichBool("openResult",true,"Show Result","if not checked the result is only saved into the current directory"));
		parlst.addParam(    RichInt("smoothNum",1,"Volume Laplacian iter","How many volume smoothing step are performed to clean out the eventually noisy borders"));
		parlst.addParam(    RichInt("wideNum",3,"Widening" ," How many voxel the field is expanded. Larger this value more holes will be filled"));
		parlst.addParam(   RichBool("mergeColor",false,"Vertex Splatting","This option use a different way to build up the volume, instead of using rasterization of the triangular face it splat the vertices into the grids. It works under the assumption that you have at least one sample for each voxel of your reconstructed volume."));
		parlst.addParam(   RichBool("simplification",false,"Post Merge simplification","After the merging an automatic simplification step is performed."));
		parlst.addParam(    RichInt("normalSmooth",3,"PreSmooth iter" ,"How many times, before converting meshes into volume, the normal of the surface are smoothed. It is useful only to get more smooth expansion in case of noisy borders."));
		break;
	case FP_MC_SIMPLIFY :
		break;
	default: break; // do not add any parameter for the other filters
	}
	return parlst;
}

// The Real Core Function doing the actual mesh processing.
std::map<std::string, QVariant> PlyMCPlugin::applyFilter(
		const QAction *filter,
		const RichParameterList & par,
		MeshDocument &md,
		unsigned int& /*postConditionMask*/,
		vcg::CallBackPos * cb)
{
	switch(ID(filter))
	{
	case  FP_PLYMC:
	{
		srand(time(NULL));
		
		//check if folder is writable
		QTemporaryFile file("./_tmp_XXXXXX.tmp");
		if (!file.open()) 
		{
			log("ERROR - current folder is not writable. VCG Merging needs to save intermediate files in the current working folder. Project and meshes must be in a write-enabled folder. Please save your data in a suitable folder before applying.");
			throw MLException("current folder is not writable.<br> VCG Merging needs to save intermediate files in the current working folder.<br> Project and meshes must be in a write-enabled folder.<br> Please save your data in a suitable folder before applying.");
		}
		
		tri::PlyMC<SMesh,SimpleMeshProvider<SMesh> > pmc;
		pmc.MP.setCacheSize(64);
		tri::PlyMC<SMesh,SimpleMeshProvider<SMesh> >::Parameter &p = pmc.p;
		
		int subdiv=par.getInt("subdiv");
		
		p.IDiv=Point3i(subdiv,subdiv,subdiv);
		p.IPosS=Point3i(0,0,0);
		p.IPosE[0]=p.IDiv[0]-1; p.IPosE[1]=p.IDiv[1]-1; p.IPosE[2]=p.IDiv[2]-1;
		printf("AutoComputing all subVolumes on a %ix%ix%i\n",p.IDiv[0],p.IDiv[1],p.IDiv[2]);
		
		p.VoxSize=par.getAbsPerc("voxSize");
		p.QualitySmoothVox = par.getFloat("geodesic");
		p.SmoothNum = par.getInt("smoothNum");
		p.WideNum = par.getInt("wideNum");
		p.NCell=0;
		p.FullyPreprocessedFlag=true;
		p.MergeColor=p.VertSplatFlag=par.getBool("mergeColor");
		p.SimplificationFlag = par.getBool("simplification");
		for(MeshModel& mm: md.meshIterator())
		{
			if(mm.isVisible())
			{
				SMesh sm;
				mm.updateDataMask(MeshModel::MM_FACEQUALITY);
				tri::Append<SMesh,CMeshO>::Mesh(sm, mm.cm/*,false,p.VertSplatFlag*/); // note the last parameter of the append to prevent removal of unreferenced vertices...
				tri::UpdatePosition<SMesh>::Matrix(sm, Matrix44f::Construct(mm.cm.Tr),true);
				tri::UpdateBounding<SMesh>::Box(sm);
				tri::UpdateNormal<SMesh>::NormalizePerVertex(sm);
				tri::UpdateTopology<SMesh>::VertexFace(sm);
				tri::UpdateFlags<SMesh>::VertexBorderFromNone(sm);
				tri::Geodesic<SMesh>::DistanceFromBorder(sm);
				for(int i=0;i<par.getInt("normalSmooth");++i)
					tri::Smooth<SMesh>::FaceNormalLaplacianVF(sm);
				//QString mshTmpPath=QDir::tempPath()+QString("/")+QString(mm->shortName())+QString(".vmi");
				QString mshTmpPath=QString("__TMP")+QString(mm.shortName())+QString(".vmi");
				qDebug("Saving tmp file %s",qUtf8Printable(mshTmpPath));
				int retVal = tri::io::ExporterVMI<SMesh>::Save(sm,qUtf8Printable(mshTmpPath) );
				if(retVal!=0)
				{
					qDebug("Failed to write vmi temp file %s",qUtf8Printable(mshTmpPath));

					log("ERROR - Failed to write vmi temp file %s", qUtf8Printable(mshTmpPath));
					throw MLException("Failed to write vmi temp file " + mshTmpPath);
				}
				pmc.MP.AddSingleMesh(qUtf8Printable(mshTmpPath));
				log("Preprocessing mesh %s",qUtf8Printable(mm.shortName()));
			}
		}
		
		if(pmc.Process(cb)==false)
		{
			throw MLException(pmc.errorMessage.c_str());
		}
		
		
		if(par.getBool("openResult"))
		{
			for(size_t i=0;i<p.OutNameVec.size();++i)
			{string name;
				if(!p.SimplificationFlag) name = p.OutNameVec[i].c_str();
				else name = p.OutNameSimpVec[i].c_str();
				
				MeshModel *mp=md.addNewMesh("",name.c_str(),true);  // created mesh is the current one, if multiple meshes are created last mesh is the current one
				int loadMask=-1;
				tri::io::ImporterPLY<CMeshO>::Open(mp->cm,name.c_str(),loadMask);
				if(p.MergeColor) mp->updateDataMask(MeshModel::MM_VERTCOLOR);
				mp->updateDataMask(MeshModel::MM_VERTQUALITY);
				mp->updateBoxAndNormals();
			}
		}
		
		for(int i=0;i<pmc.MP.size();++i)
			QFile::remove(pmc.MP.MeshName(i).c_str());
	} break;
	case FP_MC_SIMPLIFY:
	{
		MeshModel &mm=*md.mm();
		if (mm.cm.fn == 0)
		{
			log("Cannot simplify: no faces.");
			throw MLException("Cannot simplify: no faces.");
		}
		mm.updateDataMask(MeshModel::MM_VERTFACETOPO+MeshModel::MM_FACEFACETOPO+MeshModel::MM_VERTMARK);
		int res = tri::MCSimplify<CMeshO>(mm.cm,0.0f,false);
		if (res !=1)
		{
			log("Cannot simplify: this is not a Marching Cube -generated mesh. Mesh should have some of its edges 'straight' along axes.");
			mm.clearDataMask(MeshModel::MM_VERTFACETOPO);
			mm.clearDataMask(MeshModel::MM_FACEFACETOPO);
			throw MLException("Cannot simplify: this is not a Marching Cube -generated mesh.");
		}
		
		tri::Allocator<CMeshO>::CompactFaceVector(mm.cm);
		tri::Clean<CMeshO>::RemoveTVertexByFlip(mm.cm,20,true);
		tri::Clean<CMeshO>::RemoveFaceFoldByFlip(mm.cm);
		mm.clearDataMask(MeshModel::MM_VERTFACETOPO);
		mm.clearDataMask(MeshModel::MM_FACEFACETOPO);
	} break;
	default:
		wrongActionCalled(filter);
	}
	return std::map<std::string, QVariant>();
}

FilterPlugin::FilterArity PlyMCPlugin::filterArity(const QAction * filter ) const
{
	switch(ID(filter)) 
	{
	case FP_PLYMC :       return FilterPlugin::VARIABLE;
	case FP_MC_SIMPLIFY : return FilterPlugin::SINGLE_MESH;
	default:              return FilterPlugin::NONE;
	}
}

int PlyMCPlugin::postCondition(const QAction * filter) const
{
	switch (ID(filter))
	{
	case FP_PLYMC:        return MeshModel::MM_NONE; // no change to old layers
	case FP_MC_SIMPLIFY:  return MeshModel::MM_GEOMETRY_AND_TOPOLOGY_CHANGE;
	default:              return MeshModel::MM_ALL;
	}
}

MESHLAB_PLUGIN_NAME_EXPORTER(PlyMCPlugin)
