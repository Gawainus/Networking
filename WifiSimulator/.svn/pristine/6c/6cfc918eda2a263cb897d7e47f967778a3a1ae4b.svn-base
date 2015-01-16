#include "ftostr.h"

Lines::Lines()
{
	num = 0;
}

unsigned int Lines::numLines()
{
	return num;
}

string Lines::strAt( unsigned int loc )
{
	return input.at(loc);
}

void Lines::fToStrs( char * path)
{
	string buff;
	
	ifstream file( path );
    
    while( getline(file, buff) )
    {
		input.push_back( buff );
		num++;
	}
}

unsigned int Lines::retNumNodes()
{
	istringstream buf(input.at(0));
    string token;
    getline(buf, token, ' ');
    getline(buf, token, ' ');
	
	return stoul( token );
}

unsigned int Lines::retPktLen()
{
	istringstream buf(input.at(1));
    string token;
    getline(buf, token, ' ');
    getline(buf, token, ' ');
	
	return stoul( token );
}

void Lines::retRs( vector<unsigned int> * ranges)
{
	istringstream buf(input.at(2));
    string token;
    
    getline(buf, token, ' ');
    
    while( getline(buf, token, ' ') )
    {
		ranges->push_back(stoul(token));
    }
}

unsigned int Lines::retMax()
{
	istringstream buf(input.at(3));
    string token;
    getline(buf, token, ' ');
    getline(buf, token, ' ');
	
	return stoul( token );
}

unsigned long long int Lines::retDura()
{
	istringstream buf(input.at(4));
    string token;
    getline(buf, token, ' ');
    getline(buf, token, ' ');
	
	return stoull( token );
}
