/****************************************************************************
**
** Copyright (C) 2014 Tim Zaman.
** Contact: http://www.timzaman.com/ , timbobel@gmail.com
**
**
****************************************************************************/

#ifndef RTI_H
#define RTI_H



#include "ui_rti_mainwindow.h"

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
#include <QFileDialog>
#include <QTreeView>
#include <QList>
#include <QMetaType>
#include <QMessageBox>
#include <QProgressBar>
#include <QInputDialog>
#include <QFileSystemModel>
#include <QGraphicsView>

#include <QtGui/QGuiApplication>
#include <QtGui/QMatrix4x4>
#include <QtGui/QOpenGLShaderProgram>
#include <QtGui/QScreen>


//#include <QtSql>
//#include <QtWidgets/QSplitter>


#include <iostream>
#include <stdio.h> //file streams
#include <string>
#include <clocale>


#include <fcntl.h>
#include <pthread.h> //Multithreading (GTK)

#include <lcms2.h> //liblcms2-dev
//#include <tiffio.h> //libtiff5-dev
#include <time.h>

//#include <process.h> //putenv() getenv()

#ifndef Q_MOC_RUN
	#include <boost/program_options.hpp> //libboost-dev, libboost-1.53-all-dev
	#include <boost/filesystem.hpp>
	#include <boost/lexical_cast.hpp> //int to std:string conversion
	#include <boost/format.hpp>
	#include <boost/thread/thread.hpp> //for this::thread::sleep
	#include <boost/variant.hpp>
	#include <boost/regex.hpp>
	#include <boost/algorithm/string.hpp>
#endif

#ifndef __APPLE__ 
	#define __OMP__
	#include <omp.h> 
#endif

#include <jpeglib.h> //libjpeg-dev
#include <sys/types.h>
#include <sys/stat.h>
//#include <libexif/exif-data.h> //libexif-dev
//#include <exiv2/exiv2.hpp> //libexiv2-dev

#include <unistd.h>
#include <pwd.h>

#include <iomanip>
#include <cstdlib>
#include <sys/stat.h> //mkdir
#include <dirent.h> //dir listing

#include <math.h>

#include <vector>
#include <opencv2/opencv.hpp>
#include "opencv2/ocl/ocl.hpp"
#include <libconfig.h++>
//#include "svnversion.h"

#include "gpWrapper.h"
#include "setcontrol2.h"

#include "photometricstereo.h"


#include <gphoto2/gphoto2-camera.h> //libgphoto2-2-dev libusb-dev

#include "imagearea.h"

#include "mainwidget.h"

#include "trianglewindow.h"

Q_DECLARE_METATYPE(cv::Mat)


class MainWindow : public QMainWindow{
	Q_OBJECT


	public:
		MainWindow(std::string, QWidget *parent = 0);
		void processFolder(std::string, int);
		int lightidx;
		int numleds;

		//TriangleWindow * window;
		MainWidget * mwindow;

	private:
		Ui::MainWindow ui;


		std::vector<ImageArea *> imageAreas;

		//Cameras
		QThread * camthreads[1];
		std::vector<gpWrapper *> gpCams;

		//Hardware
		QThread * setControlThread;
		setcontrol2 * setControlUnit;

		std::string projectDir, homeDir;

		std::vector<std::vector<cv::Mat> > vecMatCoefMaps;

		std::vector<cv::Mat> getCoefMats(std::vector<cv::Mat> vMatImages, cv::Mat m_uv );
		std::vector<cv::Mat> calcSurfaceNormals(std::vector<cv::Mat> matCoefMaps);
		
		cv::Mat matRGB;

		double unow, vnow;

		int quadsize; //Size of a tile or quadrant

		int goQuadrant(std::vector<cv::Mat> matImages, int id_parent,  cv::Size origSize, std::vector<std::string> &vsXML, int id_parents_parent = -1, cv::Point ptOrigin = cv::Point(0,0));
		std::vector<cv::Mat> multipleRoi(std::vector<cv::Mat> matImages, cv::Rect roi);
		void imwriteVectorResize(std::vector<cv::Mat> vMat, int id , cv::Size newsize, int bordersz);
		float getval(cv::Mat m_coef, float u, float v);

		//QGraphicsScene         scene[0];
		//QGraphicsPixmapItem* pixItem[0];
		//QGraphicsView*         gView[0];

		void setImage(cv::Mat matImage, int i);
		QImage Mat2QImage(const cv::Mat3b &src);
		void timshow(cv::Mat matImage, std::string description);

		void makeWebRTI(std::vector<cv::Mat> matCoef_R, std::vector<cv::Mat> matCoef_G,std::vector<cv::Mat> matCoef_B, float scales[6], int biases[6]);

		cv::Mat gradientToDepth_FC(cv::Mat dzdy, cv::Mat dzdx);
		cv::Mat gradientToDepth_WEI(cv::Mat Pgrads, cv::Mat Qgrads);


		cv::Mat doShift(cv::Mat frame, int x, int y);
		cv::Mat wiggleInPlace(const cv::Mat matReference, const cv::Mat matPlacement, int maxpx);

		void writeNormalMap(std::vector<cv::Mat> normalMap);

	protected:
		void dropEvent(QDropEvent *de);
		void dragEnterEvent(QDragEnterEvent *e);

	   // void mousePressEvent(QMouseEvent *e) Q_DECL_OVERRIDE;
	   // void mouseReleaseEvent(QMouseEvent *e) Q_DECL_OVERRIDE;

	public slots:
		void on_pushButton_clicked();
		void on_pushButton_2_clicked();
		//void mouseMoveEvent(QMouseEvent *event);
		//void keyPressEvent(QKeyEvent *event);

		//GPhoto related
		void slotNewGPImage(CameraFile *, int, int); //New image received from GPhoto
		//Set+Arduino related
		void slotSetIsReady(bool);
		void slotSetIsConnected(bool);



	signals:
		void sigTest();
		//GPhoto
		void sigGPcapture(int);
		void sigGPcapturePreview(int);
		void sigGPcaptureTethered(int);
		//Setcontrol (Arduino)
		void sigPollSetSignal(); //
		void sigSetContinue(); //Sends '1' to set

};



#endif // RTI_H






