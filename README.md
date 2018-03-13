# Superfluous

Superfluous is a Battlefield 2 server rcon administration program for Windows 2K/XP (also should work in Wine and 9x/ME), the server needs to be running BF2CC/ModManager scripts version 4.6 or later and PunkBuster be enabled.

* [Home page](https://www.nisda.net/superfluous.html)
* [superfluous.txt](bin/superfluous.txt)

## Features

* Very small, very fast and very low memory usage
* Match players on a wide variety of options
* Teamswap, kick, ban players with a reason
* View all player chat from team, squad, commander and global
* Send server chat
* Hotkey to redirect all keyboard input to Superfluous
* Alias frequently used commands
* Send raw commands to the server

## Releases

Version|Date|Download
---|---|---
v1.00|6th February 2006|
v1.04|21st January 2007|[win32 i386](https://www.nisda.net/files/superfluous-v1.04.zip)

## Source

The source code was released on 28th January 2008.

### Windows

Compiled with Microsoft Visual Studio 6 which requires the Microsoft Platform SDK.
* Parts of the SDK which are required:
    * Microsoft Windows Core SDK -> Build Environment -> Build Environment (x86 32-bit)
    * Microsoft Web Workshop (IE) SDK -> Build Environment
* After installing the SDK add the following directories to Visual Studio (Tools -> Options -> Directories) at the top of the list:
    * Include files: \Platform SDK (R2)\Include
    * Library files: \Platform SDK (R2)\Lib

### Third Party

* [Aggressive Optimizations](https://web.archive.org/web/20061115204456fw_/http://www.nopcode.com/index.shtml): Quest for a smaller EXE size.
* [DOS Stub](http://web.archive.org/web/20070124160645/http://its.mine.nu/html/coding/essays/stub.html): The 120 byte standard stub is far too big, the tiny one is 64 bytes which leaves room to use the extra bytes for something interesting.
* [Windows Hook without external DLL](http://web.archive.org/web/20030824225005/rattlesnake.at.box.sk/newsread.php?newsid=193): Usually an external DLL is required for windows hooks however this is not the case.

## Notes

* During the release compile there's a bunch of warnings which can be safely ignored, they're part of making the smallest EXE possible.
* The "Enter rcon password" icon is an Archon from Blizzard's Starcraft with a padlock from Windows XP. Archon..rcon.. Geddit? Ha ha!
* As for the main icon, don't ask what icon you should use on IRC, ever. [This](https://web.archive.org/web/20070206211925/http://www.originalicons.com:80) is possibly where it's from.
