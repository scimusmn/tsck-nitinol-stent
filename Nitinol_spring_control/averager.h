
#include "arduino.h"

class averager {
public:

  // variables for averaging.
  int numSamps;
  int * samps;
  int pntr;
long tot;
long ave;

  averager(int num = 5){
  // init the averaging variables.
    pntr=tot=ave=0;
    numSamps = num;
    samps = new int[numSamps];
    for(int i=0; i<numSamps; i++){
      samps[i]=0;
      tot+=samps[i];
    }
  }
long operator()(){
    return ave;
  }

  void reset(){
    for(int i=0; i<numSamps; i++){
      samps[i]=0;
    }
    tot = ave = 0;
  }

  void idle(int newRead){
    tot-=samps[pntr];
    samps[pntr]=newRead;
    tot+=newRead;
    ave = tot/numSamps;
    pntr=(pntr + 1)%numSamps;
  }
};
