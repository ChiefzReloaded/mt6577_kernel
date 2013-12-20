#include <cust_vibrator.h>
#include <linux/types.h>

static struct vibrator_hw cust_vibrator_hw = {
	.vib_timer = 50,
//                                                                                                  
	.vib_timer_min = 5, 
//                                             
};

struct vibrator_hw *get_cust_vibrator_hw(void)
{
    return &cust_vibrator_hw;
}

