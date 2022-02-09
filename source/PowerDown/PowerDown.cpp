//#include "PowerDown.h"

#include <InputServerFilter.h>
#include <InterfaceDefs.h>
#include <Message.h>
#include <Looper.h>
#include <Invoker.h>
#include <Handler.h>
#include <Messenger.h>

#include <Alert.h>
#include <String.h>

#ifdef __POWERPC__
#pragma export on
#endif


extern "C" _EXPORT BInputServerFilter *instantiate_input_filter(void);  


// from roster_private.h
const uint32 CMD_SHUTDOWN_SYSTEM = 301;
const uint32 CMD_REBOOT_SYSTEM = 302;
const uint32 CMD_SUSPEND_SYSTEM = 304;

#define ROSTER_SIG "application/x-vnd.Be-ROST"

#define POWER_DOWN 'PwrD'
#define POWER_BUTTON 0x6b


class MPowerDownLooper : public BLooper
{  
  public:
    virtual void MessageReceived(BMessage *message)
    {
       BMessenger _Messenger(ROSTER_SIG);	  
       
       if (message->what == POWER_DOWN)
       {
         int32 which;
         if (message->FindInt32("which", &which) == B_OK)
         {
           switch (which) 
           { 
             case 0:
               //do nothing...
               break;
             case 1: 
               //reboot system...
               _Messenger.SendMessage(new BMessage(CMD_REBOOT_SYSTEM));
               break;
             case 2:
               //Shut down... 
               _Messenger.SendMessage(new BMessage(CMD_SHUTDOWN_SYSTEM));
  		   }
  		 }
       } 
    }
  
};


class MPowerDownInputFilter : public BInputServerFilter
{
  public:
    MPowerDownInputFilter();
    ~MPowerDownInputFilter();
    
    filter_result Filter(BMessage *message, BList *outList);

  private:
    MPowerDownLooper* _looper; //message loop to catch the BAlert mesages  
    BInvoker* _invoker; //days are numbered?? 	
  
};

MPowerDownInputFilter::MPowerDownInputFilter(void): BInputServerFilter()
{
  _looper = new MPowerDownLooper();
  _looper->Run(); //Hmmm.. second thread starts...
  
  //ME 15/08/2002 .. note to self, try removing this variable, and go back to just creating in the
  //instantiation of the BAlert::Go()
  _invoker = new BInvoker( new BMessage(POWER_DOWN), _looper);
}

MPowerDownInputFilter::~MPowerDownInputFilter()
{
  //ME 15/08/2002... MUST LOCK LOOPER. Doh!
  //This bug caused the input_server to crash. Except not on
  //intel with single processor !!! (the hint was in the dual processor I guess...)
  //Alpha tester also used dual machine to test (I'm guessing..)
  if (_looper->Lock()) _looper->Quit();
}

filter_result MPowerDownInputFilter::Filter(BMessage *message, BList *outList)
{
  switch (message->what)
  {
    case B_UNMAPPED_KEY_DOWN: //power key is 'unmapped'
	case B_KEY_DOWN: 
	{
	  int32 key;
	  message->FindInt32("key", &key);
	
	  if(key == POWER_BUTTON)
	  {	 
	    //make the alert look as much like the MacOS one as is possible... 
	    BAlert *shutdownAlert = new BAlert("Shutdown/Reboot", 
	                                       "Are you sure you want to shut down your computer?", 	                                       
	                                       "Cancel", 
	                                       "Restart", 
	                                       "Shutdown", 
	                                       B_WIDTH_AS_USUAL, 
	                                       B_WARNING_ALERT
	                                       );
  		shutdownAlert->Go( _invoker );  	
	  }
	}
  }
  return B_DISPATCH_MESSAGE;
}

BInputServerFilter *instantiate_input_filter(void)
{
	return new MPowerDownInputFilter();
}

#ifdef __POWERPC__
#pragma export reset
#endif

