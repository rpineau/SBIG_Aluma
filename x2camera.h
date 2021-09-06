#pragma once
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "../../licensedinterfaces/cameradriverinterface.h"
#include "../../licensedinterfaces/pixelsizeinterface.h"
#include "../../licensedinterfaces/noshutterinterface.h"
#include "../../licensedinterfaces/modalsettingsdialoginterface.h"
#include "../../licensedinterfaces/sberrorx.h"
#include "../../licensedinterfaces/theskyxfacadefordriversinterface.h"
#include "../../licensedinterfaces/sleeperinterface.h"
#include "../../licensedinterfaces/loggerinterface.h"
#include "../../licensedinterfaces/basiciniutilinterface.h"
#include "../../licensedinterfaces/mutexinterface.h"
#include "../../licensedinterfaces/tickcountinterface.h"
#include "../../licensedinterfaces/basicstringinterface.h"
#include "../../licensedinterfaces/x2guiinterface.h"
#include "../../licensedinterfaces/addfitskeyinterface.h"
#include "../../licensedinterfaces/subframeinterface.h"
#include "../../licensedinterfaces/cameradependentsettinginterface.h"


#include "SBIG_Aluma.h"

class SerXInterface;		
class TheSkyXFacadeForDriversInterface;
class SleeperInterface;
class BasicIniUtilInterface;
class LoggerInterface;
class MutexInterface;
class TickCountInterface;
class AddFITSKeyInterface;

//As far as a naming convention goes, X2 implementors could do a search and
//replace in all files and change "X2Camera" to "CoolCompanyCamera"
//where CoolCompany is your company name.  This is not a requirement.

//For properties that need to be persistent
#define KEY_X2CAM_ROOT              "SBIG_ALUMA"
#define KEY_SERIAL                  "SBIG_ALUMA_SERIAL"
#define KEY_GAIN                    "Gain"

enum DIALOGS {SELECT, SETTINGS };


// extra fit heard indexes
enum EXTRA_FIT_STRING {
    F_BAYER = 0,
    F_BAYERPAT
};

enum EXTRA_FIT_INT {
    F_GAIN = 0,
};


/*!
 \b
 rief The X2Camera example.

\ingroup Example

Use this example to write an X2Camera driver.
*/
class X2Camera: public CameraDriverInterface, public SubframeInterface, public  NoShutterInterface ,  public ModalSettingsDialogInterface, public X2GUIEventInterface, public PixelSizeInterface, public  AddFITSKeyInterface, public CameraDependentSettingInterface
{
public: 
	/*!Standard X2 constructor*/
	X2Camera(const char* pszSelectionString, 
					const int& nISIndex,
					SerXInterface*						pSerX,
					TheSkyXFacadeForDriversInterface* pTheSkyXForMounts,
					SleeperInterface*					pSleeper,
					BasicIniUtilInterface*				pIniUtil,
					LoggerInterface*					pLogger,
					MutexInterface*					pIOMutex,
					TickCountInterface*				pTickCount);
	virtual ~X2Camera();  

	/*!\name DriverRootInterface Implementation
	See DriverRootInterface.*/
	//@{ 
	virtual int queryAbstraction(const char* pszName, void** ppVal);
	//@} 

	/*!\name DriverInfoInterface Implementation
	See DriverInfoInterface.*/
	//@{ 
	virtual void    driverInfoDetailedInfo(BasicStringInterface& str) const;
	virtual double  driverInfoVersion(void) const;
	//@} 

	/*!\name HardwareInfoInterface Implementation
	See HardwareInfoInterface.*/
	//@{ 
	virtual void deviceInfoNameShort(BasicStringInterface& str) const;
	virtual void deviceInfoNameLong(BasicStringInterface& str) const;
	virtual void deviceInfoDetailedDescription(BasicStringInterface& str) const;
	virtual void deviceInfoFirmwareVersion(BasicStringInterface& str);
	virtual void deviceInfoModel(BasicStringInterface& str);
	//@} 

public://Properties

	/*!\name CameraDriverInterface Implementation
	See CameraDriverInterface.*/
	//@{ 

	virtual enumCameraIndex	cameraId();
	virtual	void            setCameraId(enumCameraIndex Cam);
    virtual bool            isLinked()					{return m_bLinked;};
    virtual void            setLinked(const bool bYes)	{m_bLinked = bYes;};
	
    virtual int             GetVersion(void)			{return CAMAPIVERSION;};
	virtual CameraDriverInterface::ReadOutMode readoutMode(void);
	virtual int             pathTo_rm_FitsOnDisk(char* lpszPath, const int& nPathSize);

public://Methods

	virtual int CCSettings(const enumCameraIndex& Camera, const enumWhichCCD& CCD);

	virtual int CCEstablishLink(enumLPTPort portLPT, const enumWhichCCD& CCD, enumCameraIndex DesiredCamera, enumCameraIndex& CameraFound, const int nDesiredCFW, int& nFoundCFW);
	virtual int CCDisconnect(const bool bShutDownTemp);

	virtual int CCGetChipSize(const enumCameraIndex& Camera, const enumWhichCCD& CCD, const int& nXBin, const int& nYBin, const bool& bOffChipBinning, int& nW, int& nH, int& nReadOut);
	virtual int CCGetNumBins(const enumCameraIndex& Camera, const enumWhichCCD& CCD, int& nNumBins);
	virtual	int CCGetBinSizeFromIndex(const enumCameraIndex& Camera, const enumWhichCCD& CCD, const int& nIndex, long& nBincx, long& nBincy);

	virtual int CCSetBinnedSubFrame(const enumCameraIndex& Camera, const enumWhichCCD& CCD, const int& nLeft, const int& nTop, const int& nRight, const int& nBottom);

	virtual void CCMakeExposureState(int* pnState, enumCameraIndex Cam, int nXBin, int nYBin, int abg, bool bRapidReadout);//SBIG specific

	virtual int CCStartExposure(const enumCameraIndex& Cam, const enumWhichCCD CCD, const double& dTime, enumPictureType Type, const int& nABGState, const bool& bLeaveShutterAlone);
	virtual int CCIsExposureComplete(const enumCameraIndex& Cam, const enumWhichCCD CCD, bool* pbComplete, unsigned int* pStatus);
	virtual int CCEndExposure(const enumCameraIndex& Cam, const enumWhichCCD CCD, const bool& bWasAborted, const bool& bLeaveShutterAlone);

	virtual int CCReadoutLine(const enumCameraIndex& Cam, const enumWhichCCD& CCD, const int& pixelStart, const int& pixelLength, const int& nReadoutMode, unsigned char* pMem);
	virtual int CCDumpLines(const enumCameraIndex& Cam, const enumWhichCCD& CCD, const int& nReadoutMode, const unsigned int& lines);

	virtual int CCReadoutImage(const enumCameraIndex& Cam, const enumWhichCCD& CCD, const int& nWidth, const int& nHeight, const int& nMemWidth, unsigned char* pMem);

	virtual int CCRegulateTemp(const bool& bOn, const double& dTemp);
	virtual int CCQueryTemperature(double& dCurTemp, double& dCurPower, char* lpszPower, const int nMaxLen, bool& bCurEnabled, double& dCurSetPoint);
	virtual int	CCGetRecommendedSetpoint(double& dRecSP);
	virtual int	CCSetFan(const bool& bOn);

	virtual int CCActivateRelays(const int& nXPlus, const int& nXMinus, const int& nYPlus, const int& nYMinus, const bool& bSynchronous, const bool& bAbort, const bool& bEndThread);

	virtual int CCPulseOut(unsigned int nPulse, bool bAdjust, const enumCameraIndex& Cam);

	virtual int CCSetShutter(bool bOpen);
	virtual int CCUpdateClock(void);

	virtual int CCSetImageProps(const enumCameraIndex& Camera, const enumWhichCCD& CCD, const int& nReadOut, void* pImage);	
	virtual int CCGetFullDynamicRange(const enumCameraIndex& Camera, const enumWhichCCD& CCD, unsigned long& dwDynRg);
	
	virtual void CCBeforeDownload(const enumCameraIndex& Cam, const enumWhichCCD& CCD);
	virtual void CCAfterDownload(const enumCameraIndex& Cam, const enumWhichCCD& CCD);
	//@} 

    virtual int PixelSize1x1InMicrons(const enumCameraIndex &Camera, const enumWhichCCD &CCD, double &x, double &y);

	//
	/*!\name ModalSettingsDialogInterface Implementation
	See ModalSettingsDialogInterface.*/
	//@{ 
	virtual int initModalSettingsDialog(void){return 0;}
	virtual int execModalSettingsDialog(void);
	//@} 
	
	//
	/*!\name X2GUIEventInterface Implementation
	See X2GUIEventInterface.*/
	//@{ 
	virtual void uiEvent(X2GUIExchangeInterface* uiex, const char* pszEvent);
	//@} 

    // AddFITSKeyInterface
    virtual int     countOfIntegerFields (int &nCount);
    virtual int     valueForIntegerField (int nIndex, BasicStringInterface &sFieldName, BasicStringInterface &sFieldComment, int &nFieldValue);
    virtual int     countOfDoubleFields (int &nCount);
    virtual int     valueForDoubleField (int nIndex, BasicStringInterface &sFieldName, BasicStringInterface &sFieldComment, double &dFieldValue);
    virtual int     countOfStringFields (int &nCount);
    virtual int     valueForStringField (int nIndex, BasicStringInterface &sFieldName, BasicStringInterface &sFieldComment, BasicStringInterface &sFieldValue);
    
    // SubframeInterface
    virtual int     CCSetBinnedSubFrame3(const enumCameraIndex &Camera, const enumWhichCCD &CCDOrig, const int &nLeft, const int &nTop,  const int &nWidth,  const int &nHeight);

    // CameraDependentSettingInterface
    virtual int     CCGetExtendedSettingName (const enumCameraIndex &Camera, const enumWhichCCD &CCDOrig, BasicStringInterface &sSettingName);
    virtual int     CCGetExtendedValueCount (const enumCameraIndex &Camera, const enumWhichCCD &CCDOrig, int &nCount);
    virtual int     CCGetExtendedValueName (const enumCameraIndex &Camera, const enumWhichCCD &CCDOrig, const int nIndex, BasicStringInterface &sName);
    virtual int     CCStartExposureAdditionalArgInterface (const enumCameraIndex &Cam, const enumWhichCCD CCD, const double &dTime, enumPictureType Type, const int &nABGState, const bool &bLeaveShutterAlone, const int &nIndex);

    // NoShutterInterface
    virtual int     CCHasShutter (const enumCameraIndex &Camera, const enumWhichCCD &CCDOrig, bool &bHasShutter);

	//Implemenation below here
private:

	SerXInterface 									*	GetSerX() {return m_pSerX; }		
	TheSkyXFacadeForDriversInterface				*	GetTheSkyXFacadeForDrivers() {return m_pTheSkyXForMounts;}
	SleeperInterface								*	GetSleeper() {return m_pSleeper; }
	BasicIniUtilInterface							*	GetBasicIniUtil() {return m_pIniUtil; }
	LoggerInterface									*	GetLogger() {return m_pLogger; }
	MutexInterface									*	GetMutex() const  {return m_pIOMutex;}
	TickCountInterface								*	GetTickCountInterface() {return m_pTickCount;}
    
	SerXInterface									*	m_pSerX;		
	TheSkyXFacadeForDriversInterface				*	m_pTheSkyXForMounts;
	SleeperInterface								*	m_pSleeper;
	BasicIniUtilInterface							*	m_pIniUtil;
	LoggerInterface									*	m_pLogger;
	MutexInterface									*	m_pIOMutex;
	TickCountInterface								*	m_pTickCount;

    double  mPixelSizeX;
    double  mPixelSizeY;

	int     m_nPrivateISIndex;

	double  m_dCurTemp;
    double  m_dCurSetPoint;
	double  m_dCurPower;
    
    int     m_nCurrentDialog;

    CSBigAluma      m_Camera;
    enumCameraIndex m_CameraIdx;
    enumWhichCCD    m_CCD_Sensor;

    int             m_nCameraID;
    char            m_szCameraSerial[128];

    std::vector<camera_info_t>           m_tCameraIdList;

    int     m_nReadoutMode;
    bool    m_bUseRBIFlash;
    int     doCamFeatureConfig();
    void    doSelectCamEvent(X2GUIExchangeInterface* uiex, const char* pszEvent);
    void    doSettingsCamEvent(X2GUIExchangeInterface* uiex, const char* pszEvent);

    int     mapWhichCCDToSensorId(enumWhichCCD CCD);
};



