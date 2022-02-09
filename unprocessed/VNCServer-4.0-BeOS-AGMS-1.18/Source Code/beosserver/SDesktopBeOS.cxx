/******************************************************************************
 * $Header: /CommonPPC/agmsmith/VNCServer-4.0-BeOS-AGMS-1.18/Source\040Code/beosserver/RCS/SDesktopBeOS.cxx,v 1.26 2005/02/27 17:16:12 agmsmith Exp $
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
 * $Log: SDesktopBeOS.cxx,v $
 * Revision 1.26  2005/02/27 17:16:12  agmsmith
 * Standard STD STL library hack glue.
 *
 * Revision 1.25  2005/02/14 02:31:01  agmsmith
 * Added an option to turn off various screen scanning methods,
 * and an option and code to draw a cursor in the outgoing data.
 *
 * Revision 1.24  2005/02/13 01:42:05  agmsmith
 * Can now receive clipboard text from the remote clients and
 * put it on the BeOS clipboard.
 *
 * Revision 1.23  2005/02/06 22:03:10  agmsmith
 * Changed to use the new BScreen reading method if the
 * BDirectWindow one doesn't work.  Also removed the screen
 * mode slow change with the yellow bar fake screen.
 *
 * Revision 1.22  2005/01/02 23:57:17  agmsmith
 * Fixed up control keys to avoid using the defective BeOS keyboard
 * mapping, which maps control-D to the End key and other similar
 * problems.
 *
 * Revision 1.21  2005/01/02 21:57:29  agmsmith
 * Made the event injector simpler - only need one device, not
 * separate ones for keyboard and mouse.  Also renamed it to
 * InputEventInjector to be in line with it's more general use.
 *
 * Revision 1.20  2005/01/02 21:05:33  agmsmith
 * Found screen resolution bug - wasn't testing the screen width
 * or height to detect a change, just depth.  Along the way added
 * some cool colour shifting animations on a fake screen.
 *
 * Revision 1.19  2005/01/01 21:31:02  agmsmith
 * Added double click timing detection, so that you can now double
 * click on a window title to minimize it.  Was missing the "clicks"
 * field in mouse down BMessages.
 *
 * Revision 1.18  2005/01/01 20:46:47  agmsmith
 * Make the default control/alt key swap detection be the alt is alt
 * and control is control setting, in case the keyboard isn't a
 * standard USA keyboard layout.
 *
 * Revision 1.17  2005/01/01 20:34:34  agmsmith
 * Swap control and alt keys if the user's keymap has them swapped.
 * Since it uses a physical key code (92) to detect the left control
 * key, it won't work on all keyboards, just US standard.
 *
 * Revision 1.16  2004/12/13 03:57:37  agmsmith
 * Combined functions for doing background update with grabbing the
 * screen memory.  Also limit update size to at least 4 scan lines.
 *
 * Revision 1.15  2004/12/02 02:24:28  agmsmith
 * Check for buggy operating systems which might allow the screen to
 * change memory addresses even while it is locked.
 *
 * Revision 1.14  2004/11/28 00:22:11  agmsmith
 * Also show frame rate.
 *
 * Revision 1.13  2004/11/27 22:53:59  agmsmith
 * Changed update technique to scan a small part of the screen each time
 * so that big updates don't slow down the interactivity by being big.
 * There is also an adaptive algorithm that makes the updates small
 * enough to be quick on the average.
 *
 * Revision 1.12  2004/09/13 01:41:24  agmsmith
 * Trying to get control keys working.
 *
 * Revision 1.11  2004/09/13 00:18:27  agmsmith
 * Do updates separately, only based on the timer running out,
 * so that other events all get processed first before the slow
 * screen update starts.
 *
 * Revision 1.10  2004/08/23 00:51:59  agmsmith
 * Force an update shortly after a key press.
 *
 * Revision 1.9  2004/08/23 00:24:17  agmsmith
 * Added a search for plain keyboard keys, so now you can type text
 * over VNC!  But funny key combinations likely won't work.
 *
 * Revision 1.8  2004/08/22 21:15:38  agmsmith
 * Keyboard work continues, adding the Latin-1 character set.
 *
 * Revision 1.7  2004/08/02 22:00:35  agmsmith
 * Got nonprinting keys working, next up is UTF-8 generating keys.
 *
 * Revision 1.6  2004/08/02 15:54:24  agmsmith
 * Rearranged methods to be in alphabetical order, still under construction.
 *
 * Revision 1.5  2004/07/25 21:03:27  agmsmith
 * Under construction, adding keymap and keycode simulation.
 *
 * Revision 1.4  2004/07/19 22:30:19  agmsmith
 * Updated to work with VNC 4.0 source code (was 4.0 beta 4).
 *
 * Revision 1.3  2004/07/05 00:53:32  agmsmith
 * Added mouse event handling - break down the network mouse event into
 * individual BMessages for the different mouse things, including the
 * mouse wheel.  Also add a forced refresh once in a while.
 *
 * Revision 1.2  2004/06/27 20:31:44  agmsmith
 * Got it working, so you can now see the desktop in different
 * video modes (except 8 bit).  Even lets you switch screens!
 *
 * Revision 1.1  2004/06/07 01:07:28  agmsmith
 * Initial revision
 */

#include <vector>
namespace std {
        int ___foobar___;
}
#define std

/* VNC library headers. */

#include <rfb/PixelBuffer.h>
#include <rfb/LogWriter.h>
#include <rfb/SDesktop.h>

#define XK_MISCELLANY 1
#define XK_LATIN1 1
#include <rfb/keysymdef.h>

/* BeOS (Be Operating System) headers. */

#include <Clipboard.h>
#include <DirectWindow.h>
#include <Input.h> // For BInputDevice.
#include <Locker.h>

/* Our source code */

#include "FrameBufferBeOS.h"
#include "SDesktopBeOS.h"



/******************************************************************************
 * Global variables, and not-so-variable things too.  Grouped by functionality.
 */

static rfb::LogWriter vlog("SDesktopBeOS");

static rfb::BoolParameter TryBDirectWindow ("ScreenReaderBDirect",
  "Set to TRUE if you want to include the BDirectWindow screen reader "
  "technique in the set of techniques the program attempts to use.  "
  "This one is the recommended one for faster responses since "
  "it maps the video memory into main memory, rather than requiring a "
  "copy operation to read it.  Thus waving the mouse pointer will be able "
  "to update the screen under the mouse with the contents at that instant.  "
  "However, only well supported video boards can use this and ones that "
  "aren't supported will just fail the startup test and not use this "
  "technique even if you ask for it.",
  1);

static rfb::BoolParameter TryBScreen ("ScreenReaderBScreen",
  "Set to TRUE if you want to include the BScreen based screen reader "
  "technique in the set of techniques the program attempts to use.  "
  "This one is compatible with all video boards, but much slower since "
  "it copies the whole screen to a bitmap before starting to work.  This "
  "also makes small updates useless - waving the mouse pointer won't "
  "reveal anything new until the next full screen update.  One advantage "
  "is that if you boot up without a good video driver, it does show the "
  "mouse cursor, since the cursor is actually drawn into the frame buffer "
  "by BeOS rather than using a hardware sprite.",
  1);

static rfb::BoolParameter ShowCheapCursor ("ShowCheapCursor",
  "If you want to see a marker on the screen where the mouse may be, turn on "
  "this parameter.  It will draw a cross shape into the data being sent out.  "
  "It's better if you instead use a client which draws the cursor.",
  0);


static char UTF8_Backspace [] = {B_BACKSPACE, 0};
static char UTF8_Return [] = {B_RETURN, 0};
static char UTF8_Tab [] = {B_TAB, 0};
static char UTF8_Escape [] = {B_ESCAPE, 0};
static char UTF8_LeftArrow [] = {B_LEFT_ARROW, 0};
static char UTF8_RightArrow [] = {B_RIGHT_ARROW, 0};
static char UTF8_UpArrow [] = {B_UP_ARROW, 0};
static char UTF8_DownArrow [] = {B_DOWN_ARROW, 0};
static char UTF8_Insert [] = {B_INSERT, 0};
static char UTF8_Delete [] = {B_DELETE, 0};
static char UTF8_Home [] = {B_HOME, 0};
static char UTF8_End [] = {B_END, 0};
static char UTF8_PageUp [] = {B_PAGE_UP, 0};
static char UTF8_PageDown [] = {B_PAGE_DOWN, 0};


// This table is used for converting VNC key codes into UTF-8 characters, but
// only for the normal printable characters.  Function keys and modifier keys
// (like shift) are handled separately.

typedef struct VNCKeyToUTF8Struct
{
  uint16 vncKeyCode;
  char  *utf8String;
} VNCKeyToUTF8Record, *VNCKeyToUTF8Pointer;

extern "C" int CompareVNCKeyRecords (const void *APntr, const void *BPntr)
{
  int Result;

  Result = ((VNCKeyToUTF8Pointer) APntr)->vncKeyCode;
  Result -= ((VNCKeyToUTF8Pointer) BPntr)->vncKeyCode;
  return Result;
}

static VNCKeyToUTF8Record VNCKeyToUTF8Array [] =
{ // Note that this table is in increasing order of VNC key code,
  // so that a binary search can be done.
  {/* 0x0020 */ XK_space, " "},
  {/* 0x0021 */ XK_exclam, "!"},
  {/* 0x0022 */ XK_quotedbl, "\""},
  {/* 0x0023 */ XK_numbersign, "#"},
  {/* 0x0024 */ XK_dollar, "$"},
  {/* 0x0025 */ XK_percent, "%"},
  {/* 0x0026 */ XK_ampersand, "&"},
  {/* 0x0027 */ XK_apostrophe, "'"},
  {/* 0x0028 */ XK_parenleft, "("},
  {/* 0x0029 */ XK_parenright, ")"},
  {/* 0x002a */ XK_asterisk, "*"},
  {/* 0x002b */ XK_plus, "+"},
  {/* 0x002c */ XK_comma, ","},
  {/* 0x002d */ XK_minus, "-"},
  {/* 0x002e */ XK_period, "."},
  {/* 0x002f */ XK_slash, "/"},
  {/* 0x0030 */ XK_0, "0"},
  {/* 0x0031 */ XK_1, "1"},
  {/* 0x0032 */ XK_2, "2"},
  {/* 0x0033 */ XK_3, "3"},
  {/* 0x0034 */ XK_4, "4"},
  {/* 0x0035 */ XK_5, "5"},
  {/* 0x0036 */ XK_6, "6"},
  {/* 0x0037 */ XK_7, "7"},
  {/* 0x0038 */ XK_8, "8"},
  {/* 0x0039 */ XK_9, "9"},
  {/* 0x003a */ XK_colon, ":"},
  {/* 0x003b */ XK_semicolon, ";"},
  {/* 0x003c */ XK_less, "<"},
  {/* 0x003d */ XK_equal, "="},
  {/* 0x003e */ XK_greater, ">"},
  {/* 0x003f */ XK_question, "?"},
  {/* 0x0040 */ XK_at, "@"},
  {/* 0x0041 */ XK_A, "A"},
  {/* 0x0042 */ XK_B, "B"},
  {/* 0x0043 */ XK_C, "C"},
  {/* 0x0044 */ XK_D, "D"},
  {/* 0x0045 */ XK_E, "E"},
  {/* 0x0046 */ XK_F, "F"},
  {/* 0x0047 */ XK_G, "G"},
  {/* 0x0048 */ XK_H, "H"},
  {/* 0x0049 */ XK_I, "I"},
  {/* 0x004a */ XK_J, "J"},
  {/* 0x004b */ XK_K, "K"},
  {/* 0x004c */ XK_L, "L"},
  {/* 0x004d */ XK_M, "M"},
  {/* 0x004e */ XK_N, "N"},
  {/* 0x004f */ XK_O, "O"},
  {/* 0x0050 */ XK_P, "P"},
  {/* 0x0051 */ XK_Q, "Q"},
  {/* 0x0052 */ XK_R, "R"},
  {/* 0x0053 */ XK_S, "S"},
  {/* 0x0054 */ XK_T, "T"},
  {/* 0x0055 */ XK_U, "U"},
  {/* 0x0056 */ XK_V, "V"},
  {/* 0x0057 */ XK_W, "W"},
  {/* 0x0058 */ XK_X, "X"},
  {/* 0x0059 */ XK_Y, "Y"},
  {/* 0x005a */ XK_Z, "Z"},
  {/* 0x005b */ XK_bracketleft, "["},
  {/* 0x005c */ XK_backslash, "\\"},
  {/* 0x005d */ XK_bracketright, "]"},
  {/* 0x005e */ XK_asciicircum, "^"},
  {/* 0x005f */ XK_underscore, "_"},
  {/* 0x0060 */ XK_grave, "`"},
  {/* 0x0061 */ XK_a, "a"},
  {/* 0x0062 */ XK_b, "b"},
  {/* 0x0063 */ XK_c, "c"},
  {/* 0x0064 */ XK_d, "d"},
  {/* 0x0065 */ XK_e, "e"},
  {/* 0x0066 */ XK_f, "f"},
  {/* 0x0067 */ XK_g, "g"},
  {/* 0x0068 */ XK_h, "h"},
  {/* 0x0069 */ XK_i, "i"},
  {/* 0x006a */ XK_j, "j"},
  {/* 0x006b */ XK_k, "k"},
  {/* 0x006c */ XK_l, "l"},
  {/* 0x006d */ XK_m, "m"},
  {/* 0x006e */ XK_n, "n"},
  {/* 0x006f */ XK_o, "o"},
  {/* 0x0070 */ XK_p, "p"},
  {/* 0x0071 */ XK_q, "q"},
  {/* 0x0072 */ XK_r, "r"},
  {/* 0x0073 */ XK_s, "s"},
  {/* 0x0074 */ XK_t, "t"},
  {/* 0x0075 */ XK_u, "u"},
  {/* 0x0076 */ XK_v, "v"},
  {/* 0x0077 */ XK_w, "w"},
  {/* 0x0078 */ XK_x, "x"},
  {/* 0x0079 */ XK_y, "y"},
  {/* 0x007a */ XK_z, "z"},
  {/* 0x007b */ XK_braceleft, "{"},
  {/* 0x007c */ XK_bar, "|"},
  {/* 0x007d */ XK_braceright, "}"},
  {/* 0x007e */ XK_asciitilde, "~"},
  {/* 0x00a0 */ XK_nobreakspace, " "}, // If compiler barfs use: "\0xc2\0xa0"
  {/* 0x00a1 */ XK_exclamdown, "¡"},
  {/* 0x00a2 */ XK_cent, "¢"},
  {/* 0x00a3 */ XK_sterling, "£"},
  {/* 0x00a4 */ XK_currency, "¤"},
  {/* 0x00a5 */ XK_yen, "¥"},
  {/* 0x00a6 */ XK_brokenbar, "¦"},
  {/* 0x00a7 */ XK_section, "§"},
  {/* 0x00a8 */ XK_diaeresis, "¨"},
  {/* 0x00a9 */ XK_copyright, "©"},
  {/* 0x00aa */ XK_ordfeminine, "ª"},
  {/* 0x00ab */ XK_guillemotleft, "«"},
  {/* 0x00ac */ XK_notsign, "¬"},
  {/* 0x00ad */ XK_hyphen, "­"},
  {/* 0x00ae */ XK_registered, "®"},
  {/* 0x00af */ XK_macron, "¯"},
  {/* 0x00b0 */ XK_degree, "°"},
  {/* 0x00b1 */ XK_plusminus, "±"},
  {/* 0x00b2 */ XK_twosuperior, "²"},
  {/* 0x00b3 */ XK_threesuperior, "³"},
  {/* 0x00b4 */ XK_acute, "´"},
  {/* 0x00b5 */ XK_mu, "µ"},
  {/* 0x00b6 */ XK_paragraph, "¶"},
  {/* 0x00b7 */ XK_periodcentered, "·"},
  {/* 0x00b8 */ XK_cedilla, "¸"},
  {/* 0x00b9 */ XK_onesuperior, "¹"},
  {/* 0x00ba */ XK_masculine, "º"},
  {/* 0x00bb */ XK_guillemotright, "»"},
  {/* 0x00bc */ XK_onequarter, "¼"},
  {/* 0x00bd */ XK_onehalf, "½"},
  {/* 0x00be */ XK_threequarters, "¾"},
  {/* 0x00bf */ XK_questiondown, "¿"},
  {/* 0x00c0 */ XK_Agrave, "À"},
  {/* 0x00c1 */ XK_Aacute, "Á"},
  {/* 0x00c2 */ XK_Acircumflex, "Â"},
  {/* 0x00c3 */ XK_Atilde, "Ã"},
  {/* 0x00c4 */ XK_Adiaeresis, "Ä"},
  {/* 0x00c5 */ XK_Aring, "Å"},
  {/* 0x00c6 */ XK_AE, "Æ"},
  {/* 0x00c7 */ XK_Ccedilla, "Ç"},
  {/* 0x00c8 */ XK_Egrave, "È"},
  {/* 0x00c9 */ XK_Eacute, "É"},
  {/* 0x00ca */ XK_Ecircumflex, "Ê"},
  {/* 0x00cb */ XK_Ediaeresis, "Ë"},
  {/* 0x00cc */ XK_Igrave, "Ì"},
  {/* 0x00cd */ XK_Iacute, "Í"},
  {/* 0x00ce */ XK_Icircumflex, "Î"},
  {/* 0x00cf */ XK_Idiaeresis, "Ï"},
  {/* 0x00d0 */ XK_ETH, "Ð"},
  {/* 0x00d1 */ XK_Ntilde, "Ñ"},
  {/* 0x00d2 */ XK_Ograve, "Ò"},
  {/* 0x00d3 */ XK_Oacute, "Ó"},
  {/* 0x00d4 */ XK_Ocircumflex, "Ô"},
  {/* 0x00d5 */ XK_Otilde, "Õ"},
  {/* 0x00d6 */ XK_Odiaeresis, "Ö"},
  {/* 0x00d7 */ XK_multiply, "×"},
  {/* 0x00d8 */ XK_Ooblique, "Ø"},
  {/* 0x00d9 */ XK_Ugrave, "Ù"},
  {/* 0x00da */ XK_Uacute, "Ú"},
  {/* 0x00db */ XK_Ucircumflex, "Û"},
  {/* 0x00dc */ XK_Udiaeresis, "Ü"},
  {/* 0x00dd */ XK_Yacute, "Ý"},
  {/* 0x00de */ XK_THORN, "Þ"},
  {/* 0x00df */ XK_ssharp, "ß"},
  {/* 0x00e0 */ XK_agrave, "à"},
  {/* 0x00e1 */ XK_aacute, "á"},
  {/* 0x00e2 */ XK_acircumflex, "â"},
  {/* 0x00e3 */ XK_atilde, "ã"},
  {/* 0x00e4 */ XK_adiaeresis, "ä"},
  {/* 0x00e5 */ XK_aring, "å"},
  {/* 0x00e6 */ XK_ae, "æ"},
  {/* 0x00e7 */ XK_ccedilla, "ç"},
  {/* 0x00e8 */ XK_egrave, "è"},
  {/* 0x00e9 */ XK_eacute, "é"},
  {/* 0x00ea */ XK_ecircumflex, "ê"},
  {/* 0x00eb */ XK_ediaeresis, "ë"},
  {/* 0x00ec */ XK_igrave, "ì"},
  {/* 0x00ed */ XK_iacute, "í"},
  {/* 0x00ee */ XK_icircumflex, "î"},
  {/* 0x00ef */ XK_idiaeresis, "ï"},
  {/* 0x00f0 */ XK_eth, "ð"},
  {/* 0x00f1 */ XK_ntilde, "ñ"},
  {/* 0x00f2 */ XK_ograve, "ò"},
  {/* 0x00f3 */ XK_oacute, "ó"},
  {/* 0x00f4 */ XK_ocircumflex, "ô"},
  {/* 0x00f5 */ XK_otilde, "õ"},
  {/* 0x00f6 */ XK_odiaeresis, "ö"},
  {/* 0x00f7 */ XK_division, "÷"},
  {/* 0x00f8 */ XK_oslash, "ø"},
  {/* 0x00f9 */ XK_ugrave, "ù"},
  {/* 0x00fa */ XK_uacute, "ú"},
  {/* 0x00fb */ XK_ucircumflex, "û"},
  {/* 0x00fc */ XK_udiaeresis, "ü"},
  {/* 0x00fd */ XK_yacute, "ý"},
  {/* 0x00fe */ XK_thorn, "þ"},
  {/* 0x00ff */ XK_ydiaeresis, "ÿ"},
  {/* 0xFF08 */ XK_BackSpace, UTF8_Backspace},
  {/* 0xFF09 */ XK_Tab, UTF8_Tab},
  {/* 0xFF0D */ XK_Return, UTF8_Return},
  {/* 0xFF1B */ XK_Escape, UTF8_Escape},
  {/* 0xFF50 */ XK_Home, UTF8_Home},
  {/* 0xFF51 */ XK_Left, UTF8_LeftArrow},
  {/* 0xFF52 */ XK_Up, UTF8_UpArrow},
  {/* 0xFF53 */ XK_Right, UTF8_RightArrow},
  {/* 0xFF54 */ XK_Down, UTF8_DownArrow},
  {/* 0xFF55 */ XK_Page_Up, UTF8_PageUp},
  {/* 0xFF56 */ XK_Page_Down, UTF8_PageDown},
  {/* 0xFF57 */ XK_End, UTF8_End},
  {/* 0xFF63 */ XK_Insert, UTF8_Insert},
  {/* 0xFFFF */ XK_Delete, UTF8_Delete}
};



/******************************************************************************
 * Utility functions.
 */

static inline void SetKeyState (
  key_info &KeyState,
  uint8 KeyCode,
  bool KeyIsDown)
{
  uint8 BitMask;
  uint8 Index;


  if (KeyCode <= 0 || KeyCode >= 128)
    return; // Keycodes are from 1 to 127, zero means no key defined.

  Index = KeyCode / 8;
  BitMask = (1 << (7 - (KeyCode & 7)));
  if (KeyIsDown)
    KeyState.key_states[Index] |= BitMask;
  else
    KeyState.key_states[Index] &= ~BitMask;
}



/******************************************************************************
 * The SDesktopBeOS class, methods follow in mostly alphabetical order.
 */

SDesktopBeOS::SDesktopBeOS () :
  m_BackgroundNextScanLineY (-1),
  m_BackgroundNumberOfScanLinesPerUpdate (32),
  m_BackgroundUpdateStartTime (0),
  m_DoubleClickTimeLimit (500000),
  m_EventInjectorPntr (NULL),
  m_FrameBufferBeOSPntr (NULL),
  m_KeyCharStrings (NULL),
  m_KeyMapPntr (NULL),
  m_LastMouseButtonState (0),
  m_LastMouseDownCount (1),
  m_LastMouseDownTime (0),
  m_LastMouseX (-1.0F),
  m_LastMouseY (-1.0F),
  m_ServerPntr (NULL)
{
  get_click_speed (&m_DoubleClickTimeLimit);
  memset (&m_LastKeyState, 0, sizeof (m_LastKeyState));
}


SDesktopBeOS::~SDesktopBeOS ()
{
  free (m_KeyCharStrings);
  m_KeyCharStrings = NULL;
  free (m_KeyMapPntr);
  m_KeyMapPntr = NULL;

  delete m_FrameBufferBeOSPntr;
  m_FrameBufferBeOSPntr = NULL;

  delete m_EventInjectorPntr;
  m_EventInjectorPntr = NULL;
}


void SDesktopBeOS::BackgroundScreenUpdateCheck ()
{
  bigtime_t        ElapsedTime = 0;
  int              Height;
  rfb::PixelFormat NewScreenFormat;
  int              NumberOfUpdates;
  rfb::PixelFormat OldScreenFormat;
  int              OldUpdateSize = 0;
  rfb::Rect        RectangleToUpdate;
  unsigned int     ScreenFormatSerialNumber;
  char             TempString [30];
  static int       UpdateCounter = 0;
  float            UpdatesPerSecond = 0;
  int              Width;

  if (m_FrameBufferBeOSPntr == NULL || m_ServerPntr == NULL ||
  (Width = m_FrameBufferBeOSPntr->width ()) <= 0 ||
  (Height = m_FrameBufferBeOSPntr->height ()) <= 0)
    return;

  ScreenFormatSerialNumber = m_FrameBufferBeOSPntr->LockFrameBuffer ();

  try
  {
    // Get the current screen size etc, if it has changed then inform the
    // server about the change.  Since the screen is locked, this also means
    // that VNC will be reading from a screen buffer that's the size it thinks
    // it is (no going past the end of video memory and causing a crash).

    OldScreenFormat = m_FrameBufferBeOSPntr->getPF ();
    m_FrameBufferBeOSPntr->UpdatePixelFormatEtc ();
    NewScreenFormat = m_FrameBufferBeOSPntr->getPF ();

    if (!NewScreenFormat.equal(OldScreenFormat) ||
    Width != m_FrameBufferBeOSPntr->width () ||
    Height != m_FrameBufferBeOSPntr->height ())
    {
      // This will trigger a full screen update too, which takes a while.
      m_ServerPntr->setPixelBuffer (m_FrameBufferBeOSPntr);
      if (ShowCheapCursor)
        MakeCheapCursor ();
    }
    else // No screen change, try an update.
    {
      if (m_BackgroundNextScanLineY < 0 || m_BackgroundNextScanLineY >= Height)
      {
        // Time to start a new frame.  Update the number of scan lines to
        // process based on the performance in the previous frame.  Less scan
        // lines if the number of updates per second is too small, larger
        // slower updates if they are too fast.

        NumberOfUpdates = (Height + m_BackgroundNumberOfScanLinesPerUpdate - 1)
          / m_BackgroundNumberOfScanLinesPerUpdate; // Will be at least 1.
        ElapsedTime = system_time () - m_BackgroundUpdateStartTime;
        if (ElapsedTime <= 0)
          ElapsedTime = 1;
        UpdatesPerSecond = NumberOfUpdates / (ElapsedTime / 1000000.0F);
        OldUpdateSize = m_BackgroundNumberOfScanLinesPerUpdate;
        if (UpdatesPerSecond < 20)
          m_BackgroundNumberOfScanLinesPerUpdate--;
        else if (UpdatesPerSecond > 25)
          m_BackgroundNumberOfScanLinesPerUpdate++;

        // Only go as small as 4 for performance reasons.
        if (m_BackgroundNumberOfScanLinesPerUpdate <= 4)
          m_BackgroundNumberOfScanLinesPerUpdate = 4;
        else if (m_BackgroundNumberOfScanLinesPerUpdate > Height)
          m_BackgroundNumberOfScanLinesPerUpdate = Height;

        m_BackgroundNextScanLineY = 0;
        m_BackgroundUpdateStartTime = system_time ();
        m_FrameBufferBeOSPntr->GrabScreen ();
      }

      // Mark the current work unit of scan lines as needing an update.

      RectangleToUpdate.setXYWH (0, m_BackgroundNextScanLineY,
        Width, m_BackgroundNumberOfScanLinesPerUpdate);
      if (RectangleToUpdate.br.y > Height)
        RectangleToUpdate.br.y = Height;
      rfb::Region RegionChanged (RectangleToUpdate);
      m_ServerPntr->add_changed (RegionChanged);

      // Tell the server to resend the changed areas, causing it to read the
      // screen memory in the areas marked as changed, compare it with the
      // previous version, and send out any changes.

      m_ServerPntr->tryUpdate ();

      m_BackgroundNextScanLineY += m_BackgroundNumberOfScanLinesPerUpdate;
    }
  }
  catch (...)
  {
    m_FrameBufferBeOSPntr->UnlockFrameBuffer ();
    throw;
  }

  m_FrameBufferBeOSPntr->UnlockFrameBuffer ();

  // Do the debug printing outside the lock, since printing goes through the
  // windowing system, which needs access to the screen.

  if (OldUpdateSize != 0) // If a full screen update has been finished.
  {
    if (OldUpdateSize != m_BackgroundNumberOfScanLinesPerUpdate)
      vlog.debug ("Background update size changed from %d to %d scan lines "
      "due to performance of %.4f updates per second in the previous full "
      "screen frame.  Last frame achieved %.3f frames per second.",
      OldUpdateSize, m_BackgroundNumberOfScanLinesPerUpdate,
      UpdatesPerSecond, 1000000.0F / ElapsedTime);

    UpdateCounter++;
    sprintf (TempString, "Update #%d", UpdateCounter);
    m_FrameBufferBeOSPntr->SetDisplayMessage (TempString);
  }
}

void SDesktopBeOS::clientCutText (const char* str, int len)
{
  BMessage *ClipMsgPntr;
  if (be_clipboard->Lock())
  {
    be_clipboard->Clear ();
    if ((ClipMsgPntr = be_clipboard->Data ()) != NULL)
    {
      ClipMsgPntr->AddData ("text/plain", B_MIME_TYPE,
        str, len);
      be_clipboard->Commit ();
    }
    be_clipboard->Unlock ();
  }
}


uint8 SDesktopBeOS::FindKeyCodeFromMap (
  int32 *MapOffsetArray,
  char *KeyAsString)
{
  unsigned int KeyCode;
  int32       *OffsetPntr;
  char        *StringPntr;

  OffsetPntr = MapOffsetArray + 1 /* Skip over [0]. */;
  for (KeyCode = 1 /* zero not valid */; KeyCode <= 127;
  KeyCode++, OffsetPntr++)
  {
    StringPntr = m_KeyCharStrings + *OffsetPntr;
    if (*StringPntr == 0)
      continue; // Length of string (pascal style with length byte) is zero.
    if (memcmp (StringPntr + 1, KeyAsString, *StringPntr) == 0 &&
    KeyAsString [*StringPntr] == 0 /* look for NUL at end of search string */)
      return KeyCode;
  }
  return 0;
}


rfb::Point SDesktopBeOS::getFbSize ()
{
  vlog.debug ("getFbSize called.");

  if (m_FrameBufferBeOSPntr != NULL)
  {
    m_FrameBufferBeOSPntr->LockFrameBuffer ();
    m_FrameBufferBeOSPntr->UpdatePixelFormatEtc ();
    m_FrameBufferBeOSPntr->UnlockFrameBuffer ();

    return rfb::Point (
      m_FrameBufferBeOSPntr->width (), m_FrameBufferBeOSPntr->height ());
  }
  return rfb::Point (640, 480);
}


void SDesktopBeOS::keyEvent (rdr::U32 key, bool down)
{
  uint32              ChangedModifiers;
  char                ControlCharacterAsUTF8 [2];
  BMessage            EventMessage;
  uint8               KeyCode;
  char                KeyAsString [16];
  VNCKeyToUTF8Record  KeyToUTF8SearchData;
  VNCKeyToUTF8Pointer KeyToUTF8Pntr;
  key_info            NewKeyState;

  if (m_EventInjectorPntr == NULL || m_FrameBufferBeOSPntr == NULL ||
  m_FrameBufferBeOSPntr->width () <= 0 || m_KeyMapPntr == NULL)
    return;

  vlog.debug ("VNC keycode $%04X received, key is %s.",
    key, down ? "down" : "up");
  NewKeyState = m_LastKeyState;

  // If it's a shift or other modifier key, update our internal modifiers
  // state.

  switch (key)
  {
    case XK_Caps_Lock: ChangedModifiers = B_CAPS_LOCK; break;
    case XK_Scroll_Lock: ChangedModifiers = B_SCROLL_LOCK; break;
    case XK_Num_Lock: ChangedModifiers = B_NUM_LOCK; break;
    case XK_Shift_L: ChangedModifiers = B_LEFT_SHIFT_KEY; break;
    case XK_Shift_R: ChangedModifiers = B_RIGHT_SHIFT_KEY; break;

     // Are alt/control swapped?  The menu preference does it by swapping the
     // keycodes in the keymap (couldn't find any other way of detecting it).
     // The usual left control key is 92, and left alt is 93.  Usual means on
     // the standard US keyboard layout.  Other keyboards may have other
     // physical key codes for control and alt, so the default is to have alt
     // be the command key and control be the control key.
     case XK_Control_L: ChangedModifiers =
      ((m_KeyMapPntr->left_control_key != 93) ?
      B_LEFT_CONTROL_KEY : B_LEFT_COMMAND_KEY); break;
    case XK_Control_R: ChangedModifiers =
      ((m_KeyMapPntr->left_control_key != 93) ?
      B_RIGHT_CONTROL_KEY : B_RIGHT_COMMAND_KEY); break;
    case XK_Alt_L: ChangedModifiers =
      ((m_KeyMapPntr->left_control_key != 93) ?
      B_LEFT_COMMAND_KEY : B_LEFT_CONTROL_KEY); break;
    case XK_Alt_R: ChangedModifiers =
      ((m_KeyMapPntr->left_control_key != 93) ?
      B_RIGHT_COMMAND_KEY : B_RIGHT_CONTROL_KEY); break;

    case XK_Meta_L: ChangedModifiers = B_LEFT_OPTION_KEY; break;
    case XK_Meta_R: ChangedModifiers = B_RIGHT_OPTION_KEY; break;
    default: ChangedModifiers = 0;
  }

  // Update the modifiers for the particular modifier key if one was pressed.

  if (ChangedModifiers != 0)
  {
    if (down)
      NewKeyState.modifiers |= ChangedModifiers;
    else
      NewKeyState.modifiers &= ~ChangedModifiers;

    UpdateDerivedModifiersAndPressedModifierKeys (NewKeyState);

    if (NewKeyState.modifiers != m_LastKeyState.modifiers)
    {
      // Send a B_MODIFIERS_CHANGED message to update the system with the new
      // modifier key settings.  Note that this gets sent before the unmapped
      // key message since that's what BeOS does.

      EventMessage.what = B_MODIFIERS_CHANGED;
      EventMessage.AddInt64 ("when", system_time ());
      EventMessage.AddInt32 ("be:old_modifiers", m_LastKeyState.modifiers);
      EventMessage.AddInt32 ("modifiers", NewKeyState.modifiers);
      EventMessage.AddData ("states",
        B_UINT8_TYPE, &NewKeyState.key_states, 16);
      m_EventInjectorPntr->Control ('EInj', &EventMessage);
      EventMessage.MakeEmpty ();
    }

    SendUnmappedKeys (m_LastKeyState, NewKeyState);
    m_LastKeyState = NewKeyState;

    if (ChangedModifiers != B_SCROLL_LOCK)
      return; // No actual typeable key was pressed, nothing further to do.
  }

  // Look for keys which don't have UTF-8 codes, like the function keys.  They
  // also are a direct press; they don't try to fiddle with the shift keys.

  KeyCode = 0;
  if (key >= XK_F1 && key <= XK_F12)
    KeyCode = B_F1_KEY + key - XK_F1;
  else if (key == XK_Scroll_Lock)
    KeyCode = m_KeyMapPntr->scroll_key; // Usually equals B_SCROLL_KEY.
  else if (key == XK_Print)
    KeyCode = B_PRINT_KEY;
  else if (key == XK_Pause)
    KeyCode = B_PAUSE_KEY;

  if (KeyCode != 0)
  {
    SetKeyState (m_LastKeyState, KeyCode, down);

    EventMessage.what = down ? B_KEY_DOWN : B_KEY_UP;
    EventMessage.AddInt64 ("when", system_time ());
    EventMessage.AddInt32 ("key", KeyCode);
    EventMessage.AddInt32 ("modifiers", m_LastKeyState.modifiers);
    KeyAsString [0] = B_FUNCTION_KEY;
    KeyAsString [1] = 0;
    EventMessage.AddInt8 ("byte", KeyAsString [0]);
    EventMessage.AddString ("bytes", KeyAsString);
    EventMessage.AddData ("states", B_UINT8_TYPE,
      m_LastKeyState.key_states, 16);
    EventMessage.AddInt32 ("raw_char", KeyAsString [0]);
    m_EventInjectorPntr->Control ('EInj', &EventMessage);
    EventMessage.MakeEmpty ();
    return;
  }

  // Special case for control keys.  If control is down (but Command isn't,
  // since if Command is down then control is ignored by BeOS) then convert the
  // VNC keycodes into control characters.  That's because VNC sends control-A
  // as the same code for the letter A.

  KeyToUTF8Pntr = NULL;
  if ((m_LastKeyState.modifiers & B_COMMAND_KEY) == 0 &&
  (m_LastKeyState.modifiers & B_CONTROL_KEY) != 0)
  {
    key = (key & 31); // Force it to a control character.
    KeyToUTF8SearchData.vncKeyCode = key; // Not really searching.
    ControlCharacterAsUTF8 [0] = key;
    ControlCharacterAsUTF8 [1] = 0;
    KeyToUTF8SearchData.utf8String = ControlCharacterAsUTF8;
    KeyToUTF8Pntr = &KeyToUTF8SearchData;
  }
  else
  {
    // The rest of the keys have an equivalent UTF-8 character.  Convert the
    // key code into a UTF-8 character, which will later be used to determine
    // which keys to press.

    KeyToUTF8SearchData.vncKeyCode = key;
    KeyToUTF8Pntr = (VNCKeyToUTF8Pointer) bsearch (
      &KeyToUTF8SearchData,
      VNCKeyToUTF8Array,
      sizeof (VNCKeyToUTF8Array) / sizeof (VNCKeyToUTF8Record),
      sizeof (VNCKeyToUTF8Record),
      CompareVNCKeyRecords);
  }
  if (KeyToUTF8Pntr == NULL)
  {
    vlog.info ("VNC keycode $%04X (%s) isn't recognised, ignoring it.",
      key, down ? "down" : "up");
    return; // Key not handled, ignore it.
  }

  // Look for the UTF-8 string in the keymap tables which are currently active
  // (reflecting the current modifier keys) to see which key code it is.

  strcpy (KeyAsString, KeyToUTF8Pntr->utf8String);
  KeyCode = 0;
  if (KeyCode == 0 && (m_LastKeyState.modifiers & B_CONTROL_KEY) != 0 &&
  /* Can't type control characters while the command key is down */
  (m_LastKeyState.modifiers & B_COMMAND_KEY) == 0)
  {
    /* This keymap doesn't work - converts control-D to the END key etc.
    KeyCode = FindKeyCodeFromMap (
      m_KeyMapPntr->control_map, KeyAsString);
    */
    if (key <= 0) // Control-space on the keyboard for NUL byte.
      KeyCode = FindKeyCodeFromMap (m_KeyMapPntr->normal_map, " ");
    else if (key <= 26) // Control A to Z use the letter keys.
    {
      ControlCharacterAsUTF8 [0] = key + 0x60; // Lower case letter a to z.
      ControlCharacterAsUTF8 [1] = 0;
      KeyCode = FindKeyCodeFromMap (
        m_KeyMapPntr->normal_map, ControlCharacterAsUTF8);
    }
    else if (key == 27)
      KeyCode = FindKeyCodeFromMap (m_KeyMapPntr->normal_map, "[");
    // Can't type code 28 in BeOS, no normal key for it.  Somebody goofed.
    else if (key == 29)
      KeyCode = FindKeyCodeFromMap (m_KeyMapPntr->normal_map, "]");
    else if (key == 30)
      KeyCode = FindKeyCodeFromMap (m_KeyMapPntr->shift_map, "^");
    else if (key == 31)
      KeyCode = FindKeyCodeFromMap (m_KeyMapPntr->shift_map, "_");
    if (KeyCode == 0)
      KeyCode = 1; // The rest get what's usually the escape key.
  }
  if (KeyCode == 0 && (m_LastKeyState.modifiers & B_OPTION_KEY) &&
  (m_LastKeyState.modifiers & B_CAPS_LOCK) &&
  (m_LastKeyState.modifiers & B_SHIFT_KEY))
    KeyCode = FindKeyCodeFromMap (
      m_KeyMapPntr->option_caps_shift_map, KeyAsString);
  if (KeyCode == 0 && (m_LastKeyState.modifiers & B_OPTION_KEY) &&
  (m_LastKeyState.modifiers & B_CAPS_LOCK))
    KeyCode = FindKeyCodeFromMap (
      m_KeyMapPntr->option_caps_map, KeyAsString);
  if (KeyCode == 0 && (m_LastKeyState.modifiers & B_OPTION_KEY) &&
  (m_LastKeyState.modifiers & B_SHIFT_KEY))
    KeyCode = FindKeyCodeFromMap (
      m_KeyMapPntr->option_shift_map, KeyAsString);
  if (KeyCode == 0 && (m_LastKeyState.modifiers & B_OPTION_KEY))
    KeyCode = FindKeyCodeFromMap (
      m_KeyMapPntr->option_map, KeyAsString);
  if (KeyCode == 0 && (m_LastKeyState.modifiers & B_CAPS_LOCK) &&
  (m_LastKeyState.modifiers & B_SHIFT_KEY))
    KeyCode = FindKeyCodeFromMap (
      m_KeyMapPntr->caps_shift_map, KeyAsString);
  if (KeyCode == 0 && (m_LastKeyState.modifiers & B_CAPS_LOCK))
    KeyCode = FindKeyCodeFromMap (
      m_KeyMapPntr->caps_map, KeyAsString);
  if (KeyCode == 0 && (m_LastKeyState.modifiers & B_SHIFT_KEY))
    KeyCode = FindKeyCodeFromMap (
      m_KeyMapPntr->shift_map, KeyAsString);
  if (KeyCode == 0)
    KeyCode = FindKeyCodeFromMap (
      m_KeyMapPntr->normal_map, KeyAsString);

  if (KeyCode != 0)
  {
    // Got a key that works with the current modifier settings.
    // Just simulate pressing it.

    SetKeyState (m_LastKeyState, KeyCode, down);

    EventMessage.what = down ? B_KEY_DOWN : B_KEY_UP;
    EventMessage.AddInt64 ("when", system_time ());
    EventMessage.AddInt32 ("key", KeyCode);
    EventMessage.AddInt32 ("modifiers", m_LastKeyState.modifiers);
    EventMessage.AddInt8 ("byte", KeyAsString [0]);
    EventMessage.AddString ("bytes", KeyAsString);
    EventMessage.AddData ("states", B_UINT8_TYPE,
      m_LastKeyState.key_states, 16);
    EventMessage.AddInt32 ("raw_char", key & 0xFF); // Could be wrong.
    m_EventInjectorPntr->Control ('EInj', &EventMessage);
    EventMessage.MakeEmpty ();
    return;
  }

  // The key isn't an obvious one.  Check all the keymap tables and simulate
  // pressing shift keys etc. to get that code, then send the message for that
  // key pressed, then release the temporary modifier key changes.  Well,
  // that's a lot of work so it will be done later if needed.  I won't even
  // write much about dead keys.

  vlog.debug ("VNC keycode $%04X (%s) maps to \"%s\", but it isn't obvious "
    "which BeOS keys need to be \"pressed\" to achieve that.  Ignoring it.",
    key, down ? "down" : "up", KeyAsString);
}


void SDesktopBeOS::MakeCheapCursor (void)
{
  if (m_FrameBufferBeOSPntr == NULL || m_ServerPntr == NULL)
    return;

  static unsigned char CrossMask [7 /* Our cursor is 7 rows tall */] =
  { // A one bit per pixel transparency mask, high bit first in each byte.
    0x10, // 00010000
    0x10, // 00010000
    0x10, // 00010000
    0xEE, // 11101110
    0x10, // 00010000
    0x10, // 00010000
    0x10, // 00010000
  };

  rfb::Pixel         Black;
  rfb::Pixel         White;
  rfb::Rect          Rectangle;
  rfb::ManagedPixelBuffer CursorBitmap (m_FrameBufferBeOSPntr->getPF (), 7, 7);

  Black = m_FrameBufferBeOSPntr->getPF().pixelFromRGB (
    (rdr::U16) 0, (rdr::U16) 0, (rdr::U16) 0,
    m_FrameBufferBeOSPntr->getColourMap());
  White = m_FrameBufferBeOSPntr->getPF().pixelFromRGB (
    (rdr::U16) 0xFFFF, (rdr::U16) 0xFFFF, (rdr::U16) 0xFFFF,
    m_FrameBufferBeOSPntr->getColourMap());

  // Draw a cross.
  Rectangle.setXYWH (0, 0, 7, 7);
  CursorBitmap.fillRect (Rectangle, White);
  Rectangle.setXYWH (3, 0, 1, 7);
  CursorBitmap.fillRect (Rectangle, Black);
  Rectangle.setXYWH (0, 3, 7, 1);
  CursorBitmap.fillRect (Rectangle, Black);

  // Tell the server to use it.
  m_ServerPntr->setCursor (/* x, y, hotx, hoty */ 7, 7, 3, 3,
    CursorBitmap.data, CrossMask);
}


void SDesktopBeOS::pointerEvent (const rfb::Point& pos, rdr::U8 buttonmask)
{
  if (m_EventInjectorPntr == NULL || m_FrameBufferBeOSPntr == NULL ||
  m_ServerPntr == NULL || m_FrameBufferBeOSPntr->width () <= 0)
    return;

  float     AbsoluteY;
  float     AbsoluteX;
  bigtime_t CurrentTime;
  bigtime_t ElapsedTime;
  BMessage  EventMessage;
  rdr::U8   NewMouseButtons;
  rdr::U8   OldMouseButtons;

  // Swap buttons 2 and 3, since BeOS uses left, middle, right and the rest of
  // the world has left, right, middle.  Or vice versa?

  NewMouseButtons = (buttonmask & 0xF9);
  if (buttonmask & 2)
    NewMouseButtons |= 4;
  if (buttonmask & 4)
    NewMouseButtons |= 2;
  buttonmask = NewMouseButtons;

  // Check for regular mouse button presses, don't include mouse wheel etc.

  NewMouseButtons = (buttonmask & 7);
  OldMouseButtons = (m_LastMouseButtonState & 7);

  if (NewMouseButtons == 0 && OldMouseButtons != 0)
    EventMessage.what = B_MOUSE_UP;
  else if (NewMouseButtons != 0 && OldMouseButtons == 0)
    EventMessage.what = B_MOUSE_DOWN;
  else
    EventMessage.what = B_MOUSE_MOVED;

  // Use absolute positions for the mouse, which just means writing a float
  // rather than an int for the X and Y values.  Scale it assuming the current
  // screen size.

  AbsoluteX = pos.x / (float) m_FrameBufferBeOSPntr->width ();
  if (AbsoluteX < 0.0F) AbsoluteX = 0.0F;
  if (AbsoluteX > 1.0F) AbsoluteX = 1.0F;

  AbsoluteY = pos.y / (float) m_FrameBufferBeOSPntr->height ();
  if (AbsoluteY < 0.0F) AbsoluteY = 0.0F;
  if (AbsoluteY > 1.0F) AbsoluteY = 1.0F;

  // Send the mouse movement or button press message to our Input Server
  // add-on, which will then forward it to the event queue input.  Avoid
  // sending messages which do nothing (can happen when the mouse wheel is
  // used).  We just need to provide absolute position "x" and "y", which the
  // system will convert to a "where" BPoint, "buttons" for the current button
  // state and the "when" time field.  We also have to fake the "clicks" field
  // with the current double click count, but only for mouse down messages.
  // The system will add "modifiers", "be:transit" and "be:view_where".

  if (EventMessage.what != B_MOUSE_MOVED ||
  m_LastMouseX != AbsoluteX || m_LastMouseY != AbsoluteY ||
  NewMouseButtons != OldMouseButtons)
  {
    CurrentTime = system_time ();
    EventMessage.AddFloat ("x", AbsoluteX);
    EventMessage.AddFloat ("y", AbsoluteY);
    EventMessage.AddInt64 ("when", CurrentTime);
    EventMessage.AddInt32 ("buttons", NewMouseButtons);
    if (EventMessage.what == B_MOUSE_DOWN)
    {
      ElapsedTime = CurrentTime - m_LastMouseDownTime;
      if (ElapsedTime <= m_DoubleClickTimeLimit)
        m_LastMouseDownCount++;
      else // Counts as a new single click.
        m_LastMouseDownCount = 1;
      EventMessage.AddInt32 ("clicks", m_LastMouseDownCount);
      m_LastMouseDownTime = CurrentTime;
    }

    m_EventInjectorPntr->Control ('EInj', &EventMessage);
    EventMessage.MakeEmpty ();

    // Also request a screen update later on for the area around the mouse
    // coordinates.  That way moving the mouse around will update the screen
    // under the mouse pointer.  Same for clicking, since that often brings up
    // menus which need to be made visible.

    if (ShowCheapCursor)
      m_ServerPntr->setCursorPos (pos.x, pos.y);

    rfb::Rect ScreenRect;
    ScreenRect.setXYWH (0, 0,
      m_FrameBufferBeOSPntr->width (), m_FrameBufferBeOSPntr->height ());
    rfb::Rect RectangleToUpdate;
    RectangleToUpdate.setXYWH (pos.x - 32, pos.y - 32, 64, 64);
    rfb::Region RegionChanged (RectangleToUpdate.intersect (ScreenRect));
    m_ServerPntr->add_changed (RegionChanged);
  }

  // Check for a mouse wheel change (button 4 press+release is wheel up one
  // notch, button 5 is wheel down).  These get sent as a separate
  // B_MOUSE_WHEEL_CHANGED message.  We do it on the release.

  NewMouseButtons = (buttonmask & 0x18); // Just examine mouse wheel "buttons".
  OldMouseButtons = (m_LastMouseButtonState & 0x18);
  EventMessage.what = 0; // Means no event decided on yet.

  if (NewMouseButtons != OldMouseButtons)
  {
    if ((NewMouseButtons & 0x8) == 0 && (OldMouseButtons & 0x8) != 0)
    {
      // Button 4 has been released.  Wheel up a notch.
      EventMessage.what = B_MOUSE_WHEEL_CHANGED;
      EventMessage.AddInt64 ("when", system_time ());
      EventMessage.AddFloat ("be:wheel_delta_x", 0.0F);
      EventMessage.AddFloat ("be:wheel_delta_y", -1.0F);
    }
    else if ((NewMouseButtons & 0x10) == 0 && (OldMouseButtons & 0x10) != 0)
    {
      // Button 5 has been released.  Wheel down a notch.
      EventMessage.what = B_MOUSE_WHEEL_CHANGED;
      EventMessage.AddInt64 ("when", system_time ());
      EventMessage.AddFloat ("be:wheel_delta_x", 0.0F);
      EventMessage.AddFloat ("be:wheel_delta_y", 1.0F);
    }
  }
  if (EventMessage.what != 0)
    m_EventInjectorPntr->Control ('EInj', &EventMessage);

  m_LastMouseButtonState = buttonmask;
  m_LastMouseX = AbsoluteX;
  m_LastMouseY = AbsoluteY;
}


void SDesktopBeOS::SendUnmappedKeys (
  key_info &OldKeyState,
  key_info &NewKeyState)
{
  int      BitIndex;
  int      ByteIndex;
  uint8    DeltaBit;
  BMessage EventMessage;
  int      KeyCode;
  uint8    Mask;
  uint8    NewBits;
  uint8    OldBits;

  KeyCode = 0;
  for (ByteIndex = 0; ByteIndex < 7; ByteIndex++)
  {
    OldBits = OldKeyState.key_states[ByteIndex];
    NewBits = NewKeyState.key_states[ByteIndex];
    for (BitIndex = 7; BitIndex >= 0; BitIndex--, KeyCode++)
    {
      Mask = 1 << BitIndex;
      DeltaBit = (OldBits ^ NewBits) & Mask;
      if (DeltaBit == 0)
        continue; // Key hasn't changed.
      EventMessage.what =
        (NewBits & Mask) ? B_UNMAPPED_KEY_DOWN : B_UNMAPPED_KEY_UP;
      EventMessage.AddInt64 ("when", system_time ());
      EventMessage.AddInt32 ("key", KeyCode);
      EventMessage.AddInt32 ("modifiers", NewKeyState.modifiers);
      EventMessage.AddData ("states",
        B_UINT8_TYPE, &NewKeyState.key_states, 16);
      m_EventInjectorPntr->Control ('EInj', &EventMessage);
      EventMessage.MakeEmpty ();
    }
  }
}



void SDesktopBeOS::setServer (rfb::VNCServer *ServerPntr)
{
  vlog.debug ("setServer called.");
  m_ServerPntr = ServerPntr;
}


void SDesktopBeOS::start (rfb::VNCServer* vs)
{
  vlog.debug ("start called.");

  // Try various different techniques for reading the screen, their
  // constructors will fail if they aren't supported.

  if (m_FrameBufferBeOSPntr == NULL && TryBDirectWindow)
  {
    try
    {
      m_FrameBufferBeOSPntr = new FrameBufferBDirect;
    }
    catch (rdr::Exception &e)
    {
      vlog.error(e.str());
      m_FrameBufferBeOSPntr = NULL;
    }
  }

  if (m_FrameBufferBeOSPntr == NULL && TryBScreen)
  {
    try
    {
      m_FrameBufferBeOSPntr = new FrameBufferBScreen;
    }
    catch (rdr::Exception &e)
    {
      vlog.error(e.str());
      m_FrameBufferBeOSPntr = NULL;
    }
  }

  if (m_FrameBufferBeOSPntr == NULL)
    throw rdr::Exception ("Unable to find any screen reading techniques "
    "which are compatible with your video card.  Only tried the ones not "
    "turned off on the command line.  Perhaps try a different video card?",
    "SDesktopBeOS::start");

  m_ServerPntr->setPixelBuffer (m_FrameBufferBeOSPntr);

  if (ShowCheapCursor)
    MakeCheapCursor ();

  if (m_EventInjectorPntr == NULL)
    m_EventInjectorPntr =
    find_input_device ("InputEventInjector FakeKeyboard");

  if (m_KeyMapPntr != NULL || m_KeyCharStrings != NULL)
    throw rfb::Exception ("SDesktopBeOS::start: key map pointers not "
    "NULL, bug!");
  get_key_map (&m_KeyMapPntr, &m_KeyCharStrings);
  if (m_KeyMapPntr == NULL || m_KeyCharStrings == NULL)
    throw rfb::Exception ("SDesktopBeOS::start: get_key_map has failed, "
    "so we can't simulate the keyboard buttons being pressed!");
}


void SDesktopBeOS::stop ()
{
  vlog.debug ("stop called.");

  free (m_KeyCharStrings);
  m_KeyCharStrings = NULL;
  free (m_KeyMapPntr);
  m_KeyMapPntr = NULL;

  delete m_FrameBufferBeOSPntr;
  m_FrameBufferBeOSPntr = NULL;
  m_ServerPntr->setPixelBuffer (NULL);

  delete m_EventInjectorPntr;
  m_EventInjectorPntr = NULL;
}


void SDesktopBeOS::UpdateDerivedModifiersAndPressedModifierKeys (
  key_info &KeyState)
{
  uint32 TempModifiers;

  // Update the virtual modifiers, ones which have a left and right key that do
  // the same thing to reflect the state of the real keys.

  TempModifiers = KeyState.modifiers;

  if (TempModifiers & (B_LEFT_SHIFT_KEY | B_RIGHT_SHIFT_KEY))
    TempModifiers |= B_SHIFT_KEY;
  else
    TempModifiers &= ~ (uint32) B_SHIFT_KEY;

  if (TempModifiers & (B_LEFT_COMMAND_KEY | B_RIGHT_COMMAND_KEY))
    TempModifiers |= B_COMMAND_KEY;
  else
    TempModifiers &= ~ (uint32) B_COMMAND_KEY;

  if (TempModifiers & (B_LEFT_CONTROL_KEY | B_RIGHT_CONTROL_KEY))
    TempModifiers |= B_CONTROL_KEY;
  else
    TempModifiers &= ~ (uint32) B_CONTROL_KEY;

  if (TempModifiers & (B_LEFT_OPTION_KEY | B_RIGHT_OPTION_KEY))
    TempModifiers |= B_OPTION_KEY;
  else
    TempModifiers &= ~ (uint32) B_OPTION_KEY;

  KeyState.modifiers = TempModifiers;

  if (m_KeyMapPntr == NULL)
    return; // Can't do anything more without it.

  // Update the pressed keys to reflect the modifiers.  Use the keymap to find
  // the keycode for the actual modifier key.

  SetKeyState (KeyState,
    m_KeyMapPntr->caps_key, TempModifiers & B_CAPS_LOCK);
  SetKeyState (KeyState,
    m_KeyMapPntr->scroll_key, TempModifiers & B_SCROLL_LOCK);
  SetKeyState (KeyState,
    m_KeyMapPntr->num_key, TempModifiers & B_NUM_LOCK);
  SetKeyState (KeyState,
    m_KeyMapPntr->left_shift_key, TempModifiers & B_LEFT_SHIFT_KEY);
  SetKeyState (KeyState,
    m_KeyMapPntr->right_shift_key, TempModifiers & B_RIGHT_SHIFT_KEY);
  SetKeyState (KeyState,
    m_KeyMapPntr->left_command_key, TempModifiers & B_LEFT_COMMAND_KEY);
  SetKeyState (KeyState,
    m_KeyMapPntr->right_command_key, TempModifiers & B_RIGHT_COMMAND_KEY);
  SetKeyState (KeyState,
    m_KeyMapPntr->left_control_key, TempModifiers & B_LEFT_CONTROL_KEY);
  SetKeyState (KeyState,
    m_KeyMapPntr->right_control_key, TempModifiers & B_RIGHT_CONTROL_KEY);
  SetKeyState (KeyState,
    m_KeyMapPntr->left_option_key, TempModifiers & B_LEFT_OPTION_KEY);
  SetKeyState (KeyState,
    m_KeyMapPntr->right_option_key, TempModifiers & B_RIGHT_OPTION_KEY);
}
