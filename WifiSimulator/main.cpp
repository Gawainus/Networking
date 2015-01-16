#include <math.h>
#include "ftostr.h"
#include "node.h"

int main(int argc, char ** argv)
{
	Lines lns;
	lns.fToStrs(argv[1]);
	
	unsigned int numNodes = lns.retNumNodes();
	unsigned int pktLen = lns.retPktLen();
	unsigned int max = lns.retMax();
	unsigned long long int dura = lns.retDura();

	vector<unsigned int> ranges;
	lns.retRs( &ranges );
	cout<<"N: "<<numNodes<< " L: "<<pktLen<<" RMin: "<< ranges.at(0) << " RMax "<< ranges.at(max-1) << " M: "<<max<<" T: "<< dura<<endl;

	unsigned long long int busyTimes = 0;
	unsigned long long int idleTimes = 0;
	unsigned long long int colTimes = 0;

	unsigned long long int currTime;

	bool channel = false;
	unsigned int readyNum;
	
	vector<Node *> nodes;
	vector<Node *> readyVec;
	Node * trans = NULL;

	srand(time(NULL));
	for(int n = 0; n < numNodes; n++ )
	{
		nodes.push_back( new Node(pktLen, ranges.at(0)) );
		nodes.at(n)->pick(); // new node pick a random number
		if(nodes.at(n)->retReady())
		{
			readyVec.push_back(nodes.at(n));
		}
	}
	
	readyNum = readyVec.size();
	if(readyNum == 1) //ready to transmit 
	{
		trans = readyVec.at(0);
		trans->succeed();

		channel = 1;
	}
	readyVec.clear();

	//global timer
	for( currTime = 0; currTime < dura; currTime++ )
	{
		if(channel)
		{
			busyTimes++;
			trans->remainDecre();
			if( trans->timeRemain() < 1 )
			{
				trans->pick();
				trans->setRemain(pktLen);
				channel = false;
			}
			continue;
		}

		//take care of each nodes
		for( unsigned int n = 0; n < numNodes; n++ )
		{
			//cout << n << ": "<< nodes.at(n)->countDown()<<" " ;
			if(nodes.at(n)->retReady())
			{
				readyVec.push_back(nodes.at(n));
			}
		}
		//cout<<endl;

		readyNum = readyVec.size();
		
		if (readyNum < 1)
		{
			idleTimes++;
			for(int n = 0; n < numNodes; n++ )
			{
				nodes[n]->cdDecre();
			}
		}

		else if(readyNum == 1) //ready to transmit 
		{
			trans = readyVec.at(0);
			trans->succeed();
			channel = 1;
			busyTimes++;
			trans->remainDecre();
			if( trans->timeRemain() < 1 )
			{
				trans->pick();
				trans->setRemain(pktLen);
				channel = false;
			}
		}
		else	//collision 
		{
			//cout<< "round "<< currTime<<" collision"<<endl; 
			colTimes++;
			for(int n = 0; n < readyNum; n++ )
			{
				readyVec.at(n)->collide(max, &ranges);
				readyVec.at(n)->pick(); // new node pick a random number
			}
		}
		readyVec.clear();
	}

	double meanSuc = 0;
	double meanCol = 0;

	for(int n = 0; n < numNodes; n++)
	{
		meanSuc += nodes.at(n)->retSucTot();
		meanCol += nodes.at(n)->retColTot();
	}

	meanSuc /= numNodes;
	meanCol /= numNodes;
	cout<< "meanSuc: " << meanSuc << " meanCol: " << meanCol << endl;
	cout<< "totol: " <<((double)busyTimes/dura) + ((double)idleTimes/dura) + ((double)colTimes/dura) << endl;

	double varSuc = 0;
	double varCol = 0;

	for(int n = 0; n < numNodes; n++)
	{
		varSuc += pow( (nodes.at(n)->retSucTot() - meanSuc), 2);
		varCol += pow( (nodes.at(n)->retColTot() - meanCol), 2);
	}

	varSuc /= numNodes;
	varCol /= numNodes;

	cout << endl;
	cout << fixed;

	cout << "Channel utilization (in percentage) " << setprecision(3) << ((double)busyTimes/dura) * 100 << "\%" << endl; 
	cout << "Channel idle fraction (in percentage) " <<((double)idleTimes/dura) * 100 << "\%" << endl; 
	cout << "Total number of collisions " << colTimes << endl; 
	cout << "Variance in number of successful transmissions (across all nodes) " << setprecision(3) << varSuc << endl;
	cout << "Variance in number of collisions (across all nodes) " << varCol <<endl;

	ofstream out;
	out.open("output.txt");

	out << fixed;
	out << "Channel utilization (in percentage) " << setprecision(3) << ((double)busyTimes/dura) * 100 << "\%" << endl; 
	out << "Channel idle fraction (in percentage) " <<((double)idleTimes/dura) * 100 << "\%" << endl; 
	out << "Total number of collisions " << colTimes << endl; 
	out << "Variance in number of successful transmissions (across all nodes) " << setprecision(3) << varSuc << endl;
	out << "Variance in number of collisions (across all nodes) " << varCol <<endl;

	out.close();

	for(int n = 0; n < numNodes; n++ )
	{
		delete nodes.at(n);
	}

	return 0;
}
