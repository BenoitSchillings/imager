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

QSICamera cam;
float g_exp = 0.001;
int   g_filter = 1;
int   g_bin = 2;

//-----------------------------------------------------------------------
#define ushort unsigned short
//-----------------------------------------------------------------------


class Cam {
   
public:;
 
    Cam();
    ~Cam();
    void	Update(bool force);
    int		Take();
    int		Find();
    void	AutoLevel();
    ushort 	Pixel(int x, int y);
    void	Save(const char *name);
    void 	WriteLine(FILE *file, int y);

public:
    Mat	cv_image;

    short binX;
    short binY;
    long xsize;
    long ysize;
    long startX;
    long startY;
    float min_v;
    float range_v;
};


//-----------------------------------------------------------------------

#include "fits_header.h"

//-----------------------------------------------------------------------

void Cam::WriteLine(FILE *file, int y)
{
    ushort  tmp[16384];      //large enough for my camera
    
    int     x;
    
    for (int x = 0; x < xsize; x++) {
        ushort  v;
        
        v = Pixel(x, y);
        //v = v + 32768;
        v >>= 1; 
        v = (v>>8) | ((v&0xff) << 8);
        
        tmp[x] = v;
    }
    fwrite(tmp, xsize, 2, file);
}

//-----------------------------------------------------------------------

void IntTo4(int v, char *p)
{
	*p++ = 0x30 + (v/10000);
	v = v % 10000;

       *p++ = 0x30 + (v/1000);
        v = v % 1000;
       
	*p++ = 0x30 + (v/100);
        v = v % 100;
       
	*p++ = 0x30 + (v/10);
        v = v % 10;

       *p = 0x30 + v;
}

//-----------------------------------------------------------------------


void Cam::Save(const char *filename)
{
    FILE *file = fopen(filename, "wb");

    char  header_buf[0xb40];

    int i;

    for (i = 0; i < 0xb40; i++) header_buf[i] = ' ';

    i = 0;

    do {
        const char*   header_line;

        header_line = header[i];

        if (strlen(header_line) > 0) {
            memcpy(&header_buf[i*80], header_line, strlen(header_line));
        }
        else
        break;
	if (i == 3) {
		char *tmp = &header_buf[i*80 + 25];
		IntTo4(xsize, tmp);
	}
        if (i == 4) {
                char *tmp = &header_buf[i*80 + 25];
                IntTo4(ysize, tmp);
        }
 
	i++;
    } while(i < 40);

    fwrite(header_buf, 0xb40, 1, file);

    int     y;

    for (y = 0; y < ysize; y++) {
        WriteLine(file, y);
    }

    fclose(file);
}

//-----------------------------------------------------------------------


Cam::Cam()
{
    int x,y,z;
    bool canSetTemp;
    bool hasFilters;
    int iNumFound;
  

    min_v = cvGetTrackbarPos("min", "img");
    range_v = cvGetTrackbarPos("range", "img");

 
    std::string serial("");
    std::string desc("");
    std::string info = "";
    std::string modelNumber("");


 
    cam.get_DriverInfo(info);
    std::string camSerial[QSICamera::MAXCAMERAS];
    std::string camDesc[QSICamera::MAXCAMERAS];
    cam.get_AvailableCameras(camSerial, camDesc, iNumFound);
    
    if (iNumFound < 1) {
        std::cout << "No cameras found\n";
        exit(-1);
    }
    
    for (int i = 0; i < iNumFound; i++) {
        std::cout << camSerial[i] << ":" << camDesc[i] << "\n";
    }
    
    cam.put_SelectCamera(camSerial[0]);
    
    cam.put_IsMainCamera(true);
    cam.put_Connected(true);
    cam.get_ModelNumber(modelNumber);
    std::cout << modelNumber << "\n";
    cam.get_Description(desc);
    cam.put_SoundEnabled(true);
    cam.put_FanMode(QSICamera::fanQuiet);
    
    // Query if the camera can control the CCD temp
    //cam.get_CanSetCCDTemperature(&canSetTemp);
    if (canSetTemp) {
        // Set the CCD temp setpoint to 10.0C
        cam.put_SetCCDTemperature(0.0);
        // Enable the cooler
        cam.put_CoolerOn(true);
    }
    cam.put_CameraGain(QSICamera::CameraGainLow); 
    if (modelNumber.substr(0,1) == "6") {
        cam.put_ReadoutSpeed(QSICamera::FastReadout); //HighImageQuality
    }
    
    cam.get_HasFilterWheel(&hasFilters);
    if ( hasFilters) {
        // Set the filter wheel to position 1 (0 based position)
        cam.put_Position(g_filter);
    } 
    
    cam.put_BinX(g_bin);
    cam.put_BinY(g_bin);
    cam.get_CameraXSize(&xsize);
    cam.get_CameraYSize(&ysize);
    xsize /= g_bin;
    ysize /= g_bin;	
    printf("%ld %ld\n", xsize, ysize);
    
    // Set the exposure to a full frame
    cam.put_StartX(0);
    cam.put_StartY(0);
    cam.put_NumX(xsize);
    cam.put_NumY(ysize);
    
    cv_image = Mat(Size(xsize, ysize), CV_16UC1);
}

//-----------------------------------------------------------------------


Cam::~Cam()
{
}

//-----------------------------------------------------------------------

ushort Cam::Pixel(int x, int y)
{
	return cv_image.at<unsigned short>(y, x);
}

//-----------------------------------------------------------------------



void Cam::Update(bool force)
{
    
    long sum = *cv_image.ptr<unsigned short>(0);
    sum -= 1000;
    float min = cvGetTrackbarPos("min", "img");
    if (min == 0)
        min = sum;
    float range = cvGetTrackbarPos("range", "img");
   
    if (range != range_v || min != min_v || force) { 
    	float mult = (32768.0/range);
	
	cv::imshow("img", mult*(cv_image - min));
   	min_v = min;
	range_v = range; 
    } 

    if (force) {
	for (int y = 0; y < ysize; y += (ysize/8)) {
		for (int x = 0; x < xsize; x+= (xsize/8)) {
			printf("%d ", Pixel(x, y));	
		}
		printf("\n");
	} 
    }
}


//-----------------------------------------------------------------------

void Cam::AutoLevel()
{
	float sum = 0;
	float dev = 0;
	float cnt = 0;

        for (int y = 0; y < ysize; y += 50) {
                for (int x = 0; x < xsize; x+= 50) {
                        sum = sum + Pixel(x, y);
			cnt = cnt + 1.0;	
		}
	}
	sum /= cnt;
	if (sum < 0) sum = 0;

	cnt = 0.0;

        for (int y = 0; y < ysize; y += 50) {
                for (int x = 0; x < xsize; x+= 50) {
                        dev =  dev + (sum - Pixel(x, y)) * (sum - Pixel(x,y));
                        cnt = cnt + 1.0;
                }
        }
	dev = dev / cnt;
	dev = sqrt(dev);
	if (dev < 10) dev = 10;
	if (dev > 5000) dev = 5000;

	sum -= dev;
 
   	setTrackbarPos("min", "img", sum);
    	setTrackbarPos("range", "img", dev * 4.0);
	Update(true);
}


//-----------------------------------------------------------------------


int Cam::Find()
{
    int x,y,z;
    
   
    while(1) {	
        bool imageReady = false;
	cam.StartExposure(g_exp, true);
	cam.get_ImageReady(&imageReady);
	
	while(!imageReady) {
            Update(false); 
	    char c = cvWaitKey(1);
            
            if (c == 27) { 
                goto exit;
            }
            if (c == 'a' || c == 'A') {
                AutoLevel();
            }
            
            usleep(100);
            cam.get_ImageReady(&imageReady);
        }
	
        // Get the image dimensions to allocate an image array
        cam.get_ImageArraySize(x, y, z);
 
        cam.get_ImageArray(cv_image.ptr<unsigned short>(0));	
        Update(true);
	char c = cvWaitKey(1);	
        
        if (c == 27) {
            goto exit;	
        }
        if (c == 'a' || c == 'A') {
        	AutoLevel();
        }



    }
    cam.put_Connected(false);
    std::cout << "Camera disconnected.\nTest complete.\n";
    std::cout.flush();
    return 0;
    
exit:;
    cam.put_Connected(false);
    return 0;
}

//-----------------------------------------------------------------------

int Cam::Take()
{
    int x,y,z;
    
   
    bool imageReady = false;
    cam.StartExposure(g_exp, true);
    cam.get_ImageReady(&imageReady);
	
    while(!imageReady) {
        Update(false); 
        char c = cvWaitKey(1);
            
        if (c == 27) { 
            goto exit;
        }
            
        usleep(100);
        cam.get_ImageReady(&imageReady);
    }
	
    cam.get_ImageArraySize(x, y, z);
 
    cam.get_ImageArray(cv_image.ptr<unsigned short>(0));	
    //Update(true);
    //char c = cvWaitKey(1);	
   
    Save("test.fit"); 
    cam.put_Connected(false);
    return 0;
    
exit:;
    cam.put_Connected(false);
    return 0;
}

//-----------------------------------------------------------------------

void help(char **argv)
{
                printf("%s -h        print this help\n", argv[0]);
                printf("%s -t(ake)   take one frame\n", argv[0]);
                printf("%s -f(ind)   continous find mode\n", argv[0]); 
		printf("exta args\n");
                printf("-exp=value (in sec)\n");
		printf("-filter=value (0-5)\n");
		printf("-bin=value (1-9)\n");
		printf("sudo ./img -exp=0.001 -filter=3 -find -bin=4\n");
}


//-----------------------------------------------------------------------


bool match(char *s1, const char *s2)
{
        return(strncmp(s1, s2, strlen(s2)) == 0);
}

//-----------------------------------------------------------------------



int main(int argc, char** argv)
{
   
    if (argc == 1 || strcmp(argv[1], "-h") == 0) {
            help(argv);
            return 0;
     }

     int pos = 1;

     while(pos < argc) {
     	if (match(argv[pos], "-exp=")) {sscanf(strchr(argv[pos], '=') , "=%f",  &g_exp); argv[pos][0] = 0;}
        if (match(argv[pos], "-filter=")) {sscanf(strchr(argv[pos], '=') , "=%d",  &g_filter); argv[pos][0] = 0;}
	if (match(argv[pos], "-bin=")) {sscanf(strchr(argv[pos], '=') , "=%d",  &g_bin); argv[pos][0] = 0;}	
	pos++;
     } 

    printf("exp    = %f\n", g_exp);
    printf("bin    = %d\n", g_bin);
    printf("filter = %d\n", g_filter);
 
    namedWindow("img", 1);
    
    createTrackbar("min", "img", 0, 64000, 0);
    createTrackbar("range", "img", 0, 32000, 0);
    
    setTrackbarPos("min", "img", 2000); 
    setTrackbarPos("range", "img", 32000);
    
    char c = cvWaitKey(1);
   
    Cam *a_cam;

    a_cam = new Cam(); 
   
    pos = 1;

    while(pos < argc) { 
    	if (match(argv[pos], "-f")) a_cam->Find();
	if (match(argv[pos], "-t")) a_cam->Take(); 
 	
	pos++;
   }
}
