/*
* Copyright (C) 2014  Fernando Amat
* See license.txt for full license and copyright notice.
*
* Authors: Fernando Amat
*  mainTest_klnIO.cxx
*
*  Created on: October 1st, 2014
*      Author: Fernando Amat
*
* \brief Test the main klb I/O library
*
*
*/


#include <string>
#include <cstdint>
#include <chrono>
#include <algorithm>
#include <random>

#include "klb_imageIO.h"


using namespace std;
typedef std::chrono::high_resolution_clock Clock;

int main(int argc, const char** argv)
{
	int numThreads = 10;//<= 0 indicates use as many as possible
	int compressionType = 1;//1->bzip2; 0->none
	std::string filenameOut("E:/compressionFormatData/debugGradient.klb");


	//very large size to test > 4GB files	
	//std::uint32_t	xyzct[KLB_DATA_DIMS] = { 2001, 2012, 2024, 1, 1 };
	//std::uint32_t	blockSize[KLB_DATA_DIMS] = { 256, 256, 32, 1, 1 };

	//medium size
	std::uint32_t	xyzct[KLB_DATA_DIMS] = {1002, 200, 54, 1, 1};
	std::uint32_t	blockSize[KLB_DATA_DIMS] = {256, 256, 32, 1, 1};

	//small size for debugging purposes
	//std::uint32_t	xyzct[KLB_DATA_DIMS] = { 90, 99, 110, 1, 1 };
	//std::uint32_t	blockSize[KLB_DATA_DIMS] = { 64, 32, 32, 1, 1 };

	//very small size for debugging purposes (it does not work with bzip2)
	//std::uint32_t	xyzct[KLB_DATA_DIMS] = { 20, 17, 10, 1, 1 };
	//std::uint32_t	blockSize[KLB_DATA_DIMS] = { 8, 4, 2, 1, 1 };

	


	//================================================
	//common definitions
	int err;
	auto t1 = Clock::now();
	auto t2 = Clock::now();
	uint64_t dim, planeSize, offsetPlane;
	klb_imageIO imgXYplane(filenameOut);
	long long int totalTime = 0;
	int numPlanes;
	uint16_t* imgB;
	klb_ROI ROIfull;
	//=========================================================================
	
	

	
	
	//initialize I/O object
	klb_imageIO imgIO( filenameOut );

	//setup header
	memcpy(imgIO.header.xyzct, xyzct, sizeof(uint32_t)* KLB_DATA_DIMS);
	memcpy(imgIO.header.blockSize, blockSize, sizeof(uint32_t)* KLB_DATA_DIMS);
	imgIO.header.dataType = 1;//uint16
	imgIO.header.compressionType = compressionType;
	for (int ii = 0; ii < KLB_DATA_DIMS; ii++)
		imgIO.header.pixelSize[ii] = 1.2f*(ii+1);


	//generate artificial image: gradient is nice since we can debug very fast by visual inspection or random for uncompressibility
	uint16_t* img = new uint16_t[imgIO.header.getImageSizePixels()];
	std::random_device rd;
	std::mt19937 gen(rd());
	std::uniform_int_distribution<int> dis(-6, 6);
	for (uint64_t ii = 0; ii < imgIO.header.getImageSizePixels(); ii++)
	{
		img[ii] = ii % 65535;
		//add some noise
		img[ii] += dis(gen);
	}

	/*
	cout << "DEBUGGING: Writing out image at " << "E:/compressionFormatData/debugGradient.raw with " << imgIO.header.getImageSizePixels() <<" voxels"<< endl;
	ofstream fout("E:/compressionFormatData/debugGradient.raw", ios::binary);
	fout.write((char*)img, sizeof(uint16_t)* imgIO.header.getImageSizePixels());
	fout.close();
	*/

	
	cout << "Compressing file to " << filenameOut << endl;

	t1 = Clock::now();
	err = imgIO.writeImage((char*)img, numThreads);//all the threads available
	if (err > 0)
		return 2;

	t2 = Clock::now();

	


	std::cout << "Written test file at "<<filenameOut<<" compress + write file =" << std::chrono::duration_cast<std::chrono::milliseconds>(t2 - t1).count() << " ms using "<<numThreads<<" threads"<< std::endl;
	
	

	//===========================================================================================
	//===========================================================================================
	
	cout << endl << endl << "Reading entire image back" << endl;
	
	
	t1 = Clock::now();
	klb_imageIO imgFull(filenameOut);

	err = imgFull.readHeader();
	if (err > 0)
		return err;
	uint64_t N = imgFull.header.getImageSizePixels();
	uint16_t* imgA = new uint16_t[N];


	ROIfull.defineFullImage(imgFull.header.xyzct);
	err = imgFull.readImage((char*)imgA, &ROIfull, numThreads);
	if (err > 0)
		return err;

	t2 = Clock::now();

	std::cout << "Read full test file at " << filenameOut << " in =" << std::chrono::duration_cast<std::chrono::milliseconds>(t2 - t1).count() << " ms using " << numThreads << " threads" << std::endl;

	//compare elements
	bool isEqual = true;
	for (uint64_t ii = 0; ii < imgFull.header.getImageSizePixels(); ii++)
	{
		if (imgA[ii] != img[ii])
		{
			isEqual = false;
			break;
		}
	}
	if (!isEqual)
		cout << "ERROR!!!: images are different" << endl;

	delete[] imgA;
	
	//===========================================================================================
	//===========================================================================================

	
	cout << endl << endl << "Reading XY planes" << endl;

	
	err = imgXYplane.readHeader();
	if (err > 0)
		return err;
	dim = 2;
	planeSize = imgXYplane.header.xyzct[0] * imgXYplane.header.xyzct[1];
	offsetPlane = 1;

	
	
	err = imgXYplane.readHeader();
	if (err > 0)
		return err;
	imgB = new uint16_t[planeSize];

	
	totalTime = 0; 
	numPlanes = numPlanes = std::min(imgXYplane.header.xyzct[dim], 100U);
	for (uint64_t ii = 0; ii < numPlanes; ii++)
	{
		t1 = Clock::now();
		ROIfull.defineSlice(ii, dim, imgXYplane.header.xyzct);
		err = imgXYplane.readImage((char*)imgB, &ROIfull, numThreads);
		if (err > 0)
			return err;
		t2 = Clock::now();
		totalTime+= std::chrono::duration_cast<std::chrono::milliseconds>(t2 - t1).count();


		//compare elements
		bool isEqual = true;
		uint64_t count = ii * planeSize;
		for (uint64_t jj = 0; jj < planeSize; jj++)
		{
			if (imgB[jj] != img[count])
			{
				isEqual = false;
				break;
			}
			count += offsetPlane;
		}
		if (isEqual == false)
		{
			cout << "ERROR!!!: images are different for plane " << ii << endl;
			break;
		}		
	}	

	std::cout << "Read all planes test file at " << filenameOut << " in =" <<  ((float)totalTime)/ (float)numPlanes << " ms per plane using " << numThreads << " threads" << std::endl;
	delete[] imgB;
	

	//===========================================================================================
	//===========================================================================================
	cout << endl << endl << "Reading XZ planes" << endl;

	
	err = imgXYplane.readHeader();
	if (err > 0)
		return err;
	dim = 1;
	planeSize = imgXYplane.header.xyzct[0] * imgXYplane.header.xyzct[2];

	imgB = new uint16_t[planeSize];


	totalTime = 0;
	numPlanes = numPlanes = std::min(imgXYplane.header.xyzct[dim], 100U);
	for (uint64_t ii = 0; ii < numPlanes; ii++)
	{
		t1 = Clock::now();
		ROIfull.defineSlice(ii, dim, imgXYplane.header.xyzct);
		err = imgXYplane.readImage((char*)imgB, &ROIfull, numThreads);
		if (err > 0)
			return err;
		t2 = Clock::now();
		totalTime += std::chrono::duration_cast<std::chrono::milliseconds>(t2 - t1).count();


		//write out plane
		/*
		cout << "DEBUGGING: Writing out planes" << endl;
		ofstream fout("E:/compressionFormatData/debugGradient_planeXZ.raw", ios::binary);
		fout.write((char*)imgB, sizeof(uint16_t)* planeSize);
		fout.close();
		*/


		//compare elements
		bool isEqual = true;
		int count = 0;
		for (uint64_t zz = 0; zz < imgXYplane.header.xyzct[2]; zz++)
		{
			for (uint64_t xx = 0; xx < imgXYplane.header.xyzct[0]; xx++)
			{
				if (imgB[count++] != img[xx + ii * (uint64_t)(imgXYplane.header.xyzct[0]) + (uint64_t)(imgXYplane.header.xyzct[0] * imgXYplane.header.xyzct[1]) * zz])
				{
					isEqual = false;
					break;
				}
			}
			if (isEqual == false)
				break;
		}	
		if (isEqual == false)
		{
			cout << "ERROR!!!: images are different for plane " << ii <<" at position "<<count<< endl;
			break;
		}		
	}

	std::cout << "Read all planes test file at " << filenameOut << " in =" << ((float)totalTime) / (float)numPlanes << " ms per plane using " << numThreads << " threads" << std::endl;
	delete[] imgB;



	//===========================================================================================
	//===========================================================================================
	cout << endl << endl << "Reading YZ planes" << endl;

	
	err = imgXYplane.readHeader();
	if (err > 0)
		return err;
	dim = 0;
	planeSize = imgXYplane.header.xyzct[1] * imgXYplane.header.xyzct[2];

	imgB = new uint16_t[planeSize];


	totalTime = 0;
	numPlanes = numPlanes = std::min(imgXYplane.header.xyzct[dim], 100U);
	for (uint64_t ii = 0; ii < numPlanes; ii++)
	{
		t1 = Clock::now();
		ROIfull.defineSlice(ii, dim, imgXYplane.header.xyzct);
		err = imgXYplane.readImage((char*)imgB, &ROIfull, numThreads);
		if (err > 0)
			return err;
		t2 = Clock::now();
		totalTime += std::chrono::duration_cast<std::chrono::milliseconds>(t2 - t1).count();


		//compare elements
		bool isEqual = true;
		int count = 0;
		for (uint64_t zz = 0; zz < imgXYplane.header.xyzct[2]; zz++)
		{
			for (uint64_t yy = 0; yy < imgXYplane.header.xyzct[1]; yy++)
			{
				if (imgB[count++] != img[ii + yy * (uint64_t)(imgXYplane.header.xyzct[0]) + (uint64_t)(imgXYplane.header.xyzct[0] * imgXYplane.header.xyzct[1]) * zz])
				{
					isEqual = false;
					break;
				}
			}
			if (isEqual == false)
				break;
		}
		if (isEqual == false)
		{
			cout << "ERROR!!!: images are different for plane " << ii << endl;
			break;
		}		
	}

	std::cout << "Read all planes test file at " << filenameOut << " in =" << ((float)totalTime) / (float)numPlanes << " ms per plane using " << numThreads << " threads" << std::endl;
	delete[] imgB;

	//===========================================================================================
	//===========================================================================================

	//release memory
	delete[] img;

	return 0;
}