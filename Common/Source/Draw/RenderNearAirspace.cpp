/*
   LK8000 Tactical Flight Computer -  WWW.LK8000.IT
   Released under GNU/GPL License v.2
   See CREDITS.TXT file for authors and copyrights

   $Id$
*/
#include "externs.h"
#include "RasterTerrain.h"
#include "LKAirspace.h"
#include "RGB.h"
#include "Sideview.h"
#include "Multimap.h"
#include "Dialogs.h"

#define GC_HORIZONTAL_TOLERANCE      100
#define GC_HORIZONTAL_THRESHOLD     2500
#define GC_VERTICAL_THRESHOLD        250
#define GC_VERTICAL_TOLERANCE         50
#define GC_HORIZONTAL_DELAY_FACT   250.0f
#define GC_VERTICAL_DELAY_FACT      25.0f

#define THICK_LINE 3

using std::min;
using std::max;


extern double fSplitFact;
extern double fOffset;
extern COLORREF  Sideview_TextColor;
extern AirSpaceSonarLevelStruct sSonarLevel[10];
extern AirSpaceSideViewSTRUCT Sideview_pHandeled[MAX_NO_SIDE_AS];
extern int Sideview_iNoHandeldSpaces;
extern bool Sonar_IsEnabled;
extern bool ActiveMap_IsEnabled;
extern int XstartScreen, YstartScreen;

TCHAR Sideview_szNearAS[80];
int iSonarLevel=0;
bool bNearAirspace_CheckAllAirspaces =false;


int CalcSonarDelay (int iNoAs, AirSpaceSideViewSTRUCT asAirspaces[], int a1, int a2);




/*********************************************************************
 * calc the sonar delay time
 *********************************************************************/
int CalcSonarDelay (int iNoAs, AirSpaceSideViewSTRUCT asAirspaces[], int iAltitudeAGL, int iAltitude)
{
int iAS_HorDist;
int iAS_VertDist;
int iAS_Bearing;
int i;
bool bAS_Inside = false;
bool bOK = false;

int iTreadLevel;
CAirspace SelectedAS;
CAirspace *Sel_AS_Ptr = NULL;

int	iH_Level = 1000;
int	iV_Level = 1000;
int	divider=1;


	if (ISPARAGLIDER) divider=2;

	for( i =  0 ; i < iNoAs ; i++)
	{

	  Sel_AS_Ptr =	asAirspaces[i].psAS;
	  if(Sel_AS_Ptr != NULL)
	  {
		SelectedAS = CAirspaceManager::Instance().GetAirspaceCopy( Sel_AS_Ptr );
		bOK = SelectedAS.GetDistanceInfo(bAS_Inside, iAS_HorDist, iAS_Bearing, iAS_VertDist);

		if(bOK)
		{
		  int iTmpV_Level = -1;
		  if((iAS_HorDist) < GC_HORIZONTAL_TOLERANCE)                          /* horizontal near or inside */
		  {
			int iTmp =	abs(iAS_VertDist);
			if(iTmp < sSonarLevel[9].iDistantrance)  {
			  iTmpV_Level = 9;
			  if(iTmp < sSonarLevel[8].iDistantrance)  {
			    iTmpV_Level = 8;
			    if(iTmp < sSonarLevel[7].iDistantrance)  {
			      iTmpV_Level = 7;
			      if(iTmp < sSonarLevel[6].iDistantrance)  {
			        iTmpV_Level = 6;
			        if(iTmp < sSonarLevel[5].iDistantrance)  {
			          iTmpV_Level = 5;
			        }
			      }
			    }
			  }
			}
		  }
		  if(iTmpV_Level != -1)
            if(iTmpV_Level < iV_Level )
        	  iV_Level = iTmpV_Level;


		  int iTmpH_Level = -1;
		  if(SelectedAS.IsAltitudeInside(iAltitude,iAltitudeAGL,GC_VERTICAL_TOLERANCE))  /* vertically near or inside ? */
		  {
			int iTmp =	(iAS_HorDist);
			if(iTmp > 0) {
		LKASSERT(divider!=0);
              if(iTmp < sSonarLevel[4].iDistantrance/divider)   {
                iTmpH_Level = 4;
                if(iTmp < sSonarLevel[3].iDistantrance/divider)   {
                  iTmpH_Level = 3;
                  if(iTmp < sSonarLevel[2].iDistantrance/divider)   {
                    iTmpH_Level = 2;
                    if(iTmp < sSonarLevel[1].iDistantrance/divider)   {
                      iTmpH_Level = 1;
                      if(iTmp < sSonarLevel[0].iDistantrance/divider)   {
                        iTmpH_Level = 0;
                      }
                    }
                  }
                }
              }
		    }
		}
		  if(iTmpH_Level != -1)
            if(iTmpH_Level < iH_Level )
        	  iH_Level = iTmpH_Level;

		  if(SelectedAS.IsAltitudeInside(iAltitude,iAltitudeAGL,0))  /* vertically inside ? */
			if(iAS_HorDist < 0)                                             /* complete inside, do no beep! */
			{
			  iV_Level = -1;   /* red allert */
			  iH_Level = -1;   /* red allert */
			}
		  }
	  }
	}

    iTreadLevel = iV_Level;
	if(iH_Level > -1)
	  if((iH_Level+5) <= iV_Level)
	    iTreadLevel = iH_Level;

    // StartupStore(_T("... HDist=%d Vdist=%d SonarLevel=%d \n"), iAS_HorDist, iAS_VertDist,iTreadLevel);
return iTreadLevel;
}





void  MapWindow::RenderNearAirspace(HDC hdc, const RECT rci)
{
RECT rc  = rci; /* rectangle for sideview */
RECT rct = rc; /* rectangle for topview */

rc.top     = (int)((double)(rci.bottom-rci.top  )*fSplitFact);
rct.bottom = rc.top ;
// Expose the topview rect size in use..
Current_Multimap_TopRect=rct;

HFONT	 hfOldFnt = (HFONT)SelectObject(hdc,LK8PanelUnitFont/* Sender->GetFont()*/);

int *iSplit = &Multimap_SizeY[Get_Current_Multimap_Type()];

int  k;
static double fZOOMScale= 1.0;
static double fHeigtScaleFact = 1.0;


  double GPSlat, GPSlon, GPSalt, GPSbrg, GPSspeed, calc_average30s;
  bool GPSValid;
  double calc_terrainalt;
  double calc_altitudeagl;

 // double alt;
  int calc_circling;
  TCHAR text[80];
  TCHAR buffer[80];

  CAirspace near_airspace;
  CAirspace *found = NULL;
//  bool bFound = false;
  DiagrammStruct sDia;
  bool bAS_Inside=false;
  int iAS_Bearing=0;
  int iAS_HorDistance=15000;
  int iABS_AS_HorDistance=0;
  int iAS_VertDistance=0;
  bool   bValid;
static  bool bHeightScale = false;
  long wpt_brg = 0;
  POINT line[2];
  POINT TxYPt;
  POINT TxXPt;
  SIZE tsize;
  COLORREF GREEN_COL     = RGB_GREEN;
  COLORREF RED_COL       = RGB_LIGHTORANGE;
  COLORREF BLUE_COL      = RGB_BLUE;
  COLORREF LIGHTBLUE_COL = RGB_LIGHTBLUE;
  BOOL bInvCol = true; //INVERTCOLORS


	  switch(LKevent) {
		case LKEVENT_NEWRUN:
			// CALLED ON ENTRY: when we select this page coming from another mapspace
			fZOOMScale = 1.0;
			fHeigtScaleFact = 1.0;
		break;
		case LKEVENT_UP:
			// click on upper part of screen, excluding center
			if(bHeightScale)
			  fHeigtScaleFact /= ZOOMFACTOR;
			else
			  fZOOMScale /= ZOOMFACTOR;
			break;

		case LKEVENT_DOWN:
			// click on lower part of screen,  excluding center
			if(bHeightScale)
			  fHeigtScaleFact *= ZOOMFACTOR;
			else
		  	  fZOOMScale *= ZOOMFACTOR;
			break;

		case LKEVENT_LONGCLICK:
		     for (k=0 ; k <= Sideview_iNoHandeldSpaces; k++)
			 {
			   if( Sideview_pHandeled[k].psAS != NULL)
			   {
				 if (PtInRect(XstartScreen,YstartScreen,Sideview_pHandeled[k].rc ))
				 {
				   if (EnableSoundModes)PlayResource(TEXT("IDR_WAV_BTONE4"));
				   dlgAirspaceDetails(Sideview_pHandeled[k].psAS);       // dlgA
				   //bFound = true;
				   return;
				 }
			   }
			 }
			 if (PtInRect(XstartScreen, YstartScreen,rc ))
			   bHeightScale = true;
			 if (PtInRect(XstartScreen, YstartScreen,rct ))
			   bHeightScale = false;
	     break;

		case LKEVENT_PAGEUP:
#ifdef OFFSET_SETP
			if(bHeightScale)
			  fOffset -= OFFSET_SETP;
			else
#endif
			{
			  if(*iSplit == SIZE1) *iSplit = SIZE0;
			  if(*iSplit == SIZE2) *iSplit = SIZE1;
			  if(*iSplit == SIZE3) *iSplit = SIZE2;
			}
		break;
		case LKEVENT_PAGEDOWN:
#ifdef OFFSET_SETP
			if(bHeightScale)
			  fOffset += OFFSET_SETP;
			else
#endif
			{
			  if(*iSplit == SIZE2) *iSplit = SIZE3;
			  if(*iSplit == SIZE1) *iSplit = SIZE2;
			  if(*iSplit == SIZE0) *iSplit = SIZE1;
			}
		break;

	  }
	  //LKevent=LKEVENT_NONE;

	  // Current_Multimap_SizeY is global, and must be used by all multimaps!
	  // It is defined in Multimap.cpp and as an external in Multimap.h
	  // It is important that it is updated, because we should resize terrain
	  // only if needed! Resizing terrain takes some cpu and some time.
	  // So we need to know when this is not necessary, having the same size of previous
	  // multimap, if we are switching.
	  // The current implementation is terribly wrong by managing resizing of sideview in
	  // each multimap: it should be done by a common layer.
	  // CAREFUL:
	  // If for any reason DrawTerrain() is called after resizing to 0 (full sideview)
	  // LK WILL CRASH with no hope to recover.
	  if(Current_Multimap_SizeY != *iSplit)
	  {
		Current_Multimap_SizeY=*iSplit;
		SetSplitScreenSize(*iSplit);
		rc.top     = (long)((double)(rci.bottom-rci.top  )*fSplitFact);
		rct.bottom = rc.top ;
	  }


  if(bInvCol)
  {
    GREEN_COL     = ChangeBrightness(GREEN_COL     , 0.6);
    RED_COL       = ChangeBrightness(RGB_RED       , 0.6);;
    BLUE_COL      = ChangeBrightness(BLUE_COL      , 0.6);;
    LIGHTBLUE_COL = ChangeBrightness(LIGHTBLUE_COL , 0.4);;
  }
  //LockFlightData();
  {
    GPSlat = DrawInfo.Latitude;
    GPSlon = DrawInfo.Longitude;
    GPSalt = DrawInfo.Altitude;
    GPSbrg = DrawInfo.TrackBearing;
    GPSspeed = DrawInfo.Speed;
    GPSValid = !DrawInfo.NAVWarning;
    calc_circling    = DerivedDrawInfo.Circling;
    calc_terrainalt  = DerivedDrawInfo.TerrainAlt;
    calc_altitudeagl = DerivedDrawInfo.AltitudeAGL;
    calc_average30s  = DerivedDrawInfo.Average30s;
    GPSalt =  DerivedDrawInfo.NavAltitude;
  }
  //UnlockFlightData();
calc_circling = false;
  bValid = false;
  iAS_HorDistance = 5000;
  iAS_Bearing     = (int)GPSbrg;
  iAS_VertDistance= 0;
  found = CAirspaceManager::Instance().GetNearestAirspaceForSideview();
  if(found != NULL) {
    near_airspace = CAirspaceManager::Instance().GetAirspaceCopy(found);
    bValid = near_airspace.GetDistanceInfo(bAS_Inside, iAS_HorDistance, iAS_Bearing, iAS_VertDistance);
  }
//if(bValid)
//  near_airspace.CalculateDistance(&iAS_sHorDistance,&iAS_VertDistance, &iAS_Bearing);
 // if(bLeft)
 // fAS_HorDistance = fabs(fAS_HorDistance);
   iABS_AS_HorDistance = abs( iAS_HorDistance);
  wpt_brg = (long)AngleLimit360(GPSbrg - iAS_Bearing + 90.0);


//  bool CAirspace::GetWarningPoint(double &longitude, double &latitude, AirspaceWarningDrawStyle_t &hdrawstyle, int &vDistance, AirspaceWarningDrawStyle_t &vdrawstyle) const
//  if(near_airspace.IsAltitudeInside(alt,calc_altitudeagl,0) && near_airspace.IsAltitudeInside(GPSlon,GPSlat))
//    bAS_Inside = true;
/*
  int iHor,iVer,  iBear;
   near_airspace.CalculateDistance(&iHor,&iVer, &iBear);

  fAS_HorDistance = (double) iHor;
  fAS_Bearing     = (double) iBear;
  iAS_VertDistance= iVer;
*/
  // Variables from ASP system here contain the following informations:
  // fAS_HorDistance - always contains horizontal distance from the asp, negative if horizontally inside (This does not mean that we're inside vertically as well!)
  // fAS_Bearing - always contains bearing to the nearest horizontal point
  // bValid - true if bAS_Inside, iAS_HorDistance, iAS_Bearing, iAS_VertDistance contains valid informations
  //          this will be true if the asp border is close enough to be tracked by the warning system
  // bAS_Inside - current position is inside in the asp, calculated by the warning system
  // iAS_HorDistance - horizontal distance to the nearest horizontal border, negative if horizontally inside, calculated by the warning system
  // iAS_Bearing - bearing to the nearest horizontal border, calculated by the warning system
  // iAS_VertDistance - vertical distance to the nearest asp border, negative if the border is above us, positive if the border below us, calculated by the warning system
  // near_airspace.WarningLevel():
  //           awNone - no warning condition exists
  //           awYellow - current position is near to a warning position
  //           awRed - current posisiton is forbidden by asp system, we are in a warning position

  /*********************************************************************
   * calc sonar delay
   *********************************************************************/

  iSonarLevel = -1;
  if(bValid)
    if(Sonar_IsEnabled)
      if(GPSValid) {
	    #if TESTBENCH
	    if(1)
	    #else
	    if(DerivedDrawInfo.FreeFlying)
	    #endif
	    {
	      if(bNearAirspace_CheckAllAirspaces)
	        iSonarLevel = CalcSonarDelay(Sideview_iNoHandeldSpaces, Sideview_pHandeled, (int)DerivedDrawInfo.AltitudeAGL, (int)DerivedDrawInfo.NavAltitude );
	      else
	      {
	    	AirSpaceSideViewSTRUCT Tmp;
	    	Tmp.psAS =  &near_airspace;
    	    iSonarLevel = CalcSonarDelay( 1, &Tmp, (int)DerivedDrawInfo.AltitudeAGL, (int)DerivedDrawInfo.NavAltitude);
	      }
	    }
      }

  /*********************************************************************
   * calc the horizontal zoom
   *********************************************************************/
  sDia.fXMin = -5000.0;
  sDia.fXMax =  5000.0;
  /* even when invalid the horizontal distance is calculated correctly */


  if(bValid)
  {
	double fScaleDist = iABS_AS_HorDistance;
	double fDist;
	if(  calc_circling  > 0)
	  fDist = (double)(fScaleDist/1000+1) * 1500.0f;   // zoom fix
	else
	  fDist = (double)(fScaleDist) * 1.5;

	sDia.fXMin = min(-2500.0 , fScaleDist * 1.5 );
	sDia.fXMax = max( 2500.0 , fScaleDist * 1.5 );

	#ifdef NEAR_AS_ZOOM_1000M
	if(((iABS_AS_HorDistance) < 900) && (bValid)) // 1km zoom
	{
	  sDia.fXMin = min(-900.0, fScaleDist * 1.5 );
	  sDia.fXMax = max( 900.0, fScaleDist * 1.5 );

	}
	#endif
	#ifdef NEAR_AS_ZOOM_1000FT
	  if((abs(iABS_AS_HorDistance) < 333)) // 1000ft zoom
	  {
		sDia.fXMin = min(-333.0, fScaleDist * 1.5 );
		sDia.fXMax = max( 333.0, fScaleDist * 1.5 );
	  }
	#endif

  }


#define RND_FACT 10.0
 //  int iTmp =  (int) (sDia.fXMax / RND_FACT);
  // sDia.fXMax = iTmp * RND_FACT;

  // iTmp =  (int) (sDia.fXMin / RND_FACT);
  // sDia.fXMin = iTmp * RND_FACT;


   if( ( sDia.fXMax  *fZOOMScale) > 100000)
	  fZOOMScale /= ZOOMFACTOR;

   if(( sDia.fXMax *fZOOMScale) < 2000)
   {
	  fZOOMScale *= ZOOMFACTOR;
   }

  double fOldZoomScale=-1;
  if(fZOOMScale != fOldZoomScale)
  {
   fOldZoomScale =  fZOOMScale;
   sDia.fXMax = sDia.fXMax *fZOOMScale;
   sDia.fXMin = -sDia.fXMax /5;
  }
 //  if(( sDia.fXMax ) < 5000)
 //    sDia.fXMin -= sDia.fXMax;


  /*********************************************************************
   * calc the vertical zoom
   *********************************************************************/
sDia.fYMin = max(0.0, GPSalt-2300);
sDia.fYMax = max(MAXALTTODAY, GPSalt+1000);

if(bValid)
{
 // double fTop    = near_airspace.Top()->Altitude;
  double fBottom = near_airspace.Base()->Altitude;
  sDia.fYMin = min(fBottom*0.8, sDia.fYMin );
  sDia.fYMin = max(0.0, sDia.fYMin );
  if(sDia.fYMin < 300) sDia.fYMin =0;
  sDia.fYMax = max((fBottom*1.2f), sDia.fYMax );

  if(abs(iAS_VertDistance) < 250)
  {
  //  if(ExternalTriggerCircling)
    sDia.fYMax =  ((int)((GPSalt+abs(iAS_VertDistance))/400) + 2) *400 ;
    sDia.fYMin =  ((int)((GPSalt-abs(iAS_VertDistance))/400) - 1) *400 ;
    if(sDia.fYMin-MIN_ALTITUDE < 0)
      sDia.fYMin = 0;
  }

#ifdef VERTICAL_ZOOM_50
  if(abs(iAS_VertDistance) < 50)
  {
    sDia.fYMax =  ((int)((GPSalt+abs(iAS_VertDistance))/100) + 2) *100 ;
    sDia.fYMin =  ((int)((GPSalt-abs(iAS_VertDistance))/100) - 1) *100 ;
    if(sDia.fYMin-MIN_ALTITUDE < 0)
      sDia.fYMin = 0;
  }
#endif
  sDia.fYMin = max((double)0.0f,(double) sDia.fYMin);

#ifdef OFFSET_SETP
	  if(( sDia.fYMax + fOffset) > MAX_ALTITUDE)
		fOffset -= OFFSET_SETP;
	  if(( sDia.fYMin + fOffset) < 0.0)
		fOffset += OFFSET_SETP;

	  sDia.fYMin +=  fOffset;
	  sDia.fYMax +=  fOffset;
#endif
  if(fHeigtScaleFact * sDia.fYMax > MAX_ALTITUDE )
	  fHeigtScaleFact /=ZOOMFACTOR;

  if(fHeigtScaleFact * sDia.fYMax < MIN_ALTITUDE )
	  fHeigtScaleFact *=ZOOMFACTOR;
  sDia.fYMax *= fHeigtScaleFact;
}


/****************************************************************************************************
 * draw topview first
 ****************************************************************************************************/

  if(fSplitFact > 0.0)
  {

    sDia.rc = rct;
    sDia.rc.bottom-=1;
    AirspaceTopView(hdc, &sDia, (double) iAS_Bearing, (double) wpt_brg);
  }

  /****************************************************************************************************
   * draw airspace and terrain elements
   ****************************************************************************************************/
  RECT rcc = rc;          /* rc corrected      */
  if(sDia.fYMin < GC_SEA_LEVEL_TOLERANCE)
    rcc.bottom -= SV_BORDER_Y; /* scale witout sea  */
  sDia.rc = rcc;

  RenderAirspaceTerrain( hdc, GPSlat, GPSlon, iAS_Bearing, &sDia );

  HFONT hfOld = (HFONT)SelectObject(hdc, LK8InfoNormalFont);
  if(bValid)
    _stprintf(Sideview_szNearAS,TEXT("%s"),  near_airspace.Name() );
  else
  {
	_stprintf(text,TEXT("%s"), gettext(TEXT("_@M1259_"))); 	 // LKTOKEN _@M1259_ "Too far, not calculated"
	GetTextExtentPoint(hdc, text, _tcslen(text), &tsize);
	TxYPt.x = (rc.right-rc.left-tsize.cx)/2;
	TxYPt.y = (rc.bottom-rc.top)/2;

	SetBkMode(hdc, TRANSPARENT);
	ExtTextOut(hdc,TxYPt.x, TxYPt.y-20, ETO_OPAQUE, NULL, text, _tcslen(text), NULL);

	_stprintf(Sideview_szNearAS,TEXT("%s"), text);

  }
  SelectObject(hdc, hfOld);
  /****************************************************************************************************
   * draw airspace and terrain elements
   ****************************************************************************************************/

  /****************************************************************************************************
   * draw diagram
   ****************************************************************************************************/
  double xtick = 1.0;
  double fRange =fabs(sDia.fXMax - sDia.fXMin) ;
  if (fRange>3.0*1000.0) xtick = 2.0;
  if (fRange>15*1000.0) xtick = 5.0;
  if (fRange>50.0*1000.0) xtick = 10.0;
  if (fRange>100.0*1000.0) xtick = 20.0;
  if (fRange>200.0*1000.0) xtick = 25.0;
  if (fRange>250.0*1000.0) xtick = 50.0;
  if (fRange>500.0*1000.0) xtick = 100.0;


  if(bInvCol)
  {
    SelectObject(hdc, GetStockObject(BLACK_PEN));
    SelectObject(hdc, GetStockObject(BLACK_BRUSH));
  }
  else
  {
    SelectObject(hdc, GetStockObject(WHITE_PEN));
    SelectObject(hdc, GetStockObject(WHITE_BRUSH));
  }

  COLORREF txtCol = GROUND_TEXT_COLOUR;
  if(bInvCol)
    if(sDia.fYMin > GC_SEA_LEVEL_TOLERANCE)
    	txtCol = INV_GROUND_TEXT_COLOUR;
  SetBkMode(hdc, TRANSPARENT);
  SetTextColor(hdc, txtCol);
  _stprintf(text, TEXT("%s"),Units::GetUnitName(Units::GetUserDistanceUnit()));
  DrawXGrid(hdc, rci, xtick/DISTANCEMODIFY, xtick, 0,TEXT_ABOVE_LEFT, RGB_BLACK,  &sDia,text);


  SetTextColor(hdc, Sideview_TextColor);

  double  fHeight = (sDia.fYMax-sDia.fYMin);
  double  ytick = 100.0;
  if (fHeight >500.0) ytick = 200.0;
  if (fHeight >1000.0) ytick = 500.0;
  if (fHeight >2000.0) ytick = 1000.0;
  if (fHeight >4000.0) ytick = 2000.0;
  if (fHeight >8000.0) ytick = 4000.0;

  if(Units::GetUserAltitudeUnit() == unFeet)
	 ytick = ytick * FEET_FACTOR;

  _stprintf(text, TEXT("%s"),Units::GetUnitName(Units::GetUserAltitudeUnit()));
  DrawYGrid(hdc, rc, ytick/ALTITUDEMODIFY,ytick, 0,TEXT_UNDER_RIGHT ,Sideview_TextColor,  &sDia, text);


  if(!bInvCol)
    SetBkMode(hdc, OPAQUE);
  /****************************************************************************************************
   * draw AGL
   ****************************************************************************************************/
  if(calc_altitudeagl - sDia.fYMin  > 500)
  {
    SetTextColor(hdc, LIGHTBLUE_COL);
    Units::FormatUserAltitude(calc_altitudeagl, buffer, 7);
    LK_tcsncpy(text, gettext(TEXT("_@M1742_")), sizeof(text)/sizeof(text[0])-1); // AGL:
    _tcscat(text,buffer);
    GetTextExtentPoint(hdc, text, _tcslen(text), &tsize);
    TxYPt.x = CalcDistanceCoordinat(0,  &sDia)- tsize.cx/2;
    TxYPt.y  = CalcHeightCoordinat(  (calc_terrainalt + calc_altitudeagl )*0.8,  &sDia );
    if((tsize.cy) < ( CalcHeightCoordinat(  calc_terrainalt, &sDia )- TxYPt.y )) {
      ExtTextOut(hdc,  TxYPt.x+IBLSCALE(1),  TxYPt.y , ETO_OPAQUE, NULL, text, _tcslen(text), NULL);
    }
  }

  SetBkMode(hdc, TRANSPARENT);

  /****************************************************************************************************
   * Print current Elevation
   ****************************************************************************************************/
  SetTextColor(hdc, RGB_BLACK);
  int x,y;
  if((calc_terrainalt-  sDia.fYMin)  > 0)
  {
	Units::FormatUserAltitude(calc_terrainalt, buffer, 7);
    LK_tcsncpy(text, gettext(TEXT("_@M1743_")), sizeof(text)/sizeof(text[0])-1);   // ELV:
    _tcscat(text,buffer);
    GetTextExtentPoint(hdc, text, _tcslen(text), &tsize);
    x = CalcDistanceCoordinat(0, &sDia) - tsize.cx/2;
    y = CalcHeightCoordinat( calc_terrainalt,  &sDia  );
    if ((ELV_FACT*tsize.cy) < abs(rc.bottom - y)) {
      ExtTextOut(hdc, x, rc.bottom -(int)(ELV_FACT * tsize.cy) , ETO_OPAQUE, NULL, text, _tcslen(text), NULL);
    }
  }


  /****************************************************************************************************
   * draw side elements
   ****************************************************************************************************/
  SetTextColor(hdc, Sideview_TextColor);
 // if(!bInvCol)
  SetBkMode(hdc, OPAQUE);
  HFONT hfOld2 = (HFONT)SelectObject(hdc, LK8InfoNormalFont);


//  DrawTelescope      ( hdc, iAS_Bearing-90.0, rc.right  - NIBLSCALE(13),  rc.top   + NIBLSCALE(58));

  SelectObject(hdc, hfOld2);
  SetBkMode(hdc, TRANSPARENT);

  SelectObject(hdc, hfOld);
  SetTextColor(hdc, GROUND_TEXT_COLOUR);
  if(bInvCol)
    if(sDia.fYMin > GC_SEA_LEVEL_TOLERANCE)
	  SetTextColor(hdc, INV_GROUND_TEXT_COLOUR);



  /****************************************************************************************************/
  /****************************************************************************************************/
  /****************************************************************************************************
   * draw distances to next airspace
   ****************************************************************************************************/
  /****************************************************************************************************/
  /****************************************************************************************************/
  if (bValid)
  {

	/****************************************************************************************************
	 * draw horizontal distance to next airspace
	 ****************************************************************************************************/
	SetTextColor(hdc, Sideview_TextColor);
	SetBkMode(hdc, OPAQUE);
	HFONT hfOldU = (HFONT)SelectObject(hdc, LK8InfoNormalFont);
    // horizontal distance
    line[0].x = CalcDistanceCoordinat(0, &sDia);
    line[0].y = CalcHeightCoordinat(  GPSalt,  &sDia );
    line[1].x = CalcDistanceCoordinat(iABS_AS_HorDistance, &sDia);
    line[1].y = line[0].y;
    DrawDashLine(hdc,THICK_LINE, line[0], line[1],  Sideview_TextColor, rc);
    if(iAS_HorDistance < 0)
    {
      line[0].y = CalcHeightCoordinat(  GPSalt - (double)iAS_VertDistance, &sDia );
      line[1].y = line[0].y;

      DrawDashLine(hdc,THICK_LINE, line[0], line[1],  Sideview_TextColor, rc);
    }

    bool bLeft = false;
    if( line[0].x < line[1].x)
      bLeft = false;
    else
      bLeft = true;

    Units::FormatUserDistance(iABS_AS_HorDistance, buffer, 7);
    LK_tcsncpy(text, TEXT(" "), sizeof(text)/sizeof(text[0])-1);
    _tcscat(text,buffer);
    GetTextExtentPoint(hdc, text, _tcslen(text), &tsize);

    if((GPSalt- sDia.fYMin /*-calc_terrainalt */) < 300)
      TxXPt.y = CalcHeightCoordinat(  GPSalt, &sDia ) -  tsize.cy;
    else
      TxXPt.y = CalcHeightCoordinat(  GPSalt, &sDia ) +  NIBLSCALE(3);


    if(tsize.cx > (line[1].x - line[0].x) )
      TxXPt.x = CalcDistanceCoordinat( iABS_AS_HorDistance ,&sDia) -tsize.cx-  NIBLSCALE(3);
    else
      TxXPt.x = CalcDistanceCoordinat( iABS_AS_HorDistance / 2.0, &sDia) -tsize.cx/2;
    ExtTextOut(hdc,  TxXPt.x,  TxXPt.y , ETO_OPAQUE, NULL, text, _tcslen(text), NULL);



	/****************************************************************************************************
	 * draw vertical distance to next airspace
	 ****************************************************************************************************/
    line[0].x = CalcDistanceCoordinat( iABS_AS_HorDistance , &sDia);
    line[0].y = CalcHeightCoordinat( GPSalt, &sDia );
    line[1].x = line[0].x;
    line[1].y = CalcHeightCoordinat( GPSalt - (double)iAS_VertDistance, &sDia );

    DrawDashLine(hdc,THICK_LINE, line[0], line[1],  Sideview_TextColor, rc);
    Units::FormatUserAltitude( (double)abs(iAS_VertDistance), buffer, 7);
    LK_tcsncpy(text, TEXT(" "), sizeof(text)/sizeof(text[0])-1);
    _tcscat(text,buffer);
    GetTextExtentPoint(hdc, text, _tcslen(text), &tsize);

    if ( bLeft )
      TxYPt.x = CalcDistanceCoordinat(iABS_AS_HorDistance,  &sDia)- tsize.cx - NIBLSCALE(3);
    else
      TxYPt.x = CalcDistanceCoordinat(iABS_AS_HorDistance,  &sDia)+ NIBLSCALE(5);
    if( abs( line[0].y -  line[1].y) > tsize.cy)
      TxYPt.y = CalcHeightCoordinat( GPSalt - (double)iAS_VertDistance/2.0, &sDia) -tsize.cy/2 ;
    else
      TxYPt.y = min( line[0].y ,  line[1].y) - tsize.cy ;
    ExtTextOut(hdc,  TxYPt.x,  TxYPt.y , ETO_OPAQUE, NULL, text, _tcslen(text), NULL);
	SelectObject(hdc, hfOldU);
  }

 /****************************************************************************************************
  * draw plane sideview at least
  ****************************************************************************************************/
  RenderPlaneSideview( hdc, 0.0 , GPSalt,wpt_brg, &sDia );



  hfOldFnt = (HFONT)SelectObject(hdc,LK8InfoNormalFont/* Sender->GetFont()*/);
  //DrawNorthArrow     ( hdc, iAS_Bearing-90        , rct.right - NIBLSCALE(11),  rct.top  + NIBLSCALE(11));
  DrawCompass( hdc, rct, iAS_Bearing-90);


  /****************************************************************************************************
   * draw selection frame
   ****************************************************************************************************/
  if(bHeightScale)
	DrawSelectionFrame(hdc,  rc);
#ifdef TOP_SELECTION_FRAME
  else
	DrawSelectionFrame(hdc,  rci);
#endif
  SelectObject(hdc,hfOldFnt/* Sender->GetFont()*/);
  SetBkMode(hdc, TRANSPARENT);
  SelectObject(hdc,hfOldFnt/* Sender->GetFont()*/);
}


