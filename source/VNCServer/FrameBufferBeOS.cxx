/******************************************************************************
 * $Header: /beos/source/VNCServer/FrameBufferBeOS.cxx,v 1.1 2022/02/07 19:23:31 matt Exp $
 *
 * This is the frame buffer access module for the BeOS version of the VNC
 * server.  It implements an rfb::FrameBuffer object, which opens a
 * slave BDirectWindow to read the screen pixels.
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
 * $Log: FrameBufferBeOS.cxx,v $
 * Revision 1.1  2022/02/07 19:23:31  matt
 * added files
 *
 * Revision 1.15  2005/02/27 18:58:48  agmsmith
 * Also have to add bigendian video modes to the BScreen reader.
 *
 * Revision 1.14  2005/02/27 17:15:40  agmsmith
 * Safer shutdown (so it doesn't crash in PPC) and added
 * bigendian video modes.
 *
 * Revision 1.13  2005/02/12 19:47:24  agmsmith
 * Moved the two different colour palette structures into the
 * parent class and unified them.
 *
 * Revision 1.12  2005/02/08 00:33:11  agmsmith
 * Bad destruction log message.
 *
 * Revision 1.11  2005/02/06 23:37:13  agmsmith
 * Destruction log message added.
 *
 * Revision 1.10  2005/02/06 23:32:14  agmsmith
 * Add a log message for BScreen.
 *
 * Revision 1.9  2005/02/06 23:24:33  agmsmith
 * Added a generic status window feature so that even the
 * BScreen approach gets a status window.
 *
 * Revision 1.8  2005/02/06 21:31:13  agmsmith
 * Split frame buffer class into two parts, one for the old BDirectWindow
 * screen reading technique, and another for the new BScreen method.
 *
 * Revision 1.7  2004/12/13 03:13:39  agmsmith
 * Found the 8 bit mode problem - the colour palette uses
 * 16 bit values for R, G, B components, not 8 bit.
 *
 * Revision 1.6  2004/11/27 22:52:42  agmsmith
 * Not much - display string should be shorter.
 *
 * Revision 1.5  2004/07/19 22:30:19  agmsmith
 * Updated to work with VNC 4.0 source code (was 4.0 beta 4).
 *
 * Revision 1.4  2004/06/27 20:31:44  agmsmith
 * Got it working, so you can now see the desktop in different
 * video modes (except 8 bit).  Even lets you switch screens!
 *
 * Revision 1.3  2004/06/07 01:06:50  agmsmith
 * Starting to get the SDesktop working with the frame buffer
 * and a BDirectWindow.
 *
 * Revision 1.2  2004/02/08 21:13:34  agmsmith
 * BDirectWindow stuff under construction.
 *
 * Revision 1.1  2004/02/08 19:44:17  agmsmith
 * Initial revision
 */

/* Posix headers. */

#include <errno.h>

/* VNC library headers. */

#include <rdr/Exception.h>
#include <rfb/PixelBuffer.h>
#include <rfb/LogWriter.h>

/* BeOS (Be Operating System) headers. */

#include <Bitmap.h>
#include <DirectWindow.h>
#include <Locker.h>
#include <Screen.h>
#include <View.h>

/* Our source code */

#include "FrameBufferBeOS.h"


/******************************************************************************
 * Global variables, and not-so-variable things too.  Grouped by functionality.
 */

static rfb::LogWriter vlog("FrameBufferBeOS");



/******************************************************************************
 * This wraps the VNC colour map around the BeOS colour map.  It's a pretty
 * passive class, so other people do things to it to change the values.
 */

void ColourMapHolder::lookup (int index, int* r, int* g, int* b)
{
  rgb_color IndexedColour;

  if (index >= 0 && index < 256)
    IndexedColour = m_BeOSColourMap.color_list [index];
  else
  {
    IndexedColour.red = IndexedColour.green = 128;
    IndexedColour.blue = IndexedColour.alpha = 255;
  }

  // Scale colours up to 16 bit component values, so white is 0xFFFF in all
  // components and black is 0x0000 in all.

  *r = IndexedColour.red * 255 + IndexedColour.red;
  *g = IndexedColour.green * 255 + IndexedColour.green;
  *b = IndexedColour.blue * 255 + IndexedColour.blue;
}



/******************************************************************************
 * This BView draws the status text line and fills the window.
 */

class StatusDisplayBView : public BView
{
public:
  StatusDisplayBView (BRect ViewSize);

  virtual void AttachedToWindow (void);
  virtual void Draw (BRect UpdateRect);

  char *m_StringPntr;
};


StatusDisplayBView::StatusDisplayBView (BRect ViewSize)
  : BView (ViewSize, "StatusDisplayBView", B_FOLLOW_ALL_SIDES, B_WILL_DRAW)
{
}


void StatusDisplayBView::AttachedToWindow (void)
{
  SetViewColor (100, 100, 0);
  SetHighColor (255, 255, 128);
  SetLowColor (ViewColor ());
}


void StatusDisplayBView::Draw (BRect UpdateRect)
{
  font_height FontHeight;
  BPoint      Location;
  float       Width;

  Location = Bounds().LeftTop ();
  GetFontHeight (&FontHeight);
  Location.y +=
    (Bounds().Height() - FontHeight.ascent - FontHeight.descent) / 2 +
    FontHeight.ascent;
  Width = StringWidth (m_StringPntr);
  Location.x += (Bounds().Width() - Width) / 2;
  DrawString (m_StringPntr, Location);
}



/******************************************************************************
 * This variation of BDirectWindow allows us to capture the pixels on the
 * screen.  It works by appearing in the corner (I couldn't make it invisible
 * and behind the desktop window).  Then rather than the conventional use of a
 * BDirectWindow where the application writes to the frame buffer memory
 * directly, we just read it directly.
 */

class BDirectWindowReader : public BDirectWindow
{
public:
  BDirectWindowReader (color_map *ColourMapPntr);
  virtual ~BDirectWindowReader ();

  virtual void DirectConnected (direct_buffer_info *ConnectionInfoPntr);
    // Callback called by the OS when the video resolution changes or the frame
    // buffer setup is otherwise changed.

  unsigned int LockSettings ();
  void UnlockSettings ();
    // Lock and unlock the access to the common bitmap information data,
    // so that the OS can't change it while the bitmap is being read.
    // Keep locked for at most 3 seconds, otherwise the OS will time out
    // and render the bitmap pointer invalid.  Lock returns the serial number
    // of the changes, so if the serial number is unchanged then the settings
    // (width, height, bitmap pointer, screen depth) are unchanged too.

  virtual void ScreenChanged (BRect frame, color_space mode);
    // Informs the window about a screen resolution change.

  color_map *m_ColourMapPntr;
  	// Points to the colour map to be updated when the palette changes.  The
  	// map is owned by the creator of the window, so don't deallocate it.

  bool m_Connected;
    // TRUE if we are connected to the video memory, FALSE if not.  TRUE means
    // that video memory has been mapped into this process's address space and
    // we have a valid pointer to the frame buffer.  Don't try reading from
    // video memory if this is FALSE!

  unsigned int m_ConnectionVersion;
    // A counter that is bumped up by one every time the connection changes.
    // Makes it easy to see if your cached connection info is still valid.

  BLocker m_ConnectionLock;
    // This mutual exclusion lock makes sure that the callbacks from the OS to
    // notify the window about frame buffer changes (usually a screen
    // resolution change and the resulting change in frame buffer address and
    // size) are mutually exclusive from other window operations (like reading
    // the frame buffer or destroying the window).  Maximum lock time is 3
    // seconds, then the OS might kill the program for not responding.

  volatile bool m_DoNotConnect;
    // A flag that the destructor sets to tell the rest of the window code not
    // to try reconnecting to the frame buffer.

  direct_buffer_info m_SavedFrameBufferInfo;
      // A copy of the frame buffer information (bitmap address, video mode,
      // screen size) from the last direct connection callback by the OS.  Only
      // valid if m_Connected is true.  You should also lock m_ConnectionLock
      // while reading information from this structure, so it doesn't change
      // unexpectedly.

  BRect m_ScreenSize;
    // The size of the screen as a rectangle.  Updated when the resolution
    // changes.

private:
  BDirectWindowReader (); // Default constructor shouldn't be used so hide it.
};


BDirectWindowReader::BDirectWindowReader (color_map *ColourMapPntr)
  : BDirectWindow (BRect (0, 0, 1, 1), "BDirectWindowReader",
    B_NO_BORDER_WINDOW_LOOK,
    B_FLOATING_ALL_WINDOW_FEEL,
    B_NOT_MOVABLE | B_NOT_ZOOMABLE | B_NOT_RESIZABLE | B_AVOID_FOCUS,
    B_ALL_WORKSPACES),
  m_ColourMapPntr (ColourMapPntr),
  m_Connected (false),
  m_ConnectionVersion (0),
  m_DoNotConnect (true)
{
  BScreen  ScreenInfo (this /* Info for screen this window is on */);

  m_ScreenSize = ScreenInfo.Frame ();
  MoveTo (m_ScreenSize.left, m_ScreenSize.top);
  ResizeTo (80, 20); // A small window so it doesn't obscure the desktop.

  memcpy (m_ColourMapPntr, ScreenInfo.ColorMap (), sizeof (*m_ColourMapPntr));

  m_DoNotConnect = false; // Now ready for active operation.
}


BDirectWindowReader::~BDirectWindowReader ()
{
  m_DoNotConnect = true;

  // Force the window to disconnect from video memory, which will result in a
  // callback to our DirectConnected function.
  Hide ();
  Sync ();
}


void BDirectWindowReader::DirectConnected (
  direct_buffer_info *ConnectionInfoPntr)
{
  if (!m_Connected && m_DoNotConnect)
      return; // Shutting down or otherwise don't want to make a connection.

  m_ConnectionLock.Lock ();

  switch (ConnectionInfoPntr->buffer_state & B_DIRECT_MODE_MASK)
  {
    case B_DIRECT_START:
      m_Connected = true;
    case B_DIRECT_MODIFY:
      m_ConnectionVersion++;
      m_SavedFrameBufferInfo = *ConnectionInfoPntr;
      break;

    case B_DIRECT_STOP:
      m_Connected = false;
      break;
   }

   m_ConnectionLock.Unlock ();
}


unsigned int BDirectWindowReader::LockSettings ()
{
  m_ConnectionLock.Lock ();
  return m_ConnectionVersion;
}


void BDirectWindowReader::ScreenChanged (BRect frame, color_space mode)
{
  BDirectWindow::ScreenChanged (frame, mode);

  m_ConnectionLock.Lock ();
  m_ScreenSize = frame;
  m_ConnectionVersion++;
  m_ConnectionLock.Unlock ();

  // Update the palette if it is relevant.

  if (mode == B_CMAP8 || mode == B_GRAY8 || mode == B_GRAYSCALE_8_BIT ||
  mode == B_COLOR_8_BIT)
  {
    BScreen  ScreenInfo (this /* Info for screen this window is on */);
    memcpy (m_ColourMapPntr, ScreenInfo.ColorMap (),
      sizeof (*m_ColourMapPntr));
  }
}


void BDirectWindowReader::UnlockSettings ()
{
  m_ConnectionLock.Unlock ();
}



/******************************************************************************
 * Implementation of the FrameBufferBeOS abstract base class.  Constructor,
 * destructor and the rest of the member functions in mostly alphabetical
 * order.
 */

FrameBufferBeOS::FrameBufferBeOS () :
  m_CachedPixelFormatVersion (0),
  m_CachedStride (0),
  m_StatusWindowPntr (NULL)
{
  strcpy (m_StatusString, "VNC-BeOS");
}


FrameBufferBeOS::~FrameBufferBeOS ()
{
  if (m_StatusWindowPntr != NULL)
  {
    BMessenger Messenger (NULL, m_StatusWindowPntr);
    BMessage   QuitMessage (B_QUIT_REQUESTED);
    BMessage   ReplyMessage;

    // Locking and then quitting causes a crash on PPC, be more gentle with
    // a shutdown message, so it exits itself using its own thread.

    Messenger.SendMessage (&QuitMessage, &ReplyMessage); // Waits for reply.
    m_StatusWindowPntr = NULL;
  }
}


int FrameBufferBeOS::getStride () const
{
  return m_CachedStride;
}


void  FrameBufferBeOS::GrabScreen ()
{
}


void FrameBufferBeOS::InitialiseStatusView ()
{
  StatusDisplayBView *StatusDisplayViewPntr;

  StatusDisplayViewPntr =
    new StatusDisplayBView (m_StatusWindowPntr->Bounds ());
  StatusDisplayViewPntr->m_StringPntr = m_StatusString;
  m_StatusWindowPntr->AddChild (StatusDisplayViewPntr);
}


unsigned int FrameBufferBeOS::LockFrameBuffer ()
{
  return m_CachedPixelFormatVersion;
}


void FrameBufferBeOS::SetDisplayMessage (const char *StringPntr)
{
  BView *ChildViewPntr = NULL;

  if (StringPntr == NULL || StringPntr[0] == 0)
    m_StatusString [0] = 0;
  else
    strncpy (m_StatusString, StringPntr, sizeof (m_StatusString));
  m_StatusString [sizeof (m_StatusString) - 1] = 0;

  if (m_StatusWindowPntr != NULL)
  {
    m_StatusWindowPntr->Lock ();
    ChildViewPntr = m_StatusWindowPntr->ChildAt (0);
    if (ChildViewPntr != NULL)
      ChildViewPntr->Invalidate ();
    m_StatusWindowPntr->Unlock ();
  }
}


void FrameBufferBeOS::UnlockFrameBuffer ()
{
}



/******************************************************************************
 * Implementation of the FrameBufferBDirect class.  Constructor, destructor and
 * the rest of the member functions in mostly alphabetical order.
 */

FrameBufferBDirect::FrameBufferBDirect ()
{
  vlog.debug ("Constructing a FrameBufferBDirect object.");

  if (BDirectWindow::SupportsWindowMode ())
  {
    m_StatusWindowPntr =
      new BDirectWindowReader (&m_ColourMap.m_BeOSColourMap);
    m_StatusWindowPntr->Show (); // Opens the window and starts its thread.
    snooze (50000 /* microseconds */);
    InitialiseStatusView ();
    m_StatusWindowPntr->Sync (); // Wait for it to finish drawing etc.
    snooze (50000 /* microseconds */);
    LockFrameBuffer ();
    UpdatePixelFormatEtc ();
    UnlockFrameBuffer ();
  }
  else
    throw rdr::Exception ("Windowed mode not supported for BDirectWindow, "
      "maybe your graphics card needs DMA support and a hardware cursor",
      "FrameBufferBDirect");
}


FrameBufferBDirect::~FrameBufferBDirect ()
{
  vlog.debug ("Destroying a FrameBufferBDirect object.");
}


unsigned int FrameBufferBDirect::LockFrameBuffer ()
{
  if (m_StatusWindowPntr != NULL)
    return (static_cast<BDirectWindowReader *> (m_StatusWindowPntr))->
      LockSettings ();
  return 0;
}


void FrameBufferBDirect::UnlockFrameBuffer ()
{
  if (m_StatusWindowPntr != NULL)
    (static_cast<BDirectWindowReader *> (m_StatusWindowPntr))->
    UnlockSettings ();
}


unsigned int FrameBufferBDirect::UpdatePixelFormatEtc ()
{
  BDirectWindowReader *BDirectWindowPntr;
  direct_buffer_info  *DirectInfoPntr;
  unsigned int         EndianTest;

  BDirectWindowPntr = static_cast<BDirectWindowReader *> (m_StatusWindowPntr);
  if (BDirectWindowPntr == NULL)
  {
    width_ = 0;
    height_ = 0;
    return m_CachedPixelFormatVersion;
  }

  if (m_CachedPixelFormatVersion != BDirectWindowPntr->m_ConnectionVersion)
  {
    m_CachedPixelFormatVersion = BDirectWindowPntr->m_ConnectionVersion;
    DirectInfoPntr = &BDirectWindowPntr->m_SavedFrameBufferInfo;

    // Set up some initial default values.  The actual values will be put in
    // depending on the particular video mode.

    colourmap = &m_ColourMap;

    format.bpp = DirectInfoPntr->bits_per_pixel;
    // Number of actual colour bits, excluding alpha and pad bits.
    format.depth = DirectInfoPntr->bits_per_pixel;
    format.trueColour = true; // It usually is a non-palette video mode.

    EndianTest = 1;
    format.bigEndian = ((* (unsigned char *) &EndianTest) == 0);

    format.blueShift = 0;
    format.greenShift = 8;
    format.redShift = 16;
    format.redMax = format.greenMax = format.blueMax = 255;

    // Now set it according to the actual screen format.

    switch (DirectInfoPntr->pixel_format)
    {
      case B_RGB32: // xRGB 8:8:8:8, stored as little endian uint32
      case B_RGBA32: // ARGB 8:8:8:8, stored as little endian uint32
        format.bpp = 32;
        format.depth = 24;
        format.blueShift = 0;
        format.greenShift = 8;
        format.redShift = 16;
        format.redMax = format.greenMax = format.blueMax = 255;
        break;

      case B_RGB24:
        format.bpp = 24;
        format.depth = 24;
        format.blueShift = 0;
        format.greenShift = 8;
        format.redShift = 16;
        format.redMax = format.greenMax = format.blueMax = 255;
        break;

      case B_RGB16: // xRGB 5:6:5, stored as little endian uint16
        format.bpp = 16;
        format.depth = 16;
        format.blueShift = 0;
        format.greenShift = 5;
        format.redShift = 11;
        format.redMax = 31;
        format.greenMax = 63;
        format.blueMax = 31;
        break;

      case B_RGB15: // RGB 1:5:5:5, stored as little endian uint16
      case B_RGBA15: // ARGB 1:5:5:5, stored as little endian uint16
        format.bpp = 16;
        format.depth = 15;
        format.blueShift = 0;
        format.greenShift = 5;
        format.redShift = 10;
        format.redMax = format.greenMax = format.blueMax = 31;
        break;

      case B_CMAP8: // 256-color index into the color table.
      case B_GRAY8: // 256-color greyscale value.
        format.bpp = 8;
        format.depth = 8;
        format.blueShift = 0;
        format.greenShift = 0;
        format.redShift = 0;
        format.redMax = 255;
        format.greenMax = 255;
        format.blueMax = 255;
        format.trueColour = false;
        break;

      case B_RGB32_BIG: // xRGB 8:8:8:8, stored as big endian uint32
      case B_RGBA32_BIG: // ARGB 8:8:8:8, stored as big endian uint32
      case B_RGB24_BIG: // Currently unused
        format.bpp = 32;
        format.depth = 24;
        format.blueShift = 0;
        format.greenShift = 8;
        format.redShift = 16;
        format.redMax = format.greenMax = format.blueMax = 255;
        break;

      case B_RGB16_BIG: // RGB 5:6:5, stored as big endian uint16
        format.bpp = 16;
        format.depth = 16;
        format.blueShift = 0;
        format.greenShift = 5;
        format.redShift = 11;
        format.redMax = 31;
        format.greenMax = 63;
        format.blueMax = 31;
        break;

      case B_RGB15_BIG: // xRGB 1:5:5:5, stored as big endian uint16
      case B_RGBA15_BIG: // ARGB 1:5:5:5, stored as big endian uint16
        format.bpp = 16;
        format.depth = 15;
        format.blueShift = 0;
        format.greenShift = 5;
        format.redShift = 10;
        format.redMax = format.greenMax = format.blueMax = 31;
        break;

      default:
        vlog.error ("Unimplemented video mode #%d in UpdatePixelFormatEtc.",
        (unsigned int) DirectInfoPntr->pixel_format);
        break;
    }

    // Also update the real data - the actual bits and buffer size.

    data = (rdr::U8 *) DirectInfoPntr->bits;

    width_ = (int) (BDirectWindowPntr->m_ScreenSize.right -
      BDirectWindowPntr->m_ScreenSize.left + 1.5);
    height_ = (int) (BDirectWindowPntr->m_ScreenSize.bottom -
      BDirectWindowPntr->m_ScreenSize.top + 1.5);

    // Update the cached stride value.  Units are pixels, not bytes!

    if (DirectInfoPntr->bits_per_pixel <= 0)
      m_CachedStride = 0;
    else
      m_CachedStride = DirectInfoPntr->bytes_per_row /
      ((DirectInfoPntr->bits_per_pixel + 7) / 8);

    // Print out the new settings.  May cause a deadlock if you happen to be
    // printing this when the video mode is switching, since the AppServer
    // would be locked out and unable to display the printed text.  But
    // that only happens in debug mode.

    char TempString [2048];
    sprintf (TempString,
      "UpdatePixelFormatEtc new settings: "
      "Width=%d, Stride=%d, Height=%d, Bits at $%08X, ",
      width_, m_CachedStride, height_, (unsigned int) data);
    format.print (TempString + strlen (TempString),
      sizeof (TempString) - strlen (TempString));
    vlog.debug (TempString);
  }

  return m_CachedPixelFormatVersion;
}



/******************************************************************************
 * Implementation of the FrameBufferBScreen class.  Constructor, destructor and
 * the rest of the member functions in mostly alphabetical order.
 */

FrameBufferBScreen::FrameBufferBScreen ()
: m_BScreenPntr (NULL),
  m_ScreenCopyPntr (NULL)
{
  vlog.debug ("Constructing a FrameBufferBScreen object.");

  m_BScreenPntr = new BScreen (B_MAIN_SCREEN_ID);
  if (!m_BScreenPntr->IsValid ())
    throw rdr::Exception ("Creation of a new BScreen object has failed",
      "FrameBufferBScreen::FrameBufferBScreen");

  // Set up the status window.

  m_StatusWindowPntr = new BWindow (BRect (0, 0, 1, 1), "StatusWindow",
    B_NO_BORDER_WINDOW_LOOK,
    B_FLOATING_ALL_WINDOW_FEEL,
    B_NOT_MOVABLE | B_NOT_ZOOMABLE | B_NOT_RESIZABLE | B_AVOID_FOCUS,
    B_ALL_WORKSPACES);
  m_StatusWindowPntr->MoveTo (m_BScreenPntr->Frame().LeftTop());
  m_StatusWindowPntr->ResizeTo (80, 20);
  m_StatusWindowPntr->Show (); // Opens the window and starts its thread.
  InitialiseStatusView ();

  // And the pixel format and frame buffer initial contents.

  UpdatePixelFormatEtc ();
  GrabScreen ();
}


FrameBufferBScreen::~FrameBufferBScreen ()
{
  vlog.debug ("Destroying a FrameBufferBScreen object.");

  delete m_ScreenCopyPntr;
  delete m_BScreenPntr;
}


void FrameBufferBScreen::GrabScreen ()
{
  m_BScreenPntr->ReadBitmap (m_ScreenCopyPntr);
}


unsigned int FrameBufferBScreen::UpdatePixelFormatEtc ()
{
  BRect        BScreenFrame;
  int          BScreenHeight;
  int          BScreenWidth;
  color_space  BScreenColourSpace;
  unsigned int EndianTest;

  // Grab current general screen settings from the OS.

  BScreenColourSpace = m_BScreenPntr->ColorSpace (); // B_RGB_15 etc.
  BScreenFrame = m_BScreenPntr->Frame ();
  BScreenHeight = (int) (BScreenFrame.Height() + 1.5F);
  BScreenWidth = (int) (BScreenFrame.Width() + 1.5F);

  // See if anything has changed since last time.

  if (m_ScreenCopyPntr != NULL &&
  m_ScreenCopyPntr->Bounds().Height() + 1 == BScreenHeight &&
  m_ScreenCopyPntr->Bounds().Width() + 1 == BScreenWidth &&
  m_ScreenCopyPntr->ColorSpace() == BScreenColourSpace)
    return m_CachedPixelFormatVersion; // Nothing has changed.

  // Changed - reallocate the bitmap and store away the current palette
  // (presumably the palette rarely changes on its own).

  m_CachedPixelFormatVersion++; // Things have changed.

  delete m_ScreenCopyPntr;
  m_ScreenCopyPntr = new BBitmap (
    BRect (0, 0, BScreenWidth - 1, BScreenHeight - 1),
    BScreenColourSpace);
  if (!m_ScreenCopyPntr->IsValid ())
    throw rdr::Exception ("BBitmap allocation failed for new screen size",
    "FrameBufferBScreen::UpdatePixelFormatEtc");
  memcpy (&m_ColourMap.m_BeOSColourMap,
    m_BScreenPntr->ColorMap (), sizeof (m_ColourMap.m_BeOSColourMap));
  colourmap = &m_ColourMap;

  // GrabScreen - don't grab here, sometimes it grabs the previous screen
  // contents before BeOS redraws the new workspace, which wastes
  // time - better to leave it as newly allocated solid white and let the
  // normal refresh read the screen later, when its contents are stable.

  // Set up some initial default values.  The actual values will be put in
  // depending on the particular video mode.

  format.bpp = 24;
    // Number of actual colour bits, excluding alpha and pad bits.
  format.depth = 24;
  format.trueColour = true; // It usually is a non-palette video mode.

  EndianTest = 1;
  format.bigEndian = ((* (unsigned char *) &EndianTest) == 0);

  format.blueShift = 0;
  format.greenShift = 8;
  format.redShift = 16;
  format.redMax = format.greenMax = format.blueMax = 255;

  // Now set it according to the actual screen format.

  switch (BScreenColourSpace)
  {
    case B_RGB32: // xRGB 8:8:8:8, stored as little endian uint32
    case B_RGBA32: // ARGB 8:8:8:8, stored as little endian uint32
      format.bpp = 32;
      format.depth = 24;
      format.blueShift = 0;
      format.greenShift = 8;
      format.redShift = 16;
      format.redMax = format.greenMax = format.blueMax = 255;
      break;

    case B_RGB24:
      format.bpp = 24;
      format.depth = 24;
      format.blueShift = 0;
      format.greenShift = 8;
      format.redShift = 16;
      format.redMax = format.greenMax = format.blueMax = 255;
      break;

    case B_RGB16: // xRGB 5:6:5, stored as little endian uint16
      format.bpp = 16;
      format.depth = 16;
      format.blueShift = 0;
      format.greenShift = 5;
      format.redShift = 11;
      format.redMax = 31;
      format.greenMax = 63;
      format.blueMax = 31;
      break;

    case B_RGB15: // RGB 1:5:5:5, stored as little endian uint16
    case B_RGBA15: // ARGB 1:5:5:5, stored as little endian uint16
      format.bpp = 16;
      format.depth = 15;
      format.blueShift = 0;
      format.greenShift = 5;
      format.redShift = 10;
      format.redMax = format.greenMax = format.blueMax = 31;
      break;

    case B_CMAP8: // 256-color index into the color table.
    case B_GRAY8: // 256-color greyscale value.
      format.bpp = 8;
      format.depth = 8;
      format.blueShift = 0;
      format.greenShift = 0;
      format.redShift = 0;
      format.redMax = 255;
      format.greenMax = 255;
      format.blueMax = 255;
      format.trueColour = false;
      break;

    case B_RGB32_BIG: // xRGB 8:8:8:8, stored as big endian uint32
    case B_RGBA32_BIG: // ARGB 8:8:8:8, stored as big endian uint32
    case B_RGB24_BIG: // Currently unused
      format.bpp = 32;
      format.depth = 24;
      format.blueShift = 0;
      format.greenShift = 8;
      format.redShift = 16;
      format.redMax = format.greenMax = format.blueMax = 255;
      break;

    case B_RGB16_BIG: // RGB 5:6:5, stored as big endian uint16
      format.bpp = 16;
      format.depth = 16;
      format.blueShift = 0;
      format.greenShift = 5;
      format.redShift = 11;
      format.redMax = 31;
      format.greenMax = 63;
      format.blueMax = 31;
      break;

    case B_RGB15_BIG: // xRGB 1:5:5:5, stored as big endian uint16
    case B_RGBA15_BIG: // ARGB 1:5:5:5, stored as big endian uint16
      format.bpp = 16;
      format.depth = 15;
      format.blueShift = 0;
      format.greenShift = 5;
      format.redShift = 10;
      format.redMax = format.greenMax = format.blueMax = 31;
      break;

   default:
      vlog.error ("Unimplemented video mode #%d in UpdatePixelFormatEtc.",
      (unsigned int) BScreenColourSpace);
      break;
  }

  // Also update the real data - the actual bits and buffer size.

  data = (rdr::U8 *) m_ScreenCopyPntr->Bits ();
  m_CachedStride = m_ScreenCopyPntr->BytesPerRow() / ((format.bpp + 7) / 8);
  width_ = BScreenWidth;
  height_ = BScreenHeight;

  // Print out the new settings.  May cause a deadlock if you happen to be
  // printing this when the video mode is switching, since the AppServer
  // would be locked out and unable to display the printed text.  But
  // that only happens in debug mode.

  char TempString [2048];
  sprintf (TempString,
    "UpdatePixelFormatEtc new settings: "
    "Width=%d, Stride=%d, Height=%d, Bits at $%08X, ",
    width_, m_CachedStride, height_, (unsigned int) data);
  format.print (TempString + strlen (TempString),
    sizeof (TempString) - strlen (TempString));
  vlog.debug (TempString);

  return m_CachedPixelFormatVersion;
}
