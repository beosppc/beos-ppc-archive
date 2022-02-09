#ifndef _POWERDOWN_H
#define _POWERDOWN_H

#include <InputServerFilter.h>
#include <InterfaceDefs.h>
#include <Message.h>

class _EXPORT MPowerDownInputFilter;

class MPowerDownInputFilter : public BInputServerFilter
{
	public:
		MPowerDownInputFilter(void);
		filter_result Filter(BMessage *message, BList *outList);
};

extern "C"  { 

_EXPORT extern BInputServerFilter *instantiate_input_filter(void); 

}

BInputServerFilter *instantiate_input_filter(void)
{
	return new MPowerDownInputFilter();
}

#endif