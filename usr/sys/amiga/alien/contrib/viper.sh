#!/bin/sh
# This is viper, a shell archive (shar 3.21)
# made 11/16/1991 07:52 UTC by ckctpa!crash@uunet.UU.NET
# Source directory /usr/sys/amiga/alien/DISTR
#
# existing files will NOT be overwritten
#
# This shar contains:
# length  mode       name
# ------ ---------- ------------------------------------------
#   9935 -rw-r--r-- COPYING
#   3521 -rw-r--r-- README.distr
#   2294 -rw-r--r-- README.ioctl
#   3521 -rw-r--r-- README.tctl
#    524 -rwxr-xr-x tape_sums
#  36682 -rw-r--r-- tape.diffs
#   1595 -rw-r--r-- kernel.diffs
#   3760 -rw-r--r-- ioctl.c
#   The tctl executable has been left out.
#
if touch 2>&1 | fgrep '[-amc]' > /dev/null
 then TOUCH=touch
 else TOUCH=true
fi
# ============= COPYING ==============
if test X"$1" != X"-c" -a -f 'COPYING'; then
	echo "File already exists: skipping 'COPYING'"
else
echo "x - extracting COPYING (Text)"
sed 's/^X//' << 'SHAR_EOF' > COPYING &&
X
X		    GNU GENERAL PUBLIC LICENSE
X		     Version 1, February 1989
X
X Copyright (C) 1989 Free Software Foundation, Inc.
X                    675 Mass Ave, Cambridge, MA 02139, USA
X Everyone is permitted to copy and distribute verbatim copies
X of this license document, but changing it is not allowed.
X
X			    Preamble
X
X  The license agreements of most software companies try to keep users
Xat the mercy of those companies.  By contrast, our General Public
XLicense is intended to guarantee your freedom to share and change free
Xsoftware--to make sure the software is free for all its users.  The
XGeneral Public License applies to the Free Software Foundation's
Xsoftware and to any other program whose authors commit to using it.
XYou can use it for your programs, too.
X
X  When we speak of free software, we are referring to freedom, not
Xprice.  Specifically, the General Public License is designed to make
Xsure that you have the freedom to give away or sell copies of free
Xsoftware, that you receive source code or can get it if you want it,
Xthat you can change the software or use pieces of it in new free
Xprograms; and that you know you can do these things.
X
X  To protect your rights, we need to make restrictions that forbid
Xanyone to deny you these rights or to ask you to surrender the rights.
XThese restrictions translate to certain responsibilities for you if you
Xdistribute copies of the software, or if you modify it.
X
X  For example, if you distribute copies of a such a program, whether
Xgratis or for a fee, you must give the recipients all the rights that
Xyou have.  You must make sure that they, too, receive or can get the
Xsource code.  And you must tell them their rights.
X
X  We protect your rights with two steps: (1) copyright the software, and
X(2) offer you this license which gives you legal permission to copy,
Xdistribute and/or modify the software.
X
X  Also, for each author's protection and ours, we want to make certain
Xthat everyone understands that there is no warranty for this free
Xsoftware.  If the software is modified by someone else and passed on, we
Xwant its recipients to know that what they have is not the original, so
Xthat any problems introduced by others will not reflect on the original
Xauthors' reputations.
X
X  The precise terms and conditions for copying, distribution and
Xmodification follow.
X
X		    GNU GENERAL PUBLIC LICENSE
X   TERMS AND CONDITIONS FOR COPYING, DISTRIBUTION AND MODIFICATION
X
X  0. This License Agreement applies to any program or other work which
Xcontains a notice placed by the copyright holder saying it may be
Xdistributed under the terms of this General Public License.  The
X"Program", below, refers to any such program or work, and a "work based
Xon the Program" means either the Program or any work containing the
XProgram or a portion of it, either verbatim or with modifications.  Each
Xlicensee is addressed as "you".
X
X  1. You may copy and distribute verbatim copies of the Program's source
Xcode as you receive it, in any medium, provided that you conspicuously and
Xappropriately publish on each copy an appropriate copyright notice and
Xdisclaimer of warranty; keep intact all the notices that refer to this
XGeneral Public License and to the absence of any warranty; and give any
Xother recipients of the Program a copy of this General Public License
Xalong with the Program.  You may charge a fee for the physical act of
Xtransferring a copy.
X
X  2. You may modify your copy or copies of the Program or any portion of
Xit, and copy and distribute such modifications under the terms of Paragraph
X1 above, provided that you also do the following:
X
X    a) cause the modified files to carry prominent notices stating that
X    you changed the files and the date of any change; and
X
X    b) cause the whole of any work that you distribute or publish, that
X    in whole or in part contains the Program or any part thereof, either
X    with or without modifications, to be licensed at no charge to all
X    third parties under the terms of this General Public License (except
X    that you may choose to grant warranty protection to some or all
X    third parties, at your option).
X
X    c) If the modified program normally reads commands interactively when
X    run, you must cause it, when started running for such interactive use
X    in the simplest and most usual way, to print or display an
X    announcement including an appropriate copyright notice and a notice
X    that there is no warranty (or else, saying that you provide a
X    warranty) and that users may redistribute the program under these
X    conditions, and telling the user how to view a copy of this General
X    Public License.
X
X    d) You may charge a fee for the physical act of transferring a
X    copy, and you may at your option offer warranty protection in
X    exchange for a fee.
X
XMere aggregation of another independent work with the Program (or its
Xderivative) on a volume of a storage or distribution medium does not bring
Xthe other work under the scope of these terms.
X
X  3. You may copy and distribute the Program (or a portion or derivative of
Xit, under Paragraph 2) in object code or executable form under the terms of
XParagraphs 1 and 2 above provided that you also do one of the following:
X
X    a) accompany it with the complete corresponding machine-readable
X    source code, which must be distributed under the terms of
X    Paragraphs 1 and 2 above; or,
X
X    b) accompany it with a written offer, valid for at least three
X    years, to give any third party free (except for a nominal charge
X    for the cost of distribution) a complete machine-readable copy of the
X    corresponding source code, to be distributed under the terms of
X    Paragraphs 1 and 2 above; or,
X
X    c) accompany it with the information you received as to where the
X    corresponding source code may be obtained.  (This alternative is
X    allowed only for noncommercial distribution and only if you
X    received the program in object code or executable form alone.)
X
XSource code for a work means the preferred form of the work for making
Xmodifications to it.  For an executable file, complete source code means
Xall the source code for all modules it contains; but, as a special
Xexception, it need not include source code for modules which are standard
Xlibraries that accompany the operating system on which the executable
Xfile runs, or for standard header files or definitions files that
Xaccompany that operating system.
X
X  4. You may not copy, modify, sublicense, distribute or transfer the
XProgram except as expressly provided under this General Public License.
XAny attempt otherwise to copy, modify, sublicense, distribute or transfer
Xthe Program is void, and will automatically terminate your rights to use
Xthe Program under this License.  However, parties who have received
Xcopies, or rights to use copies, from you under this General Public
XLicense will not have their licenses terminated so long as such parties
Xremain in full compliance.
X
X  5. By copying, distributing or modifying the Program (or any work based
Xon the Program) you indicate your acceptance of this license to do so,
Xand all its terms and conditions.
X
X  6. Each time you redistribute the Program (or any work based on the
XProgram), the recipient automatically receives a license from the original
Xlicensor to copy, distribute or modify the Program subject to these
Xterms and conditions.  You may not impose any further restrictions on the
Xrecipients' exercise of the rights granted herein.
X
X  7. The Free Software Foundation may publish revised and/or new versions
Xof the General Public License from time to time.  Such new versions will
Xbe similar in spirit to the present version, but may differ in detail to
Xaddress new problems or concerns.
X
XEach version is given a distinguishing version number.  If the Program
Xspecifies a version number of the license which applies to it and "any
Xlater version", you have the option of following the terms and conditions
Xeither of that version or of any later version published by the Free
XSoftware Foundation.  If the Program does not specify a version number of
Xthe license, you may choose any version ever published by the Free Software
XFoundation.
X
X  8. If you wish to incorporate parts of the Program into other free
Xprograms whose distribution conditions are different, write to the author
Xto ask for permission.  For software which is copyrighted by the Free
XSoftware Foundation, write to the Free Software Foundation; we sometimes
Xmake exceptions for this.  Our decision will be guided by the two goals
Xof preserving the free status of all derivatives of our free software and
Xof promoting the sharing and reuse of software generally.
X
X			    NO WARRANTY
X
X  9. BECAUSE THE PROGRAM IS LICENSED FREE OF CHARGE, THERE IS NO WARRANTY
XFOR THE PROGRAM, TO THE EXTENT PERMITTED BY APPLICABLE LAW.  EXCEPT WHEN
XOTHERWISE STATED IN WRITING THE COPYRIGHT HOLDERS AND/OR OTHER PARTIES
XPROVIDE THE PROGRAM "AS IS" WITHOUT WARRANTY OF ANY KIND, EITHER EXPRESSED
XOR IMPLIED, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
XMERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.  THE ENTIRE RISK AS
XTO THE QUALITY AND PERFORMANCE OF THE PROGRAM IS WITH YOU.  SHOULD THE
XPROGRAM PROVE DEFECTIVE, YOU ASSUME THE COST OF ALL NECESSARY SERVICING,
XREPAIR OR CORRECTION.
X
X  10. IN NO EVENT UNLESS REQUIRED BY APPLICABLE LAW OR AGREED TO IN WRITING
XWILL ANY COPYRIGHT HOLDER, OR ANY OTHER PARTY WHO MAY MODIFY AND/OR
XREDISTRIBUTE THE PROGRAM AS PERMITTED ABOVE, BE LIABLE TO YOU FOR DAMAGES,
XINCLUDING ANY GENERAL, SPECIAL, INCIDENTAL OR CONSEQUENTIAL DAMAGES ARISING
XOUT OF THE USE OR INABILITY TO USE THE PROGRAM (INCLUDING BUT NOT LIMITED
XTO LOSS OF DATA OR DATA BEING RENDERED INACCURATE OR LOSSES SUSTAINED BY
XYOU OR THIRD PARTIES OR A FAILURE OF THE PROGRAM TO OPERATE WITH ANY OTHER
XPROGRAMS), EVEN IF SUCH HOLDER OR OTHER PARTY HAS BEEN ADVISED OF THE
XPOSSIBILITY OF SUCH DAMAGES.
X
X		     END OF TERMS AND CONDITIONS
SHAR_EOF
$TOUCH -am 1116025291 COPYING &&
chmod 0644 COPYING ||
echo "restore of COPYING failed"
set `wc -c COPYING`;Wc_c=$1
if test "$Wc_c" != "9935"; then
	echo original size 9935, current size $Wc_c
fi
fi
# ============= README.distr ==============
if test X"$1" != X"-c" -a -f 'README.distr'; then
	echo "File already exists: skipping 'README.distr'"
else
echo "x - extracting README.distr (Text)"
sed 's/^X//' << 'SHAR_EOF' > README.distr &&
XThis file describes the Viper support I've developed for use with
XAmiga Unix 2.0 and later.
X
X$Id: README.distr,v 1.2 1991/11/15 23:46:59 root Rel $
X
XThis support is in the form of patches to the original Commodore
Xtape driver, "ct.c", in the /usr/sys/amiga/alien directory of the
Xkernel configuration package.
X
XThe purpose is twofold.  First, to directly support the Archive Viper as
Xa tape drive for use with Amiga Unix.  Second, to broaden the base of
Xusable tape drives, thereby adding a selling point for Amiga Unix.  I
Xhave tried to maintain consistency with the SCSI-I CCS (Common Command
XSet) specifications to the best of my knowledge and ability.  See the
Xfile COPYING for a description of warranty and disclaimer notices.
X
XThere is a list of changes for each version towards the end of this
Xfile.  Note that they are in reverse chronological order, eg. the
Xchanges for the latest version are included first.
X
XPlease note that there are components of the Makefile which are
Xonly used here internally for testing and debugging.  The one I'm thinking
Xof in particular is "distr.src".
X
XThere is one file new to the Amiga Unix distribution, "tape.h".  It is
Xthe place where I store user-visible definitions for the tape driver.
XThe driver, by default, will have compile-time statements included
Xwhich allow for run-time debugging to be toggled on and off.  This
Xheader file is for external programs that wish to control the tape
Xdrive directly.  An example of such a program is "ioctl" which allows
Xthe user to enable and/or disable kernel-mode debugging for the tape
Xdriver.
X
XIndividual programs or packages within this distribution each have their
Xown README file.  See the appropriate README file for further information.
X
XSee the file README.install for more information on installing this package.
XSee the file README.ioctl for more information on ioctl.
XSee the file README.tctl for more information on tctl.
X
XThe author may be reached at the following locations:
X
X	Email via "ckctpa!crash@uunet.UU.NET"
X
X	USnail via  Frank J. Edwards
X		    2677 Arjay Court
X		    Palm Harbor, FL  34684-4505
X----------------------------------------------------------------------
XChanges for version 1.11:
X
X	This is about as final as the code is going to get.
X
X	I have determined that the Viper doesn't stream very well with
X		Amiga Unix, and I am endeavoring to find out why and fix it!
X	
X	Most of the changes for this version are cleanups -- adding RCS
X		headers, fixing typos in debugging out, typing in this
X		file and the other README's, etc.
X	
X	This is the first major release.  It is no longer beta code.
X
XChanges for version 1.8:
X
X	In order for ioctl to work with your new tape driver, you must apply
X		the patch to kernel.c; when you apply the patches in this
X		archive, it will automatically attempt to patch the file
X		"../../kernel.c" so if you're in /usr/sys/amiga/alien it will
X		be patched correctly.
X
X	A bug in the initialization of the Viper caused me to re-examine the
X		documentation for the drive.  It appears that the Viper will
X		auto-select 60MB, 120MB, or 150MB when reading a tape, and
X		will auto-select 120MB or 150MB when writing a tape (it seems
X		the Viper will detect which type of cartridge is installed,
X		ie. DC6150 or DC600A, and pick the appropriate mode).  This
X		allowed me to remove most of the code I had inserted to check
X		for possible conditions (such as writing 120MB format and
X		changing to /dev/rmt/4h in mid-stream to write 150MB!).  The
X		code actually shrank by a few lines...
SHAR_EOF
$TOUCH -am 1116025291 README.distr &&
chmod 0644 README.distr ||
echo "restore of README.distr failed"
set `wc -c README.distr`;Wc_c=$1
if test "$Wc_c" != "3521"; then
	echo original size 3521, current size $Wc_c
fi
fi
# ============= README.ioctl ==============
if test X"$1" != X"-c" -a -f 'README.ioctl'; then
	echo "File already exists: skipping 'README.ioctl'"
else
echo "x - extracting README.ioctl (Text)"
sed 's/^X//' << 'SHAR_EOF' > README.ioctl &&
XIOCTL is Copyright (c) 1991 by Frank J. Edwards
X
XThis file documents the interface and the operation of the IOCTL program.
X
X$Id: README.ioctl,v 1.1 1991/11/15 19:53:30 root Rel $
X
XThe syntax for the IOCTL command is shown here.  The "-s" option allows
Xone to specify the SCSI ID of the tape driver one wishes to affect.  The
Xdefault is 4.  The <cmd> parameter is used to tell the tape driver which
Xtype of debug flag or sense data to display.  The comments below describe
Xit generally.  The <cmd> may be specified either numerically or as the
Xtext shown below.  Currently, the implementation is case-sensitive.
X
X------------------------------------------------------------------------
XUsage:	ioctl [-s#] <cmd> [args]
X
X    The following flags are available and take an "on"/"off" argument:
X	 1  GENERAL	General information about the execution flow
X	 2  DEFAULT	Default blocks that should never be reached
X	 3  NON_IO	Any non-I/O operation is printed as it occurs
X	 4  IO		Any     I/O operation is printed as it occurs
X	 5  STATEDATA	Information about the state machine at each iteration
X	 6  GETSENSE	Any and all REQUEST SENSE data is printed
X	 7  ERROR	Extra information about error returns
X	 8  IOSIZE	Size of the I/O blocks being processed
X	 9  INFO	General information about the driver's configuration
X	10  ALL		All of the above
X
X    The following sense data are available:
X	11  INQ		SCSI INQUIRY command
X	12  MSENS	SCSI MODE SENSE command
X	13  MSEL	SCSI MODE SELECT command
X	14  RSENS	SCSI REQUEST SENSE command
X------------------------------------------------------------------------
X
XSample usages:
X
X    $ ioctl INQ		# perform on INQUIRY on SCSI ID 4 (default)
X    $ ioctl -s 0 INQ	# perform an INQUIRY on SCSI ID 0
X    $ ioctl GENERAL on	# turns on GENERAL debug flag
X    $ ioctl DEFAULT on	# turns on DEFAULT debug flag
X    $ ioctl ALL on	# turns on all debugging flags
X    $ ioctl IOSIZE off	# turns off IOSIZE (a lot less output now)
X    $ ioctl ALL off	# turns off all debugging flags
X
XIOCTL can be used while the tape drive is opened by another process, but
Xonly one IOCTL may be accessed a particular SCSI ID at one time.  There
Xmay be some delay before the IOCTL request takes effect since the command
Xthat you request is put into the device command queue like any other
Xcommand.
SHAR_EOF
$TOUCH -am 1116025291 README.ioctl &&
chmod 0644 README.ioctl ||
echo "restore of README.ioctl failed"
set `wc -c README.ioctl`;Wc_c=$1
if test "$Wc_c" != "2294"; then
	echo original size 2294, current size $Wc_c
fi
fi
# ============= README.tctl ==============
if test X"$1" != X"-c" -a -f 'README.tctl'; then
	echo "File already exists: skipping 'README.tctl'"
else
echo "x - extracting README.tctl (Text)"
sed 's/^X//' << 'SHAR_EOF' > README.tctl &&
XTCTL is Copyright (c) 1991 by Frank J. Edwards
X
XThis file documents the interface and the operation of the TCTL program.
X
X$Id: README.tctl,v 1.1 1991/11/15 19:53:30 root Rel $
X
XThe syntax for the TCTL command is shown here.  The "-s" option allows
Xone to specify the SCSI ID of the tape driver one wishes to affect.  The
Xdefault is 4.  The "iocmd(s)" are any continuous string of commands
Xsupported by this command.  See the sample usages below for examples.
X
XThe items marked with an asterisk ("splat" in Unix parlance) are commands
Xwhich may not exist or be supported across all SCSI tape drives.  They
Xare supported by the Archive Viper 2150S, at least.
X
XCurrently, the REQBLK function will not work on an unmodified kernel.
XIn fact, it doesn't work reliably on a modified kernel!  On an unmodified
Xkernel, it will print out meaningless values, but otherwise has no impact
Xon the machine (the kernel ignores the request for byte-alignment reasons).
X
X------------------------------------------------------------------------
XUsage:	tctl [-s#] iocmd(s)
X	-s		Select the SCSI ID of the tape drive (default 4)
XThese commands take no parameters:
X     *	eod		Move to EOD
X	erase		Erase the tape
X	msense		Print the write-protect mode (on=1/off=0),
X			buffering mode (on=1/off=0), and density (120/150)
X     *	reqblk		Print out current block position (blocks start at 1)
X	rewind		Rewind the tape
X     *	sense		Print out request sense data
XThese commands take 1 parameter:
X	fsb n		Forward Space <n> Blocks
X	fsf n		Forward Space <n> Filemarks
X	fssf n		Forward Space <n> Sequential Filemarks
X     *	seek n		Seek to block <n> (blocks start at 1)
X	verify n	Run CRC verification on <n> blocks
X	writefm n	Write <n> filemarks on tape
XThese commands take special parameters:
X	inquiry [n]	Print out inquiry data (only the first <n> bytes)
X	mselect b d	Select the buffering mode (on=1/off=0)
X			and density (120/150)
XCommands marked with an asterisk ("*") may not be supported by all drives
Xor the data returned by the unit may not be a format known to this program.
X------------------------------------------------------------------------
X
XA few of these command bear more explanation.
X
XFor the MSENSE command, the first value printed is the write-protect
Xstatus, the second value is buffering enabled/disabled, and the third
Xis the current density format.  The MSELECT command is similar, except
Xthat the write-protect status cannot be set by the host!  Otherwise, the
Xparameters "b" and "d" are the same as the output values from MSENSE.
XNote, however, that once the mode has been determined by the Viper during
Xauto-sense, the format cannot be changed without resetting the drive.
XThis can be done by opening and closing the cartridge door, or turning
Xthe unit off and back on.
X
XThe WRITEFM command will write a single filemark or multiple filemarks.
XThe multiple filemark writing allows an alternative form of fast seek
Xusing the FSSF (Forward Search Sequential Filemark).  The INQUIRY command
Xwill perform a SCSI INQUIRY on the SCSI ID specified.  This is basically
Xthe same as what IOCTL INQ will perform, but it works on any SCSI ID and
Xnot just a tape drive.
X
XFor a detailed analysis, obtain a copy of the Archive Viper Product Manual,
XSCSI Models 2060S and 2150S, manual part number 21391-001.  Maynard
XElectronics is the Service organization and can be reached at 1-800-227-6296
Xin Orlando, FL.  Archive's Customer Service is 1-800-537-2248 in Costa Mesa,
XCA.  The manual I have was last revised June 1988 so it is somewhat old.
SHAR_EOF
$TOUCH -am 1116025291 README.tctl &&
chmod 0644 README.tctl ||
echo "restore of README.tctl failed"
set `wc -c README.tctl`;Wc_c=$1
if test "$Wc_c" != "3521"; then
	echo original size 3521, current size $Wc_c
fi
fi
# ============= tape_sums ==============
if test X"$1" != X"-c" -a -f 'tape_sums'; then
	echo "File already exists: skipping 'tape_sums'"
else
echo "x - extracting tape_sums (Text)"
sed 's/^X//' << 'SHAR_EOF' > tape_sums &&
X#!/bin/ksh
X# This must run under ksh if you want it to auto-check your extraction.
X#
X# Generated by $Id: tape_sums,v 1.2 1991/11/15 23:20:47 root Rel root $
X
Xdiff - <(sum  COPYING README.distr README.ioctl README.tctl Makefile tape.c tape.h kernel.diffs ioctl.c tctl) <<'SUMS' &&
X14596 20 COPYING
X42327 7 README.distr
X52631 5 README.ioctl
X36068 7 README.tctl
X37632 3 Makefile
X47406 55 tape.c
X42407 5 tape.h
X56360 4 kernel.diffs
X5082 8 ioctl.c
X22483 22 tctl
XSUMS
Xecho "No errors." || { echo "Bad extraction/transmission."; }
SHAR_EOF
$TOUCH -am 1116025291 tape_sums &&
chmod 0755 tape_sums ||
echo "restore of tape_sums failed"
set `wc -c tape_sums`;Wc_c=$1
if test "$Wc_c" != "524"; then
	echo original size 524, current size $Wc_c
fi
fi
echo "End of part 1, continue with part 2"
#exit 0

#!/bin/sh
# This is part 02 of viper
if touch 2>&1 | fgrep '[-amc]' > /dev/null
 then TOUCH=touch
 else TOUCH=true
fi
# ============= tape.diffs ==============
if test X"$1" != X"-c" -a -f 'tape.diffs'; then
	echo "File already exists: skipping 'tape.diffs'"
else
echo "x - extracting tape.diffs (Text)"
sed 's/^X//' << 'SHAR_EOF' > tape.diffs &&
X*** /tmp/da002Or	Sat Nov 16 02:52:14 1991
X--- Makefile	Sat Nov 16 02:52:08 1991
X***************
X*** 1,10 ****
X! .c.o:
X! 	$(CC) -c -O $(CFLAGS) -I../.. -I../inc $<
X  
X! OBJ	= ct.o dd.o a2090.o a2091.o a3091.o sd.o sdpart.o physdsk.o scsi.o
X  
X  exp:	$(OBJ) Makefile
X  	ld -r -o exp $(OBJ)
X  
X  clean:
X  	-rm -f $(OBJ) exp
X--- 1,45 ----
X! # Changes for Archive Viper support Copyright (c) 1991 by Frank J. Edwards.
X! # All other code is the property of Commodore-Amiga, Inc.
X! #
X! # My tests in an attempt to get the Viper working...
X! # $Id: Makefile,v 1.5 1991/11/15 18:45:03 root Rel $
X! #
X! INCLUDE = -I../.. -I../inc
X! DEBUG = -DDEBUG
X! OPT = -O
X! CFLAGS = $(OPT:x=-g) $(INCLUDE)
X  
X! # OBJ = ct.o dd.o a2090.o a2091.o a3091.o sd.o sdpart.o physdsk.o scsi.o
X! MOST = tape.o dd.o a3091.o sd.o sdpart.o physdsk.o scsi.o
X! OBJ = a2090.o a2091.o $(MOST)
X  
X  exp:	$(OBJ) Makefile
X  	ld -r -o exp $(OBJ)
X  
X+ tape.o: tape.c tape.h
X+ 	$(CC) -c $(DEBUG) $(CFLAGS) $<
X+ 
X  clean:
X  	-rm -f $(OBJ) exp
X+ 
X+ tags: $(MOST:.o=.c)
X+ 	ctags $(MOST:.o=.c)
X+ 
X+ distr.src: tctl.uuZ
X+ 	./tape.sums.tmpl >tape.sums
X+ 	chmod 755 tape.sums
X+ 	echo 'Index:  Makefile'                 >tape.diffs
X+ 	-co -p1.1 Makefile | diff - Makefile   >>tape.diffs
X+ 	echo '\nIndex:  tape.c'                >>tape.diffs
X+ 	-co -p1.1 ct.c     | diff - tape.c     >>tape.diffs
X+ 	-diff -c /dev/null tape.h              >>tape.diffs
X+ 	echo 'Index:  ../../master.d/kernel.c' >>tape.diffs
X+ 	-diff ../../master.d/kernelOLD.c ../../master.d/kernel.c >>tape.diffs
X+ 	shar tape.sums tape.diffs ioctl.c tctl.uuZ >tape.patches
X+ 	rm tape.diffs tape.sums
X+ 
X+ tctl.uuZ:
X+ 	compress < /usr/local/bin/tctl               >tctl.Z
X+ 	uuencode tctl.Z tctl.Z                       >tctl.uuZ
X+ 	(echo "size\c"; wc -c <tctl.Z)              >>tctl.uuZ
X+ 	rm tctl.Z
X*** /tmp/da002Ot	Sat Nov 16 02:52:15 1991
X--- tape.c	Sat Nov 16 02:52:13 1991
X***************
X*** 1,5 ****
X- 
X- 
X  /*
X   * QIC cartridge tape drive, SCSI controller.
X   *	For use with Caliper, Sankyo, Wangtek.
X--- 1,3 ----
X***************
X*** 6,13 ****
X--- 4,18 ----
X   *
X   * ENOSPC is returned at end-of-tape.  EIO is returned by Media Error, but
X   * read resync is possible.  New Sankyo CP-150SE supported.
X+  *
X+  * Changes for Archive Viper support Copyright (c) 1991 by Frank J. Edwards
X   */
X  
X+ #ifndef lint
X+ static char RCS_tape_c[] =	/* Can't use #ident cuz it's broke! */
X+ 	"$Id: tape.c,v 1.11 1991/11/10 22:02:35 root Rel $";
X+ #endif
X+ 
X  #include	"sys/types.h"
X  #include	"sys/param.h"
X  #include	"sys/buf.h"
X***************
X*** 18,23 ****
X--- 23,29 ----
X  #include	"rico.h"
X  #include	"sd.h"
X  
X+ #include	"tape.h"
X  
X  /*
X   * minor device options
X***************
X*** 27,41 ****
X  #define	ctappend( d)	((d) & 1<<6)
X  #define	ctden150( d)	((d) & 1<<5)
X  
X- 
X  struct ct {
X  	uint		state;
X! 	bool		open;
X  	struct buf	*bhead,
X  			*btail;
X  	struct sdcom	com;
X! 	uchar		sense[14];
X  };
X  /* ct.state
X   */
X  #define	NORMAL_IDLE		0
X--- 33,56 ----
X  #define	ctappend( d)	((d) & 1<<6)
X  #define	ctden150( d)	((d) & 1<<5)
X  
X  struct ct {
X  	uint		state;
X! 	bool		open;		/* Number of opens (normally 1) */
X! 	uchar		htype;		/* Hardware type (archive/other) */
X  	struct buf	*bhead,
X  			*btail;
X  	struct sdcom	com;
X! 	uchar		sense[36];	/* Enough room for Inquiry data */
X! 	ushort		dbgopen;	/* Number of opens for ioctl() */
X! 	ulong		debug;		/* Debug options via ioctl() */
X  };
X+ 
X+ #define ARCHIVE		"ARCHIVE"		/* Special drives supported */
X+ 
X+ #define T_UNKNOWN		0
X+ #define T_ARCHIVE		1
X+ #define T_ANONYMOUS		2
X+ 
X  /* ct.state
X   */
X  #define	NORMAL_IDLE		0
X***************
X*** 59,64 ****
X--- 74,84 ----
X  #define	BAD_READ_2		18
X  #define	BAD_READ_NULS		19
X  
X+ #define STATUS_INQUIRY		20	/* Not really states, but commands */
X+ #define STATUS_MODE_SENSE	21
X+ #define STATUS_MODE_SELECT	22
X+ #define STATUS_REQ_SENSE	23	/* Could "23" instead be "GETSENSE"? */
X+ 
X  struct special {
X  	bool		active;
X  	uchar		com,
X***************
X*** 67,74 ****
X  };
X  /* special.com
X   */
X! #define	INIT120		0
X! #define	INIT150		1
X  #define	REWIND		2
X  #define	GETSENSE	3
X  #define	APPEND		4
X--- 87,94 ----
X  };
X  /* special.com
X   */
X! #define	INIT120		0	/* Driver now auto-senses on reads, this */
X! #define	INIT150		1	/*   is only used when writing. */
X  #define	REWIND		2
X  #define	GETSENSE	3
X  #define	APPEND		4
X***************
X*** 78,94 ****
X  #define	IO		8
X  #define	CLOSE		9
X  
X- 
X  #define	BSIZE	512
X  
X- 
X  static struct ct	cttab[SDCARDS][SDUNITS];
X  static struct special	special;
X  
X! void	breakup( ),
X! 	strategy( );
X  
X  
X  ctopen( devp, flags)
X  dev_t	*devp;
X  {
X--- 98,331 ----
X  #define	IO		8
X  #define	CLOSE		9
X  
X  #define	BSIZE	512
X  
X  static struct ct	cttab[SDCARDS][SDUNITS];
X  static struct special	special;
X  
X! /* prototypes for tape.c */
X! int	ctioctl(dev_t dev,int cmd,caddr_t arg,int mode,struct cred *cr,int *rval);
X! int	ctopen(dev_t *devp, int flags);
X! int	ctclose(dev_t dev, int flags);
X! int	ctread(dev_t dev, struct uio *uiop);
X! int	ctwrite(dev_t dev, struct uio *uiop);
X  
X+ static void	 dump(uchar *buf, long len);
X+ static void	 dump_sdcom(char *msg, struct sdcom *cp);
X+ static void	 dump_sense(char *file, int line, uchar *sense);
X+ static void	 breakup(struct buf *bp);
X+ static void	 strategy(struct buf *bp);
X+ static int	 init(int dev);
X+ static int	 command(struct ct *dp, int com);
X+ static int	 queue(struct ct *dp, struct buf *bp);
X+ static void	 ihandle(struct sdcom *cp);
X+ static int	 machine(struct ct *dp);
X+ static int	 dispose(struct ct *dp, int st);
X+ static int	 start(struct ct *dp, int com, int st);
X+ static int	 setresidue(uchar sense[], struct buf *bp);
X+ static char	*sensekey(int sense);
X+ static int	 report(struct ct *dp);
X+ static int	 printbits(int bits, char *names);
X  
X+ #ifndef DEBUG
X+ # define DEBUG_ON(x)    if (0) {
X+ # define DEBUG_OFF(x)   }
X+ 
X+ /* These inline's will prevent any code from being generated... */
X+ __inline static void dump(uchar *buf, long len) {}
X+ __inline static void dump_sdcom(char *msg, struct sdcom * cp) {}
X+ __inline static void dump_sense(char *file, int line, uchar * sense) {}
X+ 
X+ /* These fulfill any syntax/declaration requirements... */
X+ static char *States[1];
X+ static char *Commands[1];
X+ static char SCSI_List[1];
X+ static char *SCSI[1];
X+ 
X+ #else /* defined(DEBUG) */
X+ 
X+ # define DEBUG_ON(x)    if (dp->debug & (x)) {
X+ # define DEBUG_OFF(x)   }
X+ 
X+ # define STATE_DATA     /* nothing */
X+ # define FILEMARK       0x80
X+ # define EOM            0x40
X+ # define SENSEKEY       0x0F
X+ 
X+ /* Called to display sense data */
X+ static void     dump(uchar * buf, long len)
X+ {
X+     if (len > 0) {
X+         register int    pos = 0;
X+         while (len-- > 0) {
X+             printf("%x ", *buf++);
X+             pos += 3;
X+             if (pos > 76) {
X+                 putchar('\n');
X+                 pos = 0;
X+             }
X+         }
X+         if (pos)
X+             putchar('\n');
X+     }
X+ }
X+ 
X+ static void     dump_sdcom(char *msg, struct sdcom * cp)
X+ {
X+     printf("Contents of (struct sdcom)%s:\n", msg);
X+     printf("  sdcom.okay:  %x    sdcom.status:  %x\n", cp->okay, cp->status);
X+     printf("  sdcom.nbyte: %x    sdcom.cdb:     ", cp->nbyte);
X+     dump((uchar *) cp->cdb, 6);
X+ }
X+ 
X+ static void     dump_sense(char *file, int line, uchar * sense)
X+ {
X+     printf("Sense data from %s:%d\n ", file, line);
X+     dump(sense, 14);
X+     printf("  %s%s%s\n", sense[2] & FILEMARK ? "Filemark, " : "",
X+         sense[2] & EOM ? "EOM, " : "",
X+         sensekey(sense[2] & SENSEKEY));
X+ }
X+ #endif              /* DEBUG */
X+ 
X+ #ifdef STATE_DATA
X+ 
X+ #define STRING(x)   "" # x
X+ 
X+ static char *States[] = {
X+     STRING( NORMAL_IDLE ),
X+     STRING( NORMAL_READ ),
X+     STRING( NORMAL_SPECIAL ),
X+     STRING( NORMAL_SEOF ),
X+     STRING( NORMAL_READ_GETSENSE ),
X+     STRING( NORMAL_SEOF_GETSENSE ),
X+     STRING( EOD_IDLE ),
X+     STRING( EOD_WRITE ),
X+     STRING( EOD_WRITE_GETSENSE ),
X+     STRING( EOD_SPECIAL ),
X+     STRING( EOM_IDLE ),
X+     STRING( EOM_SPECIAL ),
X+     STRING( EOF_IDLE ),
X+     STRING( EOF_SPECIAL ),
X+     STRING( DOOM_IDLE ),
X+     STRING( DOOM_SPECIAL ),
X+     STRING( DOOM_MISC_GETSENSE ),
X+     STRING( BAD_READ_1 ),
X+     STRING( BAD_READ_2 ),
X+     STRING( BAD_READ_NULS ),
X+     STRING( STATUS_INQUIRY ),
X+     STRING( STATUS_MODE_SENSE ),
X+     STRING( STATUS_MODE_SELECT ),
X+     STRING( STATUS_REQ_SENSE ),
X+ };
X+ static char *Commands[] = {
X+     STRING( INIT120 ),
X+     STRING( INIT150 ),
X+     STRING( REWIND ),
X+     STRING( GETSENSE ),
X+     STRING( APPEND ),
X+     STRING( WEOF ),
X+     STRING( FLUSH ),
X+     STRING( SEOF ),
X+     STRING( IO ),
X+     STRING( CLOSE ),
X+     "(NULL)",
X+     "(NULL)",
X+     "(NULL)",
X+     "(NULL)",
X+     "(NULL)",
X+     "(NULL)",
X+     "(NULL)",
X+     "(NULL)",
X+     "(NULL)",
X+     "(NULL)",
X+     STRING( STATUS_INQUIRY ),
X+     STRING( STATUS_MODE_SENSE ),
X+     STRING( STATUS_MODE_SELECT ),
X+     STRING( STATUS_REQ_SENSE ),
X+ };
X+ 
X+ static char SCSI_List[] = { 1, 3, 8, 0x0A, 0x10, 0x11, 0x12, 0x15, 0x1A, 0 };
X+ static char *SCSI[] = {
X+     "Rewind",
X+     "ReqSense",
X+     "Read",
X+     "Write",
X+     "Write FM",
X+     "Space",
X+     "Inquiry",
X+     "Mode Select",
X+     "Mode Sense",
X+ };
X+ 
X+ #undef STRING
X+ 
X+ #endif /* STATE_DATA */
X+ 
X+ int ctioctl(dev, cmd, arg, mode, cr, rval)
X+ dev_t dev;
X+ int cmd, mode, *rval;
X+ caddr_t arg;
X+ struct cred *cr;
X+ {
X+ 	struct ct *dp = &cttab [sdcard(dev)] [sdunit(dev)];
X+ 	struct { int len; char data[36]; } tparg;
X+ 	int state, err = 0, size;
X+ 
X+ 	if (!dp->dbgopen) 	 /* Special open is required */
X+ 		return( EBADF );
X+ 	switch (cmd) {
X+ 	case TPIODBGON :
X+ 	case TPIODBGOFF :
X+ 		if ((long)arg & ~TPIO_ALL)       /* Unused bits were set! */
X+ 			return( EINVAL );
X+ 		if (cmd == TPIODBGON)
X+ 			dp->debug |= (ulong) arg;
X+ 		else
X+ 			dp->debug &= ~ (ulong) arg;
X+ 		*rval = dp->debug;
X+ 		break;
X+ 
X+ 	case TPIOINQ:		/* TPIOINQ    cmd == 0x40047481 */
X+ 	case TPIOMSENS:		/* TPIOMSENS  cmd == 0x40047482 */
X+ 	case TPIOMSEL:		/* TPIOMSEL   cmd == 0x80047483 */
X+ 	case TPIORSENS:		/* TPIORSENS  cmd == 0x40047484 */
X+ 		*rval = 0;
X+ 		size = 0;
X+ 		if (arg) {
X+ 			if (copyin(arg, &tparg, sizeof(tparg)))
X+ 				return( EFAULT );
X+ 			state = dp->state;
X+ 			switch (cmd) {
X+ 			case TPIOINQ :
X+ 				cmd = STATUS_INQUIRY; size = 36; break;
X+ 			case TPIOMSENS :
X+ 				cmd = STATUS_MODE_SENSE; size = 12; break;
X+ 			case TPIORSENS :
X+ 				cmd = STATUS_REQ_SENSE; size = 14; break;
X+ 			case TPIOMSEL :
X+ 				cmd = STATUS_MODE_SELECT; size = 12;
X+ 				bcopy(tparg.data, dp->sense, 12);
X+ 				break;
X+ 			}
X+ 			err = command(dp, cmd);
X+ 			dp->state = state;
X+ 			if (!err) {
X+ 				size = min(tparg.len, size);
X+ 				tparg.len = size;
X+ 				bcopy(dp->sense, tparg.data, size);
X+ 				if (copyout(&tparg, arg, size+sizeof(int)))
X+ 					err = EFAULT;
X+ 			}
X+ 		}
X+ 		break;
X+ 	default:
X+ 		printf("Bad command 0x%x passed to ctioctl()\n", cmd);
X+ 		return( EINVAL );
X+ 	}
X+ 	return( err );
X+ }
X+ 
X  ctopen( devp, flags)
X  dev_t	*devp;
X  {
X***************
X*** 99,119 ****
X  	int		e;
X  
X  	dp = &cttab[sdcard( *devp)][sdunit( *devp)];
X! 	if (dp->open)
X  		return (EBUSY);
X! 	if ((e = sdopen( sdcard( *devp)))
X! 	or (e = command( dp, GETSENSE)))
X  		return (e);
X! 	sense2 = dp->sense[2];
X! 	sense8 = dp->sense[8];
X! 	sense9 = dp->sense[9];
X! 	if (sense8 & 1<<6)
X! 		return (ENXIO);
X! 	if (sense9 & 1<<3) {
X! 		dp->state = NORMAL_IDLE;
X! 		if (e = init( *devp))
X  			return (e);
X  	}
X  	switch (flags & (FREAD|FWRITE)) {
X  	case FREAD:
X  		if (ctappend( *devp))
X--- 336,381 ----
X  	int		e;
X  
X  	dp = &cttab[sdcard( *devp)][sdunit( *devp)];
X! 	if (dp->open && !(flags & FNDELAY))
X  		return (EBUSY);
X! 	if (flags & FNDELAY) {	/* Used to access the ioctl() routines */
X! 		dp->dbgopen++;
X! 		return( 0 );
X! 	}
X! 	if (e = sdopen( sdcard( *devp)))
X  		return (e);
X! 	if (dp->htype == T_UNKNOWN) {
X! 		if (e = command(dp, STATUS_INQUIRY))
X  			return (e);
X+ DEBUG_ON( TPIO_GENERAL )
X+ 		dump(dp->sense, 36);
X+ 		dp->sense[16] = '\0';
X+ 		printf("Sense data returned :%s:\n", dp->sense+8);
X+ DEBUG_OFF( TPIO_GENERAL )
X+ 		if (strncmp(dp->sense+8, ARCHIVE, sizeof(ARCHIVE)-1) == 0)
X+ 			dp->htype = T_ARCHIVE;
X+ 		else
X+ 			dp->htype = T_ANONYMOUS;
X  	}
X+ 	if (e = command( dp, GETSENSE))
X+ 		return (e);
X+ 
X+ DEBUG_ON( TPIO_INFO )
X+ 	printf("Tape drive type is %d (%s)\n",
X+ 		dp->htype, dp->htype == T_ARCHIVE ? ARCHIVE : "Other");
X+ DEBUG_OFF( TPIO_INFO )
X+ 
X+ 	if (dp->htype != T_ARCHIVE) {
X+ 		if (dp->sense[8] & 1<<6)
X+ 			return (ENXIO);
X+ 		if (dp->sense[9] & 1<<3) {
X+ 			dp->state = NORMAL_IDLE;
X+ 			if (e = init( *devp))
X+ 				return (e);
X+ 		}
X+ 	} else {	/* Viper */
X+ 		dp->state = NORMAL_IDLE;
X+ 	}
X  	switch (flags & (FREAD|FWRITE)) {
X  	case FREAD:
X  		if (ctappend( *devp))
X***************
X*** 120,139 ****
X  			return (EINVAL);
X  		break;
X  	case FWRITE:
X! 		if (sense8 & 1<<4)
X  			return (EROFS);
X! 		if ((flags & FAPPEND)
X! 		or (ctappend( *devp)))
X  			if (e = command( dp, APPEND))
X  				return (e);
X! 		unless ((sense9 & 1<<3)
X! 		or (dp->state == EOD_IDLE))
X! 			return (ENXIO);
X  		break;
X  	default:
X  		return (EINVAL);
X  	}
X! 	dp->open = TRUE;
X  	return (0);
X  }
X  
X--- 382,401 ----
X  			return (EINVAL);
X  		break;
X  	case FWRITE:
X! 		if (dp->htype != T_ARCHIVE && (dp->sense[8] & 1<<4))
X  			return (EROFS);
X! 		if ((flags & FAPPEND) or (ctappend( *devp)))
X  			if (e = command( dp, APPEND))
X  				return (e);
X! 		if (dp->state != EOD_IDLE) {
X! 			if (dp->htype != T_ARCHIVE && !(dp->sense[9] & 1<<3))
X! 				return( ENXIO );
X! 		}
X  		break;
X  	default:
X  		return (EINVAL);
X  	}
X! 	dp->open++;
X  	return (0);
X  }
X  
X***************
X*** 144,161 ****
X  	struct ct	*dp;
X  
X  	dp = &cttab[sdcard( dev)][sdunit( dev)];
X! 	if (ctrewind( dev)) {
X! 		if (flags & FWRITE)
X! 			command( dp, WEOF);
X! 		command( dp, REWIND);
X  	}
X! 	else
X! 		if (flags & FWRITE)
X! 			command( dp, WEOF);
X! 		else
X! 			command( dp, SEOF);
X! 	command( dp, CLOSE);
X! 	dp->open = FALSE;
X  	return (0);
X  }
X  
X--- 406,429 ----
X  	struct ct	*dp;
X  
X  	dp = &cttab[sdcard( dev)][sdunit( dev)];
X! 	if (flags & FNDELAY) {	/* Used to access the ioctl() routines */
X! 		dp->dbgopen--;
X! 		return( 0 );
X  	}
X! 	if (dp->open) {
X! 		if (ctrewind( dev)) {
X! 			if (flags & FWRITE)
X! 				command( dp, WEOF);
X! 			command( dp, REWIND);
X! 		} else {
X! 			if (flags & FWRITE)
X! 				command( dp, WEOF);
X! 			else
X! 				command( dp, SEOF);
X! 		}
X! 		command( dp, CLOSE);
X! 		dp->open--;
X! 	}
X  	return (0);
X  }
X  
X***************
X*** 164,171 ****
X  dev_t		dev;
X  struct uio	*uiop;
X  {
X  
X! 	return (uiophysio( breakup, (struct buf *)0, dev, B_READ, uiop));
X  }
X  
X  
X--- 432,443 ----
X  dev_t		dev;
X  struct uio	*uiop;
X  {
X+ 	/* FIXME:  can't just check "open" or "dbgopen" since the drive
X+ 	 * could be opened by two processes for different reasons. */
X  
X! 	if (cttab[sdcard(dev)][sdunit(dev)].open)
X! 		return (uiophysio(breakup, (struct buf *)0, dev,B_READ, uiop));
X! 	return( EINVAL );	/* If open'd to do ioctl() stuff only */
X  }
X  
X  
X***************
X*** 173,180 ****
X  dev_t		dev;
X  struct uio	*uiop;
X  {
X  
X! 	return (uiophysio( breakup, (struct buf *)0, dev, B_WRITE, uiop));
X  }
X  
X  
X--- 445,456 ----
X  dev_t		dev;
X  struct uio	*uiop;
X  {
X+ 	/* FIXME:  can't just check "open" or "dbgopen" since the drive
X+ 	 * could be opened by two processes for different reasons. */
X  
X! 	if (cttab[sdcard(dev)][sdunit(dev)].open)
X! 		return (uiophysio(breakup, (struct buf *)0, dev,B_WRITE, uiop));
X! 	return( EINVAL );	/* If open'd to do ioctl() stuff only */
X  }
X  
X  
X***************
X*** 182,188 ****
X  breakup( bp)
X  struct buf	*bp;
X  {
X- 
X  	amiga_dma_pageio( strategy, bp);
X  }
X  
X--- 458,463 ----
X***************
X*** 204,211 ****
X  init( dev)
X  {
X  
X! 	int com = ctden150( dev)? INIT150: INIT120;
X! 	return (command( &cttab[sdcard( dev)][sdunit( dev)], com)? ENXIO: 0);
X  }
X  
X  
X--- 479,486 ----
X  init( dev)
X  {
X  
X! 	int com = ctden150(dev)? INIT150: INIT120;
X! 	return (command(&cttab[sdcard(dev)][sdunit(dev)], com)? ENXIO: 0);
X  }
X  
X  
X***************
X*** 216,221 ****
X--- 491,499 ----
X  	static bool	locked;
X  	int		x;
X  
X+ DEBUG_ON( TPIO_GENERAL )
X+ 	dump_sdcom(" at beginning of command()", &dp->com);
X+ DEBUG_OFF( TPIO_GENERAL )
X  	x = sdspl( );
X  	while (locked)
X  		sleep( &locked, PZERO);
X***************
X*** 229,236 ****
X  	splx( x);
X  	locked = FALSE;
X  	wakeup( &locked);
X! 	if (special.buf.b_flags & B_ERROR)
X! 		return (special.buf.b_error? special.buf.b_error: EIO);
X  	return (0);
X  }
X  
X--- 507,522 ----
X  	splx( x);
X  	locked = FALSE;
X  	wakeup( &locked);
X! 	if (special.buf.b_flags & B_ERROR) {
X! DEBUG_ON( TPIO_ERROR )
X! 		printf("Error %d (flags 0x%x) in CT:command(..., %d)\n",
X! 			special.buf.b_error, special.buf.b_flags, com);
X! DEBUG_ON( TPIO_GETSENSE )
X! 		dump_sense(__FILE__, __LINE__, special.sense);
X! DEBUG_OFF( TPIO_GETSENSE )
X! DEBUG_OFF( TPIO_ERROR )
X! 		return (special.buf.b_error ? special.buf.b_error : EIO);
X! 	}
X  	return (0);
X  }
X  
X***************
X*** 250,265 ****
X  		dp->bhead = bp;
X  		dp->btail = bp;
X  		machine( dp);
X  	}
X  }
X  
X  
X! static
X! ihandle( cp)
X! struct sdcom	*cp;
X  {
X  
X! 	machine( &cttab[cp->card][cp->unit]);
X  }
X  
X  
X--- 536,560 ----
X  		dp->bhead = bp;
X  		dp->btail = bp;
X  		machine( dp);
X+ DEBUG_ON( TPIO_GETSENSE )
X+ 		dump_sense(__FILE__, __LINE__, dp->sense);
X+ DEBUG_OFF( TPIO_GETSENSE )
X  	}
X  }
X  
X  
X! static void     ihandle(struct sdcom * cp)
X  {
X+ 	register struct ct *dp = &cttab[cp->card][cp->unit];
X+ DEBUG_ON( TPIO_INFO )
X+ 	register char *tmp = SCSI_List;
X  
X! 	while (*tmp && *tmp != *cp->cdb)
X! 		tmp++;
X! 	printf("Called ihandle; SCSI command %d (%s)\n",
X! 		*cp->cdb, *tmp ? SCSI[ tmp - SCSI_List ] : "Unknown");
X! DEBUG_OFF( TPIO_INFO )
X! 	machine( dp );
X  }
X  
X  
X***************
X*** 269,275 ****
X  {
X  	struct buf	*bp;
X  
X! 	while (bp = dp->bhead)
X  		switch (dp->state) {
X  		case NORMAL_IDLE:
X  			unless (bp == &special.buf) {
X--- 564,586 ----
X  {
X  	struct buf	*bp;
X  
X! 	while (bp = dp->bhead) {
X! 
X! DEBUG_ON( TPIO_STATEDATA )
X! 	if (bp == &special.buf) {
X! 		printf("Entering state %d (%s) with command %d (%s)\n",
X! 			dp->state, States[dp->state],
X! 			special.com, Commands[special.com]);
X!         }
X! DEBUG_OFF( TPIO_STATEDATA )
X! DEBUG_ON( TPIO_IO )
X! 	if (bp != &special.buf) {
X! 		printf("Entering state %d (%s) for function %s\n",
X! 			dp->state, States[dp->state],
X! 			bp->b_flags & B_READ ? "READ" : "WRITE");
X!         }
X! DEBUG_OFF( TPIO_IO )
X! 
X  		switch (dp->state) {
X  		case NORMAL_IDLE:
X  			unless (bp == &special.buf) {
X***************
X*** 280,291 ****
X  				return;
X  			}
X  			switch (special.com) {
X  			case INIT120:
X  			case INIT150:
X- 			case GETSENSE:
X  			case WEOF:
X  				start( dp, special.com, NORMAL_SPECIAL);
X  				return;
X  			case REWIND:
X  				start( dp, special.com, DOOM_SPECIAL);
X  				return;
X--- 591,608 ----
X  				return;
X  			}
X  			switch (special.com) {
X+ 			case STATUS_MODE_SENSE:
X+ 			case STATUS_MODE_SELECT:
X+ 			case STATUS_INQUIRY:
X  			case INIT120:
X  			case INIT150:
X  			case WEOF:
X  				start( dp, special.com, NORMAL_SPECIAL);
X  				return;
X+ 			case STATUS_REQ_SENSE:
X+ 			case GETSENSE:
X+ 				start( dp, GETSENSE, NORMAL_SPECIAL);
X+ 				return;
X  			case REWIND:
X  				start( dp, special.com, DOOM_SPECIAL);
X  				return;
X***************
X*** 317,335 ****
X  				dp->state = DOOM_MISC_GETSENSE;
X  				break;
X  			}
X  			switch (dp->sense[2] & 0x0F) {
X! 			case 0:
X  				if (setresidue( dp->sense, bp) < bp->b_bcount)
X  					dispose( dp, EOF_IDLE);
X  				dp->state = EOF_IDLE;
X  				break;
X! 			case 8:
X  				report( dp);
X  				if (setresidue( dp->sense, bp) < bp->b_bcount)
X  					dispose( dp, EOD_IDLE);
X  				dp->state = EOD_IDLE;
X  				break;
X! 			case 3:
X  				report( dp);
X  				if (setresidue( dp->sense, bp) < bp->b_bcount)
X  					dispose( dp, BAD_READ_1);
X--- 634,655 ----
X  				dp->state = DOOM_MISC_GETSENSE;
X  				break;
X  			}
X+ DEBUG_ON( TPIO_GETSENSE )
X+ 			dump_sense(__FILE__, __LINE__, special.sense);
X+ DEBUG_OFF( TPIO_GETSENSE )
X  			switch (dp->sense[2] & 0x0F) {
X! 			case 0:	/* No sense data */
X  				if (setresidue( dp->sense, bp) < bp->b_bcount)
X  					dispose( dp, EOF_IDLE);
X  				dp->state = EOF_IDLE;
X  				break;
X! 			case 8:	/* Blank Check (no data on tape) */
X  				report( dp);
X  				if (setresidue( dp->sense, bp) < bp->b_bcount)
X  					dispose( dp, EOD_IDLE);
X  				dp->state = EOD_IDLE;
X  				break;
X! 			case 3:	/* Medium Error */
X  				report( dp);
X  				if (setresidue( dp->sense, bp) < bp->b_bcount)
X  					dispose( dp, BAD_READ_1);
X***************
X*** 413,419 ****
X--- 733,750 ----
X  			case REWIND:
X  				start( dp, special.com, DOOM_SPECIAL);
X  				return;
X+ 			case STATUS_MODE_SENSE:
X+ 			case STATUS_MODE_SELECT:
X+ 			case STATUS_INQUIRY:
X+ 				start( dp, special.com, NORMAL_SPECIAL);
X+ 				return;
X+ 			case STATUS_REQ_SENSE:
X  			case GETSENSE:
X+ 				start( dp, GETSENSE, EOD_SPECIAL);
X+ DEBUG_ON( TPIO_GETSENSE )
X+ 				dump_sense(__FILE__, __LINE__, special.sense);
X+ DEBUG_OFF( TPIO_GETSENSE )
X+ 				return;
X  			case WEOF:
X  				start( dp, special.com, EOD_SPECIAL);
X  				return;
X***************
X*** 439,447 ****
X  				dp->state = DOOM_MISC_GETSENSE;
X  				break;
X  			}
X  			switch (dp->sense[2] & 0xF) {
X! 			case 0x0:
X! 			case 0xD:
X  				report( dp);
X  				if (setresidue( dp->sense, bp) < bp->b_bcount)
X  					dispose( dp, EOM_IDLE);
X--- 770,781 ----
X  				dp->state = DOOM_MISC_GETSENSE;
X  				break;
X  			}
X+ DEBUG_ON( TPIO_GETSENSE )
X+ 			dump_sense(__FILE__, __LINE__, special.sense);
X+ DEBUG_OFF( TPIO_GETSENSE )
X  			switch (dp->sense[2] & 0xF) {
X! 			case 0x0:	/* No sense data available */
X! 			case 0xD:	/* Volume overflow */
X  				report( dp);
X  				if (setresidue( dp->sense, bp) < bp->b_bcount)
X  					dispose( dp, EOM_IDLE);
X***************
X*** 475,482 ****
X  			case REWIND:
X  				start( dp, special.com, DOOM_SPECIAL);
X  				return;
X  			case GETSENSE:
X! 				start( dp, special.com, EOM_SPECIAL);
X  				return;
X  			default:
X  				dispose( dp, EOM_IDLE);
X--- 809,822 ----
X  			case REWIND:
X  				start( dp, special.com, DOOM_SPECIAL);
X  				return;
X+ 			case STATUS_MODE_SENSE:
X+ 			case STATUS_MODE_SELECT:
X+ 			case STATUS_INQUIRY:
X+ 				start( dp, special.com, NORMAL_SPECIAL);
X+ 				return;
X+ 			case STATUS_REQ_SENSE:
X  			case GETSENSE:
X! 				start( dp, GETSENSE, EOM_SPECIAL);
X  				return;
X  			default:
X  				dispose( dp, EOM_IDLE);
X***************
X*** 504,511 ****
X  			case REWIND:
X  				start( dp, special.com, DOOM_SPECIAL);
X  				return;
X  			case GETSENSE:
X! 				start( dp, special.com, EOF_SPECIAL);
X  				return;
X  			case CLOSE:
X  				dispose( dp, NORMAL_IDLE);
X--- 844,860 ----
X  			case REWIND:
X  				start( dp, special.com, DOOM_SPECIAL);
X  				return;
X+ 			case STATUS_MODE_SENSE:
X+ 			case STATUS_MODE_SELECT:
X+ 			case STATUS_INQUIRY:
X+ 				start( dp, special.com, NORMAL_SPECIAL);
X+ 				return;
X+ 			case STATUS_REQ_SENSE:
X  			case GETSENSE:
X! 				start( dp, GETSENSE, EOF_SPECIAL);
X! DEBUG_ON( TPIO_GETSENSE )
X! 				dump_sense(__FILE__, __LINE__, special.sense);
X! DEBUG_OFF( TPIO_GETSENSE )
X  				return;
X  			case CLOSE:
X  				dispose( dp, NORMAL_IDLE);
X***************
X*** 533,542 ****
X  				break;
X  			}
X  			switch (special.com) {
X  			case REWIND:
X- 			case GETSENSE:
X  				start( dp, special.com, DOOM_SPECIAL);
X  				return;
X  			default:
X  				bp->b_flags |= B_ERROR;
X  				dispose( dp, DOOM_IDLE);
X--- 882,902 ----
X  				break;
X  			}
X  			switch (special.com) {
X+ 			case STATUS_MODE_SENSE:
X+ 			case STATUS_MODE_SELECT:
X+ 			case STATUS_INQUIRY:
X+ 				start( dp, special.com, NORMAL_SPECIAL);
X+ 				return;
X  			case REWIND:
X  				start( dp, special.com, DOOM_SPECIAL);
X  				return;
X+ 			case STATUS_REQ_SENSE:
X+ 			case GETSENSE:
X+ 				start( dp, GETSENSE, DOOM_SPECIAL);
X+ DEBUG_ON( TPIO_GETSENSE )
X+ 				dump_sense(__FILE__, __LINE__, special.sense);
X+ DEBUG_OFF( TPIO_GETSENSE )
X+ 				return;
X  			default:
X  				bp->b_flags |= B_ERROR;
X  				dispose( dp, DOOM_IDLE);
X***************
X*** 559,564 ****
X--- 919,925 ----
X  			bp->b_flags |= B_ERROR;
X  			dispose( dp, DOOM_IDLE);
X  		}
X+ 	}
X  }
X  
X  
X***************
X*** 588,604 ****
X  	static uchar init120[] = {
X  		0x00, 0x00, 0x10, 0x08,
X  		0x0F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02, 0x00
X! 	};
X  	static uchar init150[] = {
X  		0x00, 0x00, 0x10, 0x08,
X  		0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02, 0x00
X! 	};
X  	struct buf	*bp;
X  
X  	switch (com) {
X  	case APPEND:
X! 		dp->com.cdb[0] = 0x11;
X! 		dp->com.cdb[1] = 0x03;
X  		dp->com.cdb[2] = 0;
X  		dp->com.cdb[3] = 0;
X  		dp->com.cdb[4] = 0;
X--- 949,966 ----
X  	static uchar init120[] = {
X  		0x00, 0x00, 0x10, 0x08,
X  		0x0F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02, 0x00
X! 	}; /*   ^^^^  15 is value for QIC-120  */
X  	static uchar init150[] = {
X  		0x00, 0x00, 0x10, 0x08,
X  		0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02, 0x00
X! 	}; /*   ^^^^  16 is value for QIC-150  */
X  	struct buf	*bp;
X  
X+ 	dp->com.nbyte = 0;
X  	switch (com) {
X  	case APPEND:
X! 		dp->com.cdb[0] = 0x11;  /* (O) Space */
X! 		dp->com.cdb[1] = 0x03;  /* Move to EOD */
X  		dp->com.cdb[2] = 0;
X  		dp->com.cdb[3] = 0;
X  		dp->com.cdb[4] = 0;
X***************
X*** 605,654 ****
X  		dp->com.cdb[5] = 0;
X  		dp->com.reading = FALSE;
X  		break;
X  	case GETSENSE:
X! 		dp->com.addr = dp->sense;
X! 		dp->com.nbyte = sizeof dp->sense;
X! 		dp->com.cdb[0] = 0x03;
X  		dp->com.cdb[1] = 0;
X  		dp->com.cdb[2] = 0;
X  		dp->com.cdb[3] = 0;
X! 		dp->com.cdb[4] = sizeof dp->sense;
X  		dp->com.cdb[5] = 0;
X  		dp->com.reading = TRUE;
X  		break;
X  	case INIT120:
X! 		dp->com.addr = init120;
X  		dp->com.nbyte = sizeof init120;
X! 		dp->com.cdb[0] = 0x15;
X  		dp->com.cdb[1] = 0;
X  		dp->com.cdb[2] = 0;
X  		dp->com.cdb[3] = 0;
X! 		dp->com.cdb[4] = 0x0C;
X  		dp->com.cdb[5] = 0;
X  		dp->com.reading = FALSE;
X  		break;
X  	case INIT150:
X! 		dp->com.addr = init150;
X  		dp->com.nbyte = sizeof init150;
X! 		dp->com.cdb[0] = 0x15;
X  		dp->com.cdb[1] = 0;
X  		dp->com.cdb[2] = 0;
X  		dp->com.cdb[3] = 0;
X! 		dp->com.cdb[4] = 0x0C;
X  		dp->com.cdb[5] = 0;
X  		dp->com.reading = FALSE;
X  		break;
X  	case IO:
X  		bp = dp->bhead;
X! 		dp->com.addr = vtop( paddr( bp), bp->b_flags&B_KERNBUF? (struct proc *)0: bp->b_proc);
X  		dp->com.nbyte = bp->b_bcount;
X  		if (bp->b_flags & B_READ) {
X  			dp->com.reading = TRUE;
X! 			dp->com.cdb[0] = 0x08;
X  		}
X  		else {
X  			dp->com.reading = FALSE;
X! 			dp->com.cdb[0] = 0x0A;
X  		}
X  		dp->com.cdb[1] = 0x01;
X  		dp->com.cdb[2] = byte2( bp->b_bcount/BSIZE);
X--- 967,1044 ----
X  		dp->com.cdb[5] = 0;
X  		dp->com.reading = FALSE;
X  		break;
X+ 	case STATUS_INQUIRY:
X+ 	case STATUS_MODE_SENSE:
X+ 	case STATUS_MODE_SELECT:
X+ 		dp->com.reading = TRUE;
X+ 		switch (com) {
X+ 		case STATUS_INQUIRY:
X+ 			dp->com.cdb[0] = 0x12;  /* (M) Inquiry */
X+ 			dp->com.nbyte = 36; /* Viper uses 36 bytes of data */
X+ 			break;
X+ 		case STATUS_MODE_SENSE:
X+ 			dp->com.cdb[0] = 0x1a;  /* (O) Mode Sense */
X+ 			dp->com.nbyte = 12; /* Viper uses 12 bytes of mode */
X+ 			break;
X+ 		case STATUS_MODE_SELECT:
X+ 			dp->com.cdb[0] = 0x15;  /* (O) Mode Select */
X+ 			dp->com.nbyte = 12; /* Viper uses 12 bytes of mode */
X+ 			dp->com.reading = FALSE;
X+ 			break;
X+ 		}
X+ 		dp->com.addr = (caddr_t) dp->sense;
X+ 		dp->com.cdb[1] = 0;
X+ 		dp->com.cdb[2] = 0;
X+ 		dp->com.cdb[3] = 0;
X+ 		dp->com.cdb[4] = dp->com.nbyte;
X+ 		dp->com.cdb[5] = 0;
X+ 		break;
X+     /*  case STATUS_REQ_SENSE:  /* machine() uses code for GETSENSE */
X  	case GETSENSE:
X! 		dp->com.addr = (caddr_t) dp->sense;
X! 		dp->com.nbyte = 14;	/* 14 bytes of sense data */
X! 		dp->com.cdb[0] = 0x03;	/* (M) Request Sense */
X  		dp->com.cdb[1] = 0;
X  		dp->com.cdb[2] = 0;
X  		dp->com.cdb[3] = 0;
X! 		dp->com.cdb[4] = dp->com.nbyte;
X  		dp->com.cdb[5] = 0;
X  		dp->com.reading = TRUE;
X  		break;
X  	case INIT120:
X! 		dp->com.addr = (caddr_t) init120;
X  		dp->com.nbyte = sizeof init120;
X! 		dp->com.cdb[0] = 0x15;  /* (O) Mode Select */
X  		dp->com.cdb[1] = 0;
X  		dp->com.cdb[2] = 0;
X  		dp->com.cdb[3] = 0;
X! 		dp->com.cdb[4] = dp->com.nbyte;
X  		dp->com.cdb[5] = 0;
X  		dp->com.reading = FALSE;
X  		break;
X  	case INIT150:
X! 		dp->com.addr = (caddr_t) init150;
X  		dp->com.nbyte = sizeof init150;
X! 		dp->com.cdb[0] = 0x15;  /* (O) Mode Select */
X  		dp->com.cdb[1] = 0;
X  		dp->com.cdb[2] = 0;
X  		dp->com.cdb[3] = 0;
X! 		dp->com.cdb[4] = dp->com.nbyte;
X  		dp->com.cdb[5] = 0;
X  		dp->com.reading = FALSE;
X  		break;
X  	case IO:
X  		bp = dp->bhead;
X! 		dp->com.addr = (caddr_t)
X! 		    vtop(paddr(bp), bp->b_flags & B_KERNBUF ? 0 : bp->b_proc);
X  		dp->com.nbyte = bp->b_bcount;
X  		if (bp->b_flags & B_READ) {
X  			dp->com.reading = TRUE;
X! 			dp->com.cdb[0] = 0x08;		/* (M) Read */
X  		}
X  		else {
X  			dp->com.reading = FALSE;
X! 			dp->com.cdb[0] = 0x0A;		/* (M) Write */
X  		}
X  		dp->com.cdb[1] = 0x01;
X  		dp->com.cdb[2] = byte2( bp->b_bcount/BSIZE);
X***************
X*** 657,663 ****
X  		dp->com.cdb[5] = 0;
X  		break;
X  	case REWIND:
X! 		dp->com.cdb[0] = 0x01;
X  		dp->com.cdb[1] = 0;
X  		dp->com.cdb[2] = 0;
X  		dp->com.cdb[3] = 0;
X--- 1047,1053 ----
X  		dp->com.cdb[5] = 0;
X  		break;
X  	case REWIND:
X! 		dp->com.cdb[0] = 0x01;	/* (M) Rewind */
X  		dp->com.cdb[1] = 0;
X  		dp->com.cdb[2] = 0;
X  		dp->com.cdb[3] = 0;
X***************
X*** 666,682 ****
X  		dp->com.reading = FALSE;
X  		break;
X  	case SEOF:
X! 		dp->com.cdb[0] = 0x11;
X! 		dp->com.cdb[1] = 0x01;
X  		dp->com.cdb[2] = 0;
X  		dp->com.cdb[3] = 0;
X! 		dp->com.cdb[4] = 1;
X  		dp->com.cdb[5] = 0;
X  		dp->com.reading = FALSE;
X  		break;
X  	case WEOF:
X! 		dp->com.cdb[0] = 0x10;
X! 		dp->com.cdb[1] = 0x01;
X  		dp->com.cdb[2] = 0;
X  		dp->com.cdb[3] = 0;
X  		dp->com.cdb[4] = 1;
X--- 1056,1073 ----
X  		dp->com.reading = FALSE;
X  		break;
X  	case SEOF:
X! 		dp->com.cdb[0] = 0x11;  /* (O) Space */
X! 		dp->com.cdb[1] = 0x01;  /* Advance by File Mark */
X  		dp->com.cdb[2] = 0;
X  		dp->com.cdb[3] = 0;
X! 		dp->com.cdb[4] = 1; /* # File Marks */
X  		dp->com.cdb[5] = 0;
X  		dp->com.reading = FALSE;
X  		break;
X  	case WEOF:
X! 		dp->com.cdb[0] = 0x10;  /* (M) Write File Marks */
X! 		dp->com.cdb[1] = dp->htype == T_ARCHIVE ? 0 : 1;
X! 					/* [FJE] 0x00 for Viper, was 0x01 */
X  		dp->com.cdb[2] = 0;
X  		dp->com.cdb[3] = 0;
X  		dp->com.cdb[4] = 1;
X***************
X*** 684,702 ****
X  		dp->com.reading = FALSE;
X  		break;
X  	case FLUSH:
X! 		dp->com.cdb[0] = 0x10;
X  		dp->com.cdb[1] = 0;
X  		dp->com.cdb[2] = 0;
X  		dp->com.cdb[3] = 0;
X! 		dp->com.cdb[4] = 0;
X  		dp->com.cdb[5] = 0;
X  		dp->com.reading = FALSE;
X  	}
X  	dp->com.card = sdcard( dp-cttab[0]);
X! 	dp->com.unit = sdunit( dp-cttab[sdcard( dp-cttab[0])]);
X  	dp->com.intr = ihandle;
X  	dp->state = st;
X  	sdqueue( &dp->com);
X  }
X  
X  
X--- 1075,1107 ----
X  		dp->com.reading = FALSE;
X  		break;
X  	case FLUSH:
X! 		dp->com.cdb[0] = 0x10;  /* (M) Write File Marks */
X  		dp->com.cdb[1] = 0;
X  		dp->com.cdb[2] = 0;
X  		dp->com.cdb[3] = 0;
X! 		dp->com.cdb[4] = 0;     /* Zero File Marks means Flush */
X  		dp->com.cdb[5] = 0;
X  		dp->com.reading = FALSE;
X  	}
X  	dp->com.card = sdcard( dp-cttab[0]);
X! 	dp->com.unit = sdunit( dp-cttab[dp->com.card]);
X  	dp->com.intr = ihandle;
X  	dp->state = st;
X  	sdqueue( &dp->com);
X+ 
X+ DEBUG_ON( TPIO_NON_IO|TPIO_IO )
X+ 	printf("Command %d (%s)\n", com, Commands[com]);
X+ DEBUG_OFF( TPIO_NON_IO|TPIO_IO )
X+ 
X+ DEBUG_ON( TPIO_IOSIZE|TPIO_IO )
X+ 	if (com == IO)
X+ 		printf("%s size == 0x%x\n",
X+ 			bp->b_flags & B_READ ? "Read" : "Write", bp->b_bcount);
X+ DEBUG_OFF( TPIO_IOSIZE|TPIO_IO )
X+ 
X+ DEBUG_ON( TPIO_GENERAL )
X+ 	dump_sdcom(" after sdqueue in start()", &dp->com);
X+ DEBUG_OFF( TPIO_GENERAL )
X  }
X  
X  
X***************
X*** 714,740 ****
X  	return (bp->b_resid);
X  }
X  
X  
X  static
X  report( dp)
X  struct ct	*dp;
X  {
X- 	static uchar *sense[] = {
X- 		0,
X- 		0,
X- 		"not ready",
X- 		"media error",
X- 		"hardware error",
X- 		"illegal request",
X- 		"unit attention",
X- 		"data protect",
X- 		"blank check",
X- 		0,
X- 		0,
X- 		"aborted command",
X- 		0,
X- 		"volume overflow"
X- 	};
X  	uint	s;
X  
X  	printf( "%s: tape ct%d: ", sdhardwarename( dp->com.card), dp->com.card*SDUNITS+dp->com.unit);
X--- 1119,1150 ----
X  	return (bp->b_resid);
X  }
X  
X+ static char    *sensekey(int sense)
X+ {
X+     static char    *SenseKey[] = {
X+         "FM/EOM or no status available",
X+         "Recoverable error; command successful",
X+         "Not Ready",
X+         "Medium Error",
X+         "Hardware Error",
X+         "Illegal Request (illegal parameter in CDB)",
X+         "Unit Attention (tape changed or drive reset)",
X+         "Data Protect (cartridge is write-portected)",
X+         "Blank Check (no-data condition on tape)",
X+         "Vendor Unique Code -- not used on Viper",
X+         "-- undocumented error --",
X+         "Aborted Command (may retry)",
X+         "-- undocumented error --",
X+         "Volume Overflow",
X+     };
X+ #define KEY_SIZE    sizeof(SenseKey)
X+     return (sense < KEY_SIZE ? SenseKey[sense] : (char *)NULL);
X+ }
X  
X  static
X  report( dp)
X  struct ct	*dp;
X  {
X  	uint	s;
X  
X  	printf( "%s: tape ct%d: ", sdhardwarename( dp->com.card), dp->com.card*SDUNITS+dp->com.unit);
X***************
X*** 744,758 ****
X  		switch (dp->com.status) {
X  		case 0:
X  			s = dp->sense[2] & 0x0F;
X! 			if ((s < nel( sense))
X! 			and (sense[s]))
X! 				printf( "%s (", sense[s]);
X  			else
X  				printf( "sense %d (", s);
X  			printbits( dp->sense[2], "FM EOMILI");
X! 			printbits( dp->sense[8], "   CNI   WRPeomUDEBNLFIL");
X! 			printbits( dp->sense[9], "      NDT   BOMBPE");
X  			printf( "%d)\n", *(ushort *)(dp->sense+10));
X  			break;
X  		case 8:
X  			printf( "busy\n");
X--- 1154,1170 ----
X  		switch (dp->com.status) {
X  		case 0:
X  			s = dp->sense[2] & 0x0F;
X! 			if (sensekey(s))
X! 				printf( "%s (", sensekey(s));
X  			else
X  				printf( "sense %d (", s);
X  			printbits( dp->sense[2], "FM EOMILI");
X! 			if (dp->htype != T_ARCHIVE) {
X! 				printbits( dp->sense[8], "   CNI   WRPeomUDEBNLFIL");
X! 				printbits( dp->sense[9], "      NDT   BOMBPE");
X! 			}
X  			printf( "%d)\n", *(ushort *)(dp->sense+10));
X+ 			dump(dp->com.cdb, 6);
X  			break;
X  		case 8:
X  			printf( "busy\n");
X***************
X*** 765,771 ****
X  
X  static
X  printbits( bits, names)
X! uchar	*names;
X  {
X  
X  	while (*names) {
X--- 1177,1183 ----
X  
X  static
X  printbits( bits, names)
X! char	*names;
X  {
X  
X  	while (*names) {
X*** /tmp/da002Ox	Sat Nov 16 02:52:15 1991
X--- tape.h	Sat Nov 16 02:52:13 1991
X***************
X*** 0 ****
X--- 1,59 ----
X+ /*
X+  *	IOCTL Tape Drive support (originally for Archive Viper, but generalized)
X+  *	Copyright (c) 1991 by Frank J. Edwards.
X+  *
X+  *  Header file for defining ioctl() flag values.  Since the structures
X+  *  within the tape driver are only used internally, they needn't be
X+  *  defined here.
X+  */
X+ 
X+ #ifndef _SYS_TAPE_H
X+ #define _SYS_TAPE_H
X+ 
X+ #ifndef lint
X+ static char RCS_tape_h[] =
X+ 	"$Id: tape.h,v 1.3 1991/11/09 17:38:56 root Rel $";
X+ #endif
X+ 
X+ #include "sys/ioccom.h"
X+ 
X+ /*
X+  *  The next commands take an integer as a parameter.  The integer
X+  *  is a bit mask of which debug options to turn on or off.  Note that
X+  *  to set a particular state, you would need to turn off all bits
X+  *  and turn on just the ones you want.
X+  */
X+ 
X+ #define TPIODBGON    _IOR('t', 1, int)   /* Enable debugging modes */
X+ #define TPIODBGOFF   _IOW('t', 2, int)   /* Disable debugging modes */
X+ 
X+ #define TPIO_GENERAL         0x0001      /* Print general debug info */
X+ #define TPIO_DEFAULT         0x0002      /* Complain about "default:"'s taken */
X+ #define TPIO_NON_IO          0x0004      /* Only non I/O requests */
X+ #define TPIO_IO              0x0008      /* Only I/O requests */
X+ #define TPIO_STATEDATA       0x0010      /* Only state changes data */
X+ #define TPIO_GETSENSE        0x0020      /* Only GETSENSE requests */
X+ #define TPIO_ERROR           0x0040      /* Additional info on errors */
X+ #define TPIO_IOSIZE          0x0080      /* Report I/O size requests */
X+ #define TPIO_INFO            0x0100      /* Informational type messages */
X+ 
X+ #define TPIO_ALL             0x01FF      /* All valid bit positions */
X+ 
X+ /*
X+  *  The following commands take a parameter which is a pointer to
X+  *  a structure containing the address at which to store the return
X+  *  data (or where to obtain it from in the case of TPIOMSEL) and an
X+  *	integer describing the amount of memory allocated for it.
X+  */
X+ 
X+ struct tpio_arg {
X+     int     tpio_len;        /* How much is at 'tpio_addr'? */
X+     char    tpio_addr[1];    /* Start of data space */
X+ };
X+ 
X+ #define TPIOINQ      _IOR('t', 129, caddr_t)     /* Inquiry data */
X+ #define TPIOMSENS    _IOR('t', 130, caddr_t)     /* Mode Sense data */
X+ #define TPIOMSEL     _IOW('t', 131, caddr_t)     /* Mode Select data */
X+ #define TPIORSENS    _IOR('t', 132, caddr_t)     /* Request Sense data */
X+ 
X+ #endif /* _SYS_TAPE_H */
SHAR_EOF
$TOUCH -am 1116025291 tape.diffs &&
chmod 0644 tape.diffs ||
echo "restore of tape.diffs failed"
set `wc -c tape.diffs`;Wc_c=$1
if test "$Wc_c" != "36682"; then
	echo original size 36682, current size $Wc_c
fi
fi
# ============= kernel.diffs ==============
if test X"$1" != X"-c" -a -f 'kernel.diffs'; then
	echo "File already exists: skipping 'kernel.diffs'"
else
echo "x - extracting kernel.diffs (Text)"
sed 's/^X//' << 'SHAR_EOF' > kernel.diffs &&
X*** kernelOLD.c	Sun Nov 10 17:04:06 1991
X--- ../../master.d/kernel.c	Wed Dec 31 19:00:00 1969
X***************
X*** 239,245 ****
X  extern rkopen(),rkclose(),rkread(),rkwrite(),rkioctl();
X  #endif /* KERNEL_DEBUGGER */
X  extern struct streamtab audioinfo;
X! extern ctopen(),ctclose(),ctread(),ctwrite();
X  extern fdopen(),fdclose(),fdread(),fdwrite(),fdioctl(),
X  	fdstrategy(),fdprint(),fdsize();
X  extern hdopen(),hdclose(),hdread(),hdwrite(),hdioctl(),
X--- 239,245 ----
X  extern rkopen(),rkclose(),rkread(),rkwrite(),rkioctl();
X  #endif /* KERNEL_DEBUGGER */
X  extern struct streamtab audioinfo;
X! extern ctopen(),ctclose(),ctread(),ctwrite(),ctioctl();
X  extern fdopen(),fdclose(),fdread(),fdwrite(),fdioctl(),
X  	fdstrategy(),fdprint(),fdsize();
X  extern hdopen(),hdclose(),hdread(),hdwrite(),hdioctl(),
X***************
X*** 336,342 ****
X  		ND,ND,ND,ND,ND,notty,&qlinfo,nullflag,		/*13=ql*/
X  ND,ND,ND,ND,ND,ND,ND,ND,ND,ND,notty,&ptsinfo,oldflag,		/*14=pts*/
X  ND,ND,ND,ND,ND,ND,ND,ND,ND,ND,notty,&ptminfo,oldflag,		/*15=ptmx*/
X! ctopen,ctclose,ctread,ctwrite,ND,
X  		ND,ND,ND,ND,ND,notty,nostr,nullflag,		/*16=ct*/
X  fdopen,fdclose,fdread,fdwrite,ND,
X  		ND,ND,ND,ND,ND,notty,nostr,nullflag,		/*17=fd*/
X--- 336,342 ----
X  		ND,ND,ND,ND,ND,notty,&qlinfo,nullflag,		/*13=ql*/
X  ND,ND,ND,ND,ND,ND,ND,ND,ND,ND,notty,&ptsinfo,oldflag,		/*14=pts*/
X  ND,ND,ND,ND,ND,ND,ND,ND,ND,ND,notty,&ptminfo,oldflag,		/*15=ptmx*/
X! ctopen,ctclose,ctread,ctwrite,ctioctl,
X  		ND,ND,ND,ND,ND,notty,nostr,nullflag,		/*16=ct*/
X  fdopen,fdclose,fdread,fdwrite,ND,
X  		ND,ND,ND,ND,ND,notty,nostr,nullflag,		/*17=fd*/
SHAR_EOF
$TOUCH -am 1116025291 kernel.diffs &&
chmod 0644 kernel.diffs ||
echo "restore of kernel.diffs failed"
set `wc -c kernel.diffs`;Wc_c=$1
if test "$Wc_c" != "1595"; then
	echo original size 1595, current size $Wc_c
fi
fi
# ============= ioctl.c ==============
if test X"$1" != X"-c" -a -f 'ioctl.c'; then
	echo "File already exists: skipping 'ioctl.c'"
else
echo "x - extracting ioctl.c (Text)"
sed 's/^X//' << 'SHAR_EOF' > ioctl.c &&
X/*
X *	IOCTL -- interface to the run-time debugging for my tape driver
X *
X *	Copyright (c) 1991 Frank J. Edwards
X *	See the GNU Public License file "COPYING" for copyright details.
X */
X
X#ifndef lint
Xstatic char RCS_ioctl_c[] =
X	"$Id: ioctl.c,v 1.3 1991/11/15 21:46:46 root Rel $";
X#endif
X
X#include <stdio.h>
X#include <fcntl.h>
X#include <errno.h>
X#include <stdarg.h>
X#include "tape.h"
X
X#define DEFAULT_TAPE	4
X#define TAPEn		"/dev/rmt/%dhn"
X
X#define SIZEOF(x)	(sizeof(x) / sizeof((x)[0]))
X#define STRING(x)   "" # x
X
Xstatic char *Flags[] = {
X	STRING( GENERAL ),
X	STRING( DEFAULT ),
X	STRING( NON_IO ),
X	STRING( IO ),
X	STRING( STATEDATA ),
X	STRING( GETSENSE ),
X	STRING( ERROR ),
X	STRING( IOSIZE ),
X	STRING( INFO ),
X	STRING( ALL ),
X};
X
Xstatic char *Senses[] = {
X	STRING( INQ ),
X	STRING( MSENS ),
X	STRING( MSEL ),
X	STRING( RSENS ),
X};
X
Xstatic int Sense_cmd[] = {
X	TPIOINQ,
X	TPIOMSENS,
X	TPIOMSEL,
X	TPIORSENS,
X};
X
X/* Called to display sense data */
Xstatic void     dump(unsigned char * buf, long len)
X{
X    if (len > 0) {
X        register int    pos = 0;
X        while (len-- > 0) {
X            printf("%02X ", *buf++);
X            pos += 3;
X            if (pos > 76) {
X                putchar('\n');
X                pos = 0;
X            }
X        }
X        if (pos)
X            putchar('\n');
X    }
X}
X
Xvoid usage(int ier, char *fmt, ...)
X{
X	int i;
X
X	if (fmt && *fmt) {
X		va_list args;
X
X		va_start(args, fmt);
X		vfprintf(stderr, fmt, args);
X		va_end(args);
X	}
X	fprintf(stderr, "Usage:\tioctl [-s#] Flag_cmd { on | off }\n");
X	fprintf(stderr, "\tioctl [-s#] Sense_cmd\n");
X	fprintf(stderr, "\n\tThe following flags are available:\n");
X	for (i=0; i < SIZEOF(Flags); i++)
X		fprintf(stderr, "\t%2d  %s\n", i+1, Flags[i]);
X	fprintf(stderr, "\n\tThe following sense data are available:\n");
X	for (i=0; i < SIZEOF(Senses); i++)
X		fprintf(stderr, "\t%2d  %s\n", i+1+SIZEOF(Flags), Senses[i]);
X	exit( ier );
X	/* NOTREACHED */
X}
X
Xmain(int argc, char **argv)
X{
X	char buf[64], **cmd;
X	int indx, fd, err, scsi_id = DEFAULT_TAPE;
X	struct { int len; unsigned char arg[36]; } data;
X
X	if (argc < 2) {
X		usage(10, NULL);
X		/* NOTREACHED */
X	}
X	argv++, argc--;
X
X	if (argv[0][0] == '-' && argv[0][1] == 's') {
X		scsi_id = atoi(argv[0][2] ? argv[0]+2 : *++argv);
X		argv++, argc--;
X	}
X	if (err = atoi(*argv)) {
X		if (err < 1 || err > SIZEOF(Flags)+SIZEOF(Senses))
X			usage(10, "Invalid numeric parameter.");
X		if (err > SIZEOF(Flags))
X			cmd = Senses + err - SIZEOF(Flags) - 1;
X		else
X			cmd = Flags + err - 1;
X	} else {
X		for (cmd = Flags; cmd < Flags + SIZEOF(Flags); cmd++)
X			if (0 == strcmp(*cmd, *argv)) break;
X		if (cmd == Flags + SIZEOF(Flags))
X			for (cmd = Senses; cmd < Senses + SIZEOF(Senses); cmd++)
X				if (0 == strcmp(*cmd, *argv)) break;
X
X		if (cmd == Senses + SIZEOF(Senses)) {
X			usage(10, "Unknown command:  '%s'\n", *argv);
X			/* NOTREACHED */
X		}
X	}
X	sprintf(buf, TAPEn, scsi_id);
X	fd = open(buf, O_RDONLY | O_NDELAY);	/* NDELAY == no I/O to tape */
X	if (fd < 0) {
X		perror( buf );
X		exit( 10 );
X		/* NOTREACHED */
X	}
X	argv++, argc--;
X	if (cmd >= Flags && cmd < Flags + SIZEOF(Flags)) {
X		if (argc < 1)
X			usage(10, "Not enough parameters.\n");
X		indx = 1 << (cmd - Flags);
X		if (cmd - Flags == SIZEOF(Flags)-1)
X			indx = TPIO_ALL;
X		err = strcmp(*argv++, "off") & 01;
X		err = ioctl(fd, err ? TPIODBGON : TPIODBGOFF, indx);
X		if (err == -1)
X			perror("ioctl()");
X		else {
X			puts("\tCurrent flag settings:");
X			err &= TPIO_ALL;
X			for (cmd = Flags; err; err >>= 1, cmd++)
X				if (err & 1)
X					printf("\t%2d  %s\n",cmd-Flags+1, *cmd);
X		}
X	} else {
X		/* Senses */
X		indx = cmd - Senses;
X		data.len = sizeof(data.arg);
X		err = ioctl(fd, Sense_cmd[indx], &data);
X		if (err == -1)
X			perror("ioctl()");
X		else
X			dump(data.arg, data.len);
X	}
X	close(fd);
X	return( err );
X}
SHAR_EOF
$TOUCH -am 1116025291 ioctl.c &&
chmod 0644 ioctl.c ||
echo "restore of ioctl.c failed"
set `wc -c ioctl.c`;Wc_c=$1
if test "$Wc_c" != "3760"; then
	echo original size 3760, current size $Wc_c
fi
fi
echo "End of part 2"
exit 0
