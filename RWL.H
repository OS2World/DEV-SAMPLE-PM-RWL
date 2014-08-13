/**********************************************************************/
/*                                                                    */
/*  RWL - A demonstration program placed in the Public Domain         */
/*        by it author, R L Walsh, June 10, 2000                      */
/*                                                                    */
/**********************************************************************/
#if 0

    For a description of how RWL works, please see "rwlx.c"

#endif
/**********************************************************************/

//  RWL.H

/**********************************************************************/

#define INCL_WIN
#define INCL_DOS

#include <os2.h>

/**********************************************************************/

// functions exported from RWLD
void _System    RWLDInitDll( HMODULE hDll, ULONG x2d, ULONG d2x);
int  _System    RWLDInputHook( HAB hab, PQMSG pqmsg, ULONG fs);

/**********************************************************************/

// RWL struct - everything we need to know about a process,
// plus a buffer for passing strings back and forth.

typedef struct _RWL
{
    ULONG   pid;        // the target's PID
    HMQ     hmq;        // HMQ for the lowest numbered thread with a msg-q
    HWND    hwnd;       // a window belonging to that thread
    USHORT  used;       // least-recently-used indicator for array mgmt
    USHORT  inuse;      // is this instance being used by a dialog
    char *  pszProc;    // ptr to the target's f/q exe name
    char *  pszBeg;     // ptr to the initial BeginLIBPATH for use by Undo
    char *  pszEnd;     // ptr to the initial EndLIBPATH for use by Undo
    ULONG   flags;      // msg flags that tell the hook code what to do
    char    szPath[1024]; // buffer for strings
} RWL;

typedef RWL *PRWL;

/**********************************************************************/

// values for RWL.flags

#define RWLF_GET    ((ULONG)0x0001)
#define RWLF_SET    ((ULONG)0x0002)
#define RWLF_BEG    ((ULONG)0x0004)
#define RWLF_END    ((ULONG)0x0008)
#define RWLF_PROC   ((ULONG)0x0010)
#define RWLF_NOFREE ((ULONG)0x0020)

#define RWLF_OK     ((ULONG)0x00100)
#define RWLF_BAD    ((ULONG)0x00200)

#define RWLF_BEGEND (RWLF_BEG | RWLF_END)

#define RWLF_X2D    (RWLF_GET | RWLF_SET  | RWLF_BEG | \
                     RWLF_END | RWLF_PROC | RWLF_NOFREE)

#define RWLF_D2X    (RWLF_GET | RWLF_SET  | RWLF_BEG | \
                     RWLF_END | RWLF_PROC | RWLF_OK | RWLF_BAD)

/****************************************************************************/

// APP struct - used while enumerating PM-based processes;
// the pid and hwnd info is later stored in the dialog's listbox.

typedef struct _APP
{
    HWND    hwnd;
    PID     pid;
    TID     tid;
} APP;

typedef APP * PAPP;

// the maximum number of processes we can enumerate
#define MAXAPP  1024

/**********************************************************************/

// Miscellanea

// these are arbitrary msg IDs that should never end up being used
#define RWLM_X2D    0x85AB
#define RWLM_D2X    0x85AC

// a convenient macro
#ifdef MP
    #error MP already defined
#else
    #define MP      MPARAM
#endif

/**********************************************************************/

