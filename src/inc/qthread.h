

#pragma once



class Qthread {


private:



public:
  // Singleton pattern
  static Qthread& getInstance(void) {

    static Qthread *qthread_p = NULL;
    if (qthread_p == NULL) { 
      qthread_p = new Qthread();
    }

    return *qthread_p;
  }



  int create() {
    Pthread::
  }




  
}