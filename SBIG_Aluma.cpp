//
//  SBIG_Aluma.cpp
//
//  Created by Rodolphe Pineau on 2021/04/22
//  Copyright © 2021 RTI-Zone. All rights reserved.
//

#include "SBIG_Aluma.h"


CSBigAluma::CSBigAluma()
{
    m_bConnected = false;

    m_pGateway = nullptr;
    m_pCamera = nullptr;

    m_tcameraList.clear();
    //
    m_bAbort = false;
    m_nCurrentXBin = 1;
    m_nCurrentYBin = 1;
    m_dCaptureLenght = 0;
    m_bCapturerunning = false;
    m_pSleeper = nullptr;
    m_nNbBin = 1;
    m_SupportedBinsX[0] = 1;
    m_SupportedBinsY[0] = 1;

    m_dSetPoint = -15.0;

    m_nROILeft = 0;
    m_nROITop = 0;
    m_nROIWidth = 0;
    m_nROIHeight = 0;

    m_nNbGainValue = 0;

    m_nGain = 0;
    m_nExposureMs = (1 * 1000000);


#ifdef PLUGIN_DEBUG
#if defined(SB_WIN_BUILD)
    m_sLogfilePath = getenv("HOMEDRIVE");
    m_sLogfilePath += getenv("HOMEPATH");
    m_sLogfilePath += "\\SBIGAlumaLog.txt";
#elif defined(SB_LINUX_BUILD)
    m_sLogfilePath = getenv("HOME");
    m_sLogfilePath += "/SBIGAlumaLog.txt";
#elif defined(SB_MAC_BUILD)
    m_sLogfilePath = getenv("HOME");
    m_sLogfilePath += "/SBIGAlumaLog.txt";
#endif
    m_sLogFile.open(m_sLogfilePath, std::ios::out |std::ios::trunc);
#endif

#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
    m_sLogFile << "["<<getTimeStamp()<<"]"<< " [CSBigAluma] Version " << std::fixed << std::setprecision(2) << PLUGIN_VERSION << std::endl;
    m_sLogFile << "["<<getTimeStamp()<<"]"<< " [CSBigAluma] Constructor Called." << std::endl;
    m_sLogFile.flush();
#endif

    m_pGateway = getGateway();
    listCamera(m_tcameraList);
}

CSBigAluma::~CSBigAluma()
{
    if(m_pGateway)
        deleteGateway(m_pGateway);
#ifdef    PLUGIN_DEBUG
    // Close LogFile
    if(m_sLogFile.is_open())
        m_sLogFile.close();
#endif
}

void CSBigAluma::handlePromise(IPromisePtr pPromise)
{
    IPromise::Status result = pPromise->wait();
    if (result != IPromise::Complete)
    {
        char buf[512] = {0};
        size_t lng = 512; // this variable will be resized to the length of the error message
        pPromise->getLastError(&(buf[0]), lng);
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
        m_sLogFile << "["<<getTimeStamp()<<"]"<< " [handlePromise] Error : " << buf << std::endl;
        m_sLogFile.flush();
#endif
        pPromise->release();
        throw std::logic_error(std::string(&(buf[0]), lng));
    }
    pPromise->release();
}

void CSBigAluma::setCameraConnection(dl::ICameraPtr pCamera, bool enable)
{
    // See if we're running a v2 compliant interface
    using IConMgrPtr = dl::V2::IConnectionManagerPtr;
    auto pConMgr = dynamic_cast<IConMgrPtr>(pCamera);
    if (!pConMgr) return;
    pConMgr->setConnection(enable);
}


int CSBigAluma::Connect(int nCameraID)
{
    int nErr = PLUGIN_OK;
    int i = 0;
    ISensorPtr pMainSensor = nullptr;
    ISensorPtr pSecondarySensor = nullptr;

    if(m_bConnected)
        return nErr;


    if(nCameraID>=0) {
        getCameraDataFromID(nCameraID, &m_Camera);
    }
    else {
        // check if there is at least one camera connected to the system
        if(m_tcameraList.size() >= 1) {
            getCameraDataFromID(m_tcameraList.at(0).cameraId, &m_Camera);
        }
        else
            return ERR_NODEVICESELECTED;
    }

#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
    m_sLogFile << "["<<getTimeStamp()<<"]"<< " [Connect] connecting to camera ID : " << m_Camera.cameraId << " serial " << m_Camera.Sn << std::endl;
    m_sLogFile.flush();
#endif

    m_pCamera =  m_pGateway->getUSBCamera(m_Camera.cameraId);

    if (m_pCamera == nullptr) {
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
        m_sLogFile << "["<<getTimeStamp()<<"]"<< " [Connect] Error connecting to camera ID : " << m_Camera.cameraId << " serial " << m_Camera.Sn << std::endl;
        m_sLogFile.flush();
#endif
        return ERR_NORESPONSE;
        }

    m_pCamera->initialize();
    setCameraConnection(m_pCamera, true);
    m_bConnected = true;

#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
    m_sLogFile << "["<<getTimeStamp()<<"]"<< " [Connect] Connected to camera ID : " << m_Camera.cameraId << " serial " << m_Camera.Sn << std::endl;
    m_sLogFile.flush();
#endif

    // get camera properties
    pMainSensor = m_pCamera->getSensor(0);
    if(m_Camera.nbSensors>1)
        pSecondarySensor = m_pCamera->getSensor(1);

    m_mainSensorInfo = pMainSensor->getInfo();
    if(m_mainSensorInfo.filterType == Color || m_mainSensorInfo.filterType == SparseColor)
        m_bIsColorCam = true;
    else
        m_bIsColorCam = false;

    // normal case .. 1x1, 2x2, 3x3 ... let's not do weird 1x1, 1x2, .. 1xN, 2x2, 2x3, .., 2xN .. for now
    m_nNbBin = m_mainSensorInfo.maxBinX;
    for(i=0; i<m_mainSensorInfo.maxBinX; i++) {
        m_SupportedBinsX[i] = i+1;
        m_SupportedBinsY[i] = i+1;
    }
    m_nCurrentXBin = 1; // should be 1x1 by default
    m_nCurrentYBin = 1; // should be 1x1 by default
    // m_nMaxBitDepth = m_mainSensorInfo.???;


#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
    m_sLogFile << "["<<getTimeStamp()<<"]"<< " [Connect] Camera properties :"  << std::endl;
    m_sLogFile << "["<<getTimeStamp()<<"]"<< " [Connect] m_mainSensorInfo.pixelsX    : " << m_mainSensorInfo.pixelsX << std::endl;
    m_sLogFile << "["<<getTimeStamp()<<"]"<< " [Connect] m_mainSensorInfo.pixelsY    : " << m_mainSensorInfo.pixelsY << std::endl;
    m_sLogFile << "["<<getTimeStamp()<<"]"<< " [Connect] m_mainSensorInfo.maxBinX    : " << m_mainSensorInfo.maxBinX << std::endl;
    m_sLogFile << "["<<getTimeStamp()<<"]"<< " [Connect] m_mainSensorInfo.maxBinY    : " << m_mainSensorInfo.maxBinY << std::endl;
    m_sLogFile << "["<<getTimeStamp()<<"]"<< " [Connect] m_bIsColorCam               : " << (m_bIsColorCam?"Yes":"No") << std::endl;
    m_sLogFile << "["<<getTimeStamp()<<"]"<< " [Connect] m_mainSensorInfo.pixelSizeX : " << std::fixed << std::setprecision(2) << m_mainSensorInfo.pixelSizeX << std::endl;
    m_sLogFile << "["<<getTimeStamp()<<"]"<< " [Connect] m_mainSensorInfo.pixelSizeY : " << std::fixed << std::setprecision(2) << m_mainSensorInfo.pixelSizeY << std::endl;
    m_sLogFile << "["<<getTimeStamp()<<"]"<< " [Connect] m_nNbBin                    : " << m_nNbBin<< std::endl;
    m_sLogFile.flush();
#endif

    if(pSecondarySensor) {
        m_secondarySensorInfo = pSecondarySensor->getInfo();
        if(m_secondarySensorInfo.filterType == Color || m_secondarySensorInfo.filterType == SparseColor)
            m_bSecondaryIsColorCam = true;
        else
            m_bSecondaryIsColorCam = false;

        // normal case .. 1x1, 2x2, 3x3 ... let's not do weird 1x1, 1x2, ... 1xN, 2x1, 2x2, ..., 2xN ... for now
        m_nSecondaryNbBin = m_secondarySensorInfo.maxBinX;
        for(i=0; i<m_secondarySensorInfo.maxBinX; i++) {
            m_SecondarySupportedBinsX[i] = i+1;
            m_SecondarySupportedBinsY[i] = i+1;
        }
        m_nSecondaryCurrentXBin = 1; // should be 1x1 by default
        m_nSecondaryCurrentYBin = 1; // should be 1x1 by default
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
        m_sLogFile << "["<<getTimeStamp()<<"]"<< " [Connect] Secondary sensor properties :"  << std::endl;
        m_sLogFile << "["<<getTimeStamp()<<"]"<< " [Connect] m_secondarySensorInfo.pixelsX    : " << m_secondarySensorInfo.pixelsX << std::endl;
        m_sLogFile << "["<<getTimeStamp()<<"]"<< " [Connect] m_secondarySensorInfo.pixelsY    : " << m_secondarySensorInfo.pixelsY << std::endl;
        m_sLogFile << "["<<getTimeStamp()<<"]"<< " [Connect] m_secondarySensorInfo.maxBinX    : " << m_secondarySensorInfo.maxBinX << std::endl;
        m_sLogFile << "["<<getTimeStamp()<<"]"<< " [Connect] m_secondarySensorInfo.maxBinY    : " << m_secondarySensorInfo.maxBinY << std::endl;
        m_sLogFile << "["<<getTimeStamp()<<"]"<< " [Connect] m_bSecondaryIsColorCam           : " << (m_bSecondaryIsColorCam?"Yes":"No") << std::endl;
        m_sLogFile << "["<<getTimeStamp()<<"]"<< " [Connect] m_secondarySensorInfo.pixelSizeX : " << std::fixed << std::setprecision(2) << m_secondarySensorInfo.pixelSizeX << std::endl;
        m_sLogFile << "["<<getTimeStamp()<<"]"<< " [Connect] m_secondarySensorInfo.pixelSizeY : " << std::fixed << std::setprecision(2) << m_secondarySensorInfo.pixelSizeY << std::endl;
        m_sLogFile << "["<<getTimeStamp()<<"]"<< " [Connect] m_nSecondaryNbBin                : " << m_nSecondaryNbBin<< std::endl;
        m_sLogFile.flush();
#endif
    }

    try
    {
        handlePromise(m_pCamera->queryCapability(ICamera::eSupportsCooler));
        m_bSupportsCooler = m_pCamera->getCapability(ICamera::eSupportsCooler);
    }
    catch (std::exception &ex) {
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
        m_sLogFile << "["<<getTimeStamp()<<"]"<< " [Connect] Exeception = " << ex.what() << std::endl;
        m_sLogFile.flush();
#endif
        m_bSupportsCooler = false;
    }

    return nErr;
}

void CSBigAluma::Disconnect()
{
    setCoolerTemperature(false, m_dSetPoint);
    m_bConnected = false;
    m_pCamera = nullptr;
}

void CSBigAluma::setCameraId(int nCameraId)
{
    m_Camera.cameraId = nCameraId;
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
    m_sLogFile << "["<<getTimeStamp()<<"]"<< " [setCameraId] nCameraId : " << nCameraId << std::endl;
    m_sLogFile.flush();
#endif
}

void CSBigAluma::getCameraId(int &nCameraId)
{
    nCameraId =  m_Camera.cameraId;
}

void CSBigAluma::getCameraIdFromSerial(int nSerial, int &nCameraId)
{
    int i=0;
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
    m_sLogFile << "["<<getTimeStamp()<<"]"<< " [getCameraIdFromSerial] nSerial : " << nSerial << std::endl;
    m_sLogFile.flush();
#endif
    nCameraId = -1;

    for(i=0; i < m_tcameraList.size(); i++) {
        if(m_tcameraList.at(i).Sn == nSerial) {
            nCameraId = m_tcameraList.at(i).cameraId;
            break;
        }
    }
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
    m_sLogFile << "["<<getTimeStamp()<<"]"<< " [getCameraIdFromSerial] nCameraId : " << nCameraId << std::endl;
    m_sLogFile.flush();
#endif

}

void CSBigAluma::getCameraSerialFromID(int nCameraId, int &nSerial)
{
    int i=0;
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
    m_sLogFile << "["<<getTimeStamp()<<"]"<< " [getCameraSerialFromID] nCameraId : " << nCameraId << std::endl;
    m_sLogFile.flush();
#endif
    nSerial = -1;

    for(i=0; i < m_tcameraList.size(); i++) {
        if(m_tcameraList.at(i).cameraId == nCameraId) {
            nSerial = m_tcameraList.at(i).Sn;
            break;
        }
    }
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
    m_sLogFile << "["<<getTimeStamp()<<"]"<< " [getCameraSerialFromID] nSerial : " << nSerial << std::endl;
    m_sLogFile.flush();
#endif

}

void CSBigAluma::getCameraNameFromID(int nCameraId, std::string &sName)
{

    int i=0;
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
    m_sLogFile << "["<<getTimeStamp()<<"]"<< " [getCameraNameFromID] nCameraId : " << nCameraId << std::endl;
    m_sLogFile.flush();
#endif
    sName.clear();

    for(i=0; i < m_tcameraList.size(); i++) {
        if(m_tcameraList.at(i).cameraId == nCameraId) {
            sName.assign(m_tcameraList.at(i).sModel);
            break;
        }
    }
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
    m_sLogFile << "["<<getTimeStamp()<<"]"<< " [getCameraNameFromID] sName : " << sName << std::endl;
    m_sLogFile.flush();
#endif
}


void CSBigAluma::getCameraSerial(int &nSerial)
{
    nSerial = m_Camera.Sn;
}

void CSBigAluma::getCameraName(std::string &sName)
{
    sName.assign(m_Camera.sModel);
}

void CSBigAluma::getCameraDataFromID(int nCameraID, camera_info_t *p_tCamera)
{
    int i = 0;
    memset(p_tCamera, 0, sizeof(camera_info_t));

    for(i = 0; i<m_tcameraList.size(); i++) {
        if(m_tcameraList.at(i).cameraId == nCameraID) {
            memcpy(p_tCamera, &m_tcameraList.at(i), sizeof(camera_info_t));
            return;
        }
    }

}

int CSBigAluma::listCamera(std::vector<camera_info_t>  &cameraList)
{
    int nErr = PLUGIN_OK;
    ICameraPtr pCamera;
    camera_info_t   tCameraInfo;
    ICamera::Info cameraInfo;
    int cameraNum;
    size_t bufferLen = BUFFER_LEN;
    char buffer[BUFFER_LEN];

    cameraList.clear();

    if(m_tcameraList.size()>0) {
        cameraList = m_tcameraList;
        return nErr;
    }

    m_pGateway->queryUSBCameras();
    // list camera connected to the system
    cameraNum = (int)m_pGateway->getUSBCameraCount();
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
    m_sLogFile << "["<<getTimeStamp()<<"]"<< " [listCamera] cameraNum : " << cameraNum << std::endl;
    m_sLogFile.flush();
#endif
    if(cameraNum <= 0) {
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
        m_sLogFile << "["<<getTimeStamp()<<"]"<< " [listCamera] No camera detected" << std::endl;
        m_sLogFile.flush();
#endif
        return ERR_NODEVICESELECTED;
    }

    for (int i = 0; i < cameraNum; i++)
    {
        try {
            pCamera = m_pGateway->getUSBCamera(i);
            pCamera->initialize();
            try {
                handlePromise(pCamera->queryInfo());
            }
            catch (std::exception &ex) {
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
                m_sLogFile << "["<<getTimeStamp()<<"]"<< " [listCamera] Exeception = " << ex.what() << std::endl;
                m_sLogFile.flush();
#endif
                continue; // move on to next one.
            }
            cameraInfo = pCamera->getInfo();
            pCamera->getSerial(buffer, bufferLen);
            tCameraInfo.cameraId = i;
            tCameraInfo.Sn = cameraInfo.serialNumber;
            tCameraInfo.nbSensors = cameraInfo.numberOfSensors;
            tCameraInfo.sModel.assign(buffer);
            tCameraInfo.nFirmwareVersion = cameraInfo.firmwareRevision;
            tCameraInfo.nWiFiFirmwareVersion = cameraInfo.wifiFirmwareRevision;
            cameraList.push_back(tCameraInfo);
        } catch(std::exception &ex) {
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
            m_sLogFile << "["<<getTimeStamp()<<"]"<< " [listCamera] Error getting camera data : " << ex.what() << std::endl;
            m_sLogFile.flush();
#endif
            return ERR_CMDFAILED;
        }

        setCameraConnection(pCamera, false);

#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
        m_sLogFile << "["<<getTimeStamp()<<"]"<< " [listCamera] Camera Model     : " << tCameraInfo.sModel << std::endl;
        m_sLogFile << "["<<getTimeStamp()<<"]"<< " [listCamera] SN               : " << tCameraInfo.Sn << std::endl;
        m_sLogFile << "["<<getTimeStamp()<<"]"<< " [listCamera] number of sensor : " << tCameraInfo.nbSensors << std::endl;
        m_sLogFile << "["<<getTimeStamp()<<"]"<< " [listCamera] Device ID        : " << i << std::endl;
        m_sLogFile.flush();
#endif
    }

    return nErr;
}

int CSBigAluma::getNumBins(int nSensorID)
{
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
    m_sLogFile << "["<<getTimeStamp()<<"]"<< " [getNumBins]" << std::endl;
    m_sLogFile.flush();
#endif
    switch (nSensorID) {
        case 0:
            return m_nNbBin;
            break;

        case 1:
            return m_nSecondaryNbBin;
            break;

        default:
            break;
    }
    return m_nNbBin;
}

int CSBigAluma::getBinXFromIndex(int nSensorID, int nIndex)
{
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
    m_sLogFile << "["<<getTimeStamp()<<"]"<< " [getBinXFromIndex] nIndex = " << nIndex << std::endl;
    m_sLogFile.flush();
#endif
    switch (nSensorID) {
        case 0:
            if(nIndex>(m_nNbBin-1))
                return 1;
            return m_SupportedBinsX[nIndex];
            break;

        case 1:
            if(nIndex>(m_nSecondaryNbBin-1))
                return 1;
            return m_SecondarySupportedBinsX[nIndex];
            break;

        default:
            break;
    }

    return 1;  // better than a bad bin
}

int CSBigAluma::getBinYFromIndex(int nSensorID, int nIndex)
{
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
    m_sLogFile << "["<<getTimeStamp()<<"]"<< " [getBinYFromIndex] nIndex = " << nIndex << std::endl;
    m_sLogFile.flush();
#endif
    switch (nSensorID) {
        case 0:
            if(nIndex>(m_nNbBin-1))
                return 1;
            return m_SupportedBinsY[nIndex];
            break;

        case 1:
            if(nIndex>(m_nSecondaryNbBin-1))
                return 1;
            return m_SecondarySupportedBinsY[nIndex];
            break;

        default:
            break;
    }

    return 1;  // better than a bad bin
}

bool CSBigAluma::isShutterPresent()
{
    bool bShutterPresent;
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
    m_sLogFile << "["<<getTimeStamp()<<"]"<< " [isShutterPresent]" << std::endl;
    m_sLogFile.flush();
#endif

    try{
        handlePromise(m_pCamera->queryCapability(ICamera::eSupportsShutter));
        bShutterPresent = m_pCamera->getCapability(ICamera::eSupportsShutter);
    }
    catch (std::exception &ex) {
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
        m_sLogFile << "["<<getTimeStamp()<<"]"<< " [isShutterPresent] Exeception = " << ex.what() << std::endl;
        m_sLogFile.flush();
#endif
        bShutterPresent = false;
    }

#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
    m_sLogFile << "["<<getTimeStamp()<<"]"<< " [isShutterPresent] bShutterPresent = " << (bShutterPresent?"Yes":"No" )<< std::endl;
    m_sLogFile.flush();
#endif
    return bShutterPresent;
}


int CSBigAluma::startCaputure(int nSensorID, double dTime, bool bIsLightFrame, int nReadoutMode, bool bUseRBIFlash)
{
    int nErr = PLUGIN_OK;
    m_bAbort = false;
    ISensorPtr pSensor = nullptr;

#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
    m_sLogFile << "["<<getTimeStamp()<<"]"<< " [startCaputure] nSensorID = " << nSensorID << std::endl;
    m_sLogFile << "["<<getTimeStamp()<<"]"<< " [startCaputure] dTime = " << std::fixed << std::setprecision(2) << dTime << std::endl;
    m_sLogFile << "["<<getTimeStamp()<<"]"<< " [startCaputure] bIsLightFrame = " << (bIsLightFrame?"Yes":"No") << std::endl;
    m_sLogFile << "["<<getTimeStamp()<<"]"<< " [startCaputure] nReadoutMode = " << nReadoutMode << std::endl;
    m_sLogFile << "["<<getTimeStamp()<<"]"<< " [startCaputure] bUseRBIFlash = " << (bUseRBIFlash?"Yes":"No") << std::endl;
    m_sLogFile.flush();
#endif

    TExposureOptions options;
    try{
        options.duration = dTime;
        if(nSensorID == MAIN_SENSOR) {
            options.binX = m_nCurrentXBin;
            options.binY = m_nCurrentYBin;
        }
        else if (nSensorID == GUIDER) {
            options.binX = m_nSecondaryCurrentXBin;
            options.binY = m_nSecondaryCurrentYBin;
        }
        else {
            options.binX = 1;
            options.binY = 1;
        }
        options.readoutMode = nReadoutMode; // See: ISensor::getReadoutModes()
        options.isLightFrame = bIsLightFrame;
        options.useRBIPreflash = bUseRBIFlash;
        options.useExtTrigger = false;
        pSensor = m_pCamera->getSensor(nSensorID);
        handlePromise(pSensor->startExposure(options));
    }
    catch (std::exception &ex) {
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
        m_sLogFile << "["<<getTimeStamp()<<"]"<< " [startCaputure] Exeception = " << ex.what() << std::endl;
        m_sLogFile.flush();
#endif
    }
    m_dCaptureLenght = dTime;
    if(nSensorID == MAIN_SENSOR)
        m_ExposureTimer.Reset();
    else
        m_ExposureTimerGuider.Reset();
    return nErr;
}

void CSBigAluma::abortCapture(int nSensorID)
{
    ISensorPtr pSensor = nullptr;

    m_bAbort = true;
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
    m_sLogFile << "["<<getTimeStamp()<<"]"<< " [abortCapture]" << std::endl;
    m_sLogFile.flush();
#endif
    try {
        pSensor = m_pCamera->getSensor(nSensorID);
        handlePromise(pSensor->abortExposure());
    }
    catch (std::exception &ex) {
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
        m_sLogFile << "["<<getTimeStamp()<<"]"<< " [abortCapture] Exeception = " << ex.what() << std::endl;
        m_sLogFile.flush();
#endif
        return;
    }
}

int CSBigAluma::setROI(int nSensorID, int nLeft, int nTop, int nWidth, int nHeight)
{
    int nErr = PLUGIN_OK;
    ISensor::Info info;
    TSubframe subf;
    ISensorPtr pSensor = nullptr;

#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
    m_sLogFile << "["<<getTimeStamp()<<"]"<< " [setROI] Sensor ID = " << nSensorID << std::endl;
    m_sLogFile << "["<<getTimeStamp()<<"]"<< " [setROI] nLeft     = " << nLeft << std::endl;
    m_sLogFile << "["<<getTimeStamp()<<"]"<< " [setROI] nTop      = " << nTop << std::endl;
    m_sLogFile << "["<<getTimeStamp()<<"]"<< " [setROI] nWidth    = " << nWidth << std::endl;
    m_sLogFile << "["<<getTimeStamp()<<"]"<< " [setROI] nHeight   = " << nHeight << std::endl;
    m_sLogFile.flush();
#endif

    try {
        pSensor = m_pCamera->getSensor(nSensorID);
        handlePromise(pSensor->queryInfo());
        info = pSensor->getInfo();
        subf.top    = nTop;
        subf.left   = nLeft;
        subf.width  = nWidth;
        subf.height = nHeight;
        subf.binX   = m_nCurrentXBin;
        subf.binY   = m_nCurrentYBin;
        handlePromise(pSensor->setSubframe(subf));
    }
    catch (std::exception &ex) {
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
        m_sLogFile << "["<<getTimeStamp()<<"]"<< " [setROI] Exception = " << ex.what() << std::endl;
        m_sLogFile.flush();
#endif
        return ERR_CMDFAILED;
    }

    return nErr;
}


bool CSBigAluma::isFameAvailable(int nSensorID)
{
    bool bFrameAvailable = false;

#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
    m_sLogFile << "["<<getTimeStamp()<<"]"<< " [isFameAvailable] nSensorID " << nSensorID << std::endl;
    m_sLogFile.flush();
#endif

    if(nSensorID == MAIN_SENSOR) {
        if(m_ExposureTimer.GetElapsedSeconds() < m_dCaptureLenght)
            return bFrameAvailable;
    }
    else {
        if(m_ExposureTimerGuider.GetElapsedSeconds() < m_dCaptureLenght)
            return bFrameAvailable;
    }
    try {
        handlePromise(m_pCamera->queryStatus());
        m_CameraStatus = m_pCamera->getStatus();
        if(nSensorID == MAIN_SENSOR)
            bFrameAvailable = (m_CameraStatus.mainSensorState == ISensor::ReadyToDownload);
        else
            bFrameAvailable = (m_CameraStatus.extSensorState == ISensor::ReadyToDownload);
    }
    catch (std::exception &ex) {
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
        m_sLogFile << "["<<getTimeStamp()<<"]"<< " [isFameAvailable] Exception = " << ex.what() << std::endl;
        m_sLogFile.flush();
#endif
        return false;
    }

#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
    m_sLogFile << "["<<getTimeStamp()<<"]"<< " [isFameAvailable] bFrameAvailable : " << (bFrameAvailable?"Yes":"No") << std::endl;
    m_sLogFile.flush();
#endif

    return bFrameAvailable;
}

uint32_t CSBigAluma::getBitDepth(int nSensorID)
{
    return 16; // m_nMaxBitDepth;
}


bool CSBigAluma::isXferComplete(IPromise * pPromise)
{
    // Get the status of the promise
    auto status = pPromise->getStatus();
    if (status == IPromise::Complete)
    {
        // The image is ready for retrieval.
        pPromise->release();
        return true;
    }
    else if (status == IPromise::Error)
    {
        // If an error occurred, report it to the user.
        char buf[512] = {0};
        size_t blng = 512;
        pPromise->getLastError(&(buf[0]), blng);
        pPromise->release();
        throw std::logic_error(&(buf[0]));
    }

    // Otherwise, we wait.
    return false;
}

int CSBigAluma::downloadFrame(int nSensorID)
{
    int nErr = PLUGIN_OK;
    int timeout = 0;
    IPromisePtr pImgPromise;
    ISensorPtr pSensor = nullptr;

    try {
        // Start the download & wait for transfer complete
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
        m_sLogFile << "["<<getTimeStamp()<<"]"<< " [downloadFrame] Start download" << std::endl;
        m_sLogFile.flush();
#endif
        pSensor = m_pCamera->getSensor(nSensorID);
        pImgPromise = pSensor->startDownload();

        timeout = 0;
        while (!isXferComplete(pImgPromise)) {
            if(timeout > RX_TIMEOUT) {// 10 second timeout ? should be enough for testing.
                pImgPromise->release();
                return ERR_RXTIMEOUT;
            }
            m_pSleeper->sleep(RX_WAIT);
            timeout +=RX_WAIT;
        }

#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
        m_sLogFile << "["<<getTimeStamp()<<"]"<< " [downloadFrame] Download completed" << std::endl;
        m_sLogFile.flush();
#endif
    }
    catch (std::exception &ex) {
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
        m_sLogFile << "["<<getTimeStamp()<<"]"<< " [downloadFrame] exception : " << ex.what() << std::endl;
        m_sLogFile.flush();
#endif
        return ERR_CMDFAILED;
    }
    return nErr;
}

int CSBigAluma::getFrame(int nSensorID, int nHeight, int nMemWidth, unsigned char* frameBuffer)
{
    int nErr = PLUGIN_OK;
    ISensorPtr pSensor = nullptr;
    IImagePtr pImage = nullptr;
    size_t bufLng;
    unsigned short *pImgBuf;

    if(!frameBuffer)
        return ERR_POINTER;

#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
    m_sLogFile << "["<<getTimeStamp()<<"]"<< " [getFrame] nHeight         : " << nHeight << std::endl;
    m_sLogFile << "["<<getTimeStamp()<<"]"<< " [getFrame] nMemWidth       : " << nMemWidth << std::endl;
    m_sLogFile << "["<<getTimeStamp()<<"]"<< " [getFrame] getBitDepth()/8 : " << (getBitDepth(nSensorID)/8) << std::endl;
    m_sLogFile.flush();
#endif
    try {
        pSensor = m_pCamera->getSensor(nSensorID);
        pImage = pSensor->getImage();
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
        m_sLogFile << "["<<getTimeStamp()<<"]"<< " [getFrame] got image, copying to TSX buffer" << std::endl;
        m_sLogFile.flush();
#endif
        pImgBuf = pImage->getBufferData();
        bufLng  = pImage->getBufferLength() * sizeof(unsigned short); // getBufferLength returns lenght in pixel
        if(bufLng > nHeight*nMemWidth)
            bufLng = nHeight*nMemWidth;
        memmove(frameBuffer, pImgBuf, nHeight*nMemWidth);

    }
    catch (std::exception &ex) {
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
        m_sLogFile << "["<<getTimeStamp()<<"]"<< " [getFrame] exception : " << ex.what() << std::endl;
        m_sLogFile.flush();
#endif
        return ERR_CMDFAILED;
    }
    return nErr;
}

int CSBigAluma::getTemperture(double &dTemp, double &dPower, double &dSetPoint, bool &bEnabled)
{
    int nErr = PLUGIN_OK;
    ITECPtr pTec;

#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
    m_sLogFile << "["<<getTimeStamp()<<"]"<< " [getTemperture] " << std::endl;
    m_sLogFile.flush();
#endif

    if(!m_bSupportsCooler) {
        dTemp = -100.0;
        bEnabled = false;
        return nErr;
    }
    try {
        handlePromise(m_pCamera->queryStatus());
        m_CameraStatus = m_pCamera->getStatus();
    }
    catch (std::exception &ex) {
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
        m_sLogFile << "["<<getTimeStamp()<<"]"<< " [getTemperture] Exception = " << ex.what() << std::endl;
        m_sLogFile.flush();
#endif
        dTemp = -100.0;
        bEnabled = false;
        return nErr;
    }

    dTemp = m_CameraStatus.sensorTemperature;
    dPower = m_CameraStatus.coolerPower;
    dSetPoint = m_dSetPoint;
    pTec = m_pCamera->getTEC();
    bEnabled = pTec->getEnabled();
    return nErr;
}

int CSBigAluma::setCoolerTemperature(bool bOn, double dTemp)
{
    int nErr = PLUGIN_OK;
    int nMin, nMax;
    ISensorPtr pSensor = nullptr;

#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
    m_sLogFile << "["<<getTimeStamp()<<"]"<< " [setCoolerTemperature]" << std::endl;
    m_sLogFile.flush();
#endif

    if(!m_bSupportsCooler) {
        return nErr;
    }

    pSensor = m_pCamera->getSensor(MAIN_SENSOR);
    getCoolerMinMax(pSensor, nMin, nMax);
    if(int(dTemp) < nMin || int(dTemp) > nMax)
        return ERR_CMDFAILED;

    auto pTEC = m_pCamera->getTEC();
    try {
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
        m_sLogFile << "["<<getTimeStamp()<<"]"<< " [setCoolerTemperature] Setting cooler " << (bOn?"On":"Off") << std::endl;
        m_sLogFile << "["<<getTimeStamp()<<"]"<< " [setCoolerTemperature] Setting cooler to " << std::fixed << std::setprecision(2) << dTemp << std::endl;
        m_sLogFile.flush();
#endif
        setFanSpeed(true,0);    // set to auto for now
        handlePromise(pTEC->setState(bOn, dTemp));
    }
    catch (std::exception &ex) {
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
        m_sLogFile << "["<<getTimeStamp()<<"]"<< " [setCoolerTemperature] Exception = " << ex.what() << std::endl;
        m_sLogFile.flush();
#endif
        return ERR_CMDFAILED;
    }
    m_dSetPoint = dTemp;
    return nErr;
}

void CSBigAluma::setFanSpeed(bool bAuto, int fanSpeed)
{
    ISensorPtr pSensor = nullptr;
    // First, get the main imaging sensor (always index-zero).
    pSensor = m_pCamera->getSensor(MAIN_SENSOR);
    try
    {
        if(bAuto)
            handlePromise(pSensor->setSetting(ISensor::AutoFanMode, bAuto?1:0));
        else
            handlePromise(pSensor->setSetting(ISensor::FanSpeed, fanSpeed));
    }
    catch (std::exception &ex)
    {
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
        m_sLogFile << "["<<getTimeStamp()<<"]"<< " [setCoolerTemperature] Exception Failed to set fan speed to : " << fanSpeed << " => " << ex.what() << std::endl;
        m_sLogFile.flush();
#endif
    }
}

double CSBigAluma::getSetPoint()
{
    return m_dSetPoint;
}

void CSBigAluma::getCoolerMinMax(ISensorPtr pSensor, int & min, int & max)
{
    min = INT_MIN;
    max = INT_MAX;
    try
    {
        handlePromise(pSensor->queryInfo());
        auto info = pSensor->getInfo();
        min = info.minCoolerSetpoint;
        max = info.maxCoolerSetpoint;
    }
    catch (std::exception &ex) {
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
        m_sLogFile << "["<<getTimeStamp()<<"]"<< " [getCoolerMinMax] Exception = " << ex.what() << std::endl;
        m_sLogFile.flush();
#endif
        return;
    }
}

int CSBigAluma::getWidth()
{
    return m_mainSensorInfo.pixelsX;
}

int CSBigAluma::getHeight()
{
    return m_mainSensorInfo.pixelsY;
}

void CSBigAluma::getPixelSize(int nSensorID, double &sizeX, double &sizeY)
{
    if(nSensorID == MAIN_SENSOR) {
        sizeX = m_mainSensorInfo.pixelSizeX;
        sizeY = m_mainSensorInfo.pixelSizeY;
    }
    else {
        sizeX = m_secondarySensorInfo.pixelSizeX;
        sizeY = m_secondarySensorInfo.pixelSizeY;

    }
}

void CSBigAluma::setBinSize(int nSensorID, int nXBin, int nYBIN)
{
    if (nSensorID == MAIN_SENSOR) {
        m_nCurrentXBin = nXBin;
        m_nCurrentYBin = nYBIN;
    }
    else {
        m_nSecondaryCurrentXBin = nXBin;
        m_nSecondaryCurrentYBin = nYBIN;
    }
}

bool CSBigAluma::isCameraColor()
{
    return m_bIsColorCam;
}

void CSBigAluma::getBayerPattern(int nSensorID, std::string &sBayerPattern)
{
    ISensorPtr pSensor = nullptr;
    ISensor::Info info;

    try {
        pSensor = m_pCamera->getSensor(nSensorID);
        handlePromise(pSensor->queryInfo());
        info = pSensor->getInfo();
        // we can only report bayer pattern , so we treat TrueSense Sparse Color as mono
        if(info.filterType != Color) {
            sBayerPattern = "MONO";
            return ;
        }

        switch(info.model) {
            case ISensor::ICX694 :
                sBayerPattern = "RGBG";
                break;
            case ISensor::ICX814 :
                sBayerPattern = "RGGB";
                break;
            case ISensor::KAF8300 :
                sBayerPattern = "RGGB";
                break;
            case ISensor::P1300 :
                sBayerPattern = "RGGB";
                break;
            default :
                sBayerPattern = "MONO";
                break;
        }
    }
    catch (std::exception &ex) {
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
        m_sLogFile << "["<<getTimeStamp()<<"]"<< " [getBayerPattern] Exception = " << ex.what() << std::endl;
        m_sLogFile.flush();
#endif
        sBayerPattern = "MONO";
        return ;
    }

}


void CSBigAluma::getGain(long &nMin, long &nMax, long &nValue)
{
    bool bTmp = false;

#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
    m_sLogFile << "["<<getTimeStamp()<<"]"<< " [getGain] Gain is " << nValue << std::endl;
    m_sLogFile.flush();
#endif
}

int CSBigAluma::setGain(long nGain)
{
    int nErr = PLUGIN_OK;

    m_nGain = nGain;

#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
    m_sLogFile << "["<<getTimeStamp()<<"]"<< " [setGain] Gain is set to" << m_nGain << std::endl;
    m_sLogFile.flush();
#endif

    return nErr;
}

int CSBigAluma::RelayActivate(const int nXPlus, const int nXMinus, const int nYPlus, const int nYMinus, const bool bSynchronous, const bool bAbort)
{
    int nErr = PLUGIN_OK;
    int ret;
    bool bCanPulse = false;
    return nErr;

}

void CSBigAluma::buildGainList(long nMin, long nMax, long nValue)
{
    long i = 0;
    int nStep = 1;
    m_GainList.clear();
    m_nNbGainValue = 0;

#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
    m_sLogFile << "["<<getTimeStamp()<<"]"<< " [buildGainList]" << std::endl;
    m_sLogFile.flush();
#endif


    if(nMin != nValue) {
        m_GainList.push_back(std::to_string(nValue));
        m_nNbGainValue++;
    }

    nStep = int(float(nMax-nMin)/10);
    for(i=nMin; i<nMax; i+=nStep) {
        m_GainList.push_back(std::to_string(i));
        m_nNbGainValue++;
    }
    m_GainList.push_back(std::to_string(nMax));
    m_nNbGainValue++;
}
int CSBigAluma::getNbGainInList()
{
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
    m_sLogFile << "["<<getTimeStamp()<<"]"<< " [getNbGainInList] m_nNbGainValue = " << m_nNbGainValue << std::endl;
    m_sLogFile.flush();
#endif

    return m_nNbGainValue;
}

void CSBigAluma::rebuildGainList()
{
    long nMin, nMax, nVal;
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
    m_sLogFile << "["<<getTimeStamp()<<"]"<< " [rebuildGainList]" << std::endl;
    m_sLogFile.flush();
#endif

    getGain(nMin, nMax, nVal);
    buildGainList(nMin, nMax, nVal);
}

std::string CSBigAluma::getGainFromListAtIndex(int nIndex)
{
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
    m_sLogFile << "["<<getTimeStamp()<<"]"<< " [getGainFromListAtIndex] nIndex = " << nIndex << std::endl;
    m_sLogFile.flush();
#endif

    if(nIndex<m_GainList.size())
        return m_GainList.at(nIndex);
    else
        return std::string("N/A");
}

#ifdef PLUGIN_DEBUG
const std::string CSBigAluma::getTimeStamp()
{
    time_t     now = time(0);
    struct tm  tstruct;
    char       buf[80];
    tstruct = *localtime(&now);
    std::strftime(buf, sizeof(buf), "%Y-%m-%d.%X", &tstruct);

    return buf;
}
#endif
