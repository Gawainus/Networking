

#include "node.h"

Node::Node()
{

}

Node::Node( unsigned int pktLen, unsigned int rMin )
{
	ready = false;
	remain = pktLen;
	r = rMin;
	col = 0;
	colTot = 0;
	sucTot = 0;
}


void Node::setRemain( unsigned int val )
{
	remain = val;
}

void Node::setCD( unsigned int val )
{
	cd = val;
}

void Node::setReady( bool val )
{
	ready = val;
}

bool Node::retReady()
{
	return ready;
}

unsigned int Node::timeRemain()
{
	return remain;
}

unsigned int Node::countDown()
{
	return cd;
}

void Node::remainDecre()
{
	remain--;
}

void Node::cdDecre()
{
	cd--;
	if( cd < 1)
	{
		ready = true;
	}
}

void Node::pick( )
{
	ready = false;
	cd = rand() % (r+1);
	if( cd < 1)
		ready = true;
}

void Node::collide(unsigned int max, vector<unsigned int> * ranges)
{
	if( r < ranges->at(ranges->size()-1) )
	{
		for(int i = 0; i < ranges->size(); i++ )
		{
			if(r < ranges->at(i))
			{	
				r = ranges->at(i);
				break;
			}
		}
	}
	col++;
	colTot++;
	if( col >= max )
	{
		col = 0;
		r = ranges->at(0);
	}
}

void Node::succeed()
{
	sucTot++;
}

unsigned int Node::retSucTot()
{
	return sucTot;
}

unsigned int Node::retColTot()
{
	return colTot;
}
