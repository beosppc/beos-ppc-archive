/******************************************************************************
 * $Header: /CommonBe/agmsmith/Programming/VNC/vnc-4.0-beossrc/beosserver/RCS/SDesktopBeOS.h,v 1.15 2005/02/14 02:31:56 agmsmith Exp $
 *
 * This is the static desktop glue implementation that holds the frame buffer
 * and handles mouse messages, the clipboard and other BeOS things on one side,
 * and talks to the VNC Server on the other side.
 *
 * Seems simple, but it shares the BDirectWindowReader with the frame buffer (a
 * FrameBufferBeOS) and uses the BDirectWindowReader for part of its view into
 * BeOS.  However, this static desktop is in charge - it creates the
 * FrameBufferBeOS, which in turn creates the BDirectWindowReader.
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
 * $Log: SDesktopBeOS.h,v $
 * Revision 1.15  2005/02/14 02:31:56  agmsmith
 * Fake cursor drawing code added.
 *
 * Revision 1.14  2005/02/13 01:42:05  agmsmith
 * Can now receive clipboard text from the remote clients and
 * put it on the BeOS clipboard.
 *
 * Revision 1.13  2005/02/06 22:03:10  agmsmith
 * Changed to use the new BScreen reading method if the
 * BDirectWindow one doesn't work.  Also removed the screen
 * mode slow change with the yellow bar fake screen.
 *
 * Revision 1.12  2005/01/02 21:57:29  agmsmith
 * Made the event injector simpler - only need one device, not
 * separate ones for keyboard and mouse.  Also renamed it to
 * InputEventInjector to be in line with it's more general use.
 *
 * Revision 1.11  2005/01/02 21:05:33  agmsmith
 * Found screen resolution bug - wasn't testing the screen width
 * or height to detect a change, just depth.  Along the way added
 * some cool colour shifting animations on a fake screen.
 *
 * Revision 1.10  2005/01/01 21:31:02  agmsmith
 * Added double click timing detection, so that you can now double
 * click on a window title to minimize it.  Was missing the "clicks"
 * field in mouse down BMessages.
 *
 * Revision 1.9  2004/12/13 03:57:37  agmsmith
 * Combined functions for doing background update with grabbing the
 * screen memory.  Also limit update size to at least 4 scan lines.
 *
 * Revision 1.8  2004/11/27 22:53:59  agmsmith
 * Changed update technique to scan a small part of the screen each time
 * so that big updates don't slow down the interactivity by being big.
 * There is also an adaptive algorithm that makes the updates small
 * enough to be quick on the average.
 *
 * Revision 1.7  2004/09/13 00:18:27  agmsmith
 * Do updates separately, only based on the timer running out,
 * so that other events all get processed first before the slow
 * screen update starts.
 *
 * Revision 1.6  2004/08/23 00:24:17  agmsmith
 * Added a search for plain keyboard keys, so now you can type text
 * over VNC!  But funny key combinations likely won't work.
 *
 * Revision 1.5  2004/08/02 15:56:46  agmsmith
 * Alphabetically ordered.
 *
 * Revision 1.4  2004/07/25 21:02:48  agmsmith
 * Under construction - adding keycode simulation.
 *
 * Revision 1.3  2004/07/19 22:30:19  agmsmith
 * Updated to work with VNC 4.0 source code (was 4.0 beta 4).
 *
 * Revision 1.2  2004/07/05 00:53:32  agmsmith
 * Added mouse event handling - break down the network mouse event into
 * individual BMessages for the different mouse things, including the
 * mouse wheel.  Also add a forced refresh once in a while.
 *
 * Revision 1.1  2004/06/07 01:07:28  agmsmith
 * Initial revision
 */


/******************************************************************************
 * This is our main glue class for interfacing between VNC and BeOS.
 */

class SDesktopBeOS : public rfb::SDesktop
{
public:
  SDesktopBeOS ();
  virtual ~SDesktopBeOS ();

  void BackgroundScreenUpdateCheck ();
    // Checks for changes in a portion of the screen.  This gets called
    // periodically by the server, around 100 times per second.  It sends the
    // data for the changed part of the screen and also checks for a resolution
    // change.  It has a dynamic algorithm which tries to make the updates
    // small enough so that around 25 updates get done per second, including
    // network transmission time.

  virtual void clientCutText (const char* str, int len);
    // The client has placed some new text on the clipboard.  Update the local
    // clipboard to match it.

  uint8 FindKeyCodeFromMap (int32 *MapOffsetArray, char *KeyAsString);
    // Check all the keys in the given array of strings for each keycode to
    // see if any contain the given UTF-8 string.  Returns zero if it can't
    // find it.

  virtual rfb::Point getFbSize ();
    // getFbSize() returns the current dimensions of the framebuffer.
    // This can be called even while the SDesktop is not start()ed.

  virtual void keyEvent (rdr::U32 key, bool down);
    // The remote user has pressed a key.

  void MakeCheapCursor ();
    // Recreates the cheap cursor image in the current pixel format
    // then tells the server to use it.  Call after doing setPixelBuffer
    // since that's when the server resets its internal cursor image to
    // match the new screen depth.

  virtual void pointerEvent (const rfb::Point& pos, rdr::U8 buttonmask);
    // The remote user has moved the mouse or clicked a button.

  void setServer (rfb::VNCServer *ServerPntr);
    // Specifies the VNC server to use.  This is the thing which will parse VNC
    // messages, handle network connections etc.

  virtual void start (rfb::VNCServer* vs);
    // start() is called by the server when the first client authenticates
    // successfully, and can be used to begin any expensive tasks which are not
    // needed when there are no clients.  A valid PixelBuffer must have been
    // set via the VNCServer's setPixelBuffer() method by the time this call
    // returns.

  virtual void stop ();
    // stop() is called by the server when there are no longer any
    // authenticated clients, and therefore the desktop can cease any expensive
    // tasks.

  void SendUnmappedKeys (key_info &OldKeyState, key_info &NewKeyState);
    // Sends B_UNMAPPED_KEY_UP or B_UNMAPPED_KEY_DOWN messages for all keys
    // that have changed between the old and new states.

  void UpdateDerivedModifiersAndPressedModifierKeys (key_info &KeyState);
    // Looks at the modifier flags for individual modifier keys (left and right
    // control, L&R shift, etc) and sets the derived modifier flags (plain
    // control, plain shift, etc) to match.  Then it updates the keyboard bits
    // to show the corresponding buttons being pressed down or up (using the
    // previously obtained keymap).

protected:
  int m_BackgroundNextScanLineY;
    // When doing incremental updates of the screen, m_BackgroundNextScanLineY
    // identifies the next scan line to start checking for changes to the
    // screen.  If its out of range then a new full screen update is started.

  int m_BackgroundNumberOfScanLinesPerUpdate;
    // This many scan lines are read from the screen to see if they have
    // changed.  The number varies depending on the current perfomance,
    // adjusted at the end of every full screen scan to make the typical update
    // take only 1/100 of a second.  Minimum value 1, maximum is the height of
    // the screen.

  bigtime_t m_BackgroundUpdateStartTime;
    // The system clock at the moment the next full screen scan is started.
    // Used at the end of the full screen to evaluate performance and help
    // adjust m_BackgroundNumberOfScanLinesPerUpdate.

  bigtime_t m_DoubleClickTimeLimit;
    // The time in microseconds when a second mouse click counts as a double
    // click rather than a single click.  Grabbed from the OS preferences when
    // the desktop starts up.

  BInputDevice *m_EventInjectorPntr;
    // Gives access to our Input Server add-on which lets us inject mouse and
    // keyboard event messages.  NULL if the connection isn't open or isn't
    // available.  Connected when the desktop starts, disconnected when it
    // stops.

  class FrameBufferBeOS *m_FrameBufferBeOSPntr;
    // Our FrameBufferBeOS instance and the associated BDirectWindowReader
    // (which may or may not exist) which is used for accessing the frame
    // buffer.  NULL if it hasn't been created.

  char    *m_KeyCharStrings;
  key_map *m_KeyMapPntr;
    // NULL if not in use, otherwise they point to our copy (call free() when
    // done) of the keyboard mapping strings and tables that convert keyboard
    // scan codes into UTF-8 strings and various other keyboard mode
    // operations.  We actually use the tables in reverse to figure out which
    // buttons to press.  The keymap is copied from the current active one when
    // the desktop starts, so it doesn't reflect changes to the keymap while it
    // is running.

  key_info m_LastKeyState;
    // Identifies which of the 127 keys are currently being held down on the
    // imaginary ghost of the user's keyboard (using the current keymap to
    // figure out which keys do what).  Also remembers the modifier modes (caps
    // lock etc) last in use.

  rdr::U8 m_LastMouseButtonState;
    // The mouse buttons from the last remote mouse update.  Needed since
    // we have to convert the mouse events into up, down and moved events.

  unsigned int m_LastMouseDownCount;
  bigtime_t m_LastMouseDownTime;
  	// These two member variables help detect double clicks.  The time stamp is
  	// the time when the mouse was previously clicked down.  If the next click
  	// comes within the user's prefered mouse double click time then we count
  	// it as a double click, and increment m_LastMouseDownCount, which gets
  	// included in the mouse down message (but not the up one).  Otherwise it
  	// is a single click and m_LastMouseDownCount gets reset to 1.

  float m_LastMouseX;
  float m_LastMouseY;
    // Last absolute (0.0 to 1.0) mouse position reported to BeOS.  Needed so
    // that we can avoid sending redundant mouse moved messages, particularly
    // if the user is moving the mouse wheel or just pressing buttons.

  rfb::VNCServer *m_ServerPntr;
    // Identifies our server, which we can tell about our frame buffer and
    // other changes.  NULL if it hasn't been set yet.
};
