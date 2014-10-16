/*
* Copyright (C) 2014 by  Fernando Amat
* See license.txt for full license and copyright notice.
*
* Authors: Fernando Amat
*  klb_imageIO.h
*
*  Created on: October 2nd, 2014
*      Author: Fernando Amat
*
* \brief Main class to read/write klb format. THIS CLASS DOES NOT STORE ANY POINTER TO THE IMAGE. IT JUST RETURNS THEM AND KEEPS THE FILE SYSTEM OPEN TO INTERACT WITH THE FILE
*/

#ifndef __KLB_IMAGE_IO_H__
#define __KLB_IMAGE_IO_H__

#include <string>
#include <mutex>
#include <condition_variable>
#include <vector>

#include "klb_imageHeader.h"
#include "klb_circularDequeue.h"
#include "klb_ROI.h"


class klb_imageIO
{
public:

	std::string filename;
	klb_image_header header;
	int numThreads;//number of threads to use

	//constructor / destructor
	klb_imageIO();
	klb_imageIO(const std::string &filename_);

	//set/get functions
	

	//main functions
	int readHeader(){ return header.readHeader(filename.c_str()); };
	int readHeader(const std::string &filename_)
	{
		filename = filename_;
		return readHeader();
	};

	/*
	\brief	Main function to save an image. We assume the correct header has been set prior to calling this function. 
	*/
	int writeImage(const char* img, int numThreads);


	/*
	\brief	Main function to read an image (or part of an image defined by ROI).We assume the correct header has been set prior to calling this function. 
	*/
	int readImage(char* img, const klb_ROI* ROI, int numThreads);

protected:

private:	
	static std::mutex				g_lockblockId;//so each worker reads a unique blockId
	static std::condition_variable	g_queuecheck;//to notify writer that blocks are ready

	//functions to call for each thread
	void blockWriter(std::string filenameOut, int* g_blockSize, int* g_blockThreadId, klb_circular_dequeue** cq);
	void blockCompressor(const char* buffer, int* g_blockSize, uint64_t *blockId, int* g_blockThreadId, klb_circular_dequeue* cq, int threadId);

	void blockUncompressor(char* bufferOut, uint64_t *blockId, const klb_ROI* ROI);
};


#endif //end of __KLB_IMAGE_IO_H__