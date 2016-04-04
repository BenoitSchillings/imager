all:img
img:img.cpp fits_header.h
	g++  -o img -I/usr/local/include/opencv  img.cpp ./lib/libqsiapi.so -lftdi  -lopencv_calib3d -lopencv_core -lopencv_features2d -lopencv_flann -lopencv_highgui -lopencv_imgproc -lopencv_ml -lopencv_objdetect -lopencv_photo -lopencv_stitching -lopencv_superres -lopencv_ts -lopencv_video -lopencv_videostab 

