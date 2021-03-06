- _Short: VNC Remote Screen and Keyboard access for BeOS._
- _Author: agmsmith@rogers.com (Alexander G. M. Smith)_
- _Uploader: agmsmith@rogers.com (Alexander G. M. Smith)_
- _Website: http://web.ncf.ca/au829/_
- _Version: 4.0-BeOS-AGMS-1.18_
- _Type: Internet & Network / Servers / Remote Access_
- _Requires: BeOS 5.0+_

# VNCServer

VNCServer lets you use your BeOS computer from anywhere there is an Internet 
connection.  You can think of it as a really long keyboard, mouse and video 
cable.  A VNC client (available elsewhere for Windows, Mac, Linux, others) 
shows you the BeOS screen and sends keystrokes and mouse actions to your 
BeOS system over the Internet.  The VNCServer software running on BeOS takes 
that data from the client, and simulates button presses on a fake keyboard 
and movements of an imaginary mouse.  In the opposite direction, it 
scans your BeOS screen for changes, compresses the resulting graphics data 
and transmits it to the client.

This is a port of VNC using RealVNC's version 4.0 final source code (which 
has an extremely well designed class structure, making it easy to do 
this port).  There are lots of VNC clients out there, but I can recommend 
the RealVNC ones as working very well under Windows.  You can get their 
clients, servers and source code at http://www.realvnc.com/

# Installation and Use

To install it do these steps, reverse to uninstall.  Just do the last step to use it after you have installed it:

1.  Build the source code under BeOS PowerPC. At the moment this is done via 
the *.proj files.

2.  Move the "InputEventInjector" file into the /boot/home/config/add-ons/input_server/devices/ folder.  You can just drag and drop it into the 
conveniently included symbolic link to that directory.  This small program is 
the fake keyboard and mouse simulator, which can also be used by other 
programs to inject other user action events into the BeOS Input Server.

3.  Move the vncpasswd and vncserver files to /boot/home/config/bin (or wherever you keep command line utilities).

4.  Start up Terminal and run the "vncpasswd" command.  It will prompt you for a password for people to use when they want to access your BeOS computer over the Internet.  Without a password, anyone could take control of your computer!  It saves the password in encrypted form in the /boot/home/config/settings/VNCServer/passwd file.

5.  Also in Terminal, restart the input server so that it notices the new InputEventInjector fake keyboard/mouse thing.  You only need to do this once.  A reboot or typing in this command will do it:
/system/servers/input_server -q

6.  Start up the "vncserver" program whenever you want to allow people to connect.  If you want to see error messages (such as one telling you that your video card isn't supported), or specify extra startup options or see its help file, start it in a Terminal window (use "vncserver -h" for help).  When you're done, use the "Quit Application" menu item in the deskbar to make it shut down.  Those of you not seeing a cursor in your client (makes it hard to move the mouse) may want to try the "vncserver ShowCheapCursor=1" command to draw a fake cross cursor (I don't know how to get the current BeOS cursor shape).

Unfinished Stuff

It's finished.

VNCServer is released under the GNU General Public License.  See LICENSE.TXT for details.

- Alex (Ottawa, February 2005)

# 2022 notes
This code was pulled from the original archive and put on to GitHub to enable 
others to build this product for BeOS PowerPC. As part of this process a few
changes were made:

* the source was organised in to a root directory with sub directories for the extra stuff
* a script was added to create the links mentioned. This does 2 things, created the link for install and creates the _APP_ link for the compiler

The code was built on a PM9500 under BeOS R5.03 and confirmed to work using 
the vnc protocol in MacOS X Catalina. The framerate is slow, mainly because 
of the 10Mb networking under PowerPC.

