/****************************************************************************
**
** Copyright (C) 2015 Tim Zaman.
** Contact: http://www.timzaman.com/ , timbobel@gmail.com
**
**
****************************************************************************/

#pragma once


#include <QApplication>
#include <QWidget>
#include <QLabel>
#include <QObject>
#include <QThread>
#include <QMutex>
#include <QString>
#include <QDesktopWidget>
#include <QGraphicsPixmapItem>
#include <QGraphicsScene>
#include <QtGui>
#include <QDir>

#include <iostream>
#include <string>
#include <clocale>

#include <fcntl.h>
#include <lcms2.h> //liblcms2-dev
#include <tiffio.h> //libtiff5-dev
#include <time.h>
#include <iostream>

#include <opencv2/opencv.hpp>
#include <gphoto2/gphoto2-camera.h> //libgphoto2-2-dev libusb-dev
#include <libraw/libraw.h> //libdcraw-dev libraw-dev

#ifndef Q_MOC_RUN
#include <boost/filesystem.hpp>
#include <boost/algorithm/string/case_conv.hpp>
#include <boost/lexical_cast.hpp> 
#endif

#include "utils.h"



class gpWrapper : public QObject{
	Q_OBJECT

	public:

		gpWrapper(int);
		~gpWrapper();

		bool isStopped;
		double shutterdelay; //ms

		bool isConnected;
		GPContext  *context;

		//bool camInit(int idx);
		bool camisready;

		//bool cambusy;
		int getNumCams();
		int get_list_config_string(const char *, std::vector<std::string> &, int &);
		std::string model;
		std::string serialNumber;
		int camid;

		//int capture(CameraFile * &, int&);
		//int capture_preview(CameraFile * &file);
		//int captureTethered(CameraFile * &, int &);

		void setGpCameraSettings(std::string sShut, std::string sAperture, std::string sISO);
		int set_config_value_string (const char *, const char *) ;
	private:
		
		int numCams;
		Camera *camera; //Deze camera
		CameraList *camlist;
		int sample_open_camera (Camera ** camera, const char *model, const char *port) ;
		int sample_autodetect (CameraList *camlist);
		static int _lookup_widget(CameraWidget*widget, const char *key, CameraWidget **child);
		int getFileType(std::string);
		int get_config_value_string (const char *key, char **str) ;
		int canon_enable_capture (int onoff);
		void camset();

	public slots:
		bool slotConnectCam(int);
		void slotDisconnectCam(int);

		void slotCamCapture(int, std::string);
		void slotCamCapturePreview(int);
		void slotCamCaptureTethered(int);

		void slotStop(); //Stop any loop
	signals:
		void sigCapture(CameraFile *, int, int); //Capture is done
		void sigCapturePreview(CameraFile *);
		void sigCamConnected(int);

		void sigCameraIsReady();

		void sigCamCaptureTethered(int); //For recursive self-execution-invocation

		void sigPopup(std::string, cv::Mat);

		void sigFail();

};






