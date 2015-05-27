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
#include <time.h>
#include <iostream>
#include <sys/stat.h> //fexists
#include <fstream> //fexists

#ifndef Q_MOC_RUN
	#include <boost/lexical_cast.hpp> //int to std:string conversion
	#include <boost/format.hpp>
	#include <boost/algorithm/string.hpp>
	#include <boost/thread/thread.hpp>

	//Serial stuff
	#include <boost/asio/serial_port.hpp>
	#include <boost/asio.hpp>  //for serial IO
#endif

#include <opencv2/opencv.hpp>

#include "blocking_reader.h"


class setcontrol2 : public QObject{
	Q_OBJECT






	public:
		setcontrol2();
		~setcontrol2();
		

		std::string serialGetPort();
		bool serialOpen();
		bool serialConnected;		

		bool isStopped;

		void readOnce();
		bool serialPollSig();

		
		void serialWrite(std::string strWrite);

		QTimer *timerSetConnector;

		double ms_blockingReaderTimeout;
		double ms_reconnectIntervalTimer;

	private:
		//bool *pointerStopWork;


	public slots:
		void slotReConnect();
		void slotDevChanged(QString);
		void slotPollSignal();
		void slotStop(); //Stop any loop
		void slotSendContinue(); //Sends '1' character

	signals:
		void sigSetIsConnected(bool);
		void sigSetIsReady(bool);
		void sigPopup(std::string, cv::Mat);

};




/*

	public:
		setcontrol2();
		~setcontrol2();
		
		bool serialConnected;		
		bool isStopped;

		bool serialPollSig();

		QTimer *timerSetConnector;

		double ms_readerTimeout;
		double ms_writerTimeout;
		double ms_reconnectIntervalTimer;


	private:
		//bool *pointerStopWork;
		QSerialPort * serial;

		std::string serialGetPort();



	public slots:
		void slotReConnect();
		void slotPollSignal();
		void slotStop(); //Stop any loop
		void slotSendContinue(); //Sends '1' character

		void errorReport(QSerialPort::SerialPortError ); //Connected to serialport

	signals:
		void sigSetIsConnected(bool);
		void sigSetIsReady(bool);
		void sigPopup(std::string, cv::Mat);
		void sigFail();

};
*/


