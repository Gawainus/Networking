#ifndef __NODE_H__
#define __NODE_H__

#include <iostream>
#include <vector>
#include <string>
#include <sstream>
#include <string>
#include <iterator>
#include <fstream>
#include <iomanip>
#include <algorithm>

#include <time.h>
using namespace std;

class Node
{
	public:
		//setters
		Node();
		Node( unsigned int pktLen, unsigned int rMin );

		void setCD( unsigned int val );
		void setRemain( unsigned int val );
		void setReady( bool val );

		//getters
		bool retReady();
		unsigned int timeRemain();
		unsigned int countDown();

		
		//modifiers
		void cdDecre();
		void remainDecre();

		void pick();
		void collide( unsigned int max, vector<unsigned int> * ranges );

		//stats
		void succeed();

		unsigned int retSucTot();
		unsigned int retColTot();

	private:
		bool ready;
		unsigned int remain;	//time remaining to complete transmitting
		unsigned int cd;	//count down of time slots
		unsigned int r;		//range of random
		unsigned int col;
		unsigned int colTot;
		unsigned int sucTot;

};

#endif