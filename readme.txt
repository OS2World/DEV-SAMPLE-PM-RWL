__________________________________________________________________________

RWL
__________________________________________________________________________


RWL v0.25 is a utility program which demonstrates that any PM process
can enter any other PM process at will and alter it as desired.

This file, 'readme.rwl', briefly explains how to use it.  Refer to
'goodidea.txt' for a full description, and to 'rwlx.c' for additional
details.

__________________________________________________________________________

What RWL Does
__________________________________________________________________________


RWL lets you set the BeginLIBPATH and EndLIBPATH for any PM-based
(windowed) program.  This is of limited use, with one major exception.

When a program is launched, it inherits its parent's Begin- and End-
LIBPATHs.  Since the first instance of pmshell.exe is normally the
parent of most programs, changing its paths has much the same effect
on new programs as revising your global LIBPATH would.  Apps that are
already running will be unaffected.  This may let you start using
newly-installed apps without having to reboot to update your LIBPATH.

__________________________________________________________________________

Using RWL
__________________________________________________________________________


Unzip rwl025.zip anywhere and run it.

The left side of RWL's main window lists the process IDs (PIDs) of all
PM-based processes.  Doubleclick on a PID to select that process.  Use
the 'Refresh' button to update the list.

The label above the edit window identifies the selected process and
whether its Begin- or EndLIBPATH is being displayed.  Use the radio
buttons below to switch between paths.  If the label displays
"Waiting..." (e.g. pmspool.exe) you won't be able to use RWL with
that process.  

You can add to or update the current Begin- or EndLIBPATH as desired.
You can also enter "%BeginLIBPATH%" and "%EndLIBPATH%" in place of
these paths' current values.

When you press the 'Set' button, RWL removes any line breaks in the
text and submits it to the system.  RWL then immediately queries the
system for the current value and displays it.  Don't be surprised if
the system edits or rejects your update;  for example, it won't accept
an entry without a backslash

Use the 'Undo' button to restore a process's Begin- or EndLIBPATH to
its state when RWL first queried it.

__________________________________________________________________________

Limitations
__________________________________________________________________________


RWL was written to demonstrate a programming technique and is sorely
underdeveloped as a full-featured utility.  However, its code is in
the public domain, so it's likely to be fleshed out by someone if I
don't get to it.

If the lack of a save and restore feature bothers you, get DragText.
You'll be able to save your settings by dragging the edit window's
text and dropping it on a folder to create a file.  Later, you can
restore these settings by dropping this file back in the window.
Plus, you'll be able to insert a folder's path in the text by dropping
its icon in the window.

__________________________________________________________________________

License
__________________________________________________________________________


There is none.  RWL.EXE, RWL.DLL and their respective source code
have been placed in the public domain.  The article which describes
the principle behind RWL, _But Is It a Good Idea?_, as well as
'readme.rwl' are (C) Copyright RL Walsh 2000 - All Rights Reserved.

__________________________________________________________________________
__________________________________________________________________________


Rich Walsh  (rlwalsh@packet.net)
Ft Myers, Florida

June 10, 2000

__________________________________________________________________________
__________________________________________________________________________
