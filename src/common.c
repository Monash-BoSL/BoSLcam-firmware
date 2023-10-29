#include "common.h"

void LOG_UNIXTIME(const int32_t ln){
	int ret;
	int32_t ct;
	uint64_t unix_time_ms; 
	ret = date_time_now(&unix_time_ms);
	if(ret < 0){return ret;}
	ct = (uint32_t) (unix_time_ms/1000);

	printk("%d: UNIX TIME: %d s\n", ln, ct);


	return 0;
}