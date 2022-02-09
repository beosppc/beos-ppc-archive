/* UserAppMessageMonitor

This program just prints (to standard output) the key and mouse messages
received.  Used for testing the VNC server fake keyboard and mouse.

AGMS20040721 */

#include <Application.h>
#include <Bitmap.h>
#include <View.h>
#include <Window.h>
#include <stdio.h>
#include <Input.h> // For BInputDevice.

static thread_id ViewPulseThreadID = -1;
/* The ID of the thread which periodically calls the CounterView's
   Pulse function to do the animation and invalidate the view (but
   not do any drawing or CPU intensive activity).  Negative if
   it doesn't exist. */

static volatile bool TimeToDie = false;
/* When set to TRUE, the threads will try to exit soon. */

void PrintKeyboardStateFlags (BMessage *MessagePntr)
{
  int       BitIndex;
  int       ByteIndex;
  uint8     CurrentByte;
  uint8    *DataPntr;
  ssize_t   DataSize;
  int       KeyCode;

  // Print out the list of which keys are currently down.
  if (MessagePntr == NULL || B_OK !=
  MessagePntr->FindData ("states", B_UINT8_TYPE, 0, (const void **) &DataPntr, &DataSize))
    return; // No keyboard state data in this message.
  printf ("                  states keys down:");
  KeyCode = 0;
  for (ByteIndex = 0; ByteIndex < DataSize ; ByteIndex++)
  {
    CurrentByte = DataPntr[ByteIndex];
    for (BitIndex = 7; BitIndex >= 0; BitIndex--, KeyCode++)
    {
      if (CurrentByte & 1 << BitIndex)
        printf (" %d", KeyCode);
    }
  }
  printf ("\n");
}


class CounterApp : public BApplication
{
public:
  CounterApp () : BApplication ("application/x-vnd.agmsmith.Counter") {};
  virtual void ReadyToRun();
};


class CounterWindow : public BWindow 
{
public:
  CounterWindow () : BWindow (BRect (30, 30, 600, 400), "Counter",
  B_TITLED_WINDOW, 0)
  {
  };
  ~CounterWindow ()
  {
    status_t ReturnCode;
    
    TimeToDie = true;
    if (ViewPulseThreadID > 0)
      wait_for_thread (ViewPulseThreadID, &ReturnCode);
  };
  virtual void MessageReceived(BMessage *MessagePntr)
  {
    if (MessagePntr->what == B_KEY_DOWN ||
    MessagePntr->what == B_KEY_UP ||
    MessagePntr->what == B_UNMAPPED_KEY_DOWN ||
    MessagePntr->what == B_UNMAPPED_KEY_UP ||
    MessagePntr->what == B_MODIFIERS_CHANGED ||
    MessagePntr->what == B_MOUSE_DOWN ||
    MessagePntr->what == B_MOUSE_MOVED ||
    MessagePntr->what == B_MOUSE_UP ||
    MessagePntr->what == B_MOUSE_WHEEL_CHANGED)
    {
      MessagePntr->PrintToStream();
      PrintKeyboardStateFlags (MessagePntr);
    }
    BWindow::MessageReceived(MessagePntr);
  }
  virtual bool QuitRequested ()
  {
    be_app->PostMessage (B_QUIT_REQUESTED);
    return true;
  };
};


int32 ViewPulserCode (void *PassedInData); // Forward reference.


class CounterView : public BView
{
public:
  CounterView (BRect FrameSize) : BView (FrameSize, "CounterView", B_FOLLOW_ALL_SIDES,
   B_WILL_DRAW | B_FRAME_EVENTS | B_FULL_UPDATE_ON_RESIZE | B_PULSE_NEEDED),
    m_MovingDotPoint (10, 20),
    m_BackingView (FrameSize, "BackingView", B_FOLLOW_ALL_SIDES, B_FULL_UPDATE_ON_RESIZE)
  {
    m_CurrentCount = 0;
    m_StringResizeNeededCount = 10;
    m_BackingBitmap = NULL;
    SetViewColor (B_TRANSPARENT_COLOR);
    FrameResized (FrameSize.right, FrameSize.bottom); // Set m_TextStartPoint.
    ViewPulseThreadID = spawn_thread (ViewPulserCode,
      "CounterViewPulser", 80 /* Priority */, this); // Creates but doesn't start.
    m_InputDeviceMousePntr = find_input_device ("VNC Fake Mouse");
    m_InputDeviceKeyboardPntr = find_input_device ("VNC Fake Keyboard");
    m_EventTriggerCounter = 0;
  };
  ~CounterView ()
  {
    if (m_BackingBitmap != NULL)
    {
      m_BackingBitmap->RemoveChild (&m_BackingView);
      delete m_BackingBitmap;
      m_BackingBitmap = NULL;
    }
    delete m_InputDeviceKeyboardPntr;
    delete m_InputDeviceMousePntr;
  };
  virtual void Draw (BRect updateRect);
  virtual void FrameResized (float width, float height);
  virtual void Animate (void);
  virtual void MouseDown (BPoint where)
  {
    Window()->CurrentMessage()->PrintToStream();
  };
  virtual void MouseUp (BPoint where)
  {
    Window()->CurrentMessage()->PrintToStream();
  };
  virtual void MouseMoved (BPoint point, uint32 transit, const BMessage *MessagePntr)
  {
    Window()->CurrentMessage()->PrintToStream();
  };
  virtual void Pulse (void)
  {
    return; // Not force testing message injection at this time.

    BMessage EventMessage;
    char     KeyAsString [16];
    int      i;
    m_EventTriggerCounter++;
printf ("Event %d\n", m_EventTriggerCounter);
    switch (m_EventTriggerCounter)
    {
      case 4:
//        EventMessage.what = B_MOUSE_MOVED;
//        EventMessage.AddFloat ("x", 0.2);
//        EventMessage.AddFloat ("y", 0.1);
//        EventMessage.AddInt64 ("when", system_time ());
//        EventMessage.AddInt32 ("buttons", 2);
//        EventMessage.what = B_MOUSE_WHEEL_CHANGED;
//        EventMessage.AddFloat ("be:wheel_delta_x", 0.0F);
//        EventMessage.AddFloat ("be:wheel_delta_y", -1.0F);
//        m_InputDeviceMousePntr->Control ('ViNC', &EventMessage);
        EventMessage.what = B_KEY_DOWN;
        EventMessage.AddInt64 ("when", system_time ());
        EventMessage.AddInt32 ("key", 61);
        EventMessage.AddInt32 ("modifiers", 0);
        strcpy (KeyAsString, "é");
        for (i = 0; KeyAsString[i] != 0; i++)
          EventMessage.AddInt8 ("byte", KeyAsString [i]);
        EventMessage.AddString ("bytes", KeyAsString);
        EventMessage.AddData ("states", B_UINT8_TYPE, KeyAsString, 16);
        EventMessage.AddInt32 ("raw_char", 62);
        m_InputDeviceKeyboardPntr->Control ('ViNC', &EventMessage);
        EventMessage.MakeEmpty ();
        EventMessage.what = B_KEY_UP;
        EventMessage.AddInt64 ("when", system_time ());
        EventMessage.AddInt32 ("key", 61);
        EventMessage.AddInt32 ("modifiers", 0);
        for (i = 0; KeyAsString[i] != 0; i++)
          EventMessage.AddInt8 ("byte", KeyAsString [i]);
        EventMessage.AddString ("bytes", KeyAsString);
        EventMessage.AddData ("states", B_UINT8_TYPE, KeyAsString, 16);
        EventMessage.AddInt32 ("raw_char", 62);
        m_InputDeviceKeyboardPntr->Control ('ViNC', &EventMessage);
        break;
      default:
        break;
    }
  };
private:
  int m_CurrentCount;
  int m_StringResizeNeededCount;
  BRect m_BndRect;
  BPoint m_TextStartPoint;
  BPoint m_MovingDotPoint;
  int m_MovingDotSize;
  float m_MoveSpeed;
  BBitmap *m_BackingBitmap;
  BView m_BackingView;
  BInputDevice *m_InputDeviceKeyboardPntr;
  BInputDevice *m_InputDeviceMousePntr;
  int m_EventTriggerCounter;
};


/* This is the function which the view pulsing thread executes. */

int32 ViewPulserCode (void *PassedInData)
{
  int          i;
  bigtime_t    CurrentTime;
  bigtime_t    WakeupTime = 0;
  BWindow     *WindowOfView;
  CounterView *ViewPntr;

  ViewPntr = (CounterView *) PassedInData;
  WindowOfView = ViewPntr->Window ();
  for (i = 0; i < 20000 && !TimeToDie; i++)
  {
    WakeupTime += 10000;
    WindowOfView->Lock ();
    ViewPntr->Animate ();
    WindowOfView->Unlock ();
    
    // Go to sleep until the next update time, accounting for
    // time wasted locking the window.
    
    CurrentTime = system_time ();
    if (WakeupTime < CurrentTime)
      WakeupTime = CurrentTime;
    else
      snooze_until (WakeupTime, B_SYSTEM_TIMEBASE);
  }
  return B_OK; // Return code of thread.
}


int main (int argc, char** argv)
{
  CounterApp theApp;
  theApp.Run();
  return 0;
}


void CounterApp::ReadyToRun ()
{
  CounterWindow *MyWin = new CounterWindow;
  MyWin->AddChild (new CounterView (MyWin->Bounds ()));
  MyWin->Show ();
  if (ViewPulseThreadID > 0)
    resume_thread (ViewPulseThreadID);
}


void CounterView::Draw (BRect updateRect)
{
  BRect MovingRect (
    m_MovingDotPoint.x,
    m_MovingDotPoint.y,
    m_MovingDotPoint.x + m_MovingDotSize,
    m_MovingDotPoint.y + m_MovingDotSize);
  char TempString [40];

  if (m_BackingBitmap != NULL)
  {
    m_BackingBitmap->Lock ();
    m_BackingView.SetHighColor (60, 60, 255, 8);
    m_BackingView.FillRect (m_BndRect);
    m_BackingView.SetHighColor (255, 255, 0, 255);
    m_BackingView.MovePenTo (m_TextStartPoint);
    sprintf (TempString, "%d", m_CurrentCount);
    m_BackingView.DrawString (TempString);
    m_BackingView.FillRect (MovingRect);
    m_BackingView.Sync ();
    m_BackingBitmap->Unlock ();
    MovePenTo (0, 0);
    DrawBitmap (m_BackingBitmap);
  }
}


void CounterView::Animate (void)
{
  m_CurrentCount++;
  if (m_CurrentCount >= m_StringResizeNeededCount)
  {
    m_StringResizeNeededCount *= 10;
    FrameResized (m_BndRect.right - m_BndRect.left,
      m_BndRect.bottom - m_BndRect.top);
  }

  // Move the dot around the edge of the window, counterclockwise.
  
  if (m_MovingDotPoint.x  <= m_BndRect.left) // Moving along left side.
  {
    m_MovingDotPoint.x  = m_BndRect.left;
    if (m_MovingDotPoint.y + m_MovingDotSize >= m_BndRect.bottom)
    {
      m_MovingDotPoint.y = m_BndRect.bottom - m_MovingDotSize;
      m_MovingDotPoint.x = m_BndRect.left + m_MoveSpeed;
    }
    else
      m_MovingDotPoint.y += m_MoveSpeed;
  }
  else if (m_MovingDotPoint.y <= m_BndRect.top) // Moving along top side.
  {
    m_MovingDotPoint.y = m_BndRect.top;
    if (m_MovingDotPoint.x <= m_BndRect.left)
    {
      m_MovingDotPoint.y = m_BndRect.top + m_MoveSpeed;
      m_MovingDotPoint.x = m_BndRect.left;
    }
    else
      m_MovingDotPoint.x -= m_MoveSpeed;
  }
  else if (m_MovingDotPoint.y + m_MovingDotSize >= m_BndRect.bottom) // Moving along bottom side.
  {
    m_MovingDotPoint.y = m_BndRect.bottom - m_MovingDotSize;
    if (m_MovingDotPoint.x + m_MovingDotSize >= m_BndRect.right)
    {
      m_MovingDotPoint.y = m_BndRect.bottom - m_MovingDotSize - m_MoveSpeed;
      m_MovingDotPoint.x = m_BndRect.right - m_MovingDotSize;
    }
    else
      m_MovingDotPoint.x += m_MoveSpeed;
  }
  else // Moving along right side.
  {
    m_MovingDotPoint.x = m_BndRect.right - m_MovingDotSize;
    if (m_MovingDotPoint.y <= m_BndRect.top)
    {
      m_MovingDotPoint.y = m_BndRect.top;
      m_MovingDotPoint.x = m_BndRect.right - m_MovingDotSize - m_MoveSpeed;
    }
    else
      m_MovingDotPoint.y -= m_MoveSpeed;
  }

  Invalidate ();
}


void CounterView::FrameResized (float width, float height)
{
  BRect BitmapRect (0, 0, width, height);
  char  TempString [40];

  m_BndRect = Bounds ();

  m_MovingDotSize = (int) (height / 20);
  if (m_MovingDotSize < 1)
    m_MovingDotSize = 1;
  m_MoveSpeed = m_MovingDotSize / 2.0;
  
  // Resize the offscreen bitmap and its view.

  if (m_BackingBitmap != NULL)
  {
    m_BackingBitmap->RemoveChild (&m_BackingView);
    delete m_BackingBitmap;
    m_BackingBitmap = NULL;
  }

  m_BackingView.ResizeTo (width, height);

  m_BackingBitmap = new BBitmap (BitmapRect, B_RGBA32, true /* Accepts subviews */);
  if (!m_BackingBitmap->IsValid ())
  {
    delete m_BackingBitmap;
    m_BackingBitmap = NULL;
  }
  else
  {
    m_BackingBitmap->AddChild (&m_BackingView);
    m_BackingBitmap->Lock ();
    m_BackingView.SetDrawingMode (B_OP_ALPHA);
    m_BackingView.SetFontSize (height * 0.8);
    sprintf (TempString, "%d", m_CurrentCount);
    m_TextStartPoint.x = width / 2 - m_BackingView.StringWidth (TempString) / 2;
    m_TextStartPoint.y = height / 2 + height * 0.25;
    m_BackingBitmap->Unlock ();
  }
}
