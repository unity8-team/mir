/*
 * atifillin.c: fill in a ScrnInfoPtr with the relevant information for
 * atimisc.
 *
 * (c) 2004 Adam Jackson.  Standard MIT license applies.
 */

#include "atifillin.h"

void ATIFillInScreenInfo(ScrnInfoPtr pScreenInfo)
{
    pScreenInfo->driverVersion = ATI_VERSION_CURRENT;
    pScreenInfo->driverName    = ATI_DRIVER_NAME;
    pScreenInfo->name          = ATI_NAME;
    pScreenInfo->Probe         = ATIProbe;
    pScreenInfo->PreInit       = ATIPreInit;
    pScreenInfo->ScreenInit    = ATIScreenInit;
    pScreenInfo->SwitchMode    = ATISwitchMode;
    pScreenInfo->AdjustFrame   = ATIAdjustFrame;
    pScreenInfo->EnterVT       = ATIEnterVT;
    pScreenInfo->LeaveVT       = ATILeaveVT;
    pScreenInfo->FreeScreen    = ATIFreeScreen;
    pScreenInfo->ValidMode     = ATIValidMode;
}
