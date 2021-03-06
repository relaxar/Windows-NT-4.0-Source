/*** 
*macmain.cpp
*
*  Copyright (C) 1992-1994, Microsoft Corporation.  All Rights Reserved.
*
*Purpose:
*
*Implementation Notes:
*
*****************************************************************************/

#include "dispdemo.h"

#include <stdio.h>

extern "C" {

Boolean g_fInitOle = false;
#ifndef _PPCMAC
Boolean g_fInitLibraryManager = false;
#endif //_PPCMAC

WindowPtr g_pwndClient = nil;
WindowPtr g_pwndDebug = nil;

}

void Init(void);
void EventLoop(void);

void AdjustMenus(void);
void Close(WindowPtr window);
void DoEvent(EventRecord *pevent);
void DoMenuCommand(long menuResult);
void Quit(void);
#ifndef _MSC_VER
#ifndef ConstStr255Param
#define ConstStr255Param StringPtr
#endif
#endif
void Fatal(ConstStr255Param);

Boolean
IsAppWindow(WindowPtr window)
{
    return (window == nil)
      ? false : ((WindowPeek)window)->windowKind == userKind;
}

Boolean
IsDAWindow(WindowPtr window)
{
    return (window == nil)
      ? false : (((WindowPeek)window)->windowKind < 0);
}

void
main()
{
    Init();
    EventLoop();
}

void
EventLoop()
{
    EventRecord	event;
    RgnHandle cursorRgn;

    cursorRgn = NewRgn();
    while(1){
      if(WaitNextEvent(everyEvent, &event, MAXLONG, cursorRgn))
	DoEvent(&event);
    }
}

void
DoEvent(EventRecord *pevent)
{
    char key;
    short part;
    WindowPtr window;

    switch(pevent->what){
    case mouseDown:
      part = FindWindow(pevent->where, &window);
      switch(part){
      case inMenuBar:
	AdjustMenus();
	DoMenuCommand(MenuSelect(pevent->where));
	break;

      case inSysWindow:	/* let the system handle the mouseDown */
	SystemClick(pevent, window);
	break;

      case inContent:
	if(window != FrontWindow()){
	  SelectWindow(window);
	}
	break;

      case inDrag:
	DragWindow(window, pevent->where, &qd.screenBits.bounds);
	break;
      }
      break;

    case keyDown:
    case autoKey:			/* check for menukey equivalents */
      key = (char)(pevent->message & charCodeMask);
      if(pevent->modifiers & cmdKey){	/* Command key down */
	if(pevent->what == keyDown){
	  /* enable/disable/check menu items properly */
	  AdjustMenus();
	  DoMenuCommand(MenuKey(key));
	}
      }
      break;

    case updateEvt:
      window = (WindowPtr)pevent->message;
      if(IsAppWindow(window)){
        BeginUpdate(window);
        if(!EmptyRgn(window->visRgn)){
          SetPort(window);
          EraseRect(&window->portRect);
        }
        EndUpdate(window);
      }
      break;

    case kHighLevelEvent:
      AEProcessAppleEvent(pevent);
      break;
    }
}

void
Enable(MenuHandle hmenu, short sItem, Boolean fEnable)
{
    if(fEnable)
      EnableItem(hmenu, sItem);
    else
      DisableItem(hmenu, sItem);
}

void
AdjustMenus()
{
    Boolean fIsDA;
    MenuHandle hmenu;

    fIsDA = IsDAWindow(FrontWindow());

    /* we can allow desk accessories to be closed from the menu */
    hmenu = GetMHandle(mFile);
    Enable(hmenu, iClose, fIsDA);

    hmenu = GetMHandle(mEdit);
    Enable(hmenu, iUndo,  fIsDA);
    Enable(hmenu, iCut,   fIsDA);
    Enable(hmenu, iCopy,  fIsDA);
    Enable(hmenu, iClear, fIsDA);
    Enable(hmenu, iPaste, fIsDA);
}

void
DoMenuCommand(long menuResult)
{
    short menuID;		/* the resource ID of the selected menu */
    short menuItem;		/* the item number of the selected menu */
    Str255 daName;

    menuID = HiWord(menuResult);
    menuItem = LoWord(menuResult);

    switch(menuID){
    case mApple:
      switch(menuItem){
      case iAbout:
	Alert(rAboutAlert, nil);
	break;
      default:		/* all non-About items in this menu are DAs */
	GetMenuItemText(GetMHandle(mApple), menuItem, daName);
	OpenDeskAcc(daName);
	break;
      }
      break;

    case mFile:
      switch(menuItem){
      case iClose:
	Close(FrontWindow());
	break;
      case iQuit:
	Quit();
	break;
      }
      break;

    case mEdit:
      SystemEdit(menuItem-1);
      break;

    case mSpoly:
      DoPoly(CLSID_CPoly);
      break;

    case mSpoly2:
      DoPoly(CLSID_CPoly2);
      break;
    }

    HiliteMenu(0);
}

void
Close(WindowPtr window)
{
    if(IsDAWindow(window))
      CloseDeskAcc(((WindowPeek)window)->windowKind);
    else if(IsAppWindow(window))
      CloseWindow(window);
}

#if defined(_MSC_VER)
OSErr pascal
#else
pascal OSErr
#endif
RemoteLowLevelEvt(AppleEvent theAppEvt, AppleEvent reply, long HandlerRefCon)
{
    long cb;
    OSErr err;
    DescType descType;
    EventRecord event;

    UNUSED(reply);
    UNUSED(HandlerRefCon);

    err = AEGetKeyPtr(
      &theAppEvt,
      keyDirectObject,
      typeWildCard,
      &descType,
      (Ptr)&event, sizeof(event), &cb);

    if(err != noErr)
      return err;

    DoEvent(&event);

    return noErr;
}

void
Init()
{
    Handle menuBar;

    MaxApplZone();

    InitGraf((Ptr)&qd.thePort);
    InitFonts();
    InitWindows();
    InitMenus();
    TEInit();
    InitDialogs(nil);
    InitCursor();
    FlushEvents(everyEvent, 0);

#ifndef _PPCMAC
    if (InitOleManager(0) != NOERROR)
      Fatal((ConstStr255Param)"\pCould not initialize OLE Applet");

    g_fInitLibraryManager = true;
#endif //_PPCMAC

    if(InitOle() != NOERROR)
      Fatal((ConstStr255Param)"\pUnable to Initialize Ole");
    g_fInitOle = true;

    if(AEInstallEventHandler('OLE2', 'EVNT', (EventHandlerProcPtr)RemoteLowLevelEvt, 0, false) != noErr)
      Fatal((ConstStr255Param)"\pUnable to install handler");

    if((g_pwndClient = (WindowPtr)NewPtr(sizeof(WindowRecord))) == nil)
      Fatal((ConstStr255Param)"\pOut of memory");
    g_pwndClient = GetNewCWindow(rWindow, (Ptr)g_pwndClient, (WindowPtr)-1);

    g_pwndDebug = g_pwndClient;

    if((menuBar = GetNewMBar(rMenuBar)) == nil)
      Fatal((ConstStr255Param)"\pUnable to load menu bar");
    SetMenuBar(menuBar);
    DisposHandle(menuBar);
    AddResMenu(GetMHandle(mApple), 'DRVR'); /* add DA names to Apple menu */
    DrawMenuBar();
}

void
Quit()
{
    if(g_fInitOle)
      UninitOle();
#ifndef _PPCMAC 
    if(g_fInitLibraryManager)
      UninitOleManager();			// clean up applet
#endif //_PPCMAC
    ExitToShell();
}

/* display fatal error alert, and exit */
void
Fatal(ConstStr255Param msg)
{
    SetCursor(&qd.arrow);
    ParamText(msg, (ConstStr255Param)"\p", (ConstStr255Param)"\p", (ConstStr255Param)"\p");
    Alert(rUserAlert, nil);
    Quit();
}

