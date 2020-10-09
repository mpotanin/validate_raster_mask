#pragma once
#include "external.h"

class RasterMask
{
public:
	RasterMask()
	{
		m_poDS = 0;
		m_poSRS = 0;
	}

	void Close()
	{
		if (m_poDS) GDALClose(m_poDS);
	}

	~RasterMask()
	{
		Close();
	}



	bool Init(string strRasterFile)
	{
		m_poDS = (GDALDataset*)GDALOpen(strRasterFile.c_str(), GA_ReadOnly);
		if (!m_poDS)
		{
			cout << "ERROR: can't open raster file: " << strRasterFile << endl;
			return false;
		}

		m_poSRS = m_poDS->GetSpatialRef();

		if (CE_None != m_poDS->GetGeoTransform(m_padblGeotr))
		{
			Close();
			cout << "ERROR: can't read geotransform array from file: " << strRasterFile << endl;
			return false;
		}

		return true;
	};
private:
	GDALDataset* m_poDS;
	double m_padblGeotr[6];
	const OGRSpatialReference* m_poSRS;
};