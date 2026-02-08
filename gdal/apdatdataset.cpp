/******************************************************************************
 *
 * Project:  ArduPilot terrain.dat Reader (inherited from Japanese DEM)
 *
 ******************************************************************************
 * Copyright (c) 2000, Frank Warmerdam <warmerdam@pobox.com>
 * Copyright (c) 2009-2012, Even Rouault <even dot rouault at spatialys.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 ****************************************************************************/

#include "cpl_port.h"
#include "gdal_frmts.h"
#include "gdal_pam.h"

#include <algorithm>

constexpr int HEADER_SIZE = 2048;

/************************************************************************/
/*                            APDATGetField()                            */
/************************************************************************/

static int APDATGetField(const char *pszField, int nWidth)

{
    char szWork[32] = {};
    CPLAssert(nWidth < static_cast<int>(sizeof(szWork)));

    strncpy(szWork, pszField, nWidth);
    szWork[nWidth] = '\0';

    return atoi(szWork);
}

/************************************************************************/
/*                            APDATGetAngle()                            */
/************************************************************************/

static double APDATGetAngle(const char *pszField)

{
    const int nAngle = APDATGetField(pszField, 7);

    // Note, this isn't very general purpose, but it would appear
    // from the field widths that angles are never negative.  Nice
    // to be a country in the "first quadrant".

    const int nDegree = nAngle / 10000;
    const int nMin = (nAngle / 100) % 100;
    const int nSec = nAngle % 100;

    return nDegree + nMin / 60.0 + nSec / 3600.0;
}

/************************************************************************/
/* ==================================================================== */
/*                              APDATDataset                             */
/* ==================================================================== */
/************************************************************************/

class APDATRasterBand;

class APDATDataset final : public GDALPamDataset
{
    friend class APDATRasterBand;

    VSILFILE *m_fp = nullptr;
    GByte m_abyFirstBlock[HEADER_SIZE];
    double adfGeoTransform[6];
    OGRSpatialReference m_oSRS{};

    double spacing;

    // rounded latitude/longitude in degrees. trusted in the file
    int16_t lon_degrees;
    int8_t lat_degrees;

  public:
    APDATDataset();
    ~APDATDataset();

    static GDALDataset *Open(GDALOpenInfo *);
    static int Identify(GDALOpenInfo *);

    CPLErr GetGeoTransform(double *padfTransform) override;
    const OGRSpatialReference *GetSpatialRef() const override;
};

/************************************************************************/
/* ==================================================================== */
/*                            APDATRasterBand                             */
/* ==================================================================== */
/************************************************************************/

class APDATRasterBand final : public GDALPamRasterBand
{
    friend class APDATDataset;

    int m_nRecordSize = 0;
    char *m_pszRecord = nullptr;
    bool m_bBufferAllocFailed = false;

  public:
    APDATRasterBand(APDATDataset *, int);
    ~APDATRasterBand();

    virtual CPLErr IReadBlock(int, int, void *) override;
};

/************************************************************************/
/*                           APDATRasterBand()                            */
/************************************************************************/

APDATRasterBand::APDATRasterBand(APDATDataset *poDSIn, int nBandIn)
    :  // Cannot overflow as nBlockXSize <= 999.
      m_nRecordSize(poDSIn->GetRasterXSize() * 5 + 9 + 2)
{
    poDS = poDSIn;
    nBand = nBandIn;

    eDataType = GDT_Int16;

    nBlockXSize = poDS->GetRasterXSize();
    nBlockYSize = 1;
}

/************************************************************************/
/*                          ~APDATRasterBand()                            */
/************************************************************************/

APDATRasterBand::~APDATRasterBand()
{
    VSIFree(m_pszRecord);
}

/************************************************************************/
/*                             IReadBlock()                             */
/************************************************************************/

CPLErr APDATRasterBand::IReadBlock(int /* nBlockXOff */, int nBlockYOff,
                                  void *pImage)

{
    APDATDataset *poGDS = cpl::down_cast<APDATDataset *>(poDS);

    if (m_pszRecord == nullptr)
    {
        if (m_bBufferAllocFailed)
            return CE_Failure;

        m_pszRecord = static_cast<char *>(VSI_MALLOC_VERBOSE(m_nRecordSize));
        if (m_pszRecord == nullptr)
        {
            m_bBufferAllocFailed = true;
            return CE_Failure;
        }
    }

    CPL_IGNORE_RET_VAL(
        VSIFSeekL(poGDS->m_fp, 1011 + m_nRecordSize * nBlockYOff, SEEK_SET));

    if (VSIFReadL(m_pszRecord, m_nRecordSize, 1, poGDS->m_fp) != 1)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Cannot read scanline %d",
                 nBlockYOff);
        return CE_Failure;
    }

    if (!EQUALN(reinterpret_cast<char *>(poGDS->m_abyFirstBlock), m_pszRecord, 6))
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "APDAT Scanline corrupt.  Perhaps file was not transferred "
                 "in binary mode?");
        return CE_Failure;
    }

    if (APDATGetField(m_pszRecord + 6, 3) != nBlockYOff + 1)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "APDAT scanline out of order, APDAT driver does not "
                 "currently support partial datasets.");
        return CE_Failure;
    }

    for (int i = 0; i < nBlockXSize; i++)
        static_cast<float *>(pImage)[i] =
            APDATGetField(m_pszRecord + 9 + 5 * i, 5) * 0.1f;

    return CE_None;
}

/************************************************************************/
/* ==================================================================== */
/*                              APDATDataset                             */
/* ==================================================================== */
/************************************************************************/

/************************************************************************/
/*                            APDATDataset()                             */
/************************************************************************/

APDATDataset::APDATDataset()
{
    std::fill_n(m_abyFirstBlock, CPL_ARRAYSIZE(m_abyFirstBlock), static_cast<GByte>(0));
    m_oSRS.importFromWkt(SRS_WKT_WGS84_LAT_LONG);

    adfGeoTransform[0] = 0.0;
    adfGeoTransform[1] = 1.0;
    adfGeoTransform[2] = 0.0;
    adfGeoTransform[3] = 0.0;
    adfGeoTransform[4] = 0.0;
    adfGeoTransform[5] = 1.0;

    spacing = 1;
    lat_degrees = 0;
    lon_degrees = 0;
}

/************************************************************************/
/*                           ~APDATDataset()                             */
/************************************************************************/

APDATDataset::~APDATDataset()

{
    FlushCache(true);
    if (m_fp != nullptr)
        CPL_IGNORE_RET_VAL(VSIFCloseL(m_fp));
}

/************************************************************************/
/*                          GetGeoTransform()                           */
/************************************************************************/

CPLErr APDATDataset::GetGeoTransform(double *padfTransform)

{
    memcpy(padfTransform, adfGeoTransform, sizeof(double) * 6);
    return CE_None;
}

/************************************************************************/
/*                          GetSpatialRef()                             */
/************************************************************************/

const OGRSpatialReference *APDATDataset::GetSpatialRef() const

{
    return &m_oSRS;
}

/************************************************************************/
/*                              Identify()                              */
/************************************************************************/

int APDATDataset::Identify(GDALOpenInfo *poOpenInfo)

{
    // we need the first 22 bytes of the header to decide if this may be a
    // terrain file
    if ((poOpenInfo->nHeaderBytes >= 22) || poOpenInfo->TryToIngest(22)) {
        // validate if it's our thing
        const char *psHeader = reinterpret_cast<char *>(poOpenInfo->pabyHeader);
        uint16_t version = (uint16_t)psHeader[18] | ((uint16_t)psHeader[19]);
        if (version != 1) {
            return FALSE;
        }
    } else {
        return FALSE;
    }

    // we need the first block (2048 bytes) to be absolutely sure
    if ((poOpenInfo->nHeaderBytes >= 2048) || poOpenInfo->TryToIngest(2048)) {
        // check CRC etc
        const char *psHeader = reinterpret_cast<char *>(poOpenInfo->pabyHeader);
    } else {
        return FALSE;
    }

    return TRUE;
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

GDALDataset *APDATDataset::Open(GDALOpenInfo *poOpenInfo)

{
    // Confirm that the header is compatible with a APDAT dataset.
    if (!Identify(poOpenInfo))
        return nullptr;

    // Confirm the requested access is supported.
    if (poOpenInfo->eAccess == GA_Update)
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "The APDAT driver does not support update access to existing "
                 "datasets.");
        return nullptr;
    }

    // Check that the file pointer from GDALOpenInfo* is available.
    if (poOpenInfo->fpL == nullptr)
    {
        return nullptr;
    }

    // Create a corresponding GDALDataset.
    auto poDS = std::make_unique<APDATDataset>();

    // Borrow the file pointer from GDALOpenInfo*.
    std::swap(poDS->m_fp, poOpenInfo->fpL);

    // Store the header (we have already checked it is at least HEADER_SIZE
    // byte large).
    memcpy(poDS->m_abyFirstBlock, poOpenInfo->pabyHeader, HEADER_SIZE);
    const char *psHeader = reinterpret_cast<char *>(poOpenInfo->pabyHeader);

    // const char *psHeader = reinterpret_cast<const char *>(poDS->m_abyFirstBlock);
    // poDS->nRasterXSize = APDATGetField(psHeader + 23, 3);
    // poDS->nRasterYSize = APDATGetField(psHeader + 26, 3);
    poDS->nRasterXSize = 1;
    poDS->nRasterYSize = 1;
    if (!GDALCheckDatasetDimensions(poDS->nRasterXSize, poDS->nRasterYSize))
    {
        return nullptr;
    }

    uint16_t spacing = (uint16_t)psHeader[20] | ((uint16_t)psHeader[21]);
    poDS->spacing = spacing;

    // no rotation or translation, uniform spacing in X and Y, Y increases up,
    // offset half a pixel as georeferencing is area-wise
    poDS->adfGeoTransform[0] = -spacing/2; // origin X
    poDS->adfGeoTransform[1] = spacing; // step X
    poDS->adfGeoTransform[2] = 0.0; // skew thing
    poDS->adfGeoTransform[3] = -spacing/2; // origin Y
    poDS->adfGeoTransform[4] = 0.0; // skew thing 2
    poDS->adfGeoTransform[5] = spacing; // step Y

    // like SRTM, accounted for in spacing/2 bias
    poDS->SetMetadataItem(GDALMD_AREA_OR_POINT, GDALMD_AOP_POINT);

    // Create band information objects.
    poDS->SetBand(1, new APDATRasterBand(poDS.get(), 1));

    // Initialize any PAM information.
    poDS->SetDescription(poOpenInfo->pszFilename);
    poDS->TryLoadXML();

    // Check for overviews.
    poDS->oOvManager.Initialize(poDS.get(), poOpenInfo->pszFilename);

    return poDS.release();
}

/************************************************************************/
/*                          GDALRegister_APDAT()                         */
/************************************************************************/

void GDALRegister_APDAT()

{
    if (GDALGetDriverByName("APDAT") != nullptr)
        return;

    GDALDriver *poDriver = new GDALDriver();

    poDriver->SetDescription("APDAT");
    poDriver->SetMetadataItem(GDAL_DCAP_RASTER, "YES");
    poDriver->SetMetadataItem(GDAL_DMD_LONGNAME, "ArduPilot terrain.dat (.dat)");
    // poDriver->SetMetadataItem(GDAL_DMD_HELPTOPIC, "drivers/raster/APDAT.html");
    poDriver->SetMetadataItem(GDAL_DMD_EXTENSION, "dat");
    poDriver->SetMetadataItem(GDAL_DCAP_VIRTUALIO, "YES");

    poDriver->pfnOpen = APDATDataset::Open;
    poDriver->pfnIdentify = APDATDataset::Identify;

    GetGDALDriverManager()->RegisterDriver(poDriver);
}
