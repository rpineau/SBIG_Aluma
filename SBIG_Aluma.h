//
//  SBIG_Aluma.h
//
//  Created by Rodolphe Pineau on 2021/04/22
//  Copyright © 2021 RTI-Zone. All rights reserved.
//


#ifndef SBigAluma_hpp
#define SBigAluma_hpp

#include <stdio.h>
#include <math.h>
#include <string.h>
#include <stdlib.h>
#include <limits.h>

#include <string>
#include <vector>
#include <sstream>
#include <iostream>
#include <map>
#include <iomanip>
#include <fstream>
#include <chrono>
#include <thread>
#include <ctime>

#ifndef SB_WIN_BUILD
#include <unistd.h>
#endif

#include "../../licensedinterfaces/driverrootinterface.h"
#include "../../licensedinterfaces/sberrorx.h"

#include "dlapi.h"
#include "StopWatch.h"

#define PLUGIN_DEBUG    3

#define PLUGIN_VERSION      1.01
#define BUFFER_LEN 128
#define PLUGIN_OK   0
#define MAX_NB_BIN  16

#define RX_WAIT 10
#define RX_TIMEOUT 15000 // 15 seconds

#define MAKE_ERR_CODE(P_ID, DTYPE, ERR_CODE)  (((P_ID<<24) & 0xff000000) | ((DTYPE<<16) & 0x00ff0000)  | (ERR_CODE & 0x0000ffff))
#define PLUGIN_ID   46


enum Sensors {MAIN_SENSOR=0, GUIDER};

typedef struct _camere_info {
    int         cameraId;
    int         Sn;
    std::string sModel;
    int         nbSensors;
    unsigned int nFirmwareVersion;
    unsigned int nWiFiFirmwareVersion;
} camera_info_t;

using namespace dl;

class CSBigAluma {
public:
    CSBigAluma();
    ~CSBigAluma();

    int         Connect(int nCameraID);
    void        Disconnect(void);
    void        setCameraId(int nCameraId);
    void        getCameraId(int &nCcameraId);
    void        getCameraIdFromSerial( int nSerial, int &nCameraId);
    void        getCameraSerialFromID(int nCameraId, int &nSerial);
    void        getCameraNameFromID(int nCameraId, std::string &sName);

    void        getCameraDataFromID(int nCameraID, camera_info_t *p_tCamera);

    void        getCameraName(std::string &sName);
    void        getCameraSerial(int &nSerial);

    int         listCamera(std::vector<camera_info_t>  &cameraList);

    int         getNumBins(int nSensorID);
    int         getBinXFromIndex(int nSensorID, int nIndex);
    int         getBinYFromIndex(int nSensorID, int nIndex);

    bool        isShutterPresent();
    int         startCaputure(int nSensorID, double dTime, bool bIsLightFrame, int nReadoutMode = 0, bool bUseRBIFlash = false);
    void        abortCapture(int nSensorID);

    int         setROI(int nSensorID, int nLeft, int nTop, int nWidth, int nHeight);
    int         clearROI(int nSensorID);

    bool        isFameAvailable(int nSensorID);

    uint32_t    getBitDepth(int nSensorID);
    bool        isXferComplete(IPromise * pPromise);
    int         downloadFrame(int nSensorID);
    int         getFrame(int nSensorID, int nHeight, int nMemWidth, unsigned char* frameBuffer);


    int         getTemperture(double &dTemp, double &dPower, double &dSetPoint, bool &bEnabled);
    int         setCoolerTemperature(bool bOn, double dTemp);
    double      getSetPoint();
    void        setFanSpeed(bool bAuto, int fanSpeed);

    int         getWidth();
    int         getHeight();
    void        getPixelSize(int nSensorID, double &sizeX, double &sizeY);
    void        setBinSize(int nSensorID, int nXBin, int nYBin);
    
    bool        isCameraColor();
    void        getBayerPattern(int nSensorID, std::string &sBayerPattern);

    void        getGain(long &nMin, long &nMax, long &nValue);
    int         setGain(long nGain);

    bool        isGuidePortPresent();
    int         RelayActivate(const int nXPlus, const int nXMinus, const int nYPlus, const int nYMinus, const bool bSynchronous, const bool bAbort);

    int         getNbGainInList();
    std::string getGainFromListAtIndex(int nIndex);
    void        rebuildGainList();

    unsigned long   getNbReadoutModeInList();
    std::string     getReadoutModeFromListAtIndex(int nIndex);
    void            rebuildReadoutModeList();

protected:
    void        handlePromise(IPromisePtr pPromise);
    void        setCameraConnection(dl::ICameraPtr pCamera, bool enable);
    void        getCoolerMinMax(ISensorPtr pSensor, int & min, int & max);
    void        buildGainList(long nMin, long nMax, long nValue);

    void        buidldReadoutModeList();
    int         parseFields(std::string sInput, std::vector<std::string> &svFields, char cSeparator);

    bool                m_bConnected;
    bool                m_bAbort;
    bool                m_bPromiseTimedOut;

    camera_info_t               m_Camera;
    std::vector<camera_info_t>  m_tcameraList;

    IGatewayPtr         m_pGateway;
    ICameraPtr          m_pCamera;

    ICamera::Status     m_CameraStatus;
    double              m_dSetPoint;
    bool                m_bSupportsCooler;
    bool                m_bSupportGuiding;

    // Main sensor variables
    ISensor::Info       m_mainSensorInfo;
    bool                m_bIsColorCam;
    int                 m_SupportedBinsX[MAX_NB_BIN];
    int                 m_SupportedBinsY[MAX_NB_BIN];
    int                 m_nNbBin;
    int                 m_nCurrentXBin;
    int                 m_nCurrentYBin;

    // guide sensor variables
    ISensor::Info       m_secondarySensorInfo;
    bool                m_bSecondaryIsColorCam;
    int                 m_SecondarySupportedBinsX[MAX_NB_BIN];
    int                 m_SecondarySupportedBinsY[MAX_NB_BIN];
    int                 m_nSecondaryNbBin;
    int                 m_nSecondaryCurrentXBin;
    int                 m_nSecondaryCurrentYBin;


    std::vector<std::string>    m_ReadoutModeList;
    unsigned long               m_nNbReadoutModeValue;

    std::vector<std::string>    m_GainList;
    int                         m_nNbGainValue;
    long                        m_nGain;

    //int                     m_nMaxBitDepth;

    CStopWatch              m_ExposureTimer;
    CStopWatch              m_ExposureTimerGuider;
    double                  m_dCaptureLenght;
    

#ifdef PLUGIN_DEBUG
    // timestamp for logs
    const std::string getTimeStamp();
    std::ofstream m_sLogFile;
    std::string m_sLogfilePath;
#endif

};
#endif /* SBigAluma_hpp */
