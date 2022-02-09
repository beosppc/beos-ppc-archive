/******************************************************************************
 * $Header: /CommonPPC/agmsmith/VNCServer-4.0-BeOS-AGMS-1.18/Source\040Code/beosserver/RCS/InputEventInjector.cpp,v 1.3 2005/01/02 21:57:29 agmsmith Exp $
 *
 * This is the add-in (shared .so library for BeOS) which injects keyboard and
 * mouse events into the BeOS InputServer, letting the remote system move the
 * mouse and simulate keyboard button presses.
 *
 * Put the shared library file created by this project (its default name is
 * "InputEventInjector") into /boot/home/config/add-ons/input_server/devices
 * to install it.
 *
 * It registers itself as a keyboard device with the InputServer, though it can
 * inject any kind of message, including mouse ones.  It also receives messages
 * from other programs using the BInputDevice Control system, and then copies
 * and forwards those messages to the InputServer.  So it could be used by
 * other programs than VNC, if desired.
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
 * $Log: InputEventInjector.cpp,v $
 * Revision 1.3  2005/01/02 21:57:29  agmsmith
 * Made the event injector simpler - only need one device, not
 * separate ones for keyboard and mouse.  Also renamed it to
 * InputEventInjector to be in line with it's more general use.
 *
 * Revision 1.2  2004/09/13 00:01:26  agmsmith
 * Added installation instructions and a version string.
 *
 * Revision 1.1  2004/07/04 20:25:28  agmsmith
 * Initial revision
 */

/* BeOS (Be Operating System) headers. */

#include <InputServerDevice.h>


/* POSIX headers. */

#include <stdio.h>


/******************************************************************************
 * Global variables, and not-so-variable things too.  Grouped by functionality.
 */

extern "C" _EXPORT BInputServerDevice* instantiate_input_device (void);

const char InputEventInjectorVersionString [] =
  "$Header: /CommonPPC/agmsmith/VNCServer-4.0-BeOS-AGMS-1.18/Source\040Code/beosserver/RCS/InputEventInjector.cpp,v 1.3 2005/01/02 21:57:29 agmsmith Exp $";

static struct input_device_ref FakeKeyboardLink =
{
  "InputEventInjector FakeKeyboard", // Max 31 characters to be safe.
  B_KEYBOARD_DEVICE,
  (void *) 76543210 /* cookie */
};

static struct input_device_ref *RegistrationRefList [2] =
{
  &FakeKeyboardLink,
  NULL
};



/******************************************************************************
 * The main class, does just about everything.
 */

class InputEventInjector : public BInputServerDevice
{
public:
  InputEventInjector ();
  virtual ~InputEventInjector ();
  virtual status_t InitCheck ();
  virtual status_t Start (const char *device, void *cookie);
  virtual status_t Stop (const char *device, void *cookie);
  virtual status_t Control (
    const char *device, void *cookie, uint32 code, BMessage *message);

protected:
  bool m_KeyboardEnabled;
};


InputEventInjector::InputEventInjector ()
: m_KeyboardEnabled (false)
{
}


InputEventInjector::~InputEventInjector ()
{
}


status_t InputEventInjector::InitCheck ()
{
  RegisterDevices (RegistrationRefList);

  return B_OK;
}


status_t InputEventInjector::Start (const char *device, void *cookie)
{
  if ((int) cookie == 76543210)
    m_KeyboardEnabled = true;
  else
    return B_ERROR;

  return B_OK;
}


status_t InputEventInjector::Stop (const char *device, void *cookie)
{
  if ((int) cookie == 76543210)
    m_KeyboardEnabled = false;
  else
    return B_ERROR;

  return B_OK;
}


status_t InputEventInjector::Control (
  const char *device,
  void *cookie,
  uint32 code,
  BMessage *message)
{
  BMessage *EventMsgPntr = NULL;

  if ((int) cookie == 76543210)
  {
    if (m_KeyboardEnabled && code == 'EInj' && message != NULL)
    {
      EventMsgPntr = new BMessage (*message);
      return EnqueueMessage (EventMsgPntr);
    }
  }
  else
    return B_ERROR;

  return B_OK;
}



BInputServerDevice* instantiate_input_device (void)
{
  return new InputEventInjector;
}
