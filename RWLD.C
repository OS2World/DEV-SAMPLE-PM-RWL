/**********************************************************************/
/*                                                                    */
/*  RWL - A demonstration program placed in the Public Domain         */
/*        by it author, R L Walsh, June 10, 2000                      */
/*                                                                    */
/**********************************************************************/
#if 0

    For a description of how RWL works, please see "rwlx.c"

    The version of rwl.dll included in the archive was staticly
    linked to my compiler`s subsystem libraries.  These have no C
    runtime environment, reducing both functionality and headaches.

    FWIW...  In this demo, the functionality is tied to the hook
    code.  In a more sophisticated implementation, the hook code
    would only be used once:  to initialize some independent
    "presence" in the target process.  Depending on your needs,
    you might spin off a thread, create an object window, open
    a pipe, or whatever else seems appropriate.

    If you use this method, you`ll need to ensure that your dll
    remains loaded in the target process.  Leaving the hook in place
    will accomplish this.  AFAIK, having the dll load itself in the
    target process using its fully-qualified name will also work.

#endif
/**********************************************************************/

//  RWLD.C

/**********************************************************************/

#include "rwl.h"        // most stuff (shared with rwlx.c)

/****************************************************************************/

// exported functions declared in rwl.h

//void _System   RWLDInitDll( HMODULE hDll, ULONG x2d, ULONG d2x);
//int  _System   RWLDInputHook( HAB hab, PQMSG pqmsg, ULONG fs);

// function that does the work
int     RWLDGetSet( PRWL prwl, HWND hReply);

/****************************************************************************/

// this data is in a single shared segment accessible to any
// process rwl.dll is loaded into

ULONG   ulRWLM_X2D = RWLM_X2D;      // msg IDs;  these initial values
ULONG   ulRWLM_D2X = RWLM_D2X;      // should never end up being used

HMODULE hmodRWLD = 0;               // currently set but not used

/****************************************************************************/

// called by rwl.exe at startup;  operates only in the RWL process

void _System    RWLDInitDll( HMODULE hDll, ULONG x2d, ULONG d2x)

{
// save the ID for the msg the exe posts to the dll
    if (x2d)
        ulRWLM_X2D = x2d;
// save the ID for the msg the dll posts to the exe
    if (d2x)
        ulRWLM_D2X = d2x;
// save the dll's module handle;  this version doesn't use it
    hmodRWLD = hDll;

    return;
}

/****************************************************************************/

// this hook function operates in the target process's context

int _System    RWLDInputHook( HAB hab, PQMSG pqmsg, ULONG fs)

{
// we only handle one message and it has to be constructed correctly;
// if so, we always return TRUE, so there will be no further handling;
// otherwise, we always pass msgs on by returning FALSE.

    if (pqmsg->msg == ulRWLM_X2D && // our unique msg ID
        pqmsg->hwnd == 0 &&         // this should be a queue msg
        fs == PM_REMOVE &&          // no false starts
        pqmsg->mp1 &&               // ptr to RWL struct in shared mem
        pqmsg->mp2 &&               // hwnd that posted this msg
        WinIsWindow( hab, (HWND)pqmsg->mp2))
            return (RWLDGetSet( (PRWL)pqmsg->mp1, (HWND)pqmsg->mp2));

    return (FALSE);
}

/****************************************************************************/

// this does the work:  accesses the RWL struct, acts according
// to its flag settings, then posts a reply msg to the source

int     RWLDGetSet( PRWL prwl, HWND hReply)

{
    BOOL    fFalse = 0;
    PPIB    pPib;
    APIRET  rc;

do
{
// round prwl down to the nearest segment boundary, then get it
    rc = DosGetSharedMem( (PVOID)((ULONG)prwl & 0xFFFF0000),
                          PAG_READ | PAG_WRITE);
    if (rc)
        break;

// use the flags to determine what to do
    switch (prwl->flags & ~RWLF_NOFREE)
    {

    // retrieve BeginLIBPATH
        case (RWLF_GET | RWLF_BEG):
            *(prwl->szPath) = '\0';
            rc = DosQueryExtLIBPATH( prwl->szPath, BEGIN_LIBPATH);
            break;

    // retrieve EndLIBPATH
        case (RWLF_GET | RWLF_END):
            *(prwl->szPath) = '\0';
            rc = DosQueryExtLIBPATH( prwl->szPath, END_LIBPATH);
            break;

    // retrieve this process's f/q exe name
        case (RWLF_GET | RWLF_PROC):
            *(prwl->szPath) = '\0';
            DosGetInfoBlocks( NULL, &pPib);
            rc = DosQueryModuleName( pPib->pib_hmte, sizeof( prwl->szPath),
                                     prwl->szPath);
            break;

    // set BeginLIBPATH
        case (RWLF_SET | RWLF_BEG):
            rc = DosSetExtLIBPATH( prwl->szPath, BEGIN_LIBPATH);
            break;

    // set EndLIBPATH
        case (RWLF_SET | RWLF_END):
            rc = DosSetExtLIBPATH( prwl->szPath, END_LIBPATH);
            break;

        default:
            rc = 1;
    }

// set our success/failure flags
    if (rc)
        prwl->flags |= RWLF_BAD;
    else
        prwl->flags |= RWLF_OK;

// RWLF_NOFREE will be on if we've hooked rwl.exe's msg queue;
// this prevents us from accidently freeing the shared mem
    if (prwl->flags & RWLF_NOFREE)
        prwl->flags &= ~RWLF_NOFREE;
    else
        if (DosFreeMem( (PVOID)((ULONG)prwl & 0xFFFF0000)))
            DosBeep( 1220, 65);

} while (fFalse);

// post our reply - rwl.exe is waiting for it
    if (WinPostMsg( hReply, ulRWLM_D2X, (MP)prwl, (MP)rc) == FALSE)
        DosBeep( 220, 65);

    return (TRUE);
}

/****************************************************************************/

