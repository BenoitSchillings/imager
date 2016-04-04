#include "qsiapi.h"
#include <stdio.h>
#include <unistd.h>
#include <iostream>
#include <cmath>
#include <stdlib.h>

#include <opencv2/imgproc/imgproc.hpp>
#include <time.h>
#include <opencv2/core/core.hpp>
#include <opencv2/highgui/highgui.hpp>


using namespace cv;

int main(int argc, char** argv)
{
	int x,y,z;
	std::string serial("");
	std::string desc("");
	std::string info = "";
	std::string modelNumber("");
	char filename[256];
	bool canSetTemp;
	bool hasFilters;
	short binX;
	short binY;
	long xsize;
	long ysize;
	long startX;
	long startY;
	int iNumFound;

        namedWindow("img", 1);
	char c = cvWaitKey(1);
	QSICamera cam;

	cam.put_UseStructuredExceptions(true);
	try
	{
		cam.get_DriverInfo(info);
		std::cout << "qsiapi version: " << info << "\n";
		//Discover the connected cameras
		std::string camSerial[QSICamera::MAXCAMERAS];
		std::string camDesc[QSICamera::MAXCAMERAS];
		cam.get_AvailableCameras(camSerial, camDesc, iNumFound);

		if (iNumFound < 1)
		{
			std::cout << "No cameras found\n";
			exit(1);
		}

		for (int i = 0; i < iNumFound; i++)
		{
			std::cout << camSerial[i] << ":" << camDesc[i] << "\n";
		}

		cam.put_SelectCamera(camSerial[0]);

		cam.put_IsMainCamera(true);
		// Connect to the selected camera and retrieve camera parameters
		cam.put_Connected(true);
		std::cout << "Camera connected. \n";
		// Get Model Number
		cam.get_ModelNumber(modelNumber);
		std::cout << modelNumber << "\n";
		// Get Camera Description
		cam.get_Description(desc);
		std:: cout << desc << "\n";

		// Enable the beeper
		cam.put_SoundEnabled(true);
		// Set the fan mode
		cam.put_FanMode(QSICamera::fanQuiet);
		// Query the current flush mode setting
		//cam.put_PreExposureFlush(QSICamera::FlushNormal);

		// Query if the camera can control the CCD temp
		//cam.get_CanSetCCDTemperature(&canSetTemp);
		if (canSetTemp)
		{
			// Set the CCD temp setpoint to 10.0C
			cam.put_SetCCDTemperature(10.0);
			// Enable the cooler
			cam.put_CoolerOn(true);
		}

		if (modelNumber.substr(0,1) == "6")
		{
			cam.put_ReadoutSpeed(QSICamera::FastReadout);
		}

		cam.get_HasFilterWheel(&hasFilters);
		if ( hasFilters)
		{
			// Set the filter wheel to position 1 (0 based position)
			cam.put_Position(0);
		} 

		// Set image size
		//
		cam.put_BinX(1);
		cam.put_BinY(1);
		// Get the dimensions of the CCD
		cam.get_CameraXSize(&xsize);
		cam.get_CameraYSize(&ysize);
		// Set the exposure to a full frame
		cam.put_StartX(0);
		cam.put_StartY(0);
		cam.put_NumX(xsize);
		cam.put_NumY(ysize);
	
		// take 10 test images
		for (int i = 0; i < 3; i++)
		{
			bool imageReady = false;
			// Start an exposure, 0 milliseconds long (bias frame), with shutter open
			cam.StartExposure(0.000, true);
			// Poll for image completed
			cam.get_ImageReady(&imageReady);
			while(!imageReady)
			{
				usleep(100);
				cam.get_ImageReady(&imageReady);
			}
			// Get the image dimensions to allocate an image array
			cam.get_ImageArraySize(x, y, z);
			unsigned short* image = new unsigned short[x * y];
			// Retrieve the pending image from the camera
			cam.get_ImageArray(image);
			std::cout << "exposure #" << i;
			std::cout << "\n";	
			std::cout.flush();
			delete [] image;
		}
		cam.put_Connected(false);
		std::cout << "Camera disconnected.\nTest complete.\n";
		std::cout.flush();
		return 0;

	}
	catch (std::runtime_error err)
	{
		std::string text = err.what();
		std::cout << text << "\n";
		std::string last("");
		cam.get_LastError(last);
		std::cout << last << "\n";
		std::cout << "exiting with errors\n";
		exit(1);
	}
}

