/**
 * @copyright
 * Copyright (c) 2012-2016, OpenGeoSys Community (http://www.opengeosys.org)
 *            Distributed under a Modified BSD License.
 *              See accompanying file LICENSE.txt or
 *              http://www.opengeosys.org/LICENSE.txt
 */

// TCLAP
#include "tclap/CmdLine.h"

#include "Applications/ApplicationsLib/LogogSetup.h"

// BaseLib
#include "FileTools.h"

// GeoLib
#include "GEOObjects.h"
#include "PolylineVec.h"

// FileIO
#include "MeshLib/IO/Legacy/MeshIO.h"
#include "MeshLib/IO/readMeshFromFile.h"
#include "GeoLib/IO/XmlIO/Boost/BoostXmlGmlInterface.h"

// MeshLib
#include "Mesh.h"

// MeshGeoToolsLib
#include "MeshGeoToolsLib/AppendLinesAlongPolyline.h"


int main (int argc, char* argv[])
{
	ApplicationsLib::LogogSetup logog_setup;

	TCLAP::CmdLine cmd("Append line elements into a mesh.", ' ', "0.1");
	TCLAP::ValueArg<std::string> mesh_in("i", "mesh-input-file",
	                                     "the name of the file containing the input mesh", true,
	                                     "", "file name of input mesh");
	cmd.add(mesh_in);
	TCLAP::ValueArg<std::string> mesh_out("o", "mesh-output-file",
	                                      "the name of the file the mesh will be written to", true,
	                                      "", "file name of output mesh");
	cmd.add(mesh_out);
	TCLAP::ValueArg<std::string> geoFileArg("g", "geo-file",
	                                      "the name of the geometry file which contains polylines", true, "", "the name of the geometry file");
	cmd.add(geoFileArg);

	// parse arguments
	cmd.parse(argc, argv);

	// read GEO objects
	GeoLib::GEOObjects geo_objs;
	GeoLib::IO::BoostXmlGmlInterface xml(geo_objs);
	xml.readFile(geoFileArg.getValue());

	std::vector<std::string> geo_names;
	geo_objs.getGeometryNames (geo_names);
	if (geo_names.empty ())
	{
		std::cout << "no geometries found" << std::endl;
		return EXIT_FAILURE;
	}
	const GeoLib::PolylineVec* ply_vec (geo_objs.getPolylineVecObj(geo_names[0]));
	if (!ply_vec)
	{
		std::cout << "could not found polylines" << std::endl;
		return EXIT_FAILURE;
	}

	// read a mesh
	MeshLib::Mesh const*const mesh (MeshLib::IO::readMeshFromFile(mesh_in.getValue()));
	if (!mesh)
	{
		ERR("Mesh file \"%s\" not found", mesh_in.getValue().c_str());
		return EXIT_FAILURE;
	}
	INFO("Mesh read: %d nodes, %d elements.", mesh->getNNodes(), mesh->getNElements());

	// add line elements
	std::unique_ptr<MeshLib::Mesh> new_mesh =
	    MeshGeoToolsLib::appendLinesAlongPolylines(*mesh, *ply_vec);
	INFO("Mesh created: %d nodes, %d elements.", new_mesh->getNNodes(), new_mesh->getNElements());

	MeshLib::IO::writeMeshToFile(*new_mesh, mesh_out.getValue());

	return EXIT_SUCCESS;
}
