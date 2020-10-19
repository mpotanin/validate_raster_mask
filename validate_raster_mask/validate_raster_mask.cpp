// validate_raster_mask.cpp : This file contains the 'main' function. Program execution begins and ends there.
//
#include "external.h"
#include "vector_operator.h"


//create copy of vector file: reproject features, add _FID__
//run rasterize
//calc stat

/*
class FeatureCollection:

Validate (...)
CreateTempVectorFile(path, SRS, FID_col)
CreateTempRasterFile(tempvectorfile, temprasterfile, FID_col,gdal_path)
Merge(mask_file, burned_vector)
SaveStat()
Clean()


members:
inputFile;
tempFile
Objects
*/


const list<MPLOptionDescriptor> listDescriptors = {
	{ "-m", 0, 0, 1, "raster mask (uint8/16/32)" },
	{ "-v", 0, 0, 1, "validation vector" },
	{"-c", 0, 0, 0, "class column (integer)"},
	{"-err", 0, 0, 1, "error column (float)" },
	{"-t", 0, 0, 1, "folder for temporary files"},
	{"-mono", 0, 0, 0, "mono class value"}
};


const list<string> listUsageExamples = {
  "validate_raster_mask -m crop_mask_map_39UVB.tif -v fields.shp -c CROP -err error -t /home/workfolder",
  "validate_raster_mask -m crop_mask_map_39UVB.tif -v fields.shp -mono 1 -err error -t /home/workfolder",
};

//run gdalwarp

#ifdef WIN32
int main(const int nArgs, const char* argv[])
{

	std::vector<string> vecArgs;
	for (int i = 0; i < nArgs; i++)
	{
		//vecArgs.push_back(argv[i]);
		vecArgs.push_back(MPLString::ReplaceAll(argv[i], "\\", "/"));
	}

	if (!MPLGDALDelayLoader::Load(MPLFileSys::RemoveExtension(vecArgs[0]) + ".config"))
	{
		cout << "ERROR: can't load GDAL" << endl;
		return 1;
	}

	cout << endl;

#else
int main(int nArgs, char* argv[])
{
	std::vector<string> vecArgs;
	for (int i = 0; i < nArgs; i++)
		vecArgs.push_back(argv[i]);
	GDALAllRegister();
	OGRRegisterAll();
	CPLSetConfigOption("OGR_ENABLE_PARTIAL_REPROJECTION", "YES");
#endif

	std::cout << endl;

	if (nArgs == 1)
	{
		MPLOptionParser::PrintUsage(listDescriptors, listUsageExamples);
		return 0;
	}

	MPLOptionParser oOptionParser;
	if (!oOptionParser.Init(listDescriptors, vecArgs))
	{
		cout << "ERROR: input cmd line is not valid" << endl;
		return 1;
	}


#ifdef WIN32
	string strGDALBIN = MPLGDALDelayLoader::
						ReadPathFromConfigFile(MPLFileSys::RemoveExtension(vecArgs[0]) + ".config");

	string strOGR2OGR = strGDALBIN + "/ogr2ogr";
	string strGDALRasterize = strGDALBIN + "/gdal_rasterize";

	char charBuf[255] = "";
	strOGR2OGR = _fullpath(charBuf, strOGR2OGR.c_str(), sizeof(charBuf));
	strGDALRasterize = _fullpath(charBuf, strGDALRasterize.c_str(), sizeof(charBuf));
#else
	string strOGR2OGR = "ogr2ogr";
	string strGDALRasterize = "gdal_rasterize ";
#endif
	string strMask = oOptionParser.GetOptionValue("-m");
	string strVectorFile = oOptionParser.GetOptionValue("-v");
	string strWorkFolder = oOptionParser.GetOptionValue("-t");
	string strErrorColName = oOptionParser.GetOptionValue("-err");
	string strClassColName = oOptionParser.GetOptionValue("-c");
	unsigned int nMonoVal = oOptionParser.GetOptionValue("-mono") != "" ?
							std::stoi(oOptionParser.GetOptionValue("-mono")) :-1;

	TaskOperator oTOP;
	
	if (oTOP.InitAndRun(strOGR2OGR,
		strGDALRasterize,
		strVectorFile,
		strMask,
		strWorkFolder,
		strErrorColName,
		strClassColName,
		nMonoVal ))
	{
		return 0;
	}
	else
	{
		return 1;
	}
	return 0;
}
// Run program: Ctrl + F5 or Debug > Start Without Debugging menu
// Debug program: F5 or Debug > Start Debugging menu

// Tips for Getting Started: 
//   1. Use the Solution Explorer window to add/manage files
//   2. Use the Team Explorer window to connect to source control
//   3. Use the Output window to see build output and other messages
//   4. Use the Error List window to view errors
//   5. Go to Project > Add New Item to create new code files, or Project > Add Existing Item to add existing code files to the project
//   6. In the future, to open this project again, go to File > Open > Project and select the .sln file
