/******************************************************************************
 * $Header: /CommonBe/agmsmith/Programming/VNC/vnc-4.0-beossrc/beosserver/RCS/FrameBufferBeOS.h,v 1.8 2005/02/12 19:47:24 agmsmith Exp $
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
 * $Log: FrameBufferBeOS.h,v $
 * Revision 1.8  2005/02/12 19:47:24  agmsmith
 * Moved the two different colour palette structures into the
 * parent class and unified them.
 *
 * Revision 1.7  2005/02/06 23:24:33  agmsmith
 * Added a generic status window feature so that even the
 * BScreen approach gets a status window.
 *
 * Revision 1.6  2005/02/06 21:30:43  agmsmith
 * Split frame buffer class into two parts, one for the old BDirectWindow
 * screen reading technique, and another for the new BScreen method.
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


/******************************************************************************
 * This wraps the VNC colour map around the BeOS colour map.  It's a pretty
 * passive class, so other people do things to it to change the values.
 */

class ColourMapHolder : public rfb::ColourMap
{
public:
  virtual void lookup (int index, int* r, int* g, int* b);

  color_map m_BeOSColourMap;
    // The actual BeOS colour map to use.
};



/******************************************************************************
 * This subclass of rfb:FrameBuffer lets us grab pixels from the screen.
 * It has two different subclasses itself, one for reading the bitmap using
 * the BScreen method (slower but more compatible) and one for the
 * BDirectWindow method.
 */

class FrameBufferBeOS : public rfb::FullFramePixelBuffer
{
public:
  FrameBufferBeOS ();
    // Throws an rdr::Exception if the video board is incompatible.
  virtual ~FrameBufferBeOS ();

  virtual int getStride () const;
  	// Just returns the cached stride value.  In pixels, not bytes!

  virtual void GrabScreen ();
    // Reads the whole screen into a memory buffer, for those techniques which
    // don't have live access to screen memory.  Called when the incremental
    // update has reached the bottom of the screen.  The default implementation
    // does nothing.

  virtual unsigned int LockFrameBuffer ();
  virtual void UnlockFrameBuffer ();
    // Call these to lock the frame buffer so that none of the settings
    // or data pointers change, then do your work, then unlock it.  All
    // the other functions in this class assume you have locked the
    // frame buffer if needed.  LockFrameBuffer returns the serial number
    // of the current settings, which gets incremented if the OS changes
    // screen resolution etc, so you can tell if you have to change things.
    // Maximum lock time is 3 seconds, otherwise the OS might give up on
    // the screen updates and render the bitmap pointer invalid.
    // Default implementation does nothing, returns 0.

  virtual void SetDisplayMessage (const char *StringPntr);
    // Sets the little bit of text in the corner of the screen that shows
    // the status of the server.

  virtual unsigned int UpdatePixelFormatEtc () = 0;
    // Makes sure the pixel format, width, height, raw bits pointer are
    // all up to date, matching the actual screen.  Returns the serial
    // number of the settings, so you can tell if they have changed since
    // the last time you called this function by a change in the serial
    // number.

protected:
  void InitialiseStatusView ();
    // A utility function for creating the status text BView and adding it to
    // the status window, telling it to draw its text from this object's
    // m_StatusString field.

  unsigned int m_CachedPixelFormatVersion;
    // This version number helps us quickly detect changes to the video mode,
    // and thus let us avoid setting the pixel format on every frame grab.

  unsigned int m_CachedStride;
    // Number of pixels on a whole row.  Equals number of bytes per row
    // (including padding bytes) divided by the number of bytes per pixel.

  ColourMapHolder m_ColourMap;
    // A copy of the screen's colour map, made when the pixel format was last
    // updated.

  char m_StatusString [20];
    // The currently displayed status message text.  The BView that draws the
    // status display reads the text out of this area of memory whenever it is
    // refreshed.

  BWindow *m_StatusWindowPntr;
    // These are used for the status window, which displays the update counter
    // in the top left corner of the screen.  The BDirect screen reader also
    // uses this for its special subclass of BDirectWindow which handles screen
    // reading and resolution change detection.  NULL if it hasn't been
    // created.
};


/* This one uses the BDirectWindow technique to map the frame buffer into
 * the virtual memory address space, where it can be read directly.
 */

class FrameBufferBDirect : public FrameBufferBeOS
{
public:
  FrameBufferBDirect ();
  virtual ~FrameBufferBDirect ();

  virtual unsigned int LockFrameBuffer ();
  virtual void UnlockFrameBuffer ();
  virtual unsigned int UpdatePixelFormatEtc ();
};


/* This one uses the BScreen technique to read the screen into a separate
 * bitmap, which then gets read by VNC.
 */

class FrameBufferBScreen : public FrameBufferBeOS
{
public:
  FrameBufferBScreen ();
  virtual ~FrameBufferBScreen ();

  virtual void GrabScreen ();
    // Grabs a copy of the pixels.  Assumes that the current pixel format is
    // the same as this bitmap.  The pixels are stored in m_FullScreenCopyPntr.

  virtual unsigned int UpdatePixelFormatEtc ();
    // Examines the current screen settings for changes, updates the pixel
    // format, and if necessary, also reallocates the m_FullScreenCopyPntr
    // bitmap to match the format change.

protected:
  BScreen *m_BScreenPntr;
    // The link back to the OS for the current screen settings.

  BBitmap *m_ScreenCopyPntr;
    // A copy of the screen, reallocated whenever the screen size or depth
    // changes, by UpdatePixelFormatEtc.

};
