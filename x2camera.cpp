// x2camera.cpp  
//
#include "x2camera.h"



X2Camera::X2Camera( const char* pszSelection, 
					const int& nISIndex,
					SerXInterface*						pSerX,
					TheSkyXFacadeForDriversInterface*	pTheSkyXForMounts,
					SleeperInterface*					pSleeper,
					BasicIniUtilInterface*				pIniUtil,
					LoggerInterface*					pLogger,
					MutexInterface*						pIOMutex,
					TickCountInterface*					pTickCount)
{
    int nValue = 0;
    bool bIsAuto;
    
	m_nPrivateISIndex				= nISIndex;
	m_pTheSkyXForMounts				= pTheSkyXForMounts;
	m_pSleeper						= pSleeper;
	m_pIniUtil						= pIniUtil;
	m_pLogger						= pLogger;
	m_pIOMutex						= pIOMutex;
	m_pTickCount					= pTickCount;

	m_dCurTemp = -100.0;
	m_dCurPower = 0;

    mPixelSizeX = 0.0;
    mPixelSizeY = 0.0;

    m_nReadoutMode = 0;
    m_bUseRBIFlash = false;

    // Read in settings
    if (m_pIniUtil) {
        m_pIniUtil->readString(KEY_X2CAM_ROOT, KEY_SERIAL, "0", m_szCameraSerial, 128);
        m_Camera.getCameraIdFromSerial(std::stoi(m_szCameraSerial), m_nCameraID);

        /*
        nValue = m_pIniUtil->readInt(KEY_X2CAM_ROOT, KEY_GAIN, 10);
        m_Camera.setGain((long)nValue);
*/
    }

}

X2Camera::~X2Camera()
{
	//Delete objects used through composition
	if (m_pTheSkyXForMounts)
		delete m_pTheSkyXForMounts;
	if (m_pSleeper)
		delete m_pSleeper;
	if (m_pIniUtil)
		delete m_pIniUtil;
	if (m_pLogger)
		delete m_pLogger;
	if (m_pIOMutex)
		delete m_pIOMutex;
	if (m_pTickCount)
		delete m_pTickCount;

}

#pragma mark DriverRootInterface
int	X2Camera::queryAbstraction(const char* pszName, void** ppVal)			
{
	X2MutexLocker ml(GetMutex());

	if (!strcmp(pszName, ModalSettingsDialogInterface_Name))
		*ppVal = dynamic_cast<ModalSettingsDialogInterface*>(this);
	else if (!strcmp(pszName, X2GUIEventInterface_Name))
			*ppVal = dynamic_cast<X2GUIEventInterface*>(this);
    else if (!strcmp(pszName, SubframeInterface_Name))
        *ppVal = dynamic_cast<SubframeInterface*>(this);
    else if (!strcmp(pszName, PixelSizeInterface_Name))
        *ppVal = dynamic_cast<PixelSizeInterface*>(this);
    else if (!strcmp(pszName, AddFITSKeyInterface_Name))
        *ppVal = dynamic_cast<AddFITSKeyInterface*>(this);
    else if (!strcmp(pszName, CameraDependentSettingInterface_Name))
        *ppVal = dynamic_cast<CameraDependentSettingInterface*>(this);
    else if (!strcmp(pszName, NoShutterInterface_Name))
        *ppVal = dynamic_cast<NoShutterInterface*>(this);

	return SB_OK;
}

#pragma mark UI bindings
int X2Camera::execModalSettingsDialog()
{
    int nErr = SB_OK;
    X2ModalUIUtil uiutil(this, GetTheSkyXFacadeForDrivers());
    X2GUIInterface*                    ui = uiutil.X2UI();
    X2GUIExchangeInterface*            dx = NULL;//Comes after ui is loaded
    std::stringstream ssTmp;
    bool bPressedOK = false;
    int i;
    int nCamIndex;
    bool bCameraFoud = false;
    if (NULL == ui)
        return ERR_POINTER;
    nErr = ui->loadUserInterface("SBIG_CamSelect.ui", deviceType(), m_nPrivateISIndex);
    if (nErr)
        return nErr;

    if (NULL == (dx = uiutil.X2DX()))
        return ERR_POINTER;

    //Intialize the user interface
    m_Camera.listCamera(m_tCameraIdList);
    if(!m_tCameraIdList.size()) {
        dx->comboBoxAppendString("comboBox","No Camera found");
        dx->setCurrentIndex("comboBox",0);
    }
    else {
        bCameraFoud = true;
        nCamIndex = 0;
        for(i=0; i< m_tCameraIdList.size(); i++) {
            //Populate the camera combo box and set the current index (selection)
            ssTmp << m_tCameraIdList[i].sModel << " [" << m_tCameraIdList[i].Sn << "]";
            dx->comboBoxAppendString("comboBox",ssTmp.str().c_str());
            if(m_tCameraIdList[i].cameraId == m_nCameraID)
                nCamIndex = i;
        }
        dx->setCurrentIndex("comboBox",nCamIndex);
    }
    if(m_bLinked) {
        dx->setEnabled("pushButton", true);
    }
    else {
        dx->setEnabled("pushButton", false);
    }
    m_nCurrentDialog = SELECT;

    //Display the user interface
    if ((nErr = ui->exec(bPressedOK)))
        return nErr;

    //Retreive values from the user interface
    if (bPressedOK)
    {
        if(bCameraFoud) {
            int nCamera;
            std::string sCameraSerial;
            int nCameraSerial;
            //Camera
            nCamera = dx->currentIndex("comboBox");
            m_Camera.setCameraId(m_tCameraIdList[nCamera].cameraId);
            m_nCameraID = m_tCameraIdList[nCamera].cameraId;
            m_Camera.getCameraSerialFromID(m_nCameraID, nCameraSerial);
            // m_Camera.setCameraSerial(nCameraSerial);
            // store camera ID
            m_pIniUtil->writeInt(KEY_X2CAM_ROOT, KEY_SERIAL, nCameraSerial);
        }
    }


    return nErr;
}

int X2Camera::doCamFeatureConfig()
{
    int nErr = SB_OK;
    X2ModalUIUtil uiutil(this, GetTheSkyXFacadeForDrivers());
    X2GUIInterface*                    ui = uiutil.X2UI();
    X2GUIExchangeInterface*            dx = NULL;
    long nVal, nMin, nMax;
    int nCtrlVal;
    bool bIsAuto;
    bool bPressedOK = false;
    
    if (NULL == ui)
        return ERR_POINTER;

    if ((nErr = ui->loadUserInterface("SBIG_AlumaCamera.ui", deviceType(), m_nPrivateISIndex)))
        return nErr;

    if (NULL == (dx = uiutil.X2DX()))
        return ERR_POINTER;


    if(m_bLinked){
        m_Camera.getGain(nMin, nMax, nVal);
        if(nMax == -1)
            dx->setEnabled("Gain", false);
        else {
            dx->setPropertyInt("Gain", "minimum", (int)nMin);
            dx->setPropertyInt("Gain", "maximum", (int)nMax);
            dx->setPropertyInt("Gain", "value", (int)nVal);
            if(bIsAuto) {
                dx->setEnabled("Gain", false);
                dx->setChecked("checkBox", 1);
            }
        }

    }
    else {
        dx->setEnabled("Gain", false);
    }

    m_nCurrentDialog = SETTINGS;
    //Display the user interface
    if ((nErr = ui->exec(bPressedOK)))
        return nErr;

    //Retreive values from the user interface
    if (bPressedOK) {
        dx->propertyInt("Gain", "value", nCtrlVal);
        bIsAuto = dx->isChecked("checkBox");
        nErr = m_Camera.setGain((long)nCtrlVal);
        if(!nErr) {
            m_pIniUtil->writeInt(KEY_X2CAM_ROOT, KEY_GAIN, nCtrlVal);
        }

    }

    return nErr;
}


void X2Camera::uiEvent(X2GUIExchangeInterface* uiex, const char* pszEvent)
{
    switch(m_nCurrentDialog) {
        case SELECT:
            doSelectCamEvent(uiex, pszEvent);
            break;
        case SETTINGS:
            doSettingsCamEvent(uiex, pszEvent);
            break;
        default :
            break;
    }
}

void X2Camera::doSelectCamEvent(X2GUIExchangeInterface* uiex, const char* pszEvent)
{
    if (!strcmp(pszEvent, "on_pushButton_clicked"))
    {
        int nErr=SB_OK;
        
        nErr = doCamFeatureConfig();
        m_nCurrentDialog = SELECT;
    }
}

void X2Camera::doSettingsCamEvent(X2GUIExchangeInterface* uiex, const char* pszEvent)
{
    bool bEnable;
    
    if (!strcmp(pszEvent, "on_checkBox_stateChanged")) {
        bEnable = uiex->isChecked("chackBox");
        uiex->setEnabled("Gain", !bEnable);
    }

    if (!strcmp(pszEvent, "on_checkBox_2_stateChanged")) {
        bEnable = uiex->isChecked("chackBox_2");
        uiex->setEnabled("WB_R", !bEnable);
    }

    if (!strcmp(pszEvent, "on_checkBox_3_stateChanged")) {
        bEnable = uiex->isChecked("chackBox_3");
        uiex->setEnabled("WB_G", !bEnable);
    }

    if (!strcmp(pszEvent, "on_checkBox_4_stateChanged")) {
        bEnable = uiex->isChecked("chackBox_4");
        uiex->setEnabled("WB_B", !bEnable);
    }

}

#pragma mark DriverInfoInterface
void X2Camera::driverInfoDetailedInfo(BasicStringInterface& str) const		
{
	X2MutexLocker ml(GetMutex());

    str = "SBIG Aluma camera X2 plugin by Rodolphe Pineau";
}

double X2Camera::driverInfoVersion(void) const								
{
	X2MutexLocker ml(GetMutex());

    return PLUGIN_VERSION;
}

#pragma mark HardwareInfoInterface
void X2Camera::deviceInfoNameShort(BasicStringInterface& str) const										
{
    X2Camera* pMe = (X2Camera*)this;
    X2MutexLocker ml(pMe->GetMutex());

    if(m_bLinked) {
        std::string sCameraName;
        pMe->m_Camera.getCameraName(sCameraName);
        str = sCameraName.c_str();
    }
    else {
        str = "";
    }
}

void X2Camera::deviceInfoNameLong(BasicStringInterface& str) const										
{
    X2Camera* pMe = (X2Camera*)this;
    X2MutexLocker ml(pMe->GetMutex());

    if(m_bLinked) {
        std::string sCameraName;
        pMe->m_Camera.getCameraName(sCameraName);
        str = sCameraName.c_str();
    }
    else {
        str = "";
    }
}

void X2Camera::deviceInfoDetailedDescription(BasicStringInterface& str) const								
{
	X2MutexLocker ml(GetMutex());

	str = "SBIG Aluma camera X2 plugin by Rodolphe Pineau";
}

void X2Camera::deviceInfoFirmwareVersion(BasicStringInterface& str)										
{
	X2MutexLocker ml(GetMutex());

	str = "Not available";
}

void X2Camera::deviceInfoModel(BasicStringInterface& str)													
{
	X2MutexLocker ml(GetMutex());

    if(m_bLinked) {
        std::string sCameraName;
        m_Camera.getCameraName(sCameraName);
        str = sCameraName.c_str();
    }
    else {
        str = "";
    }
}

#pragma mark Device Access
int X2Camera::CCEstablishLink(const enumLPTPort portLPT, const enumWhichCCD& CCD, enumCameraIndex DesiredCamera, enumCameraIndex& CameraFound, const int nDesiredCFW, int& nFoundCFW)
{
    int nErr = SB_OK;

    m_bLinked = false;

    m_dCurTemp = -100.0;
    nErr = m_Camera.Connect(m_nCameraID);
    if(nErr)
        m_bLinked = false;
    else
        m_bLinked = true;

    if(!m_nCameraID) {
        m_Camera.getCameraId(m_nCameraID);
        int nCameraSerial;
        m_Camera.getCameraSerialFromID(m_nCameraID, nCameraSerial);
        // store camera ID
        m_pIniUtil->writeString(KEY_X2CAM_ROOT, KEY_SERIAL, std::to_string(nCameraSerial).c_str());
    }

    m_CCD_Sensor = CCD;

    return nErr;
}


int X2Camera::CCQueryTemperature(double& dCurTemp, double& dCurPower, char* lpszPower, const int nMaxLen, bool& bCurEnabled, double& dCurSetPoint)
{   
    int nErr = SB_OK;
    X2MutexLocker ml(GetMutex());

	if (!m_bLinked)
		return ERR_NOLINK;

    nErr = m_Camera.getTemperture(m_dCurTemp, m_dCurPower, m_dCurSetPoint, bCurEnabled);
    dCurTemp = m_dCurTemp;
	dCurPower = m_dCurPower;
    dCurSetPoint = m_dCurSetPoint;

    return nErr;
}

int X2Camera::CCRegulateTemp(const bool& bOn, const double& dTemp)
{
    int nErr = SB_OK;
	X2MutexLocker ml(GetMutex());

	if (!m_bLinked)
		return ERR_NOLINK;

    nErr = m_Camera.setCoolerTemperature(bOn, dTemp);
    
	return nErr;
}

int X2Camera::CCGetRecommendedSetpoint(double& RecTemp)
{
	X2MutexLocker ml(GetMutex());
    if (!m_bLinked)
        return ERR_NOLINK;

    RecTemp = m_Camera.getSetPoint();
    return SB_OK;
}  


int X2Camera::CCStartExposure(const enumCameraIndex& Cam, const enumWhichCCD CCD, const double& dTime, enumPictureType Type, const int& nABGState, const bool& bLeaveShutterAlone)
{   
	X2MutexLocker ml(GetMutex());

	if (!m_bLinked)
		return ERR_NOLINK;

	bool bLight = true;
    int nErr = SB_OK;

    switch (Type)
    {
        case PT_FLAT:
        case PT_LIGHT:            bLight = true;    break;
        case PT_DARK:
        case PT_AUTODARK:
        case PT_BIAS:            bLight = false;    break;
        default:                return ERR_CMDFAILED;
    }

    nErr = m_Camera.startCaputure(mapWhichCCDToSensorId(CCD), dTime, bLight, m_nReadoutMode, m_bUseRBIFlash);
    return nErr;
}   



int X2Camera::CCIsExposureComplete(const enumCameraIndex& Cam, const enumWhichCCD CCD, bool* pbComplete, unsigned int* pStatus)
{   
	X2MutexLocker ml(GetMutex());

	if (!m_bLinked)
		return ERR_NOLINK;

    *pbComplete = false;

    if(m_Camera.isFameAvailable(mapWhichCCDToSensorId(CCD)))
        *pbComplete = true;

    return SB_OK;
}

int X2Camera::CCEndExposure(const enumCameraIndex& Cam, const enumWhichCCD CCD, const bool& bWasAborted, const bool& bLeaveShutterAlone)           
{   
	X2MutexLocker ml(GetMutex());

	if (!m_bLinked)
		return ERR_NOLINK;

	int nErr = SB_OK;

	if (bWasAborted) {
        m_Camera.abortCapture(mapWhichCCDToSensorId(CCD));
	}

    //  done in m_Camera.getFrame
    // nErr = m_Camera.downloadFrame(mapWhichCCDToSensorId(CCD));

    return nErr;
}

int X2Camera::CCGetChipSize(const enumCameraIndex& Camera, const enumWhichCCD& CCD, const int& nXBin, const int& nYBin, const bool& bOffChipBinning, int& nW, int& nH, int& nReadOut)
{
	X2MutexLocker ml(GetMutex());

    if (!m_bLinked)
        return ERR_NOLINK;

	nW = m_Camera.getWidth()/nXBin;
    nH = m_Camera.getHeight()/nYBin;
    nReadOut = CameraDriverInterface::rm_Image;

    m_Camera.setBinSize(mapWhichCCDToSensorId(CCD), nXBin, nYBin);
    return SB_OK;
}

int X2Camera::CCGetNumBins(const enumCameraIndex& Camera, const enumWhichCCD& CCD, int& nNumBins)
{
	X2MutexLocker ml(GetMutex());

    if (!m_bLinked)
        return ERR_NOLINK;

    nNumBins = m_Camera.getNumBins(mapWhichCCDToSensorId(CCD));

    return SB_OK;
}

int X2Camera::CCGetBinSizeFromIndex(const enumCameraIndex& Camera, const enumWhichCCD& CCD, const int& nIndex, long& nBincx, long& nBincy)
{
	X2MutexLocker ml(GetMutex());

    if (!m_bLinked)
        return ERR_NOLINK;

    nBincx = m_Camera.getBinXFromIndex(mapWhichCCDToSensorId(CCD), nIndex);
    nBincy = m_Camera.getBinYFromIndex(mapWhichCCDToSensorId(CCD),nIndex);

	return SB_OK;
}

int X2Camera::CCUpdateClock(void)
{   
	X2MutexLocker ml(GetMutex());

	return SB_OK;
}

int X2Camera::CCSetShutter(bool bOpen)           
{   
	X2MutexLocker ml(GetMutex());

	return SB_OK;;
}

int X2Camera::CCActivateRelays(const int& nXPlus, const int& nXMinus, const int& nYPlus, const int& nYMinus, const bool& bSynchronous, const bool& bAbort, const bool& bEndThread)
{   
	X2MutexLocker ml(GetMutex());
    if (!m_bLinked)
        return ERR_NOLINK;

    m_Camera.RelayActivate(nXPlus, nXMinus, nYPlus, nYMinus, bSynchronous, bAbort);
	return SB_OK;
}

int X2Camera::CCPulseOut(unsigned int nPulse, bool bAdjust, const enumCameraIndex& Cam)
{   
	X2MutexLocker ml(GetMutex());
    if (!m_bLinked)
        return ERR_NOLINK;

    return SB_OK;
}

void X2Camera::CCBeforeDownload(const enumCameraIndex& Cam, const enumWhichCCD& CCD)
{
	X2MutexLocker ml(GetMutex());
}


void X2Camera::CCAfterDownload(const enumCameraIndex& Cam, const enumWhichCCD& CCD)
{
	X2MutexLocker ml(GetMutex());
	return;
}

int X2Camera::CCReadoutLine(const enumCameraIndex& Cam, const enumWhichCCD& CCD, const int& pixelStart, const int& pixelLength, const int& nReadoutMode, unsigned char* pMem)
{   
	X2MutexLocker ml(GetMutex());
    if (!m_bLinked)
        return ERR_NOLINK;
	return SB_OK;
}           

int X2Camera::CCDumpLines(const enumCameraIndex& Cam, const enumWhichCCD& CCD, const int& nReadoutMode, const unsigned int& lines)
{                                     
	X2MutexLocker ml(GetMutex());
    if (!m_bLinked)
        return ERR_NOLINK;
	return SB_OK;
}           


int X2Camera::CCReadoutImage(const enumCameraIndex& Cam, const enumWhichCCD& CCD, const int& nWidth, const int& nHeight, const int& nMemWidth, unsigned char* pMem)
{
    int nErr = SB_OK;
    X2MutexLocker ml(GetMutex());
    
    if (!m_bLinked)
		return ERR_NOLINK;

    nErr = m_Camera.getFrame(mapWhichCCDToSensorId(CCD), nHeight, nMemWidth, pMem);

    return nErr;
}

int X2Camera::CCDisconnect(const bool bShutDownTemp)
{
	X2MutexLocker ml(GetMutex());
    if (!m_bLinked)
        return ERR_NOLINK;

	if (m_bLinked)
	{
        m_Camera.Disconnect();
		setLinked(false);
	}

	return SB_OK;
}

int X2Camera::CCSetImageProps(const enumCameraIndex& Camera, const enumWhichCCD& CCD, const int& nReadOut, void* pImage)
{
	X2MutexLocker ml(GetMutex());

    if (!m_bLinked)
        return ERR_NOLINK;

    return SB_OK;
}

int X2Camera::CCGetFullDynamicRange(const enumCameraIndex& Camera, const enumWhichCCD& CCD, unsigned long& dwDynRg)
{
    uint32_t nBitDepth;

    X2MutexLocker ml(GetMutex());

    if (!m_bLinked)
        return ERR_NOLINK;

    nBitDepth = m_Camera.getBitDepth(mapWhichCCDToSensorId(CCD));
    dwDynRg = (unsigned long)(1 << nBitDepth);

	return SB_OK;
}

void X2Camera::CCMakeExposureState(int* pnState, enumCameraIndex Cam, int nXBin, int nYBin, int abg, bool bRapidReadout)
{
	X2MutexLocker ml(GetMutex());

	return;
}

int X2Camera::CCSetBinnedSubFrame(const enumCameraIndex& Camera, const enumWhichCCD& CCD, const int& nLeft, const int& nTop, const int& nRight, const int& nBottom)
{
    int nErr = SB_OK;
	X2MutexLocker ml(GetMutex());

    if (!m_bLinked)
        return ERR_NOLINK;

    nErr = m_Camera.setROI(mapWhichCCDToSensorId(CCD), nLeft, nTop, (nRight-nLeft)+1, (nBottom-nTop)+1);
    return nErr;
}

int X2Camera::CCSetBinnedSubFrame3(const enumCameraIndex &Camera, const enumWhichCCD &CCDOrig, const int &nLeft, const int &nTop,  const int &nWidth,  const int &nHeight)
{
    int nErr = SB_OK;
    
    X2MutexLocker ml(GetMutex());
    if (!m_bLinked)
        return ERR_NOLINK;

    nErr = m_Camera.setROI(mapWhichCCDToSensorId(CCDOrig), nLeft, nTop, nWidth, nHeight);
    
    return nErr;
}


int X2Camera::CCSettings(const enumCameraIndex& Camera, const enumWhichCCD& CCD)
{
	X2MutexLocker ml(GetMutex());
    if (!m_bLinked)
        return ERR_NOLINK;

	return ERR_NOT_IMPL;
}

int X2Camera::CCSetFan(const bool& bOn)
{
	X2MutexLocker ml(GetMutex());
    if (!m_bLinked)
        return ERR_NOLINK;

	return SB_OK;
}

int	X2Camera::pathTo_rm_FitsOnDisk(char* lpszPath, const int& nPathSize)
{
	X2MutexLocker ml(GetMutex());

	if (!m_bLinked)
		return ERR_NOLINK;

	//Just give a file path to a FITS and TheSkyX will load it
		
	return SB_OK;
}

CameraDriverInterface::ReadOutMode X2Camera::readoutMode(void)		
{
	X2MutexLocker ml(GetMutex());

	return CameraDriverInterface::rm_Image;
}


enumCameraIndex	X2Camera::cameraId()
{
	X2MutexLocker ml(GetMutex());
	return m_CameraIdx;
}

void X2Camera::setCameraId(enumCameraIndex Cam)	
{
    m_CameraIdx = Cam;
}

int X2Camera::PixelSize1x1InMicrons(const enumCameraIndex &Camera, const enumWhichCCD &CCD, double &x, double &y)
{
    int nErr = SB_OK;

    if(!m_bLinked) {
        x = 0.0;
        y = 0.0;
        return ERR_COMMNOLINK;
    }
    X2MutexLocker ml(GetMutex());
    m_Camera.getPixelSize(mapWhichCCDToSensorId(CCD), x, y);
    return nErr;
}

int X2Camera::countOfIntegerFields (int &nCount)
{
    int nErr = SB_OK;
    nCount = 1;
    return nErr;
}

int X2Camera::valueForIntegerField (int nIndex, BasicStringInterface &sFieldName, BasicStringInterface &sFieldComment, int &nFieldValue)
{
    int nErr = SB_OK;
    long nVal = 0;
    long nMin = 0;
    long nMax = 0;
    bool bIsAuto;
    
    X2MutexLocker ml(GetMutex());
    if (!m_bLinked)
        return ERR_NOLINK;

    switch(nIndex) {
        case F_GAIN :
            m_Camera.getGain(nMin, nMax, nVal);
            sFieldName = "GAIN";
            sFieldComment = "";
            nFieldValue = (int)nVal;
            break;
        default :
            break;

    }
    return nErr;
}

int X2Camera::countOfDoubleFields (int &nCount)
{
    int nErr = SB_OK;
    nCount = 0;
    return nErr;
}

int X2Camera::valueForDoubleField (int nIndex, BasicStringInterface &sFieldName, BasicStringInterface &sFieldComment, double &dFieldValue)
{
    int nErr = SB_OK;
    sFieldName = "";
    sFieldComment = "";
    dFieldValue = 0;
    return nErr;
}

int X2Camera::countOfStringFields (int &nCount)
{
    int nErr = SB_OK;
    nCount = 3;
    return nErr;
}

int X2Camera::valueForStringField (int nIndex, BasicStringInterface &sFieldName, BasicStringInterface &sFieldComment, BasicStringInterface &sFieldValue)
{
    int nErr = SB_OK;
    std::string sTmp;
    
    X2MutexLocker ml(GetMutex());
    if (!m_bLinked)
        return ERR_NOLINK;

    switch(nIndex) {
        case F_BAYER :
            if(m_Camera.isCameraColor()) {
                m_Camera.getBayerPattern(m_CCD_Sensor, sTmp);
                sFieldName = "DEBAYER";
                sFieldComment = "Bayer pattern to use to decode color image";
                sFieldValue = sTmp.c_str();
            }
            else {
                sFieldName = "DEBAYER";
                sFieldComment = "Bayer pattern to use to decode color image";
                sFieldValue = "MONO";
            }
            break;

        case F_BAYERPAT: // PixInsight
            if(m_Camera.isCameraColor()) {
                m_Camera.getBayerPattern(m_CCD_Sensor, sTmp);
                sFieldName = "BAYERPAT";
                sFieldComment = "Bayer pattern to use to decode color image";
                sFieldValue = sTmp.c_str();
            }
            else {
                sFieldName = "BAYERPAT";
                sFieldComment = "Bayer pattern to use to decode color image";
                sFieldValue = "MONO";
            }
            break;

        default :
            break;
    }

    return nErr;
}

int X2Camera::CCGetExtendedSettingName (const enumCameraIndex &Camera, const enumWhichCCD &CCDOrig, BasicStringInterface &sSettingName)
{
    int nErr = SB_OK;

    sSettingName="Gain";

    return nErr;
}

int X2Camera::CCGetExtendedValueCount (const enumCameraIndex &Camera, const enumWhichCCD &CCDOrig, int &nCount)
{
    int nErr = SB_OK;

    X2MutexLocker ml(GetMutex());
    if (!m_bLinked)
        return ERR_NOLINK;

    nCount = m_Camera.getNbGainInList();
    return nErr;
}

int X2Camera::CCGetExtendedValueName (const enumCameraIndex &Camera, const enumWhichCCD &CCDOrig, const int nIndex, BasicStringInterface &sName)
{
    int nErr = SB_OK;

    X2MutexLocker ml(GetMutex());
    if (!m_bLinked)
        return ERR_NOLINK;

    sName = m_Camera.getGainFromListAtIndex(nIndex).c_str();
    return nErr;
}

int X2Camera::CCStartExposureAdditionalArgInterface (const enumCameraIndex &Cam, const enumWhichCCD CCD, const double &dTime, enumPictureType Type, const int &nABGState, const bool &bLeaveShutterAlone, const int &nIndex)
{
    X2MutexLocker ml(GetMutex());

    if (!m_bLinked)
        return ERR_NOLINK;

    bool bLight = true;
    int nErr = SB_OK;


    switch (Type)
    {
        case PT_FLAT:
        case PT_LIGHT:            bLight = true;    break;
        case PT_DARK:
        case PT_AUTODARK:
        case PT_BIAS:            bLight = false;    break;
        default:                return ERR_CMDFAILED;
    }

    nErr = m_Camera.startCaputure(dTime, bLight, m_nReadoutMode, m_bUseRBIFlash);
    return nErr;

}

int X2Camera::CCHasShutter (const enumCameraIndex &Camera, const enumWhichCCD &CCDOrig, bool &bHasShutter)
{
    X2MutexLocker ml(GetMutex());

    if (!m_bLinked)
        return ERR_NOLINK;

    bHasShutter = m_Camera.isShutterPresent();

    return SB_OK;
}

int X2Camera::mapWhichCCDToSensorId(enumWhichCCD CCD)
{
    switch (CCD) {
        case CCD_IMAGER:
            return MAIN_SENSOR;
            break;
        case CCD_GUIDER:
            return GUIDER;
            break;
        default:
            break;
    }
    return MAIN_SENSOR; // default
}
