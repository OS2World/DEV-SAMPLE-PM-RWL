/**********************************************************************/
/*                                                                    */
/*  RWL - A demonstration program placed in the Public Domain         */
/*        by it author, R L Walsh, June 10, 2000                      */
/*                                                                    */
/**********************************************************************/
#if 0

    RWL lets the user view and change the BeginLIBPATH and EndLIBPATH
    for any running PM-based process.  Changing these for the first
    instance of pmshell.exe has much the same effect on newly-launched
    programs as updating your global LIBPATH would.  This may enable
    you to begin using newly installed programs without having to
    reboot to actually update your LIBPATH.

    This program is intended to demonstrate that a PM-based process
    can selectively enter any other PM process at will by hooking
    one of the target`s message queues.  Once its code is operating
    in the target process, the program can do whatever its author
    deems prudent.  It also demonstrates a method for identifying
    threads that have message queues.

    Because RWL is a demonstration of a previously undocumented
    "wormhole" in Warp, the author makes no claims that RWL or the
    technique it demonstrates are ever suitable for use in any way.
    Nonetheless, it should be noted that this method was used by
    IBM in their implementation of Netscape v4.61 to support its
    proprietary but WPS-aware drag and drop.

/**********************************************************************/

Overview
==========

    RWL consists of two modules:
     o  rwl.exe -> rwlx.c   (most functionality)
     o  rwl.dll -> rwld.c   (hook code, get/set LIBPATH)

    rwlx starts by identifying one window for each PM process. If a
    process has several threads with message queues, it chooses a
    window belonging to the lowest-numbered thread (usually TID 1).
    It then inserts the PID and its corresponding HWND in a listbox.

    When the user selects a new PID, rwlx releases any hook currently
    in place, then hooks the queue associated with that PID`s hwnd.
    The hook code itself is in rwl.dll.

    rwlx conducts its transactions with the target process by posting
    a unique message to the target queue.  The msg ID is an atom
    derived from the string "RWLM_X2D".  Its params are a pointer to
    an RWL structure in gettable shared memory, and the hwnd to which
    it should post a reply message.  The hook intercepts this message,
    accesses the memory, and executes what ever command is indicated.
    It then frees the memory and posts its reply.  When the dialog
    receives the response, it either displays the returned string or
    posts another message requesting additional data.

Implementation
================

    While RWL currently uses a single dialog for all processes,
    it was designed with multiple windows in mind, one per process.
    In that environment, each process accessed would need its own
    i/o buffer in shared memory.  Rather than have mutiple 64k
    allocations when only 1k was needed, I opted for an array of
    "RWL" structures containing process info and a 1k string buffer,
    all stored in a single gettable segment.  This allows simultaneous
    access to 62 processes while maintaining undo info for each.
    Thereafter, RWL will replace the least-recently-used entry.

    This code is acceptable to my antique C-only compiler with
    most warnings on, but it may be nearly unuseable with yours.
    Similarly, I haven`t included any makefiles because I suspect
    they`d be useless too.  My apologies...

    IMPORTANT(?):  I always staticly link dlls like rwl.dll (rwld.c)
    with my compiler`s "subsystem" library.  Eliminating the runtime
    environment eliminates a lot of headaches (and functionality).

    rwld.c offers some additional comments.

Style
=======

    Most of my functions are structured like this:

    do {
        resource allocation & init;
        if (error)
            break;
        processing;
        if (done)
            break;
        more processing;
    } while (FALSE);
        error reporting;
        resource deallocation;
        return;

    Use of this structure, with its dummy do-while loop,
    ensures that:
     o  resources will always be deallocated
     o  error conditions are easy to identify and report
     o  code is more readable, with minimal nesting

    Whenever an error occurs or processing is complete, a "break"
    statement will skip over the remaining processing and goto
    finalization.  The only demands this style makes is that key
    variables be initialized to a known value (usually zero) so
    resources can be deallocated accordingly.

#endif
/**********************************************************************/

//  RWLX.C

/**********************************************************************/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "rwl.h"        // most stuff (shared with rwld.c)
#include "rwlx.h"       // dialog IDs

/****************************************************************************/

// init
int     main( void);
BOOL    RWLXInit( void);

// primary dialog functions
MRESULT _System MainWndProc( HWND hwnd, ULONG msg, MPARAM mp1, MPARAM mp2);
BOOL    RWLXGetLibpath( HWND hwnd);
BOOL    RWLXSetLibpath( HWND hwnd);
BOOL    RWLXUndo( HWND hwnd);
BOOL    RWLXReply( HWND hwnd, PRWL prwl, ULONG rc);
void    RWLXEnableBtns( HWND hwnd, BOOL fEnable);

// process selection & tracking
BOOL    RWLXSelectPID( HWND hwnd);
PRWL    RWLXGetPRWL( HWND hHook);
BOOL    RWLXSwitchPIDs( HWND hwnd, PRWL prwlNew);
PRWL    RWLXValidateRWL( PRWL prwl);
PRWL    RWLXNextRWL( PRWL prwl);
PRWL    RWLXAllocRWL( void);
void    RWLXFreeRWL( PRWL prwl);

// process identification
BOOL    RWLXUpdatePIDs( HWND hwnd, PID pidSelect);
LONG    RWLXEnumPIDs( PAPP paApp, ULONG cntApp);
void    RWLXListPIDs( HWND hLB, PID pidSelect, PAPP paApp);

// errors
BOOL    ErrBox( PSZ pTtl, PSZ pErr);
BOOL    RcErrBox( PSZ pTtl, PSZ pErr, ULONG rc);

/****************************************************************************/

// miscellanea
BOOL        fFalse = FALSE;     // my compiler can't handle "while(FALSE)"
USHORT      usUsed = 1;         // LRU counter
HAB         habMain = 0;        // public because WinSetHook() needs it
HMODULE     hmodRWLD = 0;       // the handle of rwl.dll
PID         pidRWL = 0;         // our PID
char        szRWLD[] = "RWL";   // unqualified name of our dll

// ptr to an array of up to 62 RWL structures stored in
// 64k of shared gettable memory
PRWL        prwlBase = NULL;    // ptr to base address of the allocation
PRWL        prwlNext = NULL;    // ptr to last element + 1 in array

// messages - the initial msg ID values will be replaced by
// atoms generated from the strings below
ULONG       ulRWLM_X2D = RWLM_X2D;  // exe to dll msg
ULONG       ulRWLM_D2X = RWLM_D2X;  // dll to exe msg
char        szRWLM_X2D[] = "RWLM_X2D";
char        szRWLM_D2X[] = "RWLM_D2X";

/**********************************************************************/

// just your basic Init / Msg Loop / Shutdown code

int     main( void)

{
    HMQ     hmq = 0;
    HWND    hwnd = 0;
    int     nRtn = 8;
    QMSG    qmsg;

do
{
    habMain = WinInitialize( 0);
    if (!habMain)
        break;

    hmq = WinCreateMsgQueue( habMain, 0);
    if (!hmq)
        break;

    if (!RWLXInit())
        break;

    hwnd = WinLoadDlg(
                HWND_DESKTOP,               //  parent-window
                NULLHANDLE,                 //  owner-window
                MainWndProc,                //  dialog proc
                NULLHANDLE,                 //  EXE module handle
                IDD_MAIN,                   //  dialog id
                NULL);                      //  pointer to create params
// in a multi-windowed version, you'd pass the desired PID
// as a create param

    if (!hwnd)
        break;

    if (!WinRestoreWindowPos( "RWL", "WNDPOS", hwnd))
        WinShowWindow( hwnd, TRUE);

    while (WinGetMsg( habMain, &qmsg, NULLHANDLE, 0, 0))
        WinDispatchMsg( habMain, &qmsg);

    nRtn = 0;

} while (fFalse);

    if (nRtn)
        DosBeep( 440, 150);

    if (hwnd)
    {
        WinStoreWindowPos( "RWL", "WNDPOS", hwnd);
        WinDestroyWindow( hwnd);
    }
    if (hmq)
        WinDestroyMsgQueue( hmq);
    if (habMain)
        WinTerminate( habMain);
    if (prwlBase)
        DosFreeMem( (PVOID)prwlBase);

    exit( nRtn);
    return (0);
}

/****************************************************************************/

//  init global data, alloc mem to be passed among processes,
//  create unique msg IDs

BOOL    RWLXInit( void)

{
    PPIB        ppib;
    APIRET  	rc = 0;
    HATOMTBL    haTbl;
    ATOM        atom;
    char *      pErr;

do
{
// save our current PID
    DosGetInfoBlocks( NULL, &ppib);
    pidRWL = ppib->pib_ulpid;

// get RWL.DLL's hmod (needed by WinSetHook() and the dll itself)
    pErr = "DosQueryModuleHandle";
    rc = DosQueryModuleHandle( szRWLD, &hmodRWLD);
    if (rc)
        break;

// alloc 64k of gettable shared memory to be used as an
// array of RWL structures
    pErr = "DosAllocSharedMem";
    rc = DosAllocSharedMem( (PVOID*)(PVOID)&prwlBase, NULL, 0x10000,
                       PAG_READ | PAG_WRITE | PAG_COMMIT | OBJ_GETTABLE);
    if (rc)
        break;

    prwlNext = prwlBase;

// create 2 atoms to be used as unique msg IDs,
    haTbl = WinQuerySystemAtomTable();

    atom = WinAddAtom( haTbl, szRWLM_X2D);
    if (atom)
        ulRWLM_X2D = atom;

    atom = WinAddAtom( haTbl, szRWLM_D2X);
    if (atom)
        ulRWLM_D2X = atom;

// pass the atoms and the dll's hmod to the dll
    RWLDInitDll( hmodRWLD, ulRWLM_X2D, ulRWLM_D2X);

} while (fFalse);

    if (rc)
        RcErrBox( __FUNCTION__, pErr, rc);

    return (BOOL)(rc ? FALSE : TRUE);
}

/**********************************************************************/

// The dialog's QWL_USER points to the RWL structure for a
// particular PID.  In this single-window version, QWL_USER is
// changed whenever the user selects a new PIDs.  A multi-
// window version would put the new PID in the CREATEPARAM
// struct so it could be passed to RWLXUpdatePIDs()

MRESULT _System MainWndProc( HWND hwnd, ULONG msg, MPARAM mp1, MPARAM mp2)
 
{
    switch (msg)
    {
        case WM_INITDLG:
            WinSetWindowPtr( hwnd, QWL_USER, NULL);
            WinSendDlgItemMsg( hwnd, IDC_MLE, MLM_SETTEXTLIMIT,
                               (MP)1023, 0);
            WinSendDlgItemMsg( hwnd, IDC_BEG, BM_SETCHECK, (MP)1, 0);
            RWLXUpdatePIDs( hwnd, 0);
            WinPostMsg( hwnd, WM_COMMAND, (MP)IDC_PIDSET, 0);
            break;

        case WM_COMMAND:
            if ((USHORT)mp1 == IDC_UNDO)
                RWLXUndo( hwnd);
            else
            if ((USHORT)mp1 == IDC_SET)
                RWLXSetLibpath( hwnd);
            else
            if ((USHORT)mp1 == IDC_PIDSET)
                RWLXSelectPID( hwnd);
            else
            if ((USHORT)mp1 == IDC_PIDUPDT)
                RWLXUpdatePIDs( hwnd, 0);
            break;

        case WM_CONTROL:
            if (mp1 == MPFROM2SHORT( IDC_BEG, BN_CLICKED) ||
                mp1 == MPFROM2SHORT( IDC_END, BN_CLICKED))
                RWLXGetLibpath( hwnd);
            else
            if (mp1 == MPFROM2SHORT( IDC_PIDLB, LN_ENTER))
                WinPostMsg( hwnd, WM_COMMAND, (MP)IDC_PIDSET, 0);
            break;

        case WM_CHAR:
        // defeat default button action
            if (((CHARMSG(&msg)->fs & (USHORT)KC_VIRTUALKEY) &&
                   CHARMSG(&msg)->vkey == VK_ENTER) ||
                ((CHARMSG(&msg)->fs & (USHORT)KC_CHAR) &&
                   CHARMSG(&msg)->chr == '\r'))
                break;

            return (WinDefDlgProc( hwnd, msg, mp1, mp2));

        case WM_CLOSE:
            WinPostMsg( hwnd, WM_QUIT, NULL, NULL);
            break;

        case WM_DESTROY:
        // unhook the current process without hooking another
            RWLXSwitchPIDs( hwnd, NULL);
            return (WinDefDlgProc( hwnd, msg, mp1, mp2));

        default:
        // this is the msg the dll posts in response to the exe's msg
            if (msg == ulRWLM_D2X)
            {
                RWLXReply( hwnd, (PRWL)mp1, (ULONG)mp2);
                break;
            }
            return (WinDefDlgProc( hwnd, msg, mp1, mp2));

    } //end switch (msg)

    return (0);
}

/****************************************************************************/
/****************************************************************************/

// query the hooked process's Begin- or EndLIBPATH

BOOL    RWLXGetLibpath( HWND hwnd)

{
    BOOL        fRtn = FALSE;
    BOOL        fDisable = TRUE;
    PRWL        prwl;
    char *      pErr;

do
{
// ensure our PID info is still valid
    pErr = "invalid PID";
    prwl = RWLXValidateRWL( WinQueryWindowPtr( hwnd, QWL_USER));
    if (prwl == NULL)
        break;

// set the Begin or End flags based on the radio buttons
    if (WinSendDlgItemMsg( hwnd, IDC_END, BM_QUERYCHECK, 0, 0))
        prwl->flags = RWLF_GET | RWLF_END;
    else
        prwl->flags = RWLF_GET | RWLF_BEG;

// if we're querying ourself, tell the hook not to free its prwl
    if (prwl->pid == pidRWL)
        prwl->flags |= RWLF_NOFREE;

// post our query to the target's queue
    pErr = "WinPostQueueMsg";
    fDisable = WinPostQueueMsg( prwl->hmq, ulRWLM_X2D, (MP)prwl, (MP)hwnd);
    if (fDisable == FALSE)
        break;

    fRtn = TRUE;

} while (fFalse);

    if (fDisable)
        RWLXEnableBtns( hwnd, FALSE);

// if anything went wrong, refresh the PID listbox
    if (fRtn == FALSE)
    {
        ErrBox( __FUNCTION__, pErr);
        RWLXUpdatePIDs( hwnd, 0);
    }

    return (fRtn);
}

/****************************************************************************/

// set the hooked process's Begin- or EndLIBPATH
// using the edited contents of the MLE

BOOL    RWLXSetLibpath( HWND hwnd)

{
    BOOL        fRtn = FALSE;
    BOOL        fDisable = TRUE;
    PRWL        prwl;
    char *      pSrc;
    char *      pDst;
    char *      pErr;

do
{
    pErr = "invalid PID";
    prwl = RWLXValidateRWL( WinQueryWindowPtr( hwnd, QWL_USER));
    if (prwl == NULL)
        break;

// get the text, then remove any CRs or LFs
    WinQueryDlgItemText( hwnd, IDC_MLE, sizeof( prwl->szPath), prwl->szPath);
    for (pSrc=pDst=prwl->szPath; *pSrc; pSrc++)
        if (*pSrc != '\r' && *pSrc != '\n')
            *pDst++ = *pSrc;
    *pDst = '\0';

// set the Begin or End flags based on the radio buttons
    if (WinSendDlgItemMsg( hwnd, IDC_END, BM_QUERYCHECK, 0, 0))
        prwl->flags = RWLF_SET | RWLF_END;
    else
        prwl->flags = RWLF_SET | RWLF_BEG;

// if we're setting ourself, tell the hook not to free its prwl
    if (prwl->pid == pidRWL)
        prwl->flags |= RWLF_NOFREE;

// post our update to the target's queue
    pErr = "WinPostQueueMsg";
    fDisable = WinPostQueueMsg( prwl->hmq, ulRWLM_X2D, (MP)prwl, (MP)hwnd);
    if (fDisable == FALSE)
        break;

    fRtn = TRUE;

} while (fFalse);

    if (fDisable)
        RWLXEnableBtns( hwnd, FALSE);

// if anything went wrong, refresh the PID listbox
    if (fRtn == FALSE)
    {
        ErrBox( __FUNCTION__, pErr);
        RWLXUpdatePIDs( hwnd, 0);
    }

    return (fRtn);
}

/****************************************************************************/

// restore the target's Begin- or EndLIBPATH to its state
// when RWL first queried it

BOOL    RWLXUndo( HWND hwnd)

{
    BOOL        fRtn = FALSE;
    BOOL        fDisable = FALSE;
    PRWL        prwl;
    char *      pErr;

do
{
// validate the dialog's current RWL ptr
    pErr = "invalid PID";
    prwl = RWLXValidateRWL( WinQueryWindowPtr( hwnd, QWL_USER));
    if (prwl == NULL)
    {
        fDisable = TRUE;
        break;
    }

// make sure we saved something, then copy it to the buffer
// and set the flags
    pErr = "Undo buffer not initialized";
    if (WinSendDlgItemMsg( hwnd, IDC_END, BM_QUERYCHECK, 0, 0))
    {
        if (prwl->pszEnd == NULL)
            break;
        strcpy( prwl->szPath, prwl->pszEnd);
        prwl->flags = RWLF_SET | RWLF_END;
    }
    else
    {
        if (prwl->pszBeg == NULL)
            break;
        strcpy( prwl->szPath, prwl->pszBeg);
        prwl->flags = RWLF_SET | RWLF_BEG;
    }

    if (prwl->pid == pidRWL)
        prwl->flags |= RWLF_NOFREE;

// post our update request to the target queue
    pErr = "WinPostQueueMsg";
    fDisable = WinPostQueueMsg( prwl->hmq, ulRWLM_X2D, (MP)prwl, (MP)hwnd);
    if (fDisable == FALSE)
        break;

    fRtn = TRUE;

} while (fFalse);

    if (fDisable)
        RWLXEnableBtns( hwnd, FALSE);

// if anything went wrong, refresh the PID listbox
    if (fRtn == FALSE)
    {
        ErrBox( __FUNCTION__, pErr);
        RWLXUpdatePIDs( hwnd, 0);
    }

    return (fRtn);
}

/****************************************************************************/

// this proc handles the hook's response to our previous msg

BOOL    RWLXReply( HWND hwnd, PRWL prwl, ULONG rc)

{
    BOOL        fRtn = TRUE;
    BOOL        fEnable = TRUE;
    char *      pErr;
    char        szText[128];

do
{
// the msg refers to a different RWL than this window's
    pErr = "misdirected reply";
    if (WinQueryWindowPtr( hwnd, QWL_USER) != prwl)
        break;

// the hook had a catastrophic failure (i.e. it couldn't
// access RWL's shared memory)
    pErr = "operation failed";
    if (rc)
        break;

// lotsa possibilities...

    switch (prwl->flags)
    {

// we queried the target's BeginLIBPATH

        case (RWLF_GET | RWLF_BEG | RWLF_OK):
        // if this is our first query, save for Undo;
        // update the MLE and caption;

            if (prwl->pszBeg == NULL)
                prwl->pszBeg = strdup( prwl->szPath);
            WinSetDlgItemText( hwnd, IDC_MLE, prwl->szPath);
            sprintf( szText, "BeginLIBPATH for PID %d <%s>", prwl->pid,
                     (prwl->pszProc ? prwl->pszProc : "unidentified process"));
            WinSetDlgItemText( hwnd, IDC_CAP, szText);
            break;

// we queried the target's EndLIBPATH

        case (RWLF_GET | RWLF_END | RWLF_OK):
        // if this is our first query, save for Undo;
        // update the MLE and caption;

            if (prwl->pszEnd == NULL)
                prwl->pszEnd = strdup( prwl->szPath);
            WinSetDlgItemText( hwnd, IDC_MLE, prwl->szPath);
            sprintf( szText, "EndLIBPATH for PID %d <%s>", prwl->pid,
                     (prwl->pszProc ? prwl->pszProc : "unidentified process"));
            WinSetDlgItemText( hwnd, IDC_CAP, szText);
            break;

// we queried the f/q name of the target's exe

        case (RWLF_GET | RWLF_PROC | RWLF_OK):
        // if this is our first query, save for Undo;
        // query the target's Begin- or EndLIBPATH;
        // leave the buttons disabled

            if (prwl->pszProc == NULL)
                prwl->pszProc = strdup( prwl->szPath);

            if (WinSendDlgItemMsg( hwnd, IDC_END, BM_QUERYCHECK, 0, 0))
                prwl->flags = RWLF_GET | RWLF_END;
            else
                prwl->flags = RWLF_GET | RWLF_BEG;
            if (prwl->pid == pidRWL)
                prwl->flags |= RWLF_NOFREE;

            pErr = "WinPostQueueMsg";
            fRtn = WinPostQueueMsg( prwl->hmq, ulRWLM_X2D, (MP)prwl, (MP)hwnd);
            fEnable = (BOOL)(fRtn ? FALSE : TRUE);
            break;

// we set the target's Begin- or EndLIBPATH

        case (RWLF_SET | RWLF_BEG | RWLF_OK):
        case (RWLF_SET | RWLF_END | RWLF_OK):
        // our update was accepted, now query the
        // target to see what the result looks like;
        // leave the buttons disabled

            prwl->flags &= RWLF_BEGEND;
            prwl->flags |= RWLF_GET;
            if (prwl->pid == pidRWL)
                prwl->flags |= RWLF_NOFREE;

            pErr = "WinPostQueueMsg";
            fRtn = WinPostQueueMsg( prwl->hmq, ulRWLM_X2D, (MP)prwl, (MP)hwnd);
            fEnable = (BOOL)(fRtn ? FALSE : TRUE);
            break;

// our command failed

        case (RWLF_GET | RWLF_BEG | RWLF_BAD):
        case (RWLF_GET | RWLF_END | RWLF_BAD):
        case (RWLF_GET | RWLF_PROC | RWLF_BAD):
        case (RWLF_SET | RWLF_BEG | RWLF_BAD):
        case (RWLF_SET | RWLF_END | RWLF_BAD):
            pErr = "operation failed";
            fRtn = FALSE;
            break;

// no other combination of flags should occur

        default:
            pErr = "invalid response";
            rc = prwl->flags;
            fRtn = FALSE;
            break;
    }

} while (fFalse);

    if (fRtn == FALSE)
        RcErrBox( __FUNCTION__, pErr, rc);

// enable/disable buttons per flag set above
    RWLXEnableBtns( hwnd, fEnable);

    return (fRtn);
}

/****************************************************************************/

// prevent user changes while a command is in progress

void    RWLXEnableBtns( HWND hwnd, BOOL fEnable)

{
    WinEnableWindow( WinWindowFromID( hwnd, IDC_SET), fEnable);
    WinEnableWindow( WinWindowFromID( hwnd, IDC_UNDO), fEnable);
    WinEnableWindow( WinWindowFromID( hwnd, IDC_BEG), fEnable);
    WinEnableWindow( WinWindowFromID( hwnd, IDC_END), fEnable);
    return;
}

/****************************************************************************/
/****************************************************************************/

//  switch to whichever PID is selected in the listbox

BOOL    RWLXSelectPID( HWND hwnd)

{
    HWND        hHook;
    BOOL        fRtn = FALSE;
    PRWL        prwl;
    SHORT       ndx;

do
{
// get the item handle for the selected LB item;  this is a window
// belonging to the selected process

    ndx = (SHORT)WinSendDlgItemMsg( hwnd, IDC_PIDLB, LM_QUERYSELECTION,
                                    (MP)LIT_FIRST, 0);
    if (ndx == LIT_NONE)
        ndx = 0;

    hHook = (HWND)WinSendDlgItemMsg( hwnd, IDC_PIDLB, LM_QUERYITEMHANDLE,
                                     (MP)ndx, 0);
    if (hHook == 0)
    {
        ErrBox( __FUNCTION__, "invalid entry in PID list");
        break;
    }

// get an RWL struct for this window cum process;
// it may already exist if we've examined this PID before,
// otherwise it will be created
    prwl = RWLXGetPRWL( hHook);
    if (prwl == NULL)
        break;

// switch to the target PID;  RWLXSwitchPIDs() will unhook the
// current process, hook the new one, then reinit the dialog
// for the new PID and update its QWL_USER
    fRtn = RWLXSwitchPIDs( hwnd, prwl);

} while (fFalse);

// if anything went wrong, refresh the PID listbox
    if (fRtn == FALSE)
        RWLXUpdatePIDs( hwnd, 0);

    return (fRtn);
}

/****************************************************************************/

// return a ptr to an RWL struct for the specified window;
// it may already exist or it may need to be created

PRWL    RWLXGetPRWL( HWND hHook)

{
    ULONG       ul;
    PID         pid;
    HMQ         hmq;
    PRWL        pRtn = NULL;
    PRWL        ptr;
    char *      pErr;

do
{
// look for an existing entry
    ptr = NULL;
    while ( (ptr = RWLXNextRWL( ptr)) != NULL)
        if (ptr->hwnd == hHook)
            break;

// if we found something, validate it then exit
    pErr = "invalid PID";
    if (ptr)
    {
        pRtn = RWLXValidateRWL( ptr);
        break;
    }

// get the target window's PID and HMQ
    if (!WinQueryWindowProcess( hHook, &pid, &ul))
        break;

    pErr = "WinQueryWindowULong";
    hmq = WinQueryWindowULong( hHook, QWL_HMQ);
    if (hmq == NULLHANDLE)
        break;

// get a ptr to a new RWL struct
    pErr = "too many processes being accessed";
    pRtn = RWLXAllocRWL();
    if (pRtn == NULL)
        break;

// init it;
    pRtn->pid = pid;
    pRtn->hmq = hmq;
    pRtn->hwnd = hHook;
    pRtn->flags = (ULONG)-1;

} while (fFalse);

// if we're returning a valid ptr, update its
// least-recently-used indicator

    if (pRtn)
        pRtn->used = usUsed++;
    else
        ErrBox( __FUNCTION__, pErr);

    return (pRtn);
}

/****************************************************************************/

// unhook the current PID's queue, hook the new queue,
// then post a message that queries the process's name;
// if prwlNew is NULL, just unhook (used at shutdown)


BOOL    RWLXSwitchPIDs( HWND hwnd, PRWL prwlNew)

{
    BOOL        fRtn = FALSE;
    PRWL        prwl;
    char *      pErr;

do
{
// if the window's current RWL is still valid, unhook from
// that process's msg queue
    prwl = RWLXValidateRWL( WinQueryWindowPtr( hwnd, QWL_USER));
    if (prwl)
    {
        if (WinReleaseHook( habMain, prwl->hmq, HK_INPUT,
                            (PFN)RWLDInputHook, hmodRWLD) == FALSE)
            ErrBox( __FUNCTION__, "WinReleaseHook failed - continuing");
        else
            prwl->inuse = FALSE;
        prwl = NULL;
    }

// prevent user updates 
    RWLXEnableBtns( hwnd, FALSE);
    WinSetDlgItemText( hwnd, IDC_CAP, "Waiting...");

// if there's nothing to switch to, exit OK
    if (prwlNew == NULL)
    {
        pErr = NULL;
        fRtn = TRUE;
        break;
    }

// hook the designated msg queue in the target process;
// exit if this fails
    if (prwlNew->inuse == FALSE)
    {
        pErr = "WinSetHook";
        prwlNew->inuse = (USHORT)WinSetHook( habMain, prwlNew->hmq, HK_INPUT,
                                             (PFN)RWLDInputHook, hmodRWLD);
        if (prwlNew->inuse == FALSE)
            break;
    }
    prwl = prwlNew;

// if we don't already know the name of the target PID's exe,
// set the flags accordingly;  otherwise set the Begin or End
// flags based on the radio buttons
    if (prwl->pszProc == NULL)
        prwl->flags = RWLF_GET | RWLF_PROC;
    else
        if (WinSendDlgItemMsg( hwnd, IDC_END, BM_QUERYCHECK, 0, 0))
            prwl->flags = RWLF_GET | RWLF_END;
        else
            prwl->flags = RWLF_GET | RWLF_BEG;

// if we've hooked our own process's queue, warn the hook
// not to get or free its RWL
    if (prwl->pid == pidRWL)
        prwl->flags |= RWLF_NOFREE;

// post a msg to the queue we've hooked;
// pass the address of the current RWL struct (which is in
// shared gettable memory), and the hwnd to which it should
// post its reply
    pErr = "WinPostQueueMsg";
    fRtn = WinPostQueueMsg( prwl->hmq, ulRWLM_X2D, (MP)prwl, (MP)hwnd);

} while (fFalse);

// update QWL_USER with a ptr to the current RWL (this could be null)
    WinSetWindowPtr( hwnd, QWL_USER, prwl);

    if (fRtn == FALSE)
        ErrBox( __FUNCTION__, pErr);

    return (fRtn);
}

/****************************************************************************/

// this confirms that the target window still exists
// and belongs to the expected PID.  If so, it returns
// the input;  if not, it frees the invalid RWL and
// returns NULL.

PRWL    RWLXValidateRWL( PRWL prwl)

{
    PID         pid;
    TID         tid;
    PRWL        pRtn = NULL;

    if (prwl)
    {
        if (WinQueryWindowProcess( prwl->hwnd, &pid, &tid) &&
            pid == prwl->pid)
            pRtn = prwl;
        else
            RWLXFreeRWL( prwl);
    }
    return (pRtn);
}

/****************************************************************************/

// step through the array from the beginning or from a designated
// starting point, skipping over empty entries

PRWL    RWLXNextRWL( PRWL prwl)

{
    PRWL    pRtn = NULL;

    if (prwl == NULL)
        prwl = &prwlBase[-1];

    if (prwl >= &prwlBase[-1])
        while (++prwl < prwlNext)
            if (prwl->hwnd)
            {
                pRtn = prwl;
                break;
            }

    return (pRtn);
}

/****************************************************************************/

// Return the first available entry in our RWL array.
// (this proc was designed with a multi-window version
// in mind, so it doesn't compact the array and checks
// whether an entry is in use)

PRWL    RWLXAllocRWL( void)

{
    PRWL    pRtn = NULL;
    PRWL    pLRU;
    PRWL    ptr;

// step through currently used entries looking for an empty one;
// if found, exit;  if not, save a ptr to the oldest RWL not
// currently in use (this last helps support a multi-windowed
// version, as does not compacting the array).

    for (ptr=pLRU=prwlBase; ptr < prwlNext; ptr++)
    {
        if (ptr->hwnd == 0)
        {
            pRtn = ptr;
            break;
        }
        if (ptr->used < pLRU->used && ptr->inuse == FALSE)
            pLRU = ptr;
    }

// if nothing was found, see if there's any more room in
// the array.  if so, use it;  otherwise, recycle the
// least recently used entry
    if (pRtn == NULL)
        if (&prwlNext[1] < &prwlBase[ 0x10000/sizeof( RWL) ])
            pRtn = prwlNext++;
        else
            if (pLRU->inuse == FALSE)
            {
                RWLXFreeRWL( pLRU);
                pRtn = pLRU;
            }

    return (pRtn);
}

/****************************************************************************/

// free the storage for the PID's name and the Undo buffers;
// then clear the entry

void    RWLXFreeRWL( PRWL prwl)

{
    free( prwl->pszProc);
    free( prwl->pszBeg);
    free( prwl->pszEnd);
    memset( prwl, '\0', sizeof( RWL));
    return;
}

/**********************************************************************/
/**********************************************************************/

// identifies all PM processes, then fills a listbox with their PIDs

BOOL    RWLXUpdatePIDs( HWND hwnd, PID pidSelect)

{
    PAPP    paApp = NULL;
    PRWL    prwl;
    BOOL    fRtn = FALSE;

do
{
// alloc space for an arbitrary number of APP structures;
// this corresponds to the maximum number of PIDs we can handle
    paApp = calloc( MAXAPP, sizeof( APP));
    if (paApp == NULL)
    {
        ErrBox( __FUNCTION__, "calloc");
        break;
    }

// fill the array, one entry per PID
    if (RWLXEnumPIDs( paApp, MAXAPP-1) == 0)
        break;

// if the PID to select in the listbox wasn't specified,
// use the dialog's current PID if possible
    if (pidSelect == 0 && 
        (prwl = RWLXValidateRWL( WinQueryWindowPtr( hwnd, QWL_USER))) != NULL)
        pidSelect = prwl->pid;

// fill the listbox and select an item
    RWLXListPIDs( WinWindowFromID( hwnd, IDC_PIDLB), pidSelect, paApp);

    fRtn = TRUE;

} while (fFalse);

// free our APP structs
    free( paApp);

    return (fRtn);
}

/**********************************************************************/

// for each PID, identify a window associated with the lowest
// numbered thread that has a msg queue

LONG    RWLXEnumPIDs( PAPP paApp, ULONG cntApp)

{
    HENUM   hEnum;
    HWND    hTmp;
    LONG    ctr = 0;
    PID     pid;
    TID     tid;
    PAPP    ptr;
    char    szText[16];

do
{
    hEnum = WinBeginEnumWindows( HWND_OBJECT);
    if (hEnum == NULLHANDLE)
    {
        ErrBox( __FUNCTION__, "WinBeginEnumWindows");
        break;
    }

    while (ctr < cntApp &&
           (hTmp = WinGetNextWindow( hEnum)) != 0)
    {
        WinQueryClassName( hTmp, sizeof( szText), szText);
        if (strcmp( szText, "#32767"))
            continue;

        WinQueryWindowProcess( hTmp, &pid, &tid);

        for (ptr=paApp; ptr < &paApp[cntApp]; ptr++)
        {
            if (ptr->pid == 0)
            {
                ptr->hwnd = hTmp;
                ptr->pid  = pid;
                ptr->tid  = tid;
                ctr++;
                break;
            }

            if (ptr->pid == pid)
            {
                if (tid < ptr->tid)
                {                
                    ptr->hwnd = hTmp;
                    ptr->tid  = tid;
                }
                break;
            }

        } // end for()
    } // end while()

    WinEndEnumWindows( hEnum);

} while (fFalse);

    return (ctr);
}

/**********************************************************************/

// fills the listbox with PIDs and their corresponding hwnds,
// then selects an item

void    RWLXListPIDs( HWND hLB, PID pidSelect, PAPP paApp)

{
    SHORT   ndx;
    char    szText[16];

// clear the listbox
    WinSendMsg( hLB, LM_DELETEALL, 0, 0);

// for each entry in the APP array, format its PID,
// then insert the item sorted;  if this succeeds,
// set the item's handle with the hwnd
    while (paApp->pid)
    {
        sprintf( szText, "%4d", paApp->pid);
        ndx = (SHORT)WinSendMsg( hLB, LM_INSERTITEM,
                                 (MP)LIT_SORTASCENDING, szText);
        if (ndx >= 0)
            WinSendMsg( hLB, LM_SETITEMHANDLE,
                        (MP)ndx, (MP)paApp->hwnd);

        paApp++;
    }

// since the list was sorted during insertion, we now have
// to search for the PID we want to select;  if this fails,
// we'll select the first item (which should be pmshell)

    sprintf( szText, "%4d", pidSelect);
    ndx = (SHORT)WinSendMsg( hLB, LM_SEARCHSTRING,
                        MPFROM2SHORT( LSS_CASESENSITIVE, LIT_FIRST),
                        (MP)szText);
    if (ndx < 0)
        ndx = 0;

    WinSendMsg( hLB, LM_SELECTITEM, (MP)ndx, (MP)TRUE);

    return;
}

/**********************************************************************/
/**********************************************************************/

// invokes the main errorbox routine with a rtn code of zero

BOOL    ErrBox( PSZ pTtl, PSZ pErr)

{
    return (RcErrBox( pTtl, pErr, 0));
}

/**********************************************************************/

// a dumb little msg box

BOOL    RcErrBox( PSZ pTtl, PSZ pErr, ULONG rc)

{
    char    szText[256];

    if (rc)
        sprintf( szText, "%s:  %s  rc=%x", pTtl, pErr, rc);
    else
        sprintf( szText, "%s:  %s", pTtl, pErr);

    WinMessageBox( HWND_DESKTOP, NULLHANDLE, szText, "RWL", IDD_ERRBOX,
                   (ULONG)MB_OK | (ULONG)MB_ICONEXCLAMATION | (ULONG)MB_MOVEABLE);

    return (FALSE);
}

/**********************************************************************/
/**********************************************************************/

