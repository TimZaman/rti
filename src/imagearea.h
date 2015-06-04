/****************************************************************************
**
** Copyright (C) 2014 Tim Zaman.
** Contact: http://www.timzaman.com/ , timbobel@gmail.com
**
**
****************************************************************************/

#pragma once

#include <QColor>
#include <QImage>
#include <QPoint>
#include <QWidget>
#include <QtGui>
#include <iostream>
#include <string>

#include <QApplication>
#include <QMessageBox>

#include <opencv2/opencv.hpp>

#ifndef Q_MOC_RUN
	#include <boost/lexical_cast.hpp> //int to std:string conversion
	#include <boost/format.hpp>
#endif


class ImageArea : public QWidget{
   Q_OBJECT

	public:
		ImageArea(QWidget *parent = 0);
		cv::Rect cropRect, drawRect;
		int processID;	
		int camidx;
		double rotationRequested, rotationApplied;	
		int flipflagRequested, flipflagApplied;

		bool openImage(int, std::vector< cv::Mat>, std::vector<double>, std::vector<double>);

		void refreshFlip(bool flipCrop=true);
		//void setNewCropSize(int w, int h);

		std::vector<cv::Point> toolPoints;
		bool fixCropSize;
		bool fixAspRatio;
		cv::Size fixSizeRequested;
		cv::Size szFixAspectRatio;

		void allowRotation(bool);
		void drawImage(bool noEmit=false, double dDidRotext=360);
		void drawCropBox(bool noEmit=false);

		double rotationMax;

		bool useRotext;

		cv::Rect constrainRectInSize(cv::Rect rCrop, cv::Size sImage);

		double Rad2Deg (double Angle);
		double Deg2Rad (double Angle);
		cv::Point rotatePt(cv::Point2f pt, cv::Point ptCp, double rotDeg);
		cv::Point rotatePt(cv::Point pt, cv::Point ptCp, double rotDeg);
		bool use;
		bool allowBlurEstimation;

		std::vector<cv::Mat> rti_images;
		std::vector<double> rti_scales;
		std::vector<double> rti_bias;
		void updateRTIview(double x, double y);
		//bool loadICCprofile(std::string);
	protected:
		void keyPressEvent(QKeyEvent *event);
		void keyReleaseEvent(QKeyEvent *event);
		void mousePressEvent(QMouseEvent *event);
		void mouseMoveEvent(QMouseEvent *event);
		void mouseReleaseEvent(QMouseEvent *event);
		void paintEvent(QPaintEvent *event);
		void resizeEvent(QResizeEvent *event);

	private:
		QImage Mat2QImage(const cv::Mat3b &);
		void minglePixel(cv::Vec3b &);
		bool scribbling;
		int scribbletype;
		int keyPressedNow;
		cv::Size getFitSize(cv::Size sizeIn, cv::Size sizeOut);
		cv::Rect getBoundingRect(std::vector<cv::Point>, int idx);

		void checkCropboxEdges(cv::Rect, cv::Size);

		void rotate(cv::Mat& src, double angle, cv::Mat& dst);
		bool rotateAllowed;

		void reduceNonzero(cv::Mat matRot, cv::Mat & matCol);
		double rotateToText(cv::Mat matImage);

		//cmsHTRANSFORM hTransformLab;

		QImage image;

		void rot90(cv::Mat &matImage, int rotflag);
		cv::Rect  rotRect(int rotflag,  cv::Rect cropBox, cv::Size sizeIm);
		cv::Point rotPoint(int rotflag, cv::Point ptNow,  cv::Size sizeIm);

		QPoint lastPointCursor;


		cv::Mat matOriginal; //Original image
		cv::Mat matScaled;   //Scaled image from original
		cv::Mat matDisplay;  //Annotated matScaled

		//Zoomstuff
		double pctZoomRequest;
		cv::Point cpView;
		cv::Rect zoomCropRect;

		//void estimateBlur();
		cv::Mat matBlur;
		bool viewingBlur;
		bool madeBlur;

	public slots:
		void setCropWidth(cv::Size);
		void constrainAspectRatio();
		//void clearImage();
		//void print();
	signals:
		void signalCropRect(int, int, cv::Rect, double, int);
		void sigPixSample(int, int, int, double, double, double, double);
		//void signalCropRects(int, std::vector<cv::Rect>);
};



