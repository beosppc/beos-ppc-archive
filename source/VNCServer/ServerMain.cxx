/******************************************************************************
 * $Header: /beos/source/VNCServer/ServerMain.cxx,v 1.1 2022/02/07 19:23:31 matt Exp $
 *
 * This is the main program for the BeOS version of the VNC server.  The basic
 * functionality comes from the VNC 4.0b4 source code (available from
 * http://www.realvnc.com/), with BeOS adaptations by Alexander G. M. Smith.
 *
 * Copyright (C) 2004 by Alexander G. M. Smith.  All Rights Reserved.
 *
 * This is free software; you can redistribute it and/or modify it under the
 * terms of the GNU General Public License as published by the Free Software
 * Foundation; either version 2 of the License, or (at your option) any later
 * version.
 *
 * This software is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this software; if not, write to the Free Software Foundation, Inc., 59
 * Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 * $Log: ServerMain.cxx,v $
 * Revision 1.1  2022/02/07 19:23:31  matt
 * added files
 *
 * Revision 1.19  2005/02/27 17:16:36  agmsmith
 * Standard STL STD namespace hack for PPC.
 *
 * Revision 1.18  2005/02/14 02:29:56  agmsmith
 * Removed unused parameters - HTTP servers and host wildcards.
 *
 * Revision 1.17  2005/02/13 01:28:44  agmsmith
 * Now notices clipboard changes and informs all the clients
 * about the new text contents.
 *
 * Revision 1.16  2005/02/06 23:39:14  agmsmith
 * Bumpped version number.
 *
 * Revision 1.15  2005/01/03 00:19:50  agmsmith
 * Based on more recent source than 4.0 Beta 4, update
 * comments to show that it's the final 4.0 VNC source.
 *
 * Revision 1.14  2005/01/02 21:09:46  agmsmith
 * Bump the version number.
 *
 * Revision 1.13  2004/12/12 21:44:39  agmsmith
 * Remove dead event loop timer, it does get stuck for long times
 * when the network connection is down.
 *
 * Revision 1.12  2004/12/05 23:40:04  agmsmith
 * Change timing system to use the event loop rather than a
 * separate thread.  Didn't fix the memory crash bug when
 * switching screen resolution - so it's not stack size or
 * multithreading.
 *
 * Revision 1.11  2004/11/27 22:53:12  agmsmith
 * Oops, forgot about the network time delay for new data.  Make it shorter
 * so that the overall update loop is faster.
 *
 * Revision 1.10  2004/11/22 02:40:40  agmsmith
 * Changed from Pulse() timing to using a separate thread, so now
 * mouse clicks and other time sensitive responses are much more
 * accurate (1/60 second accuracy at best).
 *
 * Revision 1.9  2004/09/13 01:41:53  agmsmith
 * Update rate time limits now in the desktop module.
 *
 * Revision 1.8  2004/07/19 22:30:19  agmsmith
 * Updated to work with VNC 4.0 source code (was 4.0 beta 4).
 *
 * Revision 1.7  2004/07/05 00:53:07  agmsmith
 * Check for a forced update too.
 *
 * Revision 1.6  2004/06/27 20:31:44  agmsmith
 * Got it working, so you can now see the desktop in different
 * video modes (except 8 bit).  Even lets you switch screens!
 *
 * Revision 1.5  2004/06/07 01:06:50  agmsmith
 * Starting to get the SDesktop working with the frame buffer
 * and a BDirectWindow.
 *
 * Revision 1.4  2004/02/08 19:43:57  agmsmith
 * FrameBuffer class under construction.
 *
 * Revision 1.3  2004/01/25 02:57:42  agmsmith
 * Removed loading and saving of settings, just specify the command line
 * options every time it is activated.
 *
 * Revision 1.2  2004/01/11 00:55:42  agmsmith
 * Added network initialisation and basic server code.  Now accepts incoming
 * connections!  But doesn't display a black remote screen yet.
 *
 * Revision 1.1  2004/01/03 02:32:55  agmsmith
 * Initial revision
 */

/* Posix headers. */

#include <errno.h>
#include <socket.h>

#include <list>
namespace std {
        int ___foobar___;
}
#define std

/* VNC library headers. */

#include <network/TcpSocket.h>
#include <rfb/Logger_stdio.h>
#include <rfb/LogWriter.h>
#include <rfb/SSecurityFactoryStandard.h>
#include <rfb/VNCServerST.h>

/* BeOS (Be Operating System) headers. */

#include <Alert.h>
#include <Application.h>
#include <Clipboard.h>
#include <DirectWindow.h>
#include <Locker.h>

/* Our source code */

#include "SDesktopBeOS.h"



/******************************************************************************
 * Global variables, and not-so-variable things too.  Grouped by functionality.
 */

static const unsigned int MSG_DO_POLLING_STEP = 'VPol';
  /* The message code for the BMessage which triggers a polling code to check a
  slice of the screen for changes.  The BMessage doesn't have any data, and is
  sent when the polling work is done, so that the next polling task is
  triggered almost immedately. */

static const char *g_AppSignature =
  "application/x-vnd.agmsmith.vncserver";

static const char *g_AboutText =
  "VNC Server for BeOS, based on VNC 4.0\n"
  "Adapted for BeOS by Alexander G. M. Smith\n"
  "$Header: /beos/source/VNCServer/ServerMain.cxx,v 1.1 2022/02/07 19:23:31 matt Exp $\n"
  "Compiled on " __DATE__ " at " __TIME__ ".";

static rfb::LogWriter vlog("ServerMain");

static rfb::IntParameter port_number("PortNumber",
  "TCP/IP port on which the server will accept connections",
  5900);

static rfb::VncAuthPasswdFileParameter vncAuthPasswd;
  // Creating this object is enough to register it with the
  // SSecurityFactoryStandard class system, specifying that we
  // store passwords in a file.



/******************************************************************************
 * ServerApp is the top level class for this program.  This handles messages
 * from the outside world and does some of the processing.  It also has
 * pointers to important data structures, like the VNC server stuff, or the
 * desktop (screen buffer access thing).
 */

class ServerApp : public BApplication
{
public: /* Constructor and destructor. */
  ServerApp ();
  ~ServerApp ();

  /* BeOS virtual functions. */
  virtual void AboutRequested ();
  virtual void MessageReceived (BMessage *MessagePntr);
  virtual void Pulse ();
  virtual bool QuitRequested ();
  virtual void ReadyToRun ();

  /* Our class methods. */
  void PollNetwork ();

public: /* Member variables. */
  SDesktopBeOS *m_FakeDesktopPntr;
    /* Provides access to the frame buffer, mouse, etc for VNC to use. */

  bigtime_t m_TimeOfLastBackgroundUpdate;
    /* The server main loop updates this with the current time whenever it
    finishes an update (checking for network input and optionally sending a
    sliver of the screen to the client).  If a long time goes by without an
    update, the pulse thread will inject a new BMessage, just in case the chain
    of update BMessages was broken. */

  network::TcpListener *m_TcpListenerPntr;
    /* A socket that listens for incoming connections. */

  rfb::VNCServerST *m_VNCServerPntr;
    /* A lot of the pre-made message processing logic is in this object. */
};



/******************************************************************************
 * Implementation of the ServerApp class.  Constructor, destructor and the rest
 * of the member functions in mostly alphabetical order.
 */

ServerApp::ServerApp ()
: BApplication (g_AppSignature),
  m_FakeDesktopPntr (NULL),
  m_TimeOfLastBackgroundUpdate (0),
  m_TcpListenerPntr (NULL),
  m_VNCServerPntr (NULL)
{
}


ServerApp::~ServerApp ()
{
  // Deallocate our main data structures.

  delete m_TcpListenerPntr;
  delete m_VNCServerPntr;
  delete m_FakeDesktopPntr;
}


/* Display a box showing information about this program. */

void ServerApp::AboutRequested ()
{
  BAlert *AboutAlertPntr;

  AboutAlertPntr = new BAlert ("About", g_AboutText, "Done");
  if (AboutAlertPntr != NULL)
  {
    AboutAlertPntr->SetShortcut (0, B_ESCAPE);
    AboutAlertPntr->Go ();
  }
}


void ServerApp::MessageReceived (BMessage *MessagePntr)
{
  if (MessagePntr->what == MSG_DO_POLLING_STEP)
    PollNetwork ();
  else if (MessagePntr->what == B_CLIPBOARD_CHANGED)
  {
    BMessage   *ClipMsgPntr;
    int32       TextLength;
    const char *TextPntr;

    if (m_VNCServerPntr != NULL && be_clipboard->Lock())
    {
      if ((ClipMsgPntr = be_clipboard->Data ()) != NULL)
      {
        TextPntr = NULL;
        ClipMsgPntr->FindData ("text/plain", B_MIME_TYPE,
          (const void **) &TextPntr, &TextLength);
        if (TextPntr != NULL)
          m_VNCServerPntr->serverCutText (TextPntr, TextLength);
      }
      be_clipboard->Unlock ();
    }
  }
  else
    /* Pass the unprocessed message to the inherited function, maybe it knows
    what to do.  This includes replies to messages we sent ourselves. */
    BApplication::MessageReceived (MessagePntr);
}


void ServerApp::PollNetwork ()
{
  if (m_VNCServerPntr == NULL)
    return;

  try
  {
    fd_set         rfds;
    struct timeval tv;

    tv.tv_sec = 0;
    tv.tv_usec = 5000; // Time delay in millionths of a second, keep short.

    FD_ZERO(&rfds);
    FD_SET(m_TcpListenerPntr->getFd(), &rfds);

    std::list<network::Socket*> sockets;
    m_VNCServerPntr->getSockets(&sockets);
    std::list<network::Socket*>::iterator iter;
    for (iter = sockets.begin(); iter != sockets.end(); iter++)
      FD_SET((*iter)->getFd(), &rfds);

    int n = select(FD_SETSIZE, &rfds, 0, 0, &tv);
    if (n < 0) throw rdr::SystemException("select",errno);

    for (iter = sockets.begin(); iter != sockets.end(); iter++) {
      if (FD_ISSET((*iter)->getFd(), &rfds)) {
        m_VNCServerPntr->processSocketEvent(*iter);
      }
    }

    if (FD_ISSET(m_TcpListenerPntr->getFd(), &rfds)) {
      network::Socket* sock = m_TcpListenerPntr->accept();
      m_VNCServerPntr->addClient(sock);
    }

    m_VNCServerPntr->checkTimeouts();

    // Run the background scan of the screen for changes, but only when an
    // update is requested.  Otherwise the update timing feedback system won't
    // work correctly (bursts of ridiculously high frame rates when the client
    // isn't asking for a new update).

    if (m_VNCServerPntr->clientsReadyForUpdate ())
      m_FakeDesktopPntr->BackgroundScreenUpdateCheck ();

    // Trigger the next update pretty much immediately, after other intervening
    // messages have been processed.

    PostMessage (MSG_DO_POLLING_STEP);
    m_TimeOfLastBackgroundUpdate = system_time ();
  }
  catch (rdr::Exception &e)
  {
    vlog.error(e.str());
  }
}


void ServerApp::Pulse ()
{
  if (m_TimeOfLastBackgroundUpdate == 0)
  {
    vlog.debug ("ServerApp::Pulse: Starting up the BMessage timing cycle.");
    m_TimeOfLastBackgroundUpdate = system_time ();
    PostMessage (MSG_DO_POLLING_STEP);
  }
}


/* A quit request message has come in. */

bool ServerApp::QuitRequested ()
{
  be_clipboard->StopWatching (be_app_messenger);
  return BApplication::QuitRequested ();
}


void ServerApp::ReadyToRun ()
{
  try
  {
    /* VNC Setup. */

    m_FakeDesktopPntr = new SDesktopBeOS ();

    m_VNCServerPntr = new rfb::VNCServerST ("MyBeOSVNCServer",
      m_FakeDesktopPntr, NULL);

    m_FakeDesktopPntr->setServer (m_VNCServerPntr);

    network::TcpSocket::initTcpSockets();
    m_TcpListenerPntr = new network::TcpListener ((int)port_number);
    vlog.info("Listening on port %d", (int)port_number);

    be_clipboard->StartWatching (be_app_messenger);

    SetPulseRate (3000000); // Deadman timer checks every 3 seconds.
  }
  catch (rdr::Exception &e)
  {
    vlog.error(e.str());
    PostMessage (B_QUIT_REQUESTED);
  }
}


// Display the program usage info, then terminate the program.

static void usage (const char *programName)
{
  fprintf(stderr, g_AboutText);
  fprintf(stderr, "\n\nusage: %s [<parameters>]\n", programName);
  fprintf(stderr,"\n"
    "Parameters can be turned on with -<param> or off with -<param>=0\n"
    "Parameters which take a value can be specified as "
    "-<param> <value>\n"
    "Other valid forms are <param>=<value> -<param>=<value> "
    "--<param>=<value>\n"
    "Parameter names are case-insensitive.  The parameters are:\n\n");
  rfb::Configuration::listParams(79, 14);
  exit(1);
}



/******************************************************************************
 * Finally, the main program which drives it all.
 */

int main (int argc, char** argv)
{
  ServerApp MyApp;
  int       ReturnCode = 0;

  if (MyApp.InitCheck () != B_OK)
  {
    vlog.error("Unable to initialise BApplication.");
    return -1;
  }

  try {
    rfb::initStdIOLoggers();
    rfb::LogWriter::setLogParams("*:stderr:1000");
      // Normal level is 30, use 1000 for debug messages.

    // Override the default parameters with new values from the command line.
    // Display the usage message and exit the program if an unknown parameter
    // is encountered.

    for (int i = 1; i < argc; i++) {
      if (argv[i][0] == '-') {
        if (rfb::Configuration::setParam(argv[i]))
          continue;
        if (i+1 < argc) {
          if (rfb::Configuration::setParam(&argv[i][1], argv[i+1])) {
            i++;
            continue;
          }
        }
        usage(argv[0]);
      }
      if (rfb::Configuration::setParam(argv[i]))
        continue;
      usage(argv[0]);
    }

    MyApp.Run (); // Run the main event loop.
    ReturnCode = 0;
  }
  catch (rdr::SystemException &s) {
    vlog.error(s.str());
    ReturnCode = s.err;
  } catch (rdr::Exception &e) {
    vlog.error(e.str());
    ReturnCode = -1;
  }

  return ReturnCode;
}
