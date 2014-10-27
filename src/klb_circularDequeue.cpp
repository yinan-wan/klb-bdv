/*
* Copyright (C) 2014 by  Fernando Amat
* See license.txt for full license and copyright notice.
*
* Authors: Fernando Amat
*  klb_circularDequeue.cpp
*
*  Created on: October 2nd, 2014
*      Author: Fernando Amat
*
* \brief Implements a ciruclar queue to store chunks of  data. BlockCompressort writes into it and blockWirter reads from it in the multi-threaded  implementation. It uses mutex for trhead-safe implementation
*
*
*/

#include <iostream>
#include <cstring>
#include "klb_circularDequeue.h"



using namespace std;


klb_circular_dequeue::klb_circular_dequeue(int blockSizeBytes_, int numBlocks_) : blockSizeBytes(blockSizeBytes_), numBlocks(numBlocks_)
{
	dataBuffer = new char[blockSizeBytes * numBlocks];
	readIdx = 0;
	writeIdx = 0;
	numTaken = ATOMIC_VAR_INIT( 0 );
}


klb_circular_dequeue& klb_circular_dequeue::operator=(const klb_circular_dequeue& p)
{
	if (this != &p)
	{
		readIdx = p.readIdx;
		writeIdx = p.writeIdx;
		std::atomic_store(&numTaken, std::atomic_load(&(p.numTaken)));
		memcpy(dataBuffer, p.dataBuffer, blockSizeBytes * numBlocks);
	}
	return *this;
}

//copy constructor
klb_circular_dequeue::klb_circular_dequeue(const klb_circular_dequeue& p) : blockSizeBytes(p.blockSizeBytes), numBlocks(p.numBlocks)
{

	readIdx = p.readIdx;
	writeIdx = p.writeIdx;
	std::atomic_store(&numTaken, std::atomic_load(&(p.numTaken)));

	dataBuffer = new char[blockSizeBytes * numBlocks];
	memcpy(dataBuffer, p.dataBuffer, blockSizeBytes * numBlocks);	
}

klb_circular_dequeue::~klb_circular_dequeue()
{
	if (dataBuffer != NULL)
	{
		delete[] dataBuffer;
	}
}

//=================================================================
char *klb_circular_dequeue::getReadBlock()
{

	if (std::atomic_load(&numTaken) == 0)//nothing to read
		return NULL;
	else
		return (&(dataBuffer[readIdx * blockSizeBytes]));
}

void klb_circular_dequeue::popReadBlock()
{
	
	if (std::atomic_load(&numTaken) > 0)//something to pop
	{
		std::atomic_fetch_sub(&numTaken, 1);
		readIdx++;		
		if (readIdx == numBlocks)//restart the queue
		{
			readIdx = 0;			
		}
		g_writeWait.notify_one();//if a thread was waiting to get a writing block wake it up
	}	
}

//=================================================================
char* klb_circular_dequeue::getWriteBlock()
{
		
	std::unique_lock<std::mutex> locker(g_lockWrite);//acquires the lock 
	g_writeWait.wait(locker, [&](){return (std::atomic_load(&numTaken) < numBlocks); });//releases the lock until notify. If condition is not satisfied, it waits again
		
	char* ptr = &(dataBuffer[writeIdx * blockSizeBytes]);	
	locker.unlock();	

	return ptr;
}

//=================================================================
void klb_circular_dequeue::pushWriteBlock()
{	
	writeIdx++;
	if (writeIdx == numBlocks)
	{
		writeIdx = 0;
	}
	std::atomic_fetch_add(&numTaken, 1);
}