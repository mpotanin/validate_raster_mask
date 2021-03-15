#pragma once
#include "external.h"

class TaskOperator
{
public:
	~TaskOperator()
	{
		Clean();
	}

	bool Clean()
	{
		if (m_poMaskDS) GDALClose(m_poMaskDS);
		m_poMaskDS = 0;

		this->m_mapClasses.clear();
		this->m_mapStat.clear();

		return true;
	}


	bool InitAndRun(string strVectorFile,
					string strMaskFile,
					string strErrorColName,
					string strClassColName,
					unsigned int nMonoVal = -1)
	{
		if (!(m_poMaskDS = (GDALDataset*)GDALOpen(strMaskFile.c_str(), GA_ReadOnly)))
		{
			std::cout << "ERROR: can't open raster: " << strMaskFile << endl;
			return false;
		}
		//std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now()).count();

		srand(time(0));
		m_strVectorFile = strVectorFile;
		m_strMaskFile = strMaskFile;
		if (nMonoVal == -1)
		{
			m_strClassColName = strClassColName;
			m_nMonoVal = -1;
		}
		else
		{
			m_strClassColName = "";
			m_nMonoVal = nMonoVal;
		}


		/**/
		m_strErrorColName = strErrorColName;
		
		string strInMemGTiff = RasterizeIntoMemGTiff();
		if (strInMemGTiff=="")
		{
			std::cout << "ERROR: RasterizeIntoMemDS failed " << endl;
			return false;
		}

		if (!CalcStatByMatchingRasters(strInMemGTiff))
		{
			VSIUnlink(strInMemGTiff.c_str());
			std::cout << "ERROR: CalcStatByMatchingRasters failed " << endl;
			return false;
		}

		VSIUnlink(strInMemGTiff.c_str());

		return WriteErrorColumn();
	};

	/*
	bool InitAndRun(string strOGR2OGR,
			string strGDALRasterize,
			string strVectorFile,
			string strMaskFile,
			string strWorkFolder,
			string strErrorColName,
			string strClassColName,
			unsigned int nMonoVal = -1)
	{
		if (!(m_poMaskDS = (GDALDataset*)GDALOpen(strMaskFile.c_str(), GA_ReadOnly)))
		{
			std::cout << "ERROR: can't open raster: " << strMaskFile << endl;
			return false;
		}
		//std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now()).count();

		srand(time(0));
		m_strOGR2OGR = strOGR2OGR;
		m_strGDALRasterize = strGDALRasterize;
		m_strVectorFile = strVectorFile;
		m_strMaskFile = strMaskFile;
		if (nMonoVal == -1)
		{
			m_strClassColName = strClassColName;
			m_nMonoVal = -1;
		}
		else
		{
			m_strClassColName = "";
			m_nMonoVal = nMonoVal;
		}

		m_strTempCopyOfVectorFile = MPLFileSys::GetAbsolutePath(strWorkFolder,
			MPLFileSys::RemoveExtension(MPLFileSys::RemovePath(strVectorFile)) + "_" + to_string(rand()) + ".shp");

		m_strTempRasterized = MPLFileSys::RemoveExtension(m_strTempCopyOfVectorFile) + ".tif";

		m_strFIDColName = "FID_" + to_string(rand()%10000);

		m_strErrorColName = strErrorColName;

		if (CreateTempVectorCopy())
			if (AddFIDcolumnAndParseClasses())
				if (RunRasterize())
					if (CalcStatByMatchingRasters())
						if (WriteErrorColumn())
							return true;

		return false;
	};
	*/


protected:

	string CreateInMemGTiff()
	{
		int nWidth = m_poMaskDS->GetRasterXSize();
		int nHeight = m_poMaskDS->GetRasterYSize();
		string strInMemGtiff = "/vsimem/tiffinmem_" + to_string(rand());
	
		//strInMemName = "/vsimem/tiffinmem_" + to_string(rand());


		GDALDataset* poOutputDS = (GDALDataset*)GDALCreate(
			GDALGetDriverByName("GTiff"),
			strInMemGtiff.c_str(),
			m_poMaskDS->GetRasterXSize(),
			m_poMaskDS->GetRasterYSize(),
			1, GDT_UInt32, 0);

		double dblGeotransform[6];
		m_poMaskDS->GetGeoTransform(dblGeotransform);
		poOutputDS->SetGeoTransform(dblGeotransform);
		poOutputDS->SetSpatialRef(m_poMaskDS->GetSpatialRef());

		unsigned long int nPixelsCount = nWidth * nHeight;
		unsigned int* panNDV = new unsigned int[nPixelsCount];
		for (unsigned long int i = 0; i < nPixelsCount; i++)
			panNDV[i] = m_nNDV;
		poOutputDS->RasterIO(GF_Write,0,0,nWidth,nHeight,panNDV,nWidth,nHeight, GDT_UInt32,1,0,0,0,0,0);
		GDALFlushCache(poOutputDS);
		GDALClose(poOutputDS);
		delete[]panNDV;

		return strInMemGtiff;
	}

	int ParseFeatures(OGRGeometryH* &papoGeometries, double* &padfFID)
	{
		GDALDataset* poVecDS = (GDALDataset*)GDALOpenEx(m_strVectorFile.c_str(),
			GDAL_OF_VECTOR,	NULL, NULL, NULL);
		OGRLayer* poLayer = poVecDS->GetLayer(0);


		papoGeometries = new OGRGeometryH[poLayer->GetFeatureCount()];
		padfFID = new double[poLayer->GetFeatureCount()];

	
		int nCount = 0;
		OGRSpatialReference *poSRS= m_poMaskDS->GetSpatialRef()->Clone();
		OGRFeature* poFeature = 0;
		while (poFeature = poLayer->GetNextFeature())
		{
			padfFID[nCount] = poFeature->GetFID();
			m_mapClasses[padfFID[nCount]] = poFeature->GetFieldAsInteger(m_strClassColName.c_str());
			papoGeometries[nCount] = poFeature->GetGeometryRef()->clone();
			((OGRGeometry*)papoGeometries[nCount])->transformTo(poSRS);
			nCount++;
			OGRFeature::DestroyFeature(poFeature);
		}
		
		OGRSpatialReference::DestroySpatialReference(poSRS);
		GDALClose(poVecDS);
		return nCount;
	}
	
	string RasterizeIntoMemGTiff()
	{
		OGRGeometryH* papoGeometries;
		double* padfFID;

		unsigned int nTotalFeatures = ParseFeatures(papoGeometries, padfFID);
		
		if (!nTotalFeatures) return 0;

		int bandList[1] = { 1};

		string strInMemGTiff = CreateInMemGTiff();

		bool bWasError = false;
		if (strInMemGTiff !="")
		{
			GDALDataset* poInMemDS = (GDALDataset*)GDALOpen(strInMemGTiff.c_str(), GA_Update);

			if (CE_None != GDALRasterizeGeometries(
				poInMemDS, 1, bandList, nTotalFeatures, papoGeometries, 0, 0, padfFID, 0, 0, 0))
				bWasError = true;
			GDALClose(poInMemDS);
				poInMemDS = 0;
		}
		

		delete[]padfFID;
		for (int i = 0; i < nTotalFeatures; i++)
			delete(((OGRGeometry*)papoGeometries[i]));
		delete[]papoGeometries;

		return bWasError ? "" : strInMemGTiff;
		
	};


	bool WriteErrorColumn()
	{
		GDALDataset* poVecDS = (GDALDataset*)GDALOpenEx(m_strVectorFile.c_str(),
			GDAL_OF_VECTOR | GDAL_OF_UPDATE,
			NULL, NULL, NULL);
		OGRLayer* poLayer = poVecDS->GetLayer(0);

		if (poLayer->FindFieldIndex(m_strErrorColName.c_str(),1) < 0)
		{
			OGRFieldDefn oErrDef(m_strErrorColName.c_str(), OFTReal);
			poLayer->CreateField(&oErrDef);
		}
		

		OGRFeature* poFeature;

		while (poFeature = poLayer->GetNextFeature())
		{
			if (m_mapStat.find(poFeature->GetFID()) != m_mapStat.end())
			{
				poFeature->SetField(m_strErrorColName.c_str(),
					1. - ((double)m_mapStat[poFeature->GetFID()].second) / m_mapStat[poFeature->GetFID()].first);
				poLayer->SetFeature(poFeature);
			}
			OGRFeature::DestroyFeature(poFeature);
		}

		GDALClose(poVecDS);
		return true;
	};

	bool CalcStatByMatchingRasters(string strRasterFile)
	{
		GDALDataset* poInMemDS = (GDALDataset*)GDALOpen(strRasterFile.c_str(), GA_ReadOnly);
		int nWidth = m_poMaskDS->GetRasterXSize();
		int nHeight = m_poMaskDS->GetRasterYSize();

		int nChunkMaxSize = 4000;
		
		for (int nCorrX = 0; nCorrX < nWidth; nCorrX += nChunkMaxSize)
		{
			for (int nCorrY = 0; nCorrY < nHeight; nCorrY += nChunkMaxSize)
			{
				int nChunckWidth = nCorrX + nChunkMaxSize <= nWidth ? nChunkMaxSize : nWidth - nCorrX;
				int nChunckHeight = nCorrY + nChunkMaxSize <= nHeight ? nChunkMaxSize : nHeight - nCorrY;

				unsigned int* panRasterizedPixels = new unsigned int[nChunckWidth * nChunckHeight];
				unsigned int* panMaskPixels = new unsigned int[nChunckWidth * nChunckHeight];

				poInMemDS->RasterIO(GF_Read, nCorrX, nCorrY, nChunckWidth, nChunckHeight,
					panRasterizedPixels, nChunckWidth, nChunckHeight, GDT_UInt32, 1, 0, 0, 0, 0, 0);

				m_poMaskDS->RasterIO(GF_Read, nCorrX, nCorrY, nChunckWidth, nChunckHeight,
					panMaskPixels, nChunckWidth, nChunckHeight, GDT_UInt32, 1, 0, 0, 0, 0, 0);

				int nArea = nChunckHeight * nChunckWidth;
				bool bMatch;
				for (int i = 0; i < nArea; i++)
				{
					if (panRasterizedPixels[i] != m_nNDV)
					{
						bMatch = (m_mapClasses[panRasterizedPixels[i]] == panMaskPixels[i]);
						if (m_mapStat.find(panRasterizedPixels[i]) != m_mapStat.end())
						{
							m_mapStat[panRasterizedPixels[i]].first += 1;
							m_mapStat[panRasterizedPixels[i]].second += bMatch;
						}
						else
						{
							m_mapStat[panRasterizedPixels[i]] = pair<int, int>(1, bMatch);
						}
					}
				}


				delete[]panRasterizedPixels;
				delete[]panMaskPixels;
			}
		}
		GDALClose(poInMemDS);

		return true;
	};

	/*
	bool RunRasterize()
	{
		double padblGeotr[6];
		m_poMaskDS->GetGeoTransform(padblGeotr);
		int nWidth = m_poMaskDS->GetRasterXSize();
		int nHeight = m_poMaskDS->GetRasterYSize();
		OGREnvelope oEnvp;
		oEnvp.MinX = padblGeotr[0];
		oEnvp.MaxY = padblGeotr[3];
		oEnvp.MaxX = oEnvp.MinX + nWidth * padblGeotr[1];
		oEnvp.MinY = oEnvp.MaxY - nHeight * padblGeotr[1];


		string strCommand = m_strGDALRasterize + " -of GTiff -ot UInt32 -co \"COMPRESS=LZW\" ";
		strCommand += "-a_nodata " + to_string(m_nNDV) + " -init " + to_string(m_nNDV) + " ";
		strCommand += "-a " + m_strFIDColName + " ";
		strCommand += "-te " + to_string(oEnvp.MinX) + " " + to_string(oEnvp.MinY) + " " +
			to_string(oEnvp.MaxX) + " " + to_string(oEnvp.MaxY) + " ";
		strCommand += "-ts " + to_string(nWidth) + " " + to_string(nHeight) + " ";
		strCommand += "\"" + m_strTempCopyOfVectorFile + "\" \"" + m_strTempRasterized + "\"";
		return (0==std::system(strCommand.c_str()));
	}

	bool AddFIDcolumnAndParseClasses()
	{
		GDALDataset* poVecDS = (GDALDataset*)GDALOpenEx(m_strTempCopyOfVectorFile.c_str(), 
														GDAL_OF_VECTOR | GDAL_OF_UPDATE, 
														NULL, NULL, NULL);
		OGRLayer* poLayer = poVecDS->GetLayer(0);

		OGRFieldDefn oFIDDef(m_strFIDColName.c_str(), OFTInteger);
		OGRErr oErr = poLayer->CreateField(&oFIDDef);

		//poLayer->
		OGRFeature* poFeature;
		
		while (poFeature = poLayer->GetNextFeature())
		{
			poFeature->SetField(m_strFIDColName.c_str(), poFeature->GetFID());
			poLayer->SetFeature(poFeature);
			//std::cout << poFeature->GetFieldAsInteger(strFIDName.c_str()) << endl;
			m_mapClasses[poFeature->GetFID()] = (m_nMonoVal == -1) ? 
				poFeature->GetFieldAsInteger(m_strClassColName.c_str()) : m_nMonoVal;
			OGRFeature::DestroyFeature(poFeature);
			//poLayer->
			//poLayer->
		}
	

		GDALClose(poVecDS);
		return true;
	};

	
	bool CreateTempVectorCopy()
	{
		string strCommand = m_strOGR2OGR;
		strCommand += " -f \"ESRI Shapefile\" ";
		//strCommand += "-t_srs EPSG:" + to_string(poTargetSRS->exportToProj4()) + " ";

		
		char* pachProj4;
		OGRSpatialReference oSRS;
		oSRS.SetFromUserInput(m_poMaskDS->GetProjectionRef());
		oSRS.exportToProj4(&pachProj4);
		//m_poMaskDS->GetSpatialRef()->exportToProj4(&pachProj4);
		string strProj4(pachProj4);
		strCommand += "-t_srs \"" + strProj4 + "\" ";
		strCommand += "\"" + m_strTempCopyOfVectorFile + "\" \"" + m_strVectorFile + "\"";
		//std::cout << strCommand << endl;
		return (0==std::system(strCommand.c_str()));
	};
	*/

private:
	GDALDataset* m_poMaskDS;

	std::map<unsigned int, int> m_mapClasses;
	std::map<unsigned int, pair<int, int>> m_mapStat;

	string m_strVectorFile;
	string m_strMaskFile;
	

	string m_strClassColName;

	string m_strErrorColName;
	static const unsigned int m_nNDV = UINT_MAX;
	unsigned int m_nMonoVal;
};