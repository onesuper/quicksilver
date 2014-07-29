#pragma once


// Thread local storage: use to let thread know who he is
__thread size_t private_thread_index;
  
  
int my_tid() {
	return private_thread_index;
}

void set_my_tid(size_t tid) {
  private_thread_index = tid;
}
  
