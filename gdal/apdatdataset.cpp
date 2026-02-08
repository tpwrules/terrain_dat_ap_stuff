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

// MAVLink sends 4x4 grids
#define TERRAIN_GRID_MAVLINK_SIZE 4

// a 2k grid_block on disk contains 8x7 of the mavlink grids.  Each
// grid block overlaps by one with its neighbour. This ensures that
// the altitude at any point can be calculated from a single grid
// block
#define TERRAIN_GRID_BLOCK_MUL_X 7
#define TERRAIN_GRID_BLOCK_MUL_Y 8

// this is the spacing between 32x28 grid blocks, in grid_spacing units
#define TERRAIN_GRID_BLOCK_SPACING_X ((TERRAIN_GRID_BLOCK_MUL_X-1)*TERRAIN_GRID_MAVLINK_SIZE)
#define TERRAIN_GRID_BLOCK_SPACING_Y ((TERRAIN_GRID_BLOCK_MUL_Y-1)*TERRAIN_GRID_MAVLINK_SIZE)

// giving a total grid size of a disk grid_block of 32x28
#define TERRAIN_GRID_BLOCK_SIZE_X (TERRAIN_GRID_MAVLINK_SIZE*TERRAIN_GRID_BLOCK_MUL_X)
#define TERRAIN_GRID_BLOCK_SIZE_Y (TERRAIN_GRID_MAVLINK_SIZE*TERRAIN_GRID_BLOCK_MUL_Y)

// number of grid_blocks in the LRU memory cache
#ifndef TERRAIN_GRID_BLOCK_CACHE_SIZE
#define TERRAIN_GRID_BLOCK_CACHE_SIZE 12
#endif

// format of grid on disk
#define TERRAIN_GRID_FORMAT_VERSION 1

constexpr int BLOCK_SIZE = 2048;

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
    GByte m_abyFirstBlock[BLOCK_SIZE];
    double adfGeoTransform[6];
    OGRSpatialReference m_oSRS{};

    double spacing;

    // rounded latitude/longitude in degrees. trusted in the file
    int16_t lon_degrees;
    int8_t lat_degrees;

    int blocks_east; // stored first!!
    int blocks_north;

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

  public:
    APDATRasterBand(APDATDataset *, int);
    ~APDATRasterBand();

    virtual CPLErr IReadBlock(int, int, void *) override;
};

/************************************************************************/
/*                           APDATRasterBand()                            */
/************************************************************************/

APDATRasterBand::APDATRasterBand(APDATDataset *poDSIn, int nBandIn)
{
    poDS = poDSIn;
    nBand = nBandIn;

    eDataType = GDT_Int16;

    nBlockXSize = TERRAIN_GRID_BLOCK_SPACING_Y;
    nBlockYSize = TERRAIN_GRID_BLOCK_SPACING_X;
}

/************************************************************************/
/*                          ~APDATRasterBand()                            */
/************************************************************************/

APDATRasterBand::~APDATRasterBand()
{
}

/************************************************************************/
/*                             IReadBlock()                             */
/************************************************************************/

CPLErr APDATRasterBand::IReadBlock(int nBlockXOff, int nBlockYOff,
                                  void *pImage)

{
    APDATDataset *poGDS = cpl::down_cast<APDATDataset *>(poDS);

    size_t pos = BLOCK_SIZE * (nBlockXOff + (poGDS->blocks_east*nBlockYOff));

    CPL_IGNORE_RET_VAL(VSIFSeekL(poGDS->m_fp, pos, SEEK_SET));

    uint16_t buf[BLOCK_SIZE/2];

    if (VSIFReadL(buf, BLOCK_SIZE, 1, poGDS->m_fp) != 1)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "whoops");
        return CE_Failure;
    }

    // should make sure headers etc are okay still

    // if (!EQUALN(reinterpret_cast<char *>(poGDS->m_abyFirstBlock), m_pszRecord, 6))
    // {
    //     CPLError(CE_Failure, CPLE_AppDefined,
    //              "APDAT Scanline corrupt.  Perhaps file was not transferred "
    //              "in binary mode?");
    //     return CE_Failure;
    // }

    // if (APDATGetField(m_pszRecord + 6, 3) != nBlockYOff + 1)
    // {
    //     CPLError(CE_Failure, CPLE_AppDefined,
    //              "APDAT scanline out of order, APDAT driver does not "
    //              "currently support partial datasets.");
    //     return CE_Failure;
    // }

    // for (int i = 0; i < nBlockXSize; i++)
    //     static_cast<float *>(pImage)[i] =
    //         APDATGetField(m_pszRecord + 9 + 5 * i, 5) * 0.1f;

    uint16_t *outp = static_cast<uint16_t *>(pImage);
    for (int y=0; y<TERRAIN_GRID_BLOCK_SPACING_X; y++) {
        for (int x=0; x<TERRAIN_GRID_BLOCK_SPACING_Y; x++) {
            // 11 is header words
            int p = 11 + (x + (y*TERRAIN_GRID_BLOCK_SIZE_Y));
            // the world is little endian :)
            *outp++ = buf[p];
        }
    }

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

    blocks_east = 0;
    blocks_north = 0;
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

const float LOCATION_SCALING_FACTOR = 0.011131884502145034;
const float LOCATION_SCALING_FACTOR_INV = 89.83204953368922;

static double longitude_scale(double lat_deg) {
    double scale = cos(lat_deg*3.14159265358979323/180.);
    return (scale >= 0.01) ? scale : 0.01;
}

static int32_t diff_longitude_E7(int32_t lon1, int32_t lon2) {
    int64_t product = (int64_t)lon1 * (int64_t)lon2;
    if (product >= 0) {
        return lon1 - lon2;
    }

    int64_t dlon = lon1 - lon2;
    if (dlon > 1800000000) {
        dlon -= 3600000000;
    } else if (dlon < -1800000000) {
        dlon += 3600000000;
    }
    return (int32_t)dlon;
}

static void get_distance_NE_e7(int32_t lat1, int32_t lon1,
        int32_t lat2, int32_t lon2, double& dnorth, double& deast) {
    int32_t dlatv = lat2 - lat1;
    double dlonv = diff_longitude_E7(lon2, lon1);
    dlonv *= longitude_scale((lat1*0.5+lat2*0.5)*1e-7);

    dnorth = ((double)dlatv * LOCATION_SCALING_FACTOR);
    deast = (dlonv * LOCATION_SCALING_FACTOR);
}

static void add_offset(int32_t lat_e7, int32_t lon_e7, double ofs_north,
        double ofs_east, int32_t& lat_ret, int32_t& lon_ret) {
    double dlat = ofs_north * LOCATION_SCALING_FACTOR_INV;
    double scale = longitude_scale((lat_e7+dlat)*0.5e-7);
    double dlon = ofs_east * LOCATION_SCALING_FACTOR_INV / scale;

    lat_ret = (int32_t)(lat_e7+dlat);
    lon_ret = (int32_t)(lon_e7+dlon);
}

static int east_blocks(int lat_degrees, int lon_degrees, int spacing) {
    int32_t lat_e7 = lat_degrees*10000000;
    int32_t lon_e7 = lon_degrees*10000000;

    // shift another two blocks east to ensure room is available
    int32_t lat2_e7 = lat_e7;
    int32_t lon2_e7 = lon_e7 + 10000000;
    add_offset(lat2_e7, lon2_e7, 0, 2*spacing*TERRAIN_GRID_BLOCK_SIZE_Y,
        lat2_e7, lon2_e7);
    double dnorth, deast;
    get_distance_NE_e7(lat_e7, lon_e7, lat2_e7, lon2_e7, dnorth, deast);

    return (int)(deast / (spacing * TERRAIN_GRID_BLOCK_SPACING_Y));
}

// CUSTOM FUNC
static int north_blocks(int lat_degrees, int lon_degrees, int spacing) {
    int32_t lat_e7 = lat_degrees*10000000;
    int32_t lon_e7 = lon_degrees*10000000;

    // figure out amount of meters north
    double dnorth, deast;
    get_distance_NE_e7(lat_e7, lon_e7, lat_e7+10000000, lon_e7, dnorth, deast);

    // return rounded up blocks i guess lol. not sure if accurate in all cases,
    // the native code just hopes blocks are there and/or loops until the single
    // latitude is exceeded
    double blox = dnorth / (spacing * TERRAIN_GRID_BLOCK_SPACING_X);

    return (int)ceil(blox);
}

int APDATDataset::Identify(GDALOpenInfo *poOpenInfo)

{
    // we need the first 22 bytes of the header to decide if this may be a
    // terrain file
    if ((poOpenInfo->nHeaderBytes >= 22) || poOpenInfo->TryToIngest(22)) {
        // validate if it's our thing
        const char *psHeader = reinterpret_cast<char *>(poOpenInfo->pabyHeader);
        uint16_t version = (uint16_t)psHeader[18] | ((uint16_t)psHeader[19] << 8);
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

    // Store the header (we have already checked it is at least BLOCK_SIZE
    // byte large).
    memcpy(poDS->m_abyFirstBlock, poOpenInfo->pabyHeader, BLOCK_SIZE);
    const char *psHeader = reinterpret_cast<char *>(poOpenInfo->pabyHeader);

    uint16_t spacing = (uint16_t)psHeader[20] | ((uint16_t)psHeader[21] << 8);
    poDS->spacing = spacing;

    int16_t lon_degrees = (int16_t)((uint16_t)psHeader[1818] | 
                        ((uint16_t)psHeader[1819] << 8));
    poDS->lon_degrees = lon_degrees;

    int8_t lat_degrees = (int8_t)psHeader[1820];
    poDS->lat_degrees = lat_degrees;

    poDS->blocks_east = east_blocks(lat_degrees, lon_degrees, spacing);
    poDS->blocks_north = north_blocks(lat_degrees, lon_degrees, spacing);

    // may want to add 4 to get the last edge
    poDS->nRasterXSize = poDS->blocks_east * TERRAIN_GRID_BLOCK_SPACING_Y;
    poDS->nRasterYSize = poDS->blocks_north * TERRAIN_GRID_BLOCK_SPACING_X;
    if (!GDALCheckDatasetDimensions(poDS->nRasterXSize, poDS->nRasterYSize))
    {
        return nullptr;
    }

    // set up projection, each tile has its own! hardcoded earth radius
    // (from LOCATION_SCALING_FACTOR) and custom projection, starting with 0, 0
    // at this tile
    char proj4string[512];
    snprintf(proj4string, sizeof(proj4string),
        "+proj=ardusinu +R=6378100 +lat_0=%d +lon_0=%d",
        lat_degrees, lon_degrees);
    poDS->m_oSRS.importFromProj4(proj4string);

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
