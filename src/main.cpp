/* Name :       main.cpp]
 * Version:     v1.0
 * Date :       2014-01-01
 * Developer :  Tim Zaman (timbobel@gmail.com)
 * (c)2014, All rights reserved
 * Distribution in *any form* is forbidden.
 */
 
//QT Stuff
#include <QApplication>
#include <QSurfaceFormat>
#include <QWidget>
#include <QThread>
#include <QMutex>
#include <QString>
#include <QtGui>
#include "rti.h"

#include "window.h"


#include <iostream>

#include <boost/filesystem.hpp>

using namespace std;
namespace fs = boost::filesystem;

//int main (int argc, char *argv[]) {
int main (int argc, char ** argv) {

/*
	//// TEST
	QApplication app(argc, argv);

	//QSurfaceFormat fmt;
	//fmt.setSamples(4);
	//QSurfaceFormat::setDefaultFormat(fmt);

	Window window;
	window.show();
	return app.exec();
	//// TEST

*/




	std::cout << "init(" << argc << "," << argv[0] << ")" << std::endl;


	std::string strCmd = argv[0];
	fs::path full_path, full_path_project;
	if (strCmd.substr(0,2).compare("./")==0){ //called if not packaged
		strCmd.erase(0, 2);
		//Obtain all the path variables
		full_path = fs::initial_path<fs::path>();
		full_path = fs::system_complete( fs::path( strCmd ) );
		full_path_project = full_path.branch_path().branch_path(); //go up 2 levels
	} else { //This is called when the app has been packaged, argv[0] will give the entire path already
		full_path = (strCmd);	
		full_path_project = full_path.branch_path().branch_path(); //go up 2 levels
		#ifdef __APPLE__
			QDir dir(full_path_project.string().c_str());
			qDebug() << dir;
			assert(dir.cd("MacOS"));
			//assert(dir.cd("platforms"));
			QCoreApplication::setLibraryPaths(QStringList(dir.absolutePath()));
			//QCoreApplication::setLibraryPaths(QStringList("/Users/tzaman/Dropbox/code/pixel/trunk/package/pixel_v149M.app/Contents/MacOS/");
			
			std::cout << "LibraryPaths=" << QCoreApplication::libraryPaths().join(",").toUtf8().data() << std::endl;
		#endif
	}	
	


	QApplication app(argc, argv);

	#ifdef __APPLE__ 
		app.setAttribute(Qt::AA_UseHighDpiPixmaps);
	#endif

	QFile f(":qdarkstyle/style.qss");
	if (!f.exists()){
	    cout << "Unable to set stylesheet, file not found" << endl;
	} else {
		cout << "Making style sheet.." << endl;
		f.open(QFile::ReadOnly | QFile::Text);
		QTextStream ts(&f);
		//QApplication::instance()->setStyleSheet(ts.readAll());
		app.setStyleSheet(ts.readAll());
	}

	//app.setStyleSheet("QLineEdit { background-color: yellow }");

	//MainWindow ui(argc,argv);
	MainWindow ui(full_path_project.string());
	ui.show();

	return app.exec();
	

}


