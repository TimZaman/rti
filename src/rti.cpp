/****************************************************************************
**
** Copyright (C) 2014 Tim Zaman.
** Contact: http://www.timzaman.com/ , timbobel@gmail.com
**
**
****************************************************************************/



#include "rti.h"


using namespace std;
using namespace cv;
using namespace libconfig;
namespace po = boost::program_options;
namespace fs = boost::filesystem;



std::string path_templ;
int verbose=1;
bool dpiboost = true;
bool debug=false;
bool gui = false;
int global_dpi=300;




MainWindow::MainWindow(std::string m_projectDir, QWidget *parent) : QMainWindow(parent){
	cout << "Constructed MainWindow(" << m_projectDir << ")" << endl;

	setAcceptDrops(true);


	this->quadsize = 512; //Size of a quad

	ocl::DevicesInfo devices;
	int res = ocl::getOpenCLDevices(devices);
	if(res == 0){
		std::cout << "There is no OPENCL Here !" << std::endl;
	} else {
		for(int i = 0 ; i < devices.size() ; ++i){
			cout << "Device : " << devices[i]->deviceName << " is present" << endl;
		}
	}
	ocl::setDevice(devices[0]);



	#ifdef __APPLE__
		cout << "system(pkill PTPCamera).." << endl;
		std::system("pkill PTPCamera");
	#endif


	this->lightidx=0;
	this->numleds=60;




/*
	Mat im = imread("timshow_map_Z_0.tiff", CV_LOAD_IMAGE_ANYDEPTH);
	Mat imzm = im.clone();

	for (int x=0; x<im.cols*0.5-2; x++){
		for (int y=0; y<im.rows*0.5-2; y++){
			imzm.at<short>(y, x) = im.at<short>(round(y*2)+0, round(x*2)+0);
		}
	}

	imwrite("imzm.tif", imzm);
*/
	



	//cout << SVNVERSION << endl;
	//string SVNVERSION = "TODO";
	//this->strSoftwareRev = boost::lexical_cast<string>(SVNVERSION);
	//string strSoftwareVersion = "Pixel v1." + this->strSoftwareRev ; 
	//cout << "Software Rev:" << strSoftwareVersion << endl;
	

	this->projectDir = m_projectDir;//full_path_project.string();

	
	struct passwd* pwd = getpwuid(getuid());
	this->homeDir = string(pwd->pw_dir);
		
	cout << "Project dir:" << projectDir << " Home dir:" << homeDir << endl;
	path_templ = string(projectDir) + "/Resources/"; //used as extern variable
	
	/*
	#ifdef __APPLE__ 
		string strEnvCam = string(projectDir)+ "/libs/camlibs/";
		string strEnvIO  = string(projectDir)+ "/libs/iolibs/";
		if(!setenv("CAMLIBS",strEnvCam.c_str(),true)){
			cout << "Error loading CAMLIBS environment variable." << endl;
		}
		if(!setenv("IOLIBS",strEnvIO.c_str(),true)){
			cout << "Error loading IOLIBS environment variable." << endl;
		}
		cout << "Environment variable CAMLIBS=" << getenv("CAMLIBS") << endl;
		cout << "Environment variable IOLIBS="  << getenv("IOLIBS") << endl;
	#endif
	*/

	
	

	//qRegisterMetaType<CameraFile>("CameraFile");
	qRegisterMetaType< cv::Mat >("cv::Mat");
	//qRegisterMetaType< QSerialPort::SerialPortError >("QSerialPort::SerialPortError");
	//qRegisterMetaType< CameraFile * >("CameraFile *");

	qRegisterMetaType< QVector<QString> >("QVector<QString> >");
	qRegisterMetaType< std::string >("std::string");
	qRegisterMetaType< std::vector<std::string> >("std::vector<std::string>");
	qRegisterMetaType< cv::Rect >("cv::Rect");
	qRegisterMetaType< std::vector<cv::Rect> >("std::vector<cv::Rect>");
	qRegisterMetaType< unsigned long int >("unsigned long int");
	ui.setupUi(this);


	{ //Hardware Section
		this->setControlUnit = new setcontrol2;
		this->setControlThread = new QThread;

		//Manager-To-Set
		connect(this, SIGNAL(sigPollSetSignal()), setControlUnit, SLOT(slotPollSignal())); 
		//connect(this, SIGNAL(sigPollSetSignal()), setControlUnit, SLOT(slotPollSignal()), Qt::UniqueConnection); 
		connect(this, SIGNAL(sigStopHardware()), setControlUnit,  SLOT(slotStop()), Qt::DirectConnection); //Capture
		connect(this, SIGNAL(sigSetContinue()), setControlUnit,  SLOT(slotSendContinue()), Qt::DirectConnection); //Capture

		//Set-To-Manager
		connect(setControlUnit, SIGNAL(sigSetIsConnected(bool)), this, SLOT(slotSetIsConnected(bool)));
		connect(setControlUnit, SIGNAL(sigSetIsReady(bool)), this, SLOT(slotSetIsReady(bool))); 
		
		connect(setControlUnit, SIGNAL(sigFail()), this, SLOT(slotStop()), Qt::DirectConnection); //Stop Any Loop

		this->setControlUnit->moveToThread(this->setControlThread);
		this->setControlThread->start();
	}

	this->gpCams.resize(1);
	//this->camthreads.resize(2);
	for (int i=0; i<this->gpCams.size();i++){
		this->gpCams[i] = new gpWrapper(i);
		this->camthreads[i] = new QThread;

		//Manager-to-Camera
		connect(this, SIGNAL(sigGPcapture(int)), this->gpCams[i], SLOT(slotCamCapture(int))); //Capture an image
		connect(this, SIGNAL(sigGPcapturePreview(int)), this->gpCams[i], SLOT(slotCamCapturePreview(int))); //Capture Preview
		connect(this, SIGNAL(sigGPcaptureTethered(int)), this->gpCams[i], SLOT(slotCamCaptureTethered(int))); //Capture Tethered Loop
		
		connect(this, SIGNAL(sigStopHardware()), this->gpCams[i], SLOT(slotStop())); //Stop Any Loop
		

		//Camera-to-Manager
		connect(this->gpCams[i], SIGNAL(sigCapture(CameraFile *, int, int)), this, SLOT(slotNewGPImage(CameraFile *, int, int))); //New Image Received
		
		//connect(this->gpCams[i], SIGNAL(sigCapturePreview(CameraFile *)), this, SLOT(slotNewGPImage(CameraFile *, int))); //New Image Received
		connect(this->gpCams[i], SIGNAL(sigFail()), this, SLOT(slotStop())); //Stop Any Loop

		this->gpCams[i]->moveToThread(this->camthreads[i]);
		this->camthreads[i]->start();
	}




	//Immediatelly try to connect cameras
	int numCams = gpCams[0]->getNumCams(); //getNumCams needs to be called!
	if(this->gpCams[0]->slotConnectCam(0)){
		cout << "Initialized left camera" << endl;
		cout << "Serial = " << this->gpCams[0]->serialNumber << endl;
		//connectOK++;
		//camConnected[0]=true;
		//if (shuttertype==0){
			this->gpCams[0]->set_config_value_string("recordingmedia", "SDRAM");
			this->gpCams[0]->set_config_value_string("capturetarget", "Internal RAM");
		//}
		ui.label->setText("Connected");
	} else {
		cout << "error initializing left camera" << endl;
		ui.label->setText("No Camera?");
		//emit sigPopup("Hardware connection to the camera.\nPossible solutions:\n Turn it on\n Check cables\n Restart software", Mat());
		//camConnected[0]=false;
	}





	this->imageAreas.push_back(new ImageArea());
	ui.gridLayout_5->addWidget(imageAreas[0],0,0);
	imageAreas[0]->camidx=0;


	//testrun();



    QSurfaceFormat format;
    format.setDepthBufferSize(24);
    //QSurfaceFormat::setDefaultFormat(format);

    //this->mwindow->setApplicationName("cube");
    //this->mwindow->setApplicationVersion("0.1");
    this->mwindow = new MainWidget();
   	//this->mwindow->resize(640, 480);
   	//this->mwindow->setFormat(format);
	#ifndef QT_NO_OPENGL
	    //MainWidget widget;
	    //this->mwindow->show();
	    ui.gridLayout_5->addWidget(this->mwindow,0,1);
	#else
	    QLabel note("OpenGL Support required");
	    note.show();
	#endif




	//setMouseTracking(true); // E.g. set in your constructor of your widget.

/*

   // QGuiApplication app(argc, argv);
    QSurfaceFormat format;
    format.setSamples(16);
    
    this->window = new TriangleWindow();
	this->window->setFormat(format);
	this->window->resize(640, 480);
	this->window->show();

	this->window->setAnimating(true);

    //return app.exec();


	connect(this, SIGNAL(sigTest()), this->window, SLOT(slotTest())); */

}




















float MainWindow::getval(cv::Mat m_coef, float u, float v){
	float val = m_coef.at<float>(0,0)*u*u + m_coef.at<float>(0,1)*v*v + m_coef.at<float>(0,2)*u*v + m_coef.at<float>(0,3)*u + m_coef.at<float>(0,4)*v + m_coef.at<float>(0,5);
	return val;
}


void MainWindow::imwriteVectorResize(std::vector<cv::Mat> vMat, int id , cv::Size newsize, int bordersz){
	Size newsizeBorder = newsize + Size(bordersz*2, bordersz*2);
	for (int i=0;i<vMat.size();i++){
		Mat matNew;
		
		double resizefact = vMat[0].size().width / double(newsize.width);
		int newbordersz = resizefact*bordersz;

		//cout << "resizefact=" << resizefact << " new bordersz=" << newbordersz << endl;

		copyMakeBorder(matNew, matNew, newbordersz, newbordersz, newbordersz, newbordersz, BORDER_REPLICATE);
		
		//copyMakeBorder(matNew, matNew, bordersz, bordersz, bordersz, bordersz, BORDER_CONSTANT, Scalar(0) );

		cv::resize(vMat[i], matNew, newsizeBorder);
		
		cv::imwrite(boost::lexical_cast<string>(id) + "_" + boost::lexical_cast<string>(i+1) + ".jpg", matNew);
	}
}


std::vector<cv::Mat> MainWindow::multipleRoi(std::vector<cv::Mat> matImages, cv::Rect roi){
	vector<Mat> returnMats;
	for (int i=0;i<matImages.size();i++){
		returnMats.push_back(matImages[i](roi));
	}
	return returnMats;
}


int MainWindow::goQuadrant(std::vector<cv::Mat> matImages, int id_parent,  Size origSize, std::vector<std::string> &vsXML, int id_parents_parent, Point ptOrigin){
	//the 'int return of the function gives the last written id'

	//Assumes square input
	int cols = matImages[0].cols;
	int rows = matImages[0].rows;
	int order = ceil(log2(double(rows)));
	int minorder = round(log2(double(this->quadsize)));

	double xStart = (ptOrigin.x)/double(origSize.height);
	double yStart = (ptOrigin.y)/double(origSize.width);
	double xEnd   = (ptOrigin.x+matImages[0].cols)/double(origSize.width);
	double yEnd   = (ptOrigin.y+matImages[0].rows)/double(origSize.height);

	//For the webviewer, the origin is in the lower left, while we do it in upper right. Account for that:
	double yD = yEnd-yStart;
	yStart = 1-yEnd;
	yEnd = yStart+yD;

	if (order > minorder){ // Make sure the size is more than 2^8=256
		int newdim = pow(2,order-1);

		//Now divide in four quadrants starting with bottom left
		Rect roi1 = Rect(0,                   matImages[0].rows*0.5, matImages[0].cols*0.5, matImages[0].rows*0.5);//LL
		Rect roi2 = Rect(matImages[0].cols*0.5, matImages[0].rows*0.5, matImages[0].cols*0.5, matImages[0].rows*0.5);//LR
		Rect roi3 = Rect(0,                   0,                   matImages[0].cols*0.5, matImages[0].rows*0.5);//UL
		Rect roi4 = Rect(matImages[0].cols*0.5, 0,                   matImages[0].cols*0.5, matImages[0].rows*0.5);//UR

		//Scave the origins
		Point pto1 = ptOrigin+roi1.tl();
		Point pto2 = ptOrigin+roi2.tl();
		Point pto3 = ptOrigin+roi3.tl();
		Point pto4 = ptOrigin+roi4.tl();

		vector<Mat> m1s = multipleRoi(matImages, roi1);
		vector<Mat> m2s = multipleRoi(matImages, roi2);
		vector<Mat> m3s = multipleRoi(matImages, roi3);
		vector<Mat> m4s = multipleRoi(matImages, roi4);

		int iq1, iq2, iq3, iq4;
		iq1=id_parent+1;

		cv::Size szTile(this->quadsize,this->quadsize);

		imwriteVectorResize(m1s, iq1, szTile,1);
		int lastid1 = goQuadrant(m1s, iq1, origSize, vsXML, id_parent, pto1);
		iq2=lastid1+1;
		
		imwriteVectorResize(m2s, iq2, szTile,1);
		int lastid2 = goQuadrant(m2s, iq2, origSize, vsXML, id_parent, pto2);
		iq3 = lastid2 + 1;

		imwriteVectorResize(m3s, iq3, szTile,1);
		int lastid3 = goQuadrant(m3s, iq3, origSize, vsXML, id_parent, pto3);
		iq4 = lastid3+1;

		imwriteVectorResize(m4s, iq4, szTile,1);
		int lastid4 = goQuadrant(m4s, iq4, origSize, vsXML, id_parent, pto4);

		int id_pp_print = id_parents_parent-1;
		id_pp_print = id_pp_print < -1 ? -1 : id_pp_print;

		vsXML[id_parent] = boost::lexical_cast<string>(id_parent) + " " + boost::lexical_cast<string>(id_pp_print) + " " + boost::lexical_cast<string>(iq1-1) + " " + boost::lexical_cast<string>(iq2-1) + " " + boost::lexical_cast<string>(iq3-1) + " " + boost::lexical_cast<string>(iq4-1) + " " + boost::lexical_cast<string>(this->quadsize) +" 1 " + boost::lexical_cast<string>(xStart) + " " + boost::lexical_cast<string>(yStart) + " 0 " + boost::lexical_cast<string>(xEnd) + " " + boost::lexical_cast<string>(yEnd) + " 1";
		return lastid4;
	} else { // Last iteration/order
		vsXML[id_parent] = boost::lexical_cast<string>(id_parent) + " " + boost::lexical_cast<string>(id_parents_parent-1)  + " -1 -1 -1 -1 " + boost::lexical_cast<string>(this->quadsize) + " 1 " + boost::lexical_cast<string>(xStart) + " " + boost::lexical_cast<string>(yStart) + " 0 " + boost::lexical_cast<string>(xEnd) + " " + boost::lexical_cast<string>(yEnd) + " 1";
	}

	return id_parent;
}





void MainWindow::writeNormalMap(std::vector<cv::Mat> normalMap){

	vector<Mat> normalMap8;

	double min, max;

	cv::minMaxLoc(normalMap[0], &min, &max);
	//cout << "normalMap[0] min=" << min << " max=" << max << endl;
	cv::minMaxLoc(normalMap[1], &min, &max);
	//cout << "normalMap[1] min=" << min << " max=" << max << endl;


	normalMap8.resize(3);
	//normalMap8[0]= Mat(normalMap[0].size(), CV_8UC1, 1);
	//normalMap8[1]= Mat(normalMap[0].size(), CV_8UC1, 1);
	normalMap8[2]= Mat(normalMap[0].size(), CV_16UC1, 1);

	normalMap8[0] = (normalMap[0]+1.0) * (65535.0*0.5);
	normalMap8[1] = (normalMap[1]+1.0) * (65535.0*0.5);
	normalMap8[0].convertTo(normalMap8[0],CV_16U);
	normalMap8[1].convertTo(normalMap8[1],CV_16U);

	Mat normalMap3c;
	cv::merge(normalMap8,normalMap3c);
	imwrite("normalMap.png", normalMap3c);

}


cv::Mat calcDiffuseMap(std::vector<cv::Mat> matCoefMaps, std::vector<cv::Mat>  normalMap){
	int numimgs = normalMap.size();
	int cols    = normalMap[0].cols;
	int rows    = normalMap[0].rows;
	int pixels  = cols * rows;

	cv::Mat matDiffuse = Mat(normalMap[0].size(), CV_8UC1, 1);


	float a0, a1, a2, a3, a4, a5;
	float x, y;
	for (int k=0; k< pixels; k++) {
		a0 = matCoefMaps[0].at<float>(k); //.ptr<float> TODO
		a1 = matCoefMaps[1].at<float>(k);
		a2 = matCoefMaps[2].at<float>(k);
		a3 = matCoefMaps[3].at<float>(k);
		a4 = matCoefMaps[4].at<float>(k);
		a5 = matCoefMaps[5].at<float>(k);

		x = normalMap[0].at<float>(k);
		y = normalMap[1].at<float>(k);
		
		matDiffuse.at<uchar>(k) = a0*x*x + a1*y*y + a2*x*y + a3*x + a4*y + a5;
	}

	return matDiffuse;
}


//void MainWindow::dropEvent(QDropEvent *de){
//	cout << "dropEvent()" << endl;
 //   // Unpack dropped data and handle it the way you want
  //  qDebug("Contents: %s", de->mimeData()->text().toLatin1().data());
//}

void MainWindow::dragEnterEvent(QDragEnterEvent *e){
	cout << "dragEnterEvent()" << endl;
    if (e->mimeData()->hasUrls()) {
        e->acceptProposedAction();
    }
}

void MainWindow::dropEvent(QDropEvent *event){
	cout << "dropEvent()" << endl;
	QList<QUrl> urls = event->mimeData()->urls();
	//QString fName = urls.first().toLocalFile();
	if (urls.isEmpty()) return;
	//if (fName.isEmpty()) return;
	//std::string fileName = fName.toStdString();
	//std::string fileExt = boost::filesystem::extension(fileName) ;
	//cout << "Found file " << fileName  << " ext=" << fileExt << endl;

	qDebug() << urls ;

	double scale1=75;
	double scale2=100;

	for (int i=0; i<urls.size(); i++){
		QString qfolderNow = urls[i].toLocalFile();
		QFileInfo folderInfo(qfolderNow);
		if (!folderInfo.isDir()){
			cout << "Folder not a directory, continuing." << endl;
			continue;
		}

		string folderNow = qfolderNow.toStdString();




		cout << "=== folderNow = " << folderNow << endl;

		processFolder(folderNow, scale1);
		processFolder(folderNow, scale2);

		cout << endl;
	}


/*
	if (fileExt.compare(".jpg")==0 || fileExt.compare(".gif")==0 || fileExt.compare(".tif")==0 || fileExt.compare(".jpeg")==0 || fileExt.compare(".png")==0  ){
		cout << "Found a image file." << endl;
		cv::Mat matImage = imread(fileName);		
		if (matImage.data==NULL) return;

			setImage(matImage,0);
			

			//worker->matTemplate = matImage;
			worker->newTemplate(matImage);




	} else if (fileExt.compare(".mp4")==0 || fileExt.compare(".avi")==0){

		emit sigNewMovie(fileName);

	} else {
		cout << "Unknown extension." << endl;
	}*/


}


enum Direction{
    ShiftUp=1, ShiftRight=2, ShiftDown=3, ShiftLeft=4
   };

cv::Mat shiftFrame(cv::Mat frame, int pixels, Direction direction){
	if (pixels==0){
		return frame;
	}

	//create a same sized temporary Mat with all the pixels flagged as invalid (-1)
	cv::Mat temp;// = cv::Mat::zeros(frame.size(), frame.type());
	//Mat temp(frame.size(), frame.type(), Scalar::all(0));
	frame.copyTo(temp);

	//temp matrix is fucking around

	if (pixels<0){
		pixels=abs(pixels);
	}


	switch (direction){
	case(ShiftUp) :
	    frame(cv::Rect(0, pixels, frame.cols, frame.rows - pixels)).copyTo(temp(cv::Rect(0, 0, frame.cols, frame.rows - pixels)));
	    break;
	case(ShiftRight) :
	    frame(cv::Rect(0, 0, frame.cols - pixels, frame.rows)).copyTo(temp(cv::Rect(pixels, 0, frame.cols - pixels, frame.rows)));
	    break;
	case(ShiftDown) :
	    frame(cv::Rect(0, 0, frame.cols, frame.rows - pixels)).copyTo(temp(cv::Rect(0, pixels, frame.cols, frame.rows - pixels)));
	    break;
	case(ShiftLeft) :
	    frame(cv::Rect(pixels, 0, frame.cols - pixels, frame.rows)).copyTo(temp(cv::Rect(0, 0, frame.cols - pixels, frame.rows)));
	    break;
	default :
    	std::cout << "Shift direction is not set properly" << std::endl;
	}

	//if (frame.channels() !=1){
	//	imwrite("test.jpg", temp);
	//}

	return temp;
}


cv::Mat MainWindow::doShift(cv::Mat frame, int x, int y){
	cout << "doShift(" << x << "," << y << ")" << endl;

	if (x<0){
		frame = shiftFrame(frame, x, ShiftLeft);
	} else if (x>0){
		frame = shiftFrame(frame, x, ShiftRight);
	} else {
		//x==0, nothing
	}

	if (y<0){
		frame = shiftFrame(frame, y, ShiftUp);
	} else if (y>0){
		frame = shiftFrame(frame, y, ShiftDown);
	} else {
		//y==0, nothing
	}


	return frame;
}


cv::Mat MainWindow::wiggleInPlace(const cv::Mat matReference, const cv::Mat matPlacement, int maxpx){
	cout << "wiggleInPlace(..)" << endl;
	
	cv::Mat matReferenceHSV=cv::Mat::zeros(matReference.size(),matReference.type());
	cv::Mat matPlacementHSV=cv::Mat::zeros(matPlacement.size(),matPlacement.type());;
	//matReference.copyTo(matReferenceHSV);
	//matPlacement.copyTo(matPlacementHSV);


	cvtColor(matReference, matReferenceHSV, CV_BGR2HSV);
	cvtColor(matPlacement, matPlacementHSV, CV_BGR2HSV);

	vector<Mat> planesRef;
	split( matReferenceHSV, planesRef );

	vector<Mat> planesPlacement;
	split( matPlacementHSV, planesPlacement );

	Mat matReferenceK = planesRef[1];
	Mat matPlacementK = planesPlacement[1];

	
	int minx=0, miny=0;
	int numpx = matReference.cols*matReference.rows;
	int mindiff=10e50; //Arbitrary high number

	for (int x=-maxpx; x<=maxpx; x++){
		for (int y=-maxpx; y<=maxpx; y++){
			Mat matDiff;
			Mat matShift = doShift(matPlacementK, x, y);
			
			absdiff(matReferenceK, matShift, matDiff); 
			double d = cv::sum( matDiff(cv::Rect(maxpx, maxpx, matReference.cols - maxpx, matReference.rows - maxpx)) )[0]/numpx;
			cout << "\tx=" << x << "\ty=" << y << "\tdiff=" << d << endl;
			if (d<mindiff){
				mindiff=d;
				minx=x;
				miny=y;
			}
			//imwrite("return.jpg",matShift);
			//sleep(1);
		}
	}
	Mat matReturn = doShift(matPlacement, minx, miny);
		
	return matReturn;

}




cv::Mat MainWindow::gradientToDepth_FC(cv::Mat dzdy, cv::Mat dzdx) {

	//TODO: test dzdx.size==dzdy.size

	//Set up a mesh(grid) spanning from -0.5 to +0.5
	//cv::Mat wx(1, dzdx.cols, CV_32FC1, cv::Scalar::all(0)); //Horizontal from -0.5 to +0.5
    //cv::Mat wy(dzdy.rows, 1, CV_32FC1, cv::Scalar::all(0)); //Vertical from -0.5 to +0.5

/*
    float dwx = 1.0/wx.cols;
	for (int i=0; i<wx.cols; i++){
		wx.at<float>(i) = -0.5 + dwx*i;
	}
	//Now replicate this horizontal row, vertically 
	cv::repeat(wx, dzdx.rows, 1, wx);

	float dwy = 1.0/wy.rows;
	for (int i=0; i<wy.rows; i++){
		wy.at<float>(i) = -0.5 + dwy*i;
	}
	//Now replicate this vertical row, horizontally 
	cv::repeat(wy, 1, dzdy.cols, wy);

	cout << "wx size=" << wx.size() << endl;
	cout << "wy size=" << wy.size() << endl;*/




	//Quadrant shift to put zero frequency at the appropriate edge
	//Now construct wxi and wxi, (ifftshift)
	//wxi: horizontal from 0:0.5:-0.5:0
	//wxi: vertical from 0:0.5:-0.5:0

	cv::Mat wxi(1, dzdx.cols, CV_32FC1, cv::Scalar::all(0)); //Horizontal from -0.5 to +0.5
	cv::Mat wyi(dzdy.rows, 1, CV_32FC1, cv::Scalar::all(0)); //Vertical from -0.5 to +0.5

    float dwx = 1.0/wxi.cols;
	for (int i=0; i<wxi.cols; i++){
		if (i < wxi.cols*0.5){
			wxi.at<float>(i) = 0.0 + dwx*i;	
		} else {
			wxi.at<float>(i) = -1 + dwx*i;	
		}
	}
	//Now replicate this horizontal row, vertically 
	cv::repeat(wxi, dzdx.rows, 1, wxi);

	float dwy = 1.0/wyi.rows;
	for (int i=0; i<wyi.rows; i++){
		if (i < wyi.rows*0.5){
			wyi.at<float>(i) = 0.0 + dwy*i;	
		} else {
			wyi.at<float>(i) = -1 + dwy*i;	
		}
	}
	//Now replicate this vertical row, horizontally 
	cv::repeat(wyi, 1, dzdy.cols, wyi);

	//cout << "wxi size=" << wxi.size() << endl;
	//cout << "wyi size=" << wyi.size() << endl;
	//timshow(wxi, "wxi");
	//timshow(wyi, "wyi");
 

	//Perform DFT
	cv::Mat fdzdx(dzdx.rows, dzdx.cols, CV_32FC2, cv::Scalar::all(0));
    cv::Mat fdzdy(dzdy.rows, dzdy.cols, CV_32FC2, cv::Scalar::all(0));
	cv::dft(dzdx, fdzdx, cv::DFT_COMPLEX_OUTPUT);
    cv::dft(dzdy, fdzdy, cv::DFT_COMPLEX_OUTPUT);



	// Integrate in the frequency domain by phase shifting by pi/2 and
	// weighting the Fourier coefficients by their frequencies in x and y and
	// then dividing by the squared frequency.  eps is added to the
	// denominator to avoid division by 0.

    //In Matlab, -j swaps the two dft channels

	//cv::Mat Z = (-j*wxi.*fdzdx -j*wyi.*fdzdy)./(cv::pow(wxi,2) + cv::pow(wyi,2) + FLT_EPSILON); // FC:Equation [21]

	vector<Mat> fdzdx_planes;
	split( fdzdx, fdzdx_planes );

	vector<Mat> fdzdy_planes;
	split( fdzdy, fdzdy_planes );


	cv::Mat Z(dzdy.rows, dzdx.cols, CV_32FC2, cv::Scalar::all(0)); //Vertical from -0.5 to +0.5
	vector<Mat> Z_planes;
	split( Z, Z_planes );

    //cv::Mat Z = (-wxi.mul(fdzdx) - wyi.mul(fdzdy)) /(cv::pow(wxi,2) + cv::pow(wyi,2) + FLT_EPSILON); // FC:Equation [21]

	cv::Mat denum(wxi.rows, wxi.cols, CV_32FC1, cv::Scalar::all(0)); //Denumerator preallocation
	denum = wxi.mul(wxi) + wyi.mul(wyi) + FLT_EPSILON;

	Z_planes[0] = (-wxi.mul(fdzdx_planes[1]) - wyi.mul(fdzdy_planes[1])) / denum; // FC:Equation [21]
	Z_planes[1] = (-wxi.mul(fdzdx_planes[0]) - wyi.mul(fdzdy_planes[0])) / denum; // FC:Equation [21]


	//z = real(ifft2(Z)); //Depth reconstruction

	//Merge back
	cv::merge(Z_planes, Z);

	//Set unknown average height to 0
	Z.at<cv::Vec2f>(0, 0)[0] = 0.0f;
	Z.at<cv::Vec2f>(0, 0)[1] = 0.0f;

	//cv::Mat Z;
	cv::dft(Z, Z, cv::DFT_INVERSE | cv::DFT_SCALE |  cv::DFT_REAL_OUTPUT);

	return Z;
};


cv::Mat MainWindow::gradientToDepth_WEI(cv::Mat Pgrads, cv::Mat Qgrads) {

    cv::Mat P(Pgrads.rows, Pgrads.cols, CV_32FC2, cv::Scalar::all(0));
    cv::Mat Q(Pgrads.rows, Pgrads.cols, CV_32FC2, cv::Scalar::all(0));
    cv::Mat Z(Pgrads.rows, Pgrads.cols, CV_32FC2, cv::Scalar::all(0));

    cv::dft(Pgrads, P, cv::DFT_COMPLEX_OUTPUT);
    cv::dft(Qgrads, Q, cv::DFT_COMPLEX_OUTPUT);

    
    float mu=1.0;
    float lambda = 0;
    for (int x=0; x< Pgrads.rows; x++){
    	for (int y=0; y< Pgrads.cols; y++){	
			if ((x != 0) || (y != 0)) {
				float u  = sin((float)(x*2*M_PI/Pgrads.rows));
				float v  = sin((float)(y*2*M_PI/Pgrads.cols));
				float uv = u*u + v*v;
				float d = (1.0f + lambda)*uv + mu*pow(uv,2);
				/* offset = (row * numCols * numChannels) + (col * numChannels) + channel */
				//Z[(i*width*2)+(j*2)+(0)] = ( u*P[(i*width*2)+(j*2)+(1)] + v*Q[(i*width*2)+(j*2)+(1)]) / d; 
				//Z[(i*width*2)+(j*2)+(1)] = (-u*P[(i*width*2)+(j*2)+(0)] - v*Q[(i*width*2)+(j*2)+(0)]) / d; 
				Z.at<cv::Vec2f>(x, y)[0] = float( (  u*P.at<cv::Vec2f>(x, y)[1] + v*Q.at<cv::Vec2f>(x, y)[1]) / d);
				Z.at<cv::Vec2f>(x, y)[1] = float( ( -u*P.at<cv::Vec2f>(x, y)[0] - v*Q.at<cv::Vec2f>(x, y)[0]) / d);
		    }
		}
	}


    /* setting unknown average height to zero */
    Z.at<cv::Vec2f>(0, 0)[0] = 0.0f;
    Z.at<cv::Vec2f>(0, 0)[1] = 0.0f;

    cv::dft(Z, Z, cv::DFT_INVERSE | cv::DFT_SCALE |  cv::DFT_REAL_OUTPUT);


    //TIM START
    //cv::dft(P, P, cv::DFT_INVERSE | cv::DFT_SCALE |  cv::DFT_REAL_OUTPUT);
    //return P;
    //TIM END


    return Z;
}

	




void MainWindow::processFolder(std::string pathfiles, int scale){
	cout << "processFolder(" << pathfiles << ")" << endl;
	if (pathfiles.empty()){
		cout << "Returning, empty path." << endl;
	}








	//gView[0]  = ui.graphicsView;

	int numpos = 60;

	//int numpos = 46;
	//float light_x[numpos]={0.7143656,0.42572728,0.11300106,0.377562,0.5196628,0.5542919,0.049045008,-0.045657422,-0.011493057,-0.3124412,-0.5257201,-0.682128,-0.7191099,-0.9370867,-0.87908036,-0.6157004,-0.35321045,-0.28339386,-0.5179557,-0.8116793,-0.97235554,-0.9989772,-0.8643995,-0.6604865,-0.42859364,-0.20496407,-0.17487712,-0.22393936,-0.63647276,0.8540037,0.58946484,0.33978423,0.86484826,0.8289104,-0.528744,0.28913882,0.16664098,-0.5110437,0.39245373,0.35947376,0.95406663,-0.48079112,0.3146882,0.6202358,0.96722287,0.73871505};
	//float light_y[numpos]={0.14477676,0.010165713,0.21674325,0.56195265,0.74824154,0.7979805,0.95769477,0.6825406,0.15473834,-0.03725942,0.28435156,0.5634941,0.68953764,0.3481706,0.25231558,0.052352168,-0.15463929,-0.3730147,-0.40851504,-0.3032935,-0.20548318,0.037517123,-0.4921869,-0.7437385,-0.6996425,-0.88796735,-0.6637621,-0.43299696,-0.7224621,0.01293916,-0.11292588,-0.28402635,-0.07807658,-0.43205586,0.46452335,-0.2899255,-0.61539507,0.46809918,-0.83706313,-0.91259784,-0.25067133,0.46411717,-0.9358077,-0.7831404,0.08743258,-0.6735654};
	//float light_z[numpos]={0.68463236,0.9047944,0.9696665,0.73597294,0.4124139,0.23661709,0.28357586,0.7294201,0.98788863,0.9492061,0.80172473,0.46602127,0.0861322,0.025412202,0.40441886,0.7862393,0.92267495,0.8834863,0.75155663,0.49918908,0.1109117,0.02524042,0.10278921,0.103007644,0.5716712,0.41170835,0.72721237,0.87313503,0.27009425,0.520106,0.7998618,0.8965911,0.49592495,0.35529602,0.71038574,0.91232777,0.7704023,0.7209143,0.38118935,0.19479182,0.1640753,0.74393225,0.1588561,0.04470575,0.23840189,0.024692714};

	float light_x[60]={-0.9929516, -0.8820576, -0.43937063, 0.15783326, 0.69622546, 0.9833443, 0.8903665, 0.45092723, -0.17200173, -0.7358976, -0.7935484, -0.5660417, -0.2998489, 0.029971555, 0.30466124, 0.5834503, 0.7657216, 0.9044921, 0.9487442, 0.89201015, 0.74835813, 0.52982414, 0.29231, -0.0109395925, -0.2824854, -0.5702386, -0.7576483, -0.9074687, -0.9368542, -0.9023338, -0.7394178, -0.8103832, -0.5800948, -0.13306868, 0.36033842, 0.72681564, 0.81293744, 0.5866447, 0.15227276, -0.38498208, -0.4983769, -0.613285, -0.63904643, -0.6146188, -0.41953424, -0.14285973, 0.11096785, 0.34461218, 0.54569656, 0.6800566, 0.63681984, 0.4882633, 0.32389438, 0.05774808, -0.2501513, -0.39068577, -0.18293057, 0.28417623, 0.3481485, -0.069442354};
	float light_y[60]={-0.10541004, 0.46910587, 0.8971338, 0.9860002, 0.7149733, 0.1714591, -0.45075104, -0.89138335, -0.9844449, -0.6766637, -0.5296131, -0.77489316, -0.8907778, -0.94769317, -0.91113126, -0.7554411, -0.53766674, -0.25409675, 0.016622702, 0.33382538, 0.55737853, 0.78016424, 0.91405857, 0.9567879, 0.89390594, 0.7551072, 0.58202547, 0.2997081, 0.033295892, -0.2799729, -0.35111272, 0.16267243, 0.5938591, 0.8244787, 0.74706775, 0.3911956, -0.10396008, -0.5708838, -0.8063722, -0.7222981, -0.47377586, -0.22574845, 0.062262625, 0.32846564, 0.5007968, 0.62768745, 0.6874285, 0.56106657, 0.34176394, 0.12093571, -0.15096906, -0.3994658, -0.59747946, -0.64912844, -0.57920444, -0.042215016, 0.366906, 0.2702152, -0.17917717, -0.37330797};
	float light_z[60]={0.05418446, 0.043750226, 0.0458733, 0.053780075, 0.06390077, 0.06029678, 0.06380437, 0.04582967, 0.035827436, 0.024102926, 0.29965135, 0.28131357, 0.34147543, 0.31777254, 0.27752715, 0.2981518, 0.3529659, 0.34253302, 0.31560764, 0.30475974, 0.35956812, 0.3326112, 0.28116155, 0.29058102, 0.34804335, 0.32348263, 0.2953225, 0.29440725, 0.3481317, 0.3277329, 0.5744399, 0.5628647, 0.5575136, 0.5500251, 0.55861074, 0.5645397, 0.5729965, 0.5744037, 0.571469, 0.57452095, 0.72605574, 0.7569142, 0.766644, 0.71718484, 0.75709546, 0.7652448, 0.7177243, 0.75262654, 0.76512265, 0.72311664, 0.75608784, 0.77590334, 0.7335603, 0.75848365, 0.77585214, 0.9195556, 0.91209453, 0.9199063, 0.9201566, 0.9251048};


	//Compute UV mapping from XYZ points
	float light_u[numpos], light_v[numpos];
	for (int i=0;i<numpos;i++){
		//light_u[i] = 0.5 + atan2(light_z[i],light_x[i]) / (2*M_PI);
		//light_v[i] = 0.5 - asin(light_y[i]) / (M_PI);
		//cout << "#" << i << " u=" << light_u[i] << " v=" << light_v[i] << endl;
		//light_u[i]=(light_u[i]-1)*2;
		//light_v[i]=(light_v[i]-1)*2;
		light_u[i] = light_x[i];
		light_v[i] = light_y[i];
		//light_u[i]=-light_u[i];
		//light_v[i]=-light_v[i];

	}



	//string pathfiles = "/Volumes/MicroSD/RTI/Schilderij/aligned/";
	//string pathfiles = "/Volumes/MicroSD/RTI/calib_dome/jpeg-exports/";


	//Now we need to solve the system  C = au^2 + bv^2 + cuv + du + ev + f
	// a[0]*u[i]*u[i] + a[1]*v[i]*v[i] + a[2]*u[i]v[i] + a[3]*u[i] + a[4]*v[i] + a[5]-vals[i] = 0


	vector<cv::Mat> vMatRTI_R, vMatRTI_G, vMatRTI_B;

	vector<string> filesvector = util::folderFilesToVector(pathfiles, ".nef");
	cout << "filesvector.size()=" << filesvector.size() << " numpos=" << numpos << endl;

	if (filesvector.size()==0){
		filesvector = util::folderFilesToVector(pathfiles, ".jpg");
		cout << "filesvector.size()=" << filesvector.size() << " numpos=" << numpos << endl;
	}
	if (filesvector.size()==0){
		filesvector = util::folderFilesToVector(pathfiles, ".tif");
		cout << "filesvector.size()=" << filesvector.size() << " numpos=" << numpos << endl;
	}


	if (numpos!=filesvector.size()){
		cout << "Warning! filesvector size different from numpos size!" << endl;
		return;
	}


	cv::Mat matFirstImage;


	double dscale = double(scale)*0.01;
	cout << "dscale=" << dscale << endl;
	for (int i=0; i<numpos; i++){
		Mat imsm;
		//string fname = pathfiles + boost::str(boost::format("%03i") % i) +".jpg";

		string fname = pathfiles + "/" + filesvector[i];

		if (boost::contains(boost::filesystem::extension(fname), ".nef")){
			LibRaw iProcessor;
			libraw_processed_image_t *image; //Preallocate outside if loop so we can free it later

			int loadRawNok = iProcessor.open_file(fname.c_str()); //For opening from disk

			iProcessor.imgdata.params.output_color = 0; //(0,1,2,3,4,5)=(raw, sRGB, Adobe, Wide, ProPhoto, XYZ)
			iProcessor.imgdata.params.user_qual = 3; //AHD
			
			int rawDepthIproc = 8;
			iProcessor.imgdata.params.output_bps = rawDepthIproc;
			iProcessor.imgdata.params.user_flip = 0;
			iProcessor.imgdata.params.use_auto_wb = 1;

			// Let us unpack the image
			iProcessor.unpack();

			// call for postprocessing utility
			iProcessor.dcraw_process();

			int check_makemem;

			image = iProcessor.dcraw_make_mem_image(&check_makemem);

			int rawDepth = rawDepthIproc == 8 ? CV_8UC3 : CV_16UC3;

			Mat image_rawRGB(cv::Size(image->width, image->height), rawDepth, image->data, cv::Mat::AUTO_STEP); 

			if(image_rawRGB.data==NULL){
				string strErr = "Image image_rawRGB empty. Something went wrong from decoder to Mat!\nContact Developer";
				cout   << strErr << endl;
				return;
			}
			cvtColor(image_rawRGB, image_rawRGB, CV_RGB2BGR);  //Convert RGB to BGR	

			iProcessor.recycle();
			LibRaw::dcraw_clear_mem(image); //Very important to clear the memory

			imsm = image_rawRGB*0.75; //make a bit less brighter
		} else {
			imsm = imread(fname);
		}	










		cout  << "#" << i << " = " << fname << endl;
		cv::resize(imsm, imsm, Size(), dscale, dscale);
		//cvtColor(imsm,imsm, CV_BGR2GRAY);

/*
		imwrite("imsm_"+boost::lexical_cast<string>(i)+"A.tiff", imsm);

		//Now wiggle it in place
		if (i==0){
			imsm.copyTo(matFirstImage);
		} else {
			cv::Mat imsm4wiggle;
			imsm.copyTo(imsm4wiggle);
			imsm = wiggleInPlace(matFirstImage, imsm4wiggle, 5);
		}

		imwrite("imsm_"+boost::lexical_cast<string>(i)+"B.tiff", imsm);
*/

		vector<Mat> bgr_planes;
		split( imsm, bgr_planes );

		vMatRTI_B.push_back(bgr_planes[0]);
		vMatRTI_G.push_back(bgr_planes[1]);
		vMatRTI_R.push_back(bgr_planes[2]);
		
	}

	int cols    = vMatRTI_B[0].cols;
	int rows    = vMatRTI_B[0].rows;
	int numimgs = vMatRTI_B.size();

	cout << "#images=" << vMatRTI_B.size() << " w=" << cols << " h=" << rows << endl;

	Mat m_uv   = Mat(numimgs, 6, CV_32F, 1); // A 	//Mat(rows,cols,..)
	for (int i=0;i<numimgs;i++){
		m_uv.at<float>(i,0) = light_u[i]*light_u[i];
		m_uv.at<float>(i,1) = light_v[i]*light_v[i];
		m_uv.at<float>(i,2) = light_u[i]*light_v[i];
		m_uv.at<float>(i,3) = light_u[i];
		m_uv.at<float>(i,4) = light_v[i];
		m_uv.at<float>(i,5) = 1;
	}

	Mat m_uv_inv;
	invert(m_uv, m_uv_inv, DECOMP_SVD);

	/*
	ocl::oclMat om_uv_inv;
	om_uv_inv.upload(m_uv_inv);
	vector<ocl::oclMat> voMatRTI;
	for (int i=0;i<vMatRTI.size();i++){
		ocl::oclMat oMatRTI;
		oMatRTI.upload(vMatRTI[i]);
		voMatRTI.push_back(oMatRTI);
	}*/

	vector<Mat> matCoef_B = getCoefMats(vMatRTI_B, m_uv_inv);
	vector<Mat> matCoef_G = getCoefMats(vMatRTI_G, m_uv_inv);
	vector<Mat> matCoef_R = getCoefMats(vMatRTI_R, m_uv_inv);

	
	vector<cv::Mat> normalMap_B = calcSurfaceNormals(matCoef_B);
	vector<cv::Mat> normalMap_G = calcSurfaceNormals(matCoef_G);
	vector<cv::Mat> normalMap_R = calcSurfaceNormals(matCoef_R);
	writeNormalMap(normalMap_B);

	//int igaus = 9;
	//GaussianBlur( normalMap_B[0], normalMap_B[0], Size( igaus, igaus ), 0, 0 );
	//GaussianBlur( normalMap_B[1], normalMap_B[1], Size( igaus, igaus ), 0, 0 );


	cout << "Starting PhotometricStereo()" << endl;
	Mat depthMapFC = gradientToDepth_FC(normalMap_B[0], normalMap_B[1]);
	cout << "End PhotometricStereo()" << endl;
	timshow(depthMapFC, "map_FC");

	cout << "Starting PhotometricStereo()" << endl;
	Mat depthMap = gradientToDepth_WEI(normalMap_B[0], normalMap_B[1]);
	cout << "End PhotometricStereo()" << endl;
	timshow(depthMap, "map_WEI");

	cout << "Starting PhotometricStereo()" << endl;
	PhotometricStereo ps(normalMap_B[0].cols, normalMap_B[0].rows, 10);
	Mat depthMap2 = ps.getGlobalHeights(normalMap_B[0], normalMap_B[1]);
	cout << "End PhotometricStereo()" << endl;

	timshow(depthMap2, "map_Z");



	


	//timshow
	//imwrite("depthMap.tiff", depthMap);

/*
	vector<Mat> matDifMaps;
	matDifMaps.resize(3);
	matDifMaps[0] =  calcDiffuseMap(matCoef_B, normalMap_B);
	matDifMaps[1] =  calcDiffuseMap(matCoef_G, normalMap_G);
	matDifMaps[2] =  calcDiffuseMap(matCoef_R, normalMap_R);
	cv::Mat matDiffuse;
	cv::merge(matDifMaps, matDiffuse);
	imwrite("dif.tiff", matDiffuse);
	*/

	float min[6];
	float max[6];
	float scales[6];
	int biases[6];

	for (int i=0;i<6;i++){
		double minRGB[3];
		double maxRGB[3];
		cv::minMaxLoc(matCoef_R[i], &minRGB[0], &maxRGB[0]);
		cv::minMaxLoc(matCoef_G[i], &minRGB[1], &maxRGB[1]);
		cv::minMaxLoc(matCoef_B[i], &minRGB[2], &maxRGB[2]);
		min[i] = minRGB[0] < minRGB[1] ? minRGB[0] : minRGB[1];
		min[i] = minRGB[2] < min[i] ? minRGB[2] : min[i];
		max[i] = maxRGB[0] > maxRGB[1] ? maxRGB[0] : maxRGB[1];
		max[i] = maxRGB[2] > max[i] ? maxRGB[2] : max[i];
		// 8bit = (raw / scale) + bias
		scales[i] = (max[i]-min[i])/255.0;
		biases[i] = round(-min[i]/scales[i]);
		//cout << "#" << i << " min=" << min[i] << " max=" << max[i] << " scale=" << scales[i] << " bias=" << biases[i] << endl;

		matCoef_R[i] = matCoef_R[i]*(1.0/scales[i])+biases[i];
		matCoef_G[i] = matCoef_G[i]*(1.0/scales[i])+biases[i];
		matCoef_B[i] = matCoef_B[i]*(1.0/scales[i])+biases[i];
		matCoef_R[i].convertTo(matCoef_R[i],CV_8U);
		matCoef_G[i].convertTo(matCoef_G[i],CV_8U);
		matCoef_B[i].convertTo(matCoef_B[i],CV_8U);

		//Save stuff imwrite	
		timshow(matCoef_R[i], "coefmats_R_" + boost::lexical_cast<std::string>(i));
		timshow(matCoef_G[i], "coefmats_G_" + boost::lexical_cast<std::string>(i));
		timshow(matCoef_B[i], "coefmats_B_" + boost::lexical_cast<std::string>(i));
	}


	ofstream myfile;
	string fileptm = pathfiles + "/" + "rti(" + boost::lexical_cast<string>(scale) + "pct).ptm";
	myfile.open (fileptm.c_str());
	myfile << "PTM_1.2\n";
	//myfile << "PTM_FORMAT_LRGB\n";
	myfile << "PTM_FORMAT_RGB\n";
	myfile << boost::str(boost::format("%d") % cols) << "\n";
	myfile << boost::str(boost::format("%d") % rows) << "\n";
	myfile << boost::str(boost::format("%f") % scales[0]) << " ";
	myfile << boost::str(boost::format("%f") % scales[1]) << " ";
	myfile << boost::str(boost::format("%f") % scales[2]) << " ";
	myfile << boost::str(boost::format("%f") % scales[3]) << " ";
	myfile << boost::str(boost::format("%f") % scales[4]) << " ";
	myfile << boost::str(boost::format("%f") % scales[5]) << "\n";
	myfile << boost::str(boost::format("%d") % round(biases[0])) << " ";
	myfile << boost::str(boost::format("%d") % round(biases[1])) << " ";
	myfile << boost::str(boost::format("%d") % round(biases[2])) << " ";
	myfile << boost::str(boost::format("%d") % round(biases[3])) << " ";
	myfile << boost::str(boost::format("%d") % round(biases[4])) << " ";
	myfile << boost::str(boost::format("%d") % round(biases[5])) << "\n";
	


	
	unsigned char* num;
	for (int c=0;c<3;c++){
		for (int iy=rows-1; iy>=0; iy--){
			for (int ix=0; ix < cols; ix++){
				for (int i=0;i<6;i++){
					if (c==0){
						num = matCoef_R[i].ptr<uchar>(iy,ix);
					} else if (c==1){
						num = matCoef_G[i].ptr<uchar>(iy,ix);
					} else if (c==2){
						num = matCoef_B[i].ptr<uchar>(iy,ix);
					}
					myfile << *num;
				}
			}
		}
	}
	myfile.close();

	cout << "Ding, done." << endl;
	

	int coefs = matCoef_R.size();

	//First merge
	vector<cv::Mat> matCoef_RGB;
	matCoef_RGB.resize(coefs);
	for (int i=0;i<6;i++){
		vector<cv::Mat> matMerge;
		matMerge.push_back(matCoef_B[i]);
		matMerge.push_back(matCoef_G[i]);
		matMerge.push_back(matCoef_R[i]);
		cv::merge(matMerge, matCoef_RGB[i]);
	}

	Size origSize = matCoef_R[0].size();
	int maxdim = matCoef_R[5].rows > matCoef_R[5].cols ? matCoef_R[5].rows : matCoef_R[5].cols;
	int order = ceil(log2(double(maxdim)));
	cout << "maxdim=" << maxdim << " order=" << order << endl;
	Size maxSize(pow(2,order), pow(2,order));
	Size diffSize = maxSize - origSize;
	Size tileSize(this->quadsize, this->quadsize);
	int top   = diffSize.height*0.5;
	int bot   = maxSize.height - (top+origSize.height);
	int left  = diffSize.width*0.5;
	int right = maxSize.width - (left+origSize.width);


	for (int i=0;i<coefs;i++){

		copyMakeBorder(matCoef_RGB[i], matCoef_RGB[i], top, bot, left, right, BORDER_CONSTANT, Scalar(0) );
		//copyMakeBorder(matCoef_G[i], matCoef_G[i], top, bot, left, right, BORDER_CONSTANT, Scalar(0) );
		//copyMakeBorder(matCoef_B[i], matCoef_B[i], top, bot, left, right, BORDER_CONSTANT, Scalar(0) );
		//cout << "new size after copyMakeBorder:" << matCoef_RGB[i].size() << endl;
	}

	imwriteVectorResize(matCoef_RGB, 1, Size(this->quadsize,this->quadsize), 1);

	vector<string> vsXML;
	vsXML.resize(5000); // Preallocate a huge bunch of images

	cout << "..." << endl;
	int numids = goQuadrant(matCoef_RGB, 1, matCoef_RGB[0].size(), vsXML);
	
	//cout << "numids=" << numids << endl;



	ofstream fileXML;
	fileXML.open("info.xml");
	fileXML << "<!DOCTYPE Doc>\n";
	fileXML << "<MultiRes>\n";
	fileXML << "<Content type=\"RGB_PTM\">\n";
	fileXML << "<Size width=\"" << boost::lexical_cast<string>(origSize.width) << "\" height=\"" << boost::lexical_cast<string>(origSize.height) << "\" coefficients=\"6\"/>\n";
	fileXML << "<Scale>";
	for (int i=0;i<coefs;i++) fileXML << boost::lexical_cast<string>(scales[i]) << " ";
	fileXML << "</Scale>\n";
	fileXML << "<Bias>";
	for (int i=0;i<coefs;i++) fileXML << boost::lexical_cast<string>(biases[i]) << " ";
	fileXML << "</Bias>\n";
	fileXML << "</Content>\n";
	fileXML << "<Tree>" << boost::lexical_cast<string>(numids) << " 0\n";
	fileXML << boost::lexical_cast<string>(tileSize.width) << "\n";
	fileXML << boost::lexical_cast<string>(maxSize.width) << " " << boost::lexical_cast<string>(maxSize.height) << " 255\n";
	fileXML << "0 0 0\n";

	for (int i=0;i<numids;i++){
		fileXML << vsXML[i+1] << "\n";
	}

	fileXML << "</Tree>\n";
	fileXML << "</MultiRes>\n";
	fileXML.close();


	//makeWebRTI(matCoef_R,matCoef_G,matCoef_B, scales, biases);



	//exit(-1);
//	this->unow =0;
//	this->vnow =0;
	

}



void MainWindow::makeWebRTI(vector<cv::Mat> matCoef_R, vector<cv::Mat> matCoef_G, vector<cv::Mat> matCoef_B, float scales[6], int biases[6]){
/*
	int coefs = matCoef_R.size();

	//First merge
	vector<cv::Mat> matCoef_RGB;
	matCoef_RGB.resize(coefs);
	for (int i=0;i<6;i++){
		vector<cv::Mat> matMerge;
		matMerge.push_back(matCoef_B[i]);
		matMerge.push_back(matCoef_G[i]);
		matMerge.push_back(matCoef_R[i]);
		cv::merge(matMerge, matCoef_RGB[i]);
	}

	Size origSize = matCoef_RGB[0].size();
	int cols = origSize.width;
	int rows = origSize.height;
	

	//First get the best fitting 2^x tile size
	int maxdim = rows > cols ? rows : cols;
	int order = ceil(log2(double(maxdim)));
	cout << "maxdim=" << maxdim << " order=" << order << endl;
	Size maxSize(pow(2,order), pow(2,order));
	Size tileSize(256,256);
	cout << "newSize=" << maxSize << " tileSize=" << tileSize << endl;

	Size diffSize = maxSize - origSize;
	int top   = diffSize.height*0.5;
	int bot   = maxSize.height - (top+origSize.height);
	int left  = diffSize.width*0.5;
	int right = maxSize.width - (left+origSize.width);


	for (int i=0;i<coefs;i++){

		copyMakeBorder(matCoef_RGB[i], matCoef_RGB[i], top, bot, left, right, BORDER_CONSTANT, Scalar(0) );
		//copyMakeBorder(matCoef_G[i], matCoef_G[i], top, bot, left, right, BORDER_CONSTANT, Scalar(0) );
		//copyMakeBorder(matCoef_B[i], matCoef_B[i], top, bot, left, right, BORDER_CONSTANT, Scalar(0) );
		cout << "new size after copyMakeBorder:" << matCoef_RGB[i].size() << endl;
	}

	

	
	//int numscales = order-7;
	//cout << "numscales=" << numscales << endl;
	int numtiles=0;
	for (int io=8;io<=order;io++){
		//int orderNow = 8+numscales;
		cout << "Now at order #" << io << "/" << order << endl;
		Size sizeNow(pow(2,io),pow(2,io));
		cout << "sizeNow=" << sizeNow << endl;

		//Now this has to be divided into tiles of 256
		for (int x=0;x<sizeNow.width;x+=256){
			for (int y=0;y<sizeNow.height;y+=256){
				numtiles++;
				Rect roi = Rect(x, y, 256, 256);
						
				//cout << "roi=" << roi << endl;
				cout << numtiles << " -1 -1 -1 -1 -1 " << boost::lexical_cast<string>(roi.width) << " ";
				cout << "1 " << boost::lexical_cast<string>(x/(double)sizeNow.width) << " " << boost::lexical_cast<string>(y/(double)sizeNow.height) << " 0 ";
				cout << boost::lexical_cast<string>((x+roi.width)/(double)sizeNow.width) << " " << boost::lexical_cast<string>((y+roi.height)/(double)sizeNow.height) << " 1\n";

				fileXML << numtiles << " -1 -1 -1 -1 -1 " << boost::lexical_cast<string>(roi.width) << " ";
				fileXML << "1 " << boost::lexical_cast<string>(x/(double)sizeNow.width) << " " << boost::lexical_cast<string>(y/(double)sizeNow.height) << " 0 ";
				fileXML << boost::lexical_cast<string>((x+roi.width)/(double)sizeNow.width) << " " << boost::lexical_cast<string>((y+roi.height)/(double)sizeNow.height) << " 1\n";

				for (int i=0;i<coefs;i++){
					Mat matNow;
					cv::resize(matCoef_RGB[i], matNow, sizeNow);
					Mat matTile = matNow(roi);
					string filename = boost::lexical_cast<string>(numtiles) + "_" + boost::lexical_cast<string>(i+1) + ".jpg";
					imwrite(filename,matTile);

				}
			}
		}
	}
	


	//1 -1  1  2  3  4 256 1 0   0   0   1   1   1
	//2 0  -1 -1 -1 -1 256 1 0   0   0   0.5 0.5 1
	//3 0  -1 -1 -1 -1 256 1 0.5 0   0   1   0.5 1
	//4 0  -1 -1 -1 -1 256 1 0   0.5 0   0.5 1   1
	//5 0  -1 -1 -1 -1 256 1 0.5 0.5 0   1   1   1
	
*/

}


vector<cv::Mat> MainWindow::calcSurfaceNormals(vector<cv::Mat> matCoefMaps){
	int numimgs = matCoefMaps.size();
	int cols    = matCoefMaps[0].cols;
	int rows    = matCoefMaps[0].rows;
	int pixels = cols * rows;

	vector<Mat> normalMap;
	normalMap.resize(2);
	normalMap[0] = Mat(matCoefMaps[0].size(), CV_32F, 1);
	normalMap[1] = Mat(matCoefMaps[0].size(), CV_32F, 1);

	float a0, a1, a2, a3, a4, a5;
	//float * a0, * a1,
	for (int k=0; k< pixels; k++) {
		a0 = matCoefMaps[0].at<float>(k); //.ptr<float> TODO
		a1 = matCoefMaps[1].at<float>(k);
		a2 = matCoefMaps[2].at<float>(k);
		a3 = matCoefMaps[3].at<float>(k);
		a4 = matCoefMaps[4].at<float>(k);
		a5 = matCoefMaps[5].at<float>(k);
		float denum = 4.0*a0*a1-a2*a2;

		//cout << a0 << " " << a1 << " " << a2 << " " << a3 << " " << a4 << " " << a5 << endl;
		//cout << "denum=" << denum;

		float xn = (a2*a4 - 2.0*a1*a3)/denum;
		float yn = (a2*a3 - 2.0*a0*a4)/denum;

		float ln = sqrt(xn*xn + yn*yn);//length of vector
		xn /= ln;
		yn /= ln;

		normalMap[0].at<float>(k) = xn;
		normalMap[1].at<float>(k) = yn;
		//cout << "x=" << normalMap[0].at<float>(k) << " y=" << normalMap[1].at<float>(k) << endl;

	}

	timshow(normalMap[0], "norm0");
	timshow(normalMap[1], "norm1");

	return normalMap;
}



std::vector<cv::Mat> MainWindow::getCoefMats(vector<cv::Mat> vMatImages, cv::Mat m_uv_inv){
	int numimgs = vMatImages.size();
	int cols    = vMatImages[0].cols;
	int rows    = vMatImages[0].rows;
	int pixels = cols * rows;

	vector<Mat> matCoefMaps;
	matCoefMaps.resize(6);
	
	for (int i=0;i<matCoefMaps.size();i++){
		matCoefMaps[i] = Mat(rows, cols, CV_32F, 1 );
		//matCoefMaps[i] = Mat(pixels, 1, CV_32F, 1 );
	}
	Mat m_coef = Mat(6, 1, CV_32F, 1); // x
	
	
	//Loop over the pixels (x,y)
	//for (int x=0; x<cols; x++){
	//	cout << (k*100.0)/mp << "%" << endl;
	//	for (int y=0; y<rows; y++){
	for (int k=0; k< pixels; k++) {
		Mat m_vals = Mat(numimgs, 1, CV_32F, 1); // B
		for (int i=0;i<numimgs;i++){
			m_vals.at<float>(i) = (float)vMatImages[i].at<uchar>(k); // Pointer (.ptr) is faster than at (.at)
		}

		m_coef = m_uv_inv * m_vals; //x=A^-1 * b
		matCoefMaps[0].at<float>(k) = m_coef.at<float>(0);
		matCoefMaps[1].at<float>(k) = m_coef.at<float>(1);
		matCoefMaps[2].at<float>(k) = m_coef.at<float>(2);
		matCoefMaps[3].at<float>(k) = m_coef.at<float>(3);
		matCoefMaps[4].at<float>(k) = m_coef.at<float>(4);
		matCoefMaps[5].at<float>(k) = m_coef.at<float>(5);

	}
	//for (int i=0;i<matCoefMaps.size();i++){
	//	matCoefMaps[i].reshape(0,rows);	
	//}
	

	return matCoefMaps;
}

QImage MainWindow::Mat2QImage(const cv::Mat3b &src) {
	QImage dest(src.cols, src.rows, QImage::Format_ARGB32);
	for (int y = 0; y < src.rows; ++y) {
		const cv::Vec3b *srcrow = src[y];
		QRgb *destrow = (QRgb*)dest.scanLine(y);
		for (int x = 0; x < src.cols; ++x) {
			destrow[x] = qRgba(srcrow[x][2], srcrow[x][1], srcrow[x][0], 255);
		}
	}
	return dest;
}

/*
void MainWindow::keyPressEvent(QKeyEvent *event){
	int key_press;
	if(event->key() == Qt::Key_Up) {
		key_press = -1;
	} else if(event->key() == Qt::Key_Down) {
		key_press = 1;
	} else if(event->key() == Qt::Key_Enter) {
		key_press = 3; // But this part of the code is not reached on enter key press
	} else {
		QWidget::keyPressEvent(event);
	}
}*/

/*
// Implement in your widget
void MainWindow::mouseMoveEvent(QMouseEvent *event){
    //qDebug() << event->pos();
    double x = event->pos().x();
    double y = event->pos().y();
    //Normalize to window size approx 700x700
    //Now make 0,0 the middle
    //x-=350;
    //y-=350;

    x=x/(700);
    y=y/(700);
    
    
    x = x < 0 ? 0 : x;
    y = y < 0 ? 0 : y;
    x = x > 1 ? 1 : x;
    y = y > 1 ? 1 : y;
    x=1-x;
    cout << "x,y=" << x << "," << y << endl;
    this->unow = x;
	this->vnow = y;
	//on_pushButton_clicked();

}*/

void MainWindow::on_pushButton_2_clicked(){
	//emit sigTest();
	cout << "on_pushButton_2_clicked()" << endl;







	QString dir = QFileDialog::getExistingDirectory(this, tr("Open directory with images"), "", 
									     QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);
	string strSelected = dir.toStdString();
	cout << "Selected folder:" << strSelected << endl;
	if (strSelected.empty()){
		cout << "Selection empty." << endl;
		return;
	}

	QInputDialog* inputDialog = new QInputDialog();

	bool ok;
	int scale = QInputDialog::getInt(this, tr("Computation scale (%)"),
		tr("Scale [0-200]%:"), 50, 1, 200, 1, &ok);

	if (ok){
		processFolder(strSelected, scale);
	}


}


bool doMkDir(string thisDir){
	if ( !fs::exists( thisDir ) ) {
		try{
			return boost::filesystem::create_directory(thisDir);
		} catch (std::exception& e) {
			return false;
		}
	}
	return true;
}

void MainWindow::on_pushButton_clicked(){
	cout << "on_pushButton_clicked()" << endl;
/*
	emit sigTest();
	return;
*/
/*
	vector<cv::Mat> matImages;
	matImages.push_back(imread("/Users/tzaman/Dropbox/code/rti/doc/test/1_1.jpg"));
	matImages.push_back(imread("/Users/tzaman/Dropbox/code/rti/doc/test/1_2.jpg"));
	matImages.push_back(imread("/Users/tzaman/Dropbox/code/rti/doc/test/1_3.jpg"));
	matImages.push_back(imread("/Users/tzaman/Dropbox/code/rti/doc/test/1_4.jpg"));
	matImages.push_back(imread("/Users/tzaman/Dropbox/code/rti/doc/test/1_5.jpg"));
	matImages.push_back(imread("/Users/tzaman/Dropbox/code/rti/doc/test/1_6.jpg"));

	vector<double> rti_scales;
	rti_scales.push_back(1.03421378);
	rti_scales.push_back(0.953444779);
	rti_scales.push_back(0.378294528);
	rti_scales.push_back(0.626534283);
	rti_scales.push_back(0.7757743);
	rti_scales.push_back(1.04884148);
	vector<double> rti_bias;
	rti_bias.push_back(170);
	rti_bias.push_back(181);
	rti_bias.push_back(143);
	rti_bias.push_back(133);
	rti_bias.push_back(117);
	rti_bias.push_back(-5);


	//<Size width="1024" height="1024" coefficients="6"/>
	//<Scale>1.03421378 0.953444779 0.378294528 0.626534283 0.7757743 1.04884148 </Scale>
	//<Bias>170 181 143 133 117 -5 </Bias>	

	imageAreas[0]->openImage(0, matImages, rti_scales, rti_bias);

	cout << "DEV MODE RETURNING!" << endl;
	return;
*/




	this->lightidx=0;


	//Prompt to select target directory
	QString dirSelect = QFileDialog::getExistingDirectory(this, tr("Open Directory"), "/Users/tzaman/Desktop/", 
										     QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);

	bool okInputText;
	QInputDialog* inputDialog = new QInputDialog();
	inputDialog->setOptions(QInputDialog::NoButtons);

	//Prompt to type in book id
	QString text =  inputDialog->getText(NULL ,"Name", "Name:", QLineEdit::Normal, "noname", &okInputText);

	string dirname = QDateTime::currentDateTime().toString("yyyyMMdd").toStdString() + "_" + text.toStdString();
	string fulldir = dirSelect.toStdString() + "/" + dirname + "/";

	doMkDir(fulldir);

	QString qstrNumLeds = QString::fromStdString(boost::str(boost::format("%03d") % numleds));

	for (int i=0;i<numleds;i++){
		string cmd = "$$," + boost::lexical_cast<std::string>(this->lightidx) + "\n";
		this->setControlUnit->serialWrite((string)cmd); //next LED plz
		
		QString txt = QString::fromStdString(boost::str(boost::format("%03d") % this->lightidx)) + "/" + qstrNumLeds;

		ui.label->setText(txt);

		//string fname = "/Users/tzaman/Desktop/RAWLOG/"+ QDateTime::currentDateTime().toString("yyyyMMdd_hh-mm-ss.zzz").toStdString() + "_" + boost::str(boost::format("%03d") % this->lightidx)+".nef";
		string fname = fulldir + boost::str(boost::format("%03d") % this->lightidx)+".nef";
		gpCams[0]->slotCamCapture(0,fname);

		//Iternate to next LED
		this->lightidx++;

		QApplication::processEvents();
		//sleep(4);
	}

	ui.label->setText("Ready");

	
	



/*
	
	Mat matSynthetic = Mat(matCoefMaps[0].rows, matCoefMaps[0].cols, CV_32F, 1 );
	
	//matSynthetic = matCoefMaps[0]*unow*unow + matCoefMaps[1]*vnow*vnow + matCoefMaps[2]*unow*vnow + matCoefMaps[3]+unow + matCoefMaps[4]+vnow + matCoefMaps[5];
	
	Mat m0 = matCoefMaps[0]*(unow*unow);
	Mat m1 = matCoefMaps[1]*(vnow*vnow);
	Mat m2 = matCoefMaps[2]*(unow*vnow);
	Mat m3 = matCoefMaps[3]*(unow);
	Mat m4 = matCoefMaps[4]*(vnow);
	Mat m5 = matCoefMaps[5];

	add(matSynthetic, m0, matSynthetic);
	add(matSynthetic, m1, matSynthetic);
	add(matSynthetic, m2, matSynthetic);
	add(matSynthetic, m3, matSynthetic);
	add(matSynthetic, m4, matSynthetic);
	add(matSynthetic, m5, matSynthetic);




	//matSynthetic = matCoefMaps[5];
	matSynthetic.convertTo(matSynthetic,CV_8U);
	
	cvtColor(matSynthetic,this->matRGB, CV_GRAY2BGR);


	//imwrite("/Users/tzaman/Desktop/test.tif", this->matRGB);


	QImage image = Mat2QImage(this->matRGB);
	QGraphicsScene *scene = new QGraphicsScene (this);
	QPixmap pixmap = QPixmap::fromImage(image);
	scene->addPixmap(pixmap);
	//delete QGraphicsViewscene();
	ui.graphicsView->setScene(scene);
	ui.graphicsView->update();
	//setImage(matRGB,0);

*/

}


void MainWindow::setImage(cv::Mat matImage, int i){
	cout << "setImage()" << endl;
	//Clear the item in the scene, if there is any
	//scene[i].clear();
	//scene[i] = new QGraphicsScene();

	//gView[i]->setScene(&scene[i]);
	//QImage image = Mat2QImage(matImage);
	//pixItem[i] = new QGraphicsPixmapItem(QPixmap::fromImage(image));
	//scene[i].addItem(pixItem[i]);
	//gView[i]->fitInView(scene[i].itemsBoundingRect(), Qt::KeepAspectRatio);
	//gView[i]->setStyleSheet("background: transparent");
}

void MainWindow::timshow(cv::Mat matImage,  std::string description){

	vector<Mat> planesRef;
	split( matImage, planesRef );

	//namedWindow("timshow()", cv::WINDOW_NORMAL);  

	for (int i=0; i< planesRef.size(); i++){
		Mat planeNow, timSave;
		planesRef[i].convertTo(planeNow, CV_32FC1);

		double minVal, maxVal; 
		int minIdx, maxIdx;
		minMaxIdx(planeNow, &minVal, &maxVal, &minIdx, &maxIdx);

		timSave = ((planeNow-minVal)*1.0/(maxVal-minVal));
		timSave.convertTo(timSave, CV_16UC1,65535.0);
		imwrite("timshow_" + description + "_" +boost::lexical_cast<std::string>(i)+".tiff", timSave);
	
		//imshow("timshow()", timSave);
		//waitKey(0);
	}

}


void MainWindow::slotNewGPImage(CameraFile * g_gpFile, int filetype, int camidx){ //New image received from GPhoto
	cout << "slotNewGPImage(..," << filetype << "," << camidx << ")" << endl;


	

	//if (this->lightidx >= this->numleds){
	//	cout << "All done!" << endl;
	//	return;
	//}
/*
	string cmd = "$$," + boost::lexical_cast<std::string>(this->lightidx) + "\n";
	this->setControlUnit->serialWrite((string)cmd); //next LED plz
	//usleep(100*1000); //micros
	emit sigGPcapture(0); //Capture next image
	//sleep(1);*/

}

//Set+Arduino related
void MainWindow::slotSetIsReady(bool isok){
	cout << "slotSetIsReady(" << isok << ")" << endl;

}

void MainWindow::slotSetIsConnected(bool isconnected){
	cout << "slotSetIsConnected(" << isconnected << ")" << endl;
}
























