/****************************************************************************
**
** Copyright (C) 2014 Tim Zaman.
** Contact: http://www.timzaman.com/ , timbobel@gmail.com
**
**
****************************************************************************/


#include "imagearea.h"

using namespace std;
using namespace cv;

ImageArea::ImageArea(QWidget *parent) : QWidget(parent){
	setAttribute(Qt::WA_StaticContents);
	setMouseTracking(true);
	//this->setFocusPolicy(Qt::StrongFocus);
	//this->setFocus(Qt::OtherFocusReason);

	cout << "Initialized new image area" << endl;

	//cropRect = Rect(Point(0,0), Point(0,0));

	//this->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);

	scribbling=false;
	scribbletype=0;
	fixCropSize=false;
	keyPressedNow = -1;

	this->toolPoints.resize(8);
	this->cropRect = Rect(Point(0,0),Point(100,100));

	this->cpView = Point(100,100); //test
	this->pctZoomRequest = -1; //This is the zooming that should be added
	
	this->rotationRequested=0;
	this->rotationApplied  =0;

	rotateAllowed =true;
	this->processID=-1;
	this->camidx=-1;
	this->flipflagRequested=0; //0=0,1=CW,2=CCW,3=180
	this->flipflagApplied  =0;

	this->rotationMax = 6.0; //degrees

	this->useRotext=false;
	this->use=true;
	//image.setDevicePixelRatio(2.0);
	//setDevicePixelRatio(2.0);

	this->fixAspRatio=false;
	this->szFixAspectRatio=Size(5,4);
	
	this->allowBlurEstimation = false;
	this->viewingBlur = false;
	this->madeBlur = false;

}


QImage ImageArea::Mat2QImage(const cv::Mat3b &src) {
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


cv::Point ImageArea::rotPoint(int rotflag, cv::Point ptNow, cv::Size sizeIm){
	Point ptNew=ptNow;
	//1=CW, 2=CCW, 3=180
	if (rotflag == 1){ //CW
		ptNew.x=sizeIm.height-1-ptNow.y;
		ptNew.y=ptNow.x;
	} else if (rotflag == 2) { //CCW
		ptNew.x=ptNow.y;
		ptNew.y=sizeIm.width-1-ptNow.x;
	} else if (rotflag ==3){ //180
		ptNew.x=sizeIm.width-1-ptNow.x;
		ptNew.y=sizeIm.height-1-ptNow.y;				
	} else if (rotflag != 0){ //if not 0,1,2,3:
		cout << "Unknown rotation flag(" << rotflag << ")!" << endl;
	}
	return ptNew;
}

cv::Rect ImageArea::rotRect(int rotflag, cv::Rect cropBox, cv::Size sizeIm){
	Rect rectNew = Rect(rotPoint(rotflag, cropBox.tl(),sizeIm),rotPoint(rotflag, cropBox.br(),sizeIm));
	//cout << "Rotating rect flag " << rotflag << " from rect: " << cropBox << " to:" << rectNew << " with image size" << sizeIm << endl;
	return rectNew;
}

void ImageArea::rot90(cv::Mat &matImage, int rotflag){
	//1=CW, 2=CCW, 3=180
	if (rotflag == 1){
		transpose(matImage, matImage);	
		flip(matImage, matImage,1);	//transpose+flip(1)=CW
	} else if (rotflag == 2) {
		transpose(matImage, matImage);	
		flip(matImage, matImage,0);	//transpose+flip(0)=CCW		
	} else if (rotflag ==3){
		flip(matImage, matImage,-1);	//flip(-1)=180			
	} else if (rotflag != 0){ //if not 0,1,2,3:
		cout << "Unknown rotation flag(" << rotflag << ")" << endl;
	}
}

void ImageArea::allowRotation(bool allowflag){
	this->rotateAllowed = allowflag;
	if (!rotateAllowed){
		this->rotationRequested = 0;
	}
}


void ImageArea::minglePixel(cv::Vec3b &pixelElement){
	//Changes the pixel value in Vec3b for the cropping stripes and gui elements
	//This function is entirely for aesthetics
	int rgb = (pixelElement[0]+pixelElement[1]+pixelElement[2])/3;
	uchar pxVal=255;
	if (rgb >150){ //bright already -> make 
		pxVal=0;
	}
	pixelElement[0] = pxVal;
	pixelElement[1] = pxVal;
	pixelElement[2] = pxVal;
}


cv::Size ImageArea::getFitSize(cv::Size sizeIn, cv::Size sizeOut){
	double dAspRat    = double(sizeIn.width)/double(sizeIn.height);  //Aspect ratio of the image itself
	double dAspRatBox = double(sizeOut.width)/double(sizeOut.height); //Aspect ratio of the box 
	cv::Size sizeFit;
	if (dAspRat>dAspRatBox){ //Match width
		sizeFit = Size(sizeOut.width, floor(sizeOut.height*dAspRatBox/dAspRat));
	} else { //Match height
		sizeFit = Size(floor(sizeOut.width*dAspRat/dAspRatBox), sizeOut.height);
	}
	return sizeFit;
}


cv::Rect ImageArea::constrainRectInSize(cv::Rect rCrop, cv::Size sImage){
	Rect rImage(Point(0,0), sImage);
	Rect rIntersection =  rCrop & rImage;
	return rIntersection;
}



void ImageArea::reduceNonzero(cv::Mat matRot, cv::Mat & matCol){
	//reduction algo only ignorance for 0 values
	matCol = Mat::zeros(Size(1,matRot.rows), CV_8UC1);
	for(int y=0; y < matRot.rows; y++){  //vertical loop
		int iSum=0;
		int iNum=0;
		for(int x=0;  x < matRot.cols; x++){
			int valNow = matRot.at<uchar>(y,x);
			if (valNow){ //if nonzero (nonmasked)
				iSum+=valNow;
				iNum++;
			}
		}
		if (iNum){ //Beware for div by zero
			matCol.at<uchar>(y,0) = iSum/iNum;
		}
	}
}


double calcMedian(vector<double> scores){ //calculates median
  double median=0;

  size_t size = scores.size();

  if (scores.size()==0) {
  	cout << "Warning! Empty vector in calcMedian!" << endl;
  	return 0;
  }

  //for (int i=0;i<size;i++){
  //	cout << "entry " << i << " = " << scores[i] << endl;
  //}

  sort(scores.begin(), scores.end());

  if (size  % 2 == 0){
      median = (scores[size / 2 - 1] + scores[size / 2]) / 2;
  } else {
      median = scores[size / 2];
  }

  return median;
}

double ImageArea::rotateToText(cv::Mat matImage){ //Alias rotext 
	cout <<"rotateToText()" << endl;
	//for (int i=1;i<239;i++){
	//string filename = "/media/tzaman/DATA/wetransfer-41971d/" + boost::str(boost::format("%04i") % i) + ".jpg";
	//cout << filename << endl;
	//cv::Mat matImage = imread(filename);

	double resfact=0.35;
	Mat matSmall;
	cvtColor(matImage, matSmall, cv::COLOR_BGR2GRAY);
	cv::resize(matSmall, matSmall, Size(), resfact, resfact, INTER_AREA);

	if (matSmall.cols < 100 || matSmall.rows < 100){
		cout << "Image too small for rotext, setting rotation to 0 degrees." << endl;
		return 0;
	}



/*
	int circleRadius;// = matSmall.cols<matSmall.rows?floor(matSmall.cols*0.5):floor(matSmall.rows*0.5);

	double columnfact=1.0;
	if (useRotextOpt==1){ //3 column page
		//columnfact stays 1.0, default.
	} else if (useRotextOpt==2){ //2 column page
		columnfact=0.4;
	} else if (useRotextOpt==3){ //3 column page
		columnfact=0.25;
	}


	Rect rCropSquare;
	if (matSmall.cols<matSmall.rows){ //Minder breed, dus crop hoogte ervanaf
		circleRadius=floor(matSmall.cols*0.5) * columnfact;
		int cDif = matSmall.rows - matSmall.cols;
		rCropSquare = Rect(Point(0,floor(cDif*0.5)), Size(circleRadius*2,circleRadius*2));
	} else { //breder, columnfactor is not used here
		int cDif = matSmall.cols - matSmall.rows;
		circleRadius=floor(matSmall.rows*0.5);
		rCropSquare = Rect(Point(floor(cDif*0.5),0), Size(circleRadius*2,circleRadius*2));
	}*/


	double angleTestMax=3.0; // was 3
	double angleTestStep=0.15;           
	

	vector<double> vecBestAngles;
	vector<Rect> vecRects;


	//Now make all the rectangles we want to crop.
	int numrects_1dim = 3; //Amount of rectangles in 1 dimention
	int rectwidth = matSmall.rows > matSmall.cols ? floor(matSmall.cols/numrects_1dim) : floor(matSmall.rows/numrects_1dim);
	int circleRadius = floor(rectwidth*0.5);

	for(int x=0; x<numrects_1dim; x++){
		for (int y=0; y<numrects_1dim; y++){
			vecRects.push_back(Rect(Point(x*rectwidth,y*rectwidth),Size(rectwidth,rectwidth)) );
		}
	}


	Mat matSmallMask =  Mat(vecRects[0].size(), CV_8UC1, Scalar(0));;
	circle(matSmallMask, Point(matSmallMask.cols,matSmallMask.rows)*0.5, circleRadius-1, Scalar(255),-1); //Make sure the circle is a bit bigger
	//imwrite("/Users/tzaman/Desktop/rots/matSmallMask.jpg", matSmallMask);

	for (int i=0;i<vecRects.size();i++){

		Mat matSmallCrop = matSmall(vecRects[i]);



		

		double maxVal=0, minVal = 1337;//, bestAngle=0;
		double bestAngleNow=0;
		for (double a=-angleTestMax; a<angleTestMax; a+=angleTestStep){
			//cout << "angle=" << a << endl;
			Mat matRot;
			rotate(matSmallCrop, a, matRot);
			cv::bitwise_and(matRot, matSmallMask, matRot);
			//todo crop out a part
			

			Mat matCol;
			//cv::reduce(cross_corr, cross_corr_summed, 0, CV_REDUCE_SUM, CV_32S);
			//cv::reduce(matRot, matCol, 1, CV_REDUCE_AVG, -1); //0=row, 1=col

			reduceNonzero(matRot, matCol);

			// Crop off top and bottom
			Rect rectCrop = Rect(Point(0,2), Size(1,matCol.rows-4));
			matCol = matCol( rectCrop );


			cv::Scalar meanNow, stdevNow;
			cv::meanStdDev(matCol, meanNow, stdevNow);
			double mean = meanNow.val[0];
			double stdev  = stdevNow.val[0];
			//cout << "mean=" << mean << " std=" << std << endl;
			//cout << stdev << "," ;
			if (stdev < minVal){
				minVal = stdev;
			}
			if (stdev > maxVal){
				maxVal = stdev;
				//cout << maxVal << endl;
				//if (maxVal>3){ //We need some constrast
					bestAngleNow=a;
					
				//}
			}

			//string strText = boost::str(boost::format("%0d") % i) + "_" + boost::str(boost::format("%.1f") % a) + "deg m=" + boost::str(boost::format("%.1f") % mean) + " std=" + boost::str(boost::format("%.1f") % stdev);
			//imwrite("/Users/tzaman/Desktop/rots/"+strText+".jpg", matRot);


			/*
			Mat matGUI = repeat(matCol,1,400);
			string strText = boost::str(boost::format("%.1f") % a) + "deg m=" + boost::str(boost::format("%.1f") % mean) + " std=" + boost::str(boost::format("%.1f") % stdev);
			putText(matGUI, strText, Point(100,100), FONT_HERSHEY_PLAIN, 1, Scalar::all(255), 1, 8); 
			namedWindow( "0", 0 ); 
			imshow( "0", matRot );// waitKey(0);
			namedWindow( "1", 0 ); 
			imshow( "1", matGUI ); waitKey(0);
			*/
		}
		double dVal = maxVal-minVal;
		if (maxVal > 4 && dVal > 2){
			//cout << "Added angle " << bestAngleNow << " with maxval=" << maxVal << " and minval=" << minVal << " dVal=" << dVal << endl;
			vecBestAngles.push_back(bestAngleNow);
		}

	}


	double bestAngle = calcMedian(vecBestAngles);


	//cout << "best angle=" << bestAngle << " degrees. with value (stdev)=" << maxVal << endl;
	cout <<"rotateToText() result =" << bestAngle << " degrees." << endl;
	return bestAngle;


	/*namedWindow( "2", 0 ); 
	imshow( "2", matImage ); waitKey(100);
	rotate(matImage, bestAngle, matImage);
	namedWindow( "2", 0 ); 
	imshow( "2", matImage ); waitKey(100);*/

}


void ImageArea::checkCropboxEdges(cv::Rect rCrop, cv::Size imSize){

	double edgePercentage = 0.005; //0.02 means the crop should not be close to 2% of the edges. 0.005 means 0.5%
	bool throwWarning=false;
	if (rCrop.x < double(imSize.width)*edgePercentage){
 		throwWarning=true;
	}
	if (rCrop.y < double(imSize.height)*edgePercentage){
 		throwWarning=true;
	}
	if (rCrop.br().x > double(imSize.width)*(1.0-edgePercentage)){
 		throwWarning=true;
	}
	if (rCrop.br().y > double(imSize.height)*(1.0-edgePercentage)){
 		throwWarning=true;
	}
	if (throwWarning){
		QMessageBox messageBox;
		messageBox.information(0, "Warning", QString::fromStdString("Just a warning: the crop is very close to the edge!"));
		messageBox.setFixedSize(600,200);
	}
}



bool ImageArea::openImage(int idNow, std::vector<cv::Mat> matImages, std::vector<double> rti_scales_now, std::vector<double> rti_bias_now){

	if (this->use == false){
		return false;
	}

	for (int i=0; i<6;i++){
		matImages[i].convertTo(matImages[i],CV_32FC3);
	}


	this->rti_images = matImages;
	this->rti_scales = rti_scales_now;
	this->rti_bias   = rti_bias_now;

	cv::Mat matImage;// = matImages[0];
	matImages[0].convertTo(matImage,CV_8UC3);

	if (idNow < this->processID) {
		cout << "openImage() will not update since idNow(" << idNow << ") < this->processID(" << this->processID << ")" << endl;
		return false;
	}

	this->processID = idNow; //Assign unique process ID

	if (matImage.data==NULL){
		cout << "Error in ImageArea::openImage(), data found to be NULL. Will not show image." << endl;
		return false;
	}

	this->madeBlur=false;



/*
	if (myRotReq<-rotationMax || myRotReq >rotationMax){
		cout << "Rotation request out of bounds. reverting to 0 rotation." << endl;
		myRotReq=0;
	}*/

	//TODO: make sure the cropRect does not extend the matImage!


	cout << "New image loaded:" << matImage.cols << "x" << matImage.rows << endl;


	//If we have received a crop suggestion (f.e. from autocrop) we set it:
	//bool use_rCrop = false;
	bool doRotateCrop=false; //For refresh flip
	/*if (rCrop.width>0 && rCrop.height>0){
		this->cropRect = rCrop; 
		this->rotationRequested =myRotReq;
		doRotateCrop=true; //if autocrop (suggestion) it should also be rotated too
		//use_rCrop = true;

		//Now check if the cropbox does not pass over the edges
		checkCropboxEdges(rCrop,matImage.size());
	}*/



	//TODO: check data validity

	//this->cropRect = constrainRectInSize(this->cropRect, matImage.size());
	




	//Check if the new matImage can take the cropBox. if not: draw new smaller cropbox
	if ( (matImage.rows < this->matOriginal.rows ) || ( matImage.cols < this->matOriginal.cols) ){ //on first load
		double pctZoomMinGuiW = double(this->width()) / double(matImage.cols);
		double pctZoomMinGuiH = double(this->height()) / double(matImage.rows);
		double pctZoomMin = pctZoomMinGuiH < pctZoomMinGuiW ? pctZoomMinGuiH : pctZoomMinGuiW;
		this->pctZoomRequest =pctZoomMin;// pctZoomMinGui;
		this->cpView = Point(matImage.cols*0.5, matImage.rows*0.5);
		//if (!use_rCrop){
		//	this->cropRect = Rect(Point(matImage.cols*0.1,matImage.cols*0.1),Point(matImage.cols*0.9,matImage.rows*0.9));
		//}
	}

	if (matImage.depth() == CV_8U){
		//cout << "image is for left as is (8bit).." << endl;
		matImage.copyTo(this->matOriginal);
	} else if (matImage.depth() == CV_16U){
		//cout << "image is converted to 8 bit.." << endl;
		matImage.convertTo(this->matOriginal, CV_8U, 1.0/255.0);
	}


	



	if (this->pctZoomRequest != -1){ //not on first load
		Size newSize = Size(this->matOriginal.cols*pctZoomRequest, this->matOriginal.rows*pctZoomRequest);
		cv::resize(this->matOriginal, this->matScaled, newSize,0,0, INTER_AREA);
	}

	this->flipflagRequested=0; //I reset the flipping with each image to 0.

	this->rotationApplied = 0;

	this->flipflagApplied = 0;

	//refreshFlip(false);

	drawImage(false, false);
	return true;
}


void ImageArea::refreshFlip( bool flipCrop){ //flipCrop default true (in .h)
	if (flipflagRequested==flipflagApplied) return;
	if (this->matOriginal.data==NULL) return;
	//1 CW
	//2 CCW
	//3 180
	int flipNow=0;
	if (flipflagApplied==0){
		flipNow=flipflagRequested;
	} else if (flipflagApplied==1){ //CW is applied.
		if (flipflagRequested==0){
			flipNow=2;
		} else if (flipflagRequested==2){
			flipNow=3;
		} else if (flipflagRequested==3){
			flipNow=1;
		}
	} else if (flipflagApplied==2){//CCW is applied
		if (flipflagRequested==0){
			flipNow=1;
		} else if (flipflagRequested==1){
			flipNow=3;
		} else if (flipflagRequested==3){
			flipNow=2;
		}
	} else if (flipflagApplied==3){//180 is applied
		if (flipflagRequested==0){
			flipNow=3;
		} else if (flipflagRequested==1){
			flipNow=2;
		} else if (flipflagRequested==2){
			flipNow=1;
		}
	} else{
		cout << "Error! Unknown flipflag (imagearea)." << endl;
	}
	cout << "prev. applied.=" << flipflagApplied << " and now requested=" << flipflagRequested << " thus now we do flag:" << flipNow << endl;

	flipflagApplied=flipflagRequested;

	if (flipCrop){
		this->cropRect = rotRect(flipNow,  this->cropRect, this->matOriginal.size());
		this->cpView   = rotPoint(flipNow, this->cpView,   this->matOriginal.size());
	}

	rot90(this->matOriginal, flipNow);
	rot90(this->matScaled,   flipNow);

	if (this->fixAspRatio){
		constrainAspectRatio();
	}
	
}


void ImageArea::rotate(cv::Mat& src, double angle, cv::Mat& dst){
	//cout << "R O T A T I N G" << endl;
    //int len = std::max(src.cols, src.rows);
    cv::Point2f ptCp(src.cols*0.5, src.rows*0.5);
    //cv::Point2f pt(len/2., len/2.);
    cv::Mat r = cv::getRotationMatrix2D(ptCp, angle, 1.0);
    cv::warpAffine(src, dst, r, src.size(), INTER_CUBIC); //Nearest is too rough INTER_LINEAR is ok
}


double ImageArea::Rad2Deg (double Angle) {
  static double ratio = 180.0 / M_PI;
  return Angle * ratio;
}

double ImageArea::Deg2Rad (double Radians) {
  static double ratio = M_PI / 180.;
  return Radians * ratio;
}


cv::Point ImageArea::rotatePt(cv::Point pt, cv::Point ptCp, double rotDeg){
	Point2f pt2f = pt;
	return rotatePt(pt2f, ptCp, rotDeg);
}

cv::Point ImageArea::rotatePt(cv::Point2f pt, cv::Point ptCp, double rotDeg){
	//Translate the point by the negative of the pivot point
	Point2f ptCp2f = ptCp;
	Point2f ptTrans = pt-ptCp2f;
	Point2f ptRot;
	//Rotate the point using the standard equation for 2-d (or 3-d) rotation
	double rotRad = Deg2Rad(rotDeg);
	ptRot.x = double(ptTrans.x)*cos(rotRad) - double(ptTrans.y)*sin(rotRad);
	ptRot.y = double(ptTrans.x)*sin(rotRad) + double(ptTrans.y)*cos(rotRad);
	//cout << "ROTATE FROM:" << ptTrans <<" by rad=" << rotRad << " results in:" << ptRot << endl;
	//Translate back
	ptRot = ptRot+ptCp2f;//Point2f(ptCp.x, ptCp.y);
	Point ptRotRound = Point(round(ptRot.x), round(ptRot.y));
	//cout << "Rotation pt=" << pt << " ptTrans=" << ptTrans << " ptRot=" << ptRotRound << endl;
	return ptRotRound;
}

void ImageArea::drawImage(bool noEmit, double dDidRotext){//default is false (see .h)
	//cout << "drawImage(" << noEmit <<"," << dDidRotext << ")" << endl;
	if (matOriginal.data==NULL) return;

	if (( this->width() < 1) || (this->width() < 1))
		return;

	//Emit current zoom level this->pctZoomRequest
	emit sigPixSample(-1, -1, -1 ,-1,-1,-1, this->pctZoomRequest);



	//ONLY ON FIRST LOAD:
	if (this->pctZoomRequest == -1){ //on first load
		double pctZoomMinGuiW = double(this->width()) / double(this->matOriginal.cols);
		double pctZoomMinGuiH = double(this->height()) / double(this->matOriginal.rows);
		double pctZoomMin = pctZoomMinGuiH < pctZoomMinGuiW ? pctZoomMinGuiH : pctZoomMinGuiW;
		this->pctZoomRequest =pctZoomMin;// pctZoomMinGui;
		this->cpView = Point(matOriginal.cols*0.5, matOriginal.rows*0.5);
		if (cropRect.x==0 && cropRect.y==0 && cropRect.width==100 && cropRect.height==100){ //Dont do this when autocrop has set
			this->cropRect = Rect(Point(matOriginal.cols*0.1,matOriginal.cols*0.1),Point(matOriginal.cols*0.9,matOriginal.rows*0.9));
		}
		if (fixCropSize){
			setCropWidth(fixSizeRequested); //Make sure the crop is fixed.
		}
	}

	//cout << "drawimage (w,h)=(" << this->width() << "," << this->height() << ")" << endl;


	double pctZoomMinGuiW = double(this->width()) / double(this->matOriginal.cols);
	double pctZoomMinGuiH = double(this->height()) / double(this->matOriginal.rows);
	double pctZoomMin = pctZoomMinGuiH < pctZoomMinGuiW ? pctZoomMinGuiH : pctZoomMinGuiW;
	if (pctZoomRequest <= pctZoomMin){
		pctZoomRequest = pctZoomMin;
	} 

	//(re-)set the min and max amount of rotation
	//rotationRequested = rotationRequested >  rotationMax ?  rotationMax : rotationRequested;
	//rotationRequested = rotationRequested < -rotationMax ? -rotationMax : rotationRequested;

	bool bZoomChanged=false;
	Size newSize = Size(this->matOriginal.cols*pctZoomRequest, this->matOriginal.rows*pctZoomRequest);
//	if (newSize != this->matScaled.size()){ //Resize only when zoom has changed or first load
		bZoomChanged=true;
		//cout << "this->matOrignal.size()" << this->matOriginal.size() << endl;
		//cout << "newSize=" << newSize << endl;
		cv::resize(this->matOriginal, this->matScaled, newSize,0,0, INTER_AREA);
		this->rotationApplied = 0;
		//double rotNow = this->rotationApplied-this->rotationRequested;
		//rotate(this->matScaled, rotNow, this->matScaled); //Also rotate
		//this->rotationApplied = this->rotationRequested;
//	}

	//Re-rotate when the rotation has individually changed
	if (this->rotationRequested != this->rotationApplied){
		double rotNow = this->rotationApplied-this->rotationRequested;
		rotate(this->matScaled, rotNow, this->matScaled);
		this->rotationApplied = this->rotationRequested;


		//The below moves the cropbox with the center of the rotation. However,
		// when the zoom has changed, we rotate again but the cropbox shouldnt move.
		//-360 means TO NOT ROTATE BOX
		//360 means AUTO ROTATE
		// Any other value is use dDidRotext
		if (dDidRotext>-360 && !bZoomChanged){ 
			if (dDidRotext<360){
				rotNow = dDidRotext;
			}
			//Rotate the cropbox as well
	 		Point ptRectMid = (cropRect.tl()+cropRect.br())*0.5;
			Point ptRotated = rotatePt(ptRectMid, Point(matOriginal.cols,matOriginal.rows)*0.5, -rotNow );
			Point ptMoveRect = ptRotated-ptRectMid;
			this->cropRect += ptMoveRect;
			if (fixCropSize){
				setCropWidth(fixSizeRequested); //Make sure the crop is fixed.
			}
		}
		
	}




	zoomCropRect.width  = this->width();
	zoomCropRect.height = this->height();
	zoomCropRect.x      = (cpView.x * pctZoomRequest) - floor(zoomCropRect.width*0.5) ;
	zoomCropRect.y      = (cpView.y * pctZoomRequest) - floor(zoomCropRect.height*0.5);

	//Test constraints
	zoomCropRect.x = zoomCropRect.br().x >= this->matScaled.cols  ? this->matScaled.cols-zoomCropRect.width-1 : zoomCropRect.x;
	zoomCropRect.y = zoomCropRect.br().y >= this->matScaled.rows  ? this->matScaled.rows-zoomCropRect.height-1 : zoomCropRect.y;

	zoomCropRect.x = zoomCropRect.x < 0 ? 0 : zoomCropRect.x;
	zoomCropRect.y = zoomCropRect.y < 0 ? 0 : zoomCropRect.y;

	if (zoomCropRect.br().x > this->matScaled.cols){
		zoomCropRect.width = this->matScaled.cols;
	}
	if (zoomCropRect.br().y > this->matScaled.rows){
		zoomCropRect.height = this->matScaled.rows;
	}


	//cout << "pctZoomRequest=" << pctZoomRequest << endl;
	//cout << "matScaled.size():" << matScaled.size() << endl;
	//cout << "zoomCropRect:" << zoomCropRect << endl;

	Mat matCrop = this->matScaled(zoomCropRect);


	//cv::resize(this->matOriginal, this->matScaled, getFitSize(this->matOriginal.size(), Size(this->width(), this->height())), 0, 0, INTER_AREA);
	matCrop.copyTo(this->matDisplay);

	//Now annotate using zoomCropRect
	/*if (zoomCropRect.width>0 && zoomCropRect.height>0){
		drawCropBox(noEmit);	
	}*/

	image=Mat2QImage(this->matDisplay);


	update();

	//cout << endl;


	//cout << this->geometry().width() << endl;
	//QPoint parentPt = QPoint(this->geometry().width(), this->geometry().height());
	//QPoint ptThis   = QPoint(matDisplay.cols, matDisplay.rows);
	//QPoint ptMove   = 0.5*(parentPt-ptThis);
	//if (ptMove.x() != 0 || ptMove.y() != 0){
//FIXME		this->move( ptMove );
	//}

}


//void ImageArea::setNewCropSize(int w, int h){
//}

void ImageArea::drawCropBox(bool noEmit){ //default is false (see .h)
	double dScale = double(this->matScaled.rows)/double(this->matOriginal.rows);

	this->drawRect.x = this->cropRect.x*dScale;
	this->drawRect.y = this->cropRect.y*dScale;

	this->drawRect.x -= this->zoomCropRect.x;
	this->drawRect.y -= this->zoomCropRect.y;
	this->drawRect.width = this->cropRect.width*pctZoomRequest;
	this->drawRect.height = this->cropRect.height*pctZoomRequest;

	cout << this->cropRect << endl;
		
	//[0]NW [1]N [2]NO [3]O [4]ZO [5]Z [6]ZW [7]W (compass directions of indices starting with top left 0[NW])
	toolPoints[0] = this->drawRect.tl();
	toolPoints[4] = this->drawRect.br();
	toolPoints[2] = Point(this->drawRect.br().x,this->drawRect.tl().y);
	toolPoints[6] = Point(this->drawRect.tl().x,this->drawRect.br().y);
	toolPoints[1] = (toolPoints[0] + toolPoints[2])*0.5;
	toolPoints[3] = (toolPoints[2] + toolPoints[4])*0.5;
	toolPoints[5] = (toolPoints[4] + toolPoints[6])*0.5;
	toolPoints[7] = (toolPoints[6] + toolPoints[0])*0.5;
	
	Rect displayRect(Point(0,0),this->matDisplay.size()); //Only for size refence	

	//rectangle(this->matScaled, this->drawRect, Scalar(0,100,255), 2, CV_AA);
	//Draw the rectangle box in my own way
	//Draw the horizontal stripes 
	for (int x=this->drawRect.tl().x; x < this->drawRect.br().x; x++){
		if ( (x) % 8 < 4 ) { //Every 4 pixels, 'v*2' term creates an offset between multiple cropscopes
			Point ptNow1 = Point(x,this->drawRect.tl().y);
			Point ptNow2 = Point(x,this->drawRect.br().y);
			if ( ptNow1.inside(displayRect) )
				minglePixel(this->matDisplay.at<Vec3b>(ptNow1));
			if ( ptNow2.inside(displayRect) )
				minglePixel(this->matDisplay.at<Vec3b>(ptNow2));
			
		}
	}
	//Draw the vertical stripes
	for (int y=this->drawRect.tl().y; y<this->drawRect.br().y; y++){
		if ( (y) % 8 < 4 ) { //Every 4 pixels 'v*2' term creates an offset between multiple cropscopes
			Point ptNow1 = Point(this->drawRect.tl().x,y);
			Point ptNow2 = Point(this->drawRect.br().x,y);
			if ( ptNow1.inside(displayRect) )
				minglePixel(this->matDisplay.at<Vec3b>(ptNow1));
			if ( ptNow2.inside(displayRect) )
				minglePixel(this->matDisplay.at<Vec3b>(ptNow2));
			
		}
	}
	//Finally, draw the squares and points in between
	for (int x=-3;x<=3;x++){
		for (int y=-3;y<=3;y++){
			if ( ( (x!=-3) && (x!=3) ) && ( (y>-3) && (y<3) ) ) //Only draw the outline
				continue;
			for (int i=0;i<toolPoints.size();i++){
				Point ptNow = toolPoints[i]-Point(x,y);
				//if ( (ptNow.x >= 0) && (ptNow.y >= 0) && (ptNow.x < this->matDisplay.cols) && (ptNow.y < this->matDisplay.rows) ){ 
				if ( ptNow.inside(displayRect) ){
					minglePixel(this->matDisplay.at<Vec3b>(ptNow));
				}
			}
		}
	}
	
	if (!noEmit){
		emit signalCropRect(this->camidx, this->processID, this->cropRect, this->rotationApplied, this->flipflagApplied); 
	}

}


void ImageArea::paintEvent(QPaintEvent *event){
	//cout << "paintEvent" << endl;
	QPainter painter(this);
	QRect dirtyRect = event->rect();
	painter.drawImage(dirtyRect, image, dirtyRect);
}



void ImageArea::keyPressEvent( QKeyEvent* event ){
	//Check if image is loaded: if not, return;
	if (this->pctZoomRequest == -1) //on first load
		return;

	this->keyPressedNow = event->key();
	//if( event->key() == Qt::Key_A ){
		// do your stuff here
	//}ss
	//QWidget::keyPressEvent(event);
	//cout << "boom keypressevent:" << keyPressedNow << endl;

	//this->cpView = Point(100,100); //test
	//this->pctZoomRequest = 1.5; //This is the zooming that should be added

	//if (event->modifiers() == Qt::ControlModifier || event->modifiers() == Qt::KeypadModifier || Qt::AltModifier){
	//double pctZoomMinGui = double(this->width()) / double(this->matOriginal.cols);

	if (this->keyPressedNow == Qt::Key_Space){
		this->setCursor(Qt::ClosedHandCursor);
	}



	double pctZoomMinGuiW = double(this->width()) / double(this->matOriginal.cols);
	double pctZoomMinGuiH = double(this->height()) / double(this->matOriginal.rows);
	double pctZoomMin = pctZoomMinGuiH < pctZoomMinGuiW ? pctZoomMinGuiH : pctZoomMinGuiW;


	double newZoomRequest = this->pctZoomRequest;
	if (event->modifiers() == Qt::ControlModifier && (this->keyPressedNow == Qt::Key_Plus || this->keyPressedNow == Qt::Key_Equal) ){
		newZoomRequest *= 1.1;
	} else if (event->modifiers() == Qt::ControlModifier &&	this->keyPressedNow == Qt::Key_Minus ){
		newZoomRequest /= 1.1;
	} else if (event->modifiers() == Qt::MetaModifier &&	this->keyPressedNow == Qt::Key_0 ){
		newZoomRequest = 1.0;
	} else if (event->modifiers() == Qt::ControlModifier &&	this->keyPressedNow == Qt::Key_0 ){
		newZoomRequest = pctZoomMin;
	} else if (this->keyPressedNow == Qt::Key_Right){
		if (this->rotateAllowed){
			rotationRequested += 0.15;
		} else {
			QApplication::beep();
		}
	} else if (this->keyPressedNow == Qt::Key_Left){
		if (this->rotateAllowed){
			rotationRequested -= 0.15;
		} else {
			QApplication::beep();
		}
	}/*else if (this->keyPressedNow == Qt::Key_S && this->viewingBlur==false && this->allowBlurEstimation){
		this->viewingBlur=true;
		if (!this->madeBlur){ // Only make new blur map when we haven't made it yet
			estimateBlur();
		}
		this->matScaled.release();
		swap(this->matOriginal, this->matBlur);
		refreshFlip(true);
		drawImage(false, false);
	}*/

	

	if ((rotationRequested < -rotationMax) || (rotationRequested > rotationMax)){
		QApplication::beep();
	}

	//Set the min and max amount of rotation
	rotationRequested = rotationRequested >  rotationMax ?  rotationMax : rotationRequested;
	rotationRequested = rotationRequested < -rotationMax ? -rotationMax : rotationRequested;

	if (newZoomRequest <= pctZoomMin){
		newZoomRequest = pctZoomMin;
	} else if ( newZoomRequest > 1.0 ){
		newZoomRequest = 1.0;
	}
		
	if (newZoomRequest != this->pctZoomRequest){ //Only redraw if changed
		this->pctZoomRequest = newZoomRequest;
		drawImage();
	}

	if (this->rotationRequested != this->rotationApplied){
		drawImage();
	}

	//cout << "pctzoomreq:" << pctZoomRequest << " pctzoom:" << pctZoomMinGui << endl;


}

void ImageArea::keyReleaseEvent(QKeyEvent *event){
	this->keyPressedNow = -1;
	int keyReleasedNow = event->key();
	//this->setCursor(Qt::ArrowCursor);
	//cout << "keyReleaseEvent." << endl;
	

	if (keyReleasedNow == Qt::Key_S && this->viewingBlur){
		cout << "Released Qt::Key_S" << endl;
		this->viewingBlur=false;
		this->matScaled.release();
		swap(this->matOriginal, this->matBlur);
		refreshFlip(true);
		drawImage(false, false);
	}



}


void ImageArea::mousePressEvent(QMouseEvent *event){
	if (this->pctZoomRequest == -1) //on first load
		return;
	//cout <<"Key:" <<  keyPressedNow << endl;

	//This is only invoked the first time you click
	if (event->button() == Qt::LeftButton) {
		//cout << "Left button clicked." << endl;

		lastPointCursor = event->pos();
		//cout << "Cursor:" << lastPoint.x() << "," << lastPoint.y() << endl;

		//cout << minDist << endl;

	
		
		scribbling = true;
			
		
	} else if (event->button() == Qt::RightButton) {
		//cout << "Received right mouse button" << endl;

	}
}


cv::Rect ImageArea::getBoundingRect(std::vector<cv::Point> vPoints, int idx){
	//0 1 2
	//7   3
	//6 5 4
	Rect rRect;
	if (idx==0 || idx==4){
		rRect = Rect(vPoints[0], vPoints[4]);
	} else if (idx==2 || idx==6){
		rRect = Rect(vPoints[2], vPoints[6]);
	} else if (idx==1 || idx==5){ //y changes, fix x
		rRect = Rect(Point(vPoints[0].x, vPoints[1].y),Point(vPoints[4].x,vPoints[5].y));
	} else if (idx==3 || idx==7){ //x changes, fix y.
		rRect = Rect(Point(vPoints[3].x, vPoints[0].y),Point(vPoints[7].x,vPoints[4].y));
	} else if (idx==-1){
		rRect = Rect(vPoints[0],vPoints[4]);
	}
	return rRect;//+Size(1,1)+Point(1,1);
	//int minX=INT_MAX, minY=INT_MAX, maxX=INT_MIN, maxY=INT_MIN;
	//for (int i=0; i<vPoints.size();i++){
	//	if (vPoints[i].x < minX) minX=vPoints[i].x;
	//	if (vPoints[i].y < minY) minY=vPoints[i].y;
	//	if (vPoints[i].x > maxX) maxX=vPoints[i].x;
	//	if (vPoints[i].y > maxY) maxY=vPoints[i].y;
	//}
	//return Rect(Point(minX,minY), Point(maxX+1,maxY+1));
}


void ImageArea::constrainAspectRatio(){
	cout << "contrainAspectRatio()" << endl;
	//Aspect Ratios
	cv::Size sizeFit = getFitSize(this->szFixAspectRatio, this->cropRect.size());
	this->cropRect = Rect(this->cropRect.tl(), sizeFit);
}


void ImageArea::setCropWidth(Size sz){
	int w = sz.width;
	int h = sz.height;
	
	if (w > this->matOriginal.cols){
		//TODO throw popup, requested width is wider than original image
	} else  if (h>this->matOriginal.rows){
		//TODO throw popup, requested height is higher than original image
	}
	
	Rect newCropRect = this->cropRect;
	newCropRect.width  = w;
	newCropRect.height = h;

	//Make sure it cannot drop below zero
	newCropRect.x = newCropRect.x < 0 ? 0 :newCropRect.x;
	newCropRect.y = newCropRect.y < 0 ? 0 :newCropRect.y;

	
	if (this->matOriginal.data!=NULL){
		newCropRect.x = newCropRect.br().x < this->matOriginal.cols ? newCropRect.x : this->matOriginal.cols-1-newCropRect.width;
		newCropRect.y = newCropRect.br().y < this->matOriginal.rows ? newCropRect.y : this->matOriginal.rows-1-newCropRect.height;
	}

	//Now assign it to the global
	this->cropRect = newCropRect;
	cout << "setCropWidth(sz=" << sz <<") result rect=" << cropRect << endl;
}
	

void ImageArea::updateRTIview(double x, double y){
	x=(x/double(this->width()) *2)-1;
	y=-((y/double(this->height())*2)-1);
	cout << "updateRTIview(" << x << "," << y << ")" << endl;


	//matDiffuse.at<uchar>(k) = a0*x*x + a1*y*y + a2*x*y + a3*x + a4*y + a5;


	this->matOriginal = (rti_images[0]-rti_bias[0])*rti_scales[0]*x*x +
						(rti_images[1]-rti_bias[1])*rti_scales[1]*y*y +
						(rti_images[2]-rti_bias[2])*rti_scales[2]*x*y +
						(rti_images[3]-rti_bias[3])*rti_scales[3]*x +
						(rti_images[4]-rti_bias[4])*rti_scales[4]*y +
						(rti_images[5]-rti_bias[5])*rti_scales[5];

	this->matOriginal.convertTo(this->matOriginal,CV_8UC3);

	//matCoef_R[i] = matCoef_R[i]*(1.0/scales[i])+biases[i];
}



void ImageArea::mouseMoveEvent(QMouseEvent *event){

	//The setFocus should only be done when a key is pressed
	this->setFocus(Qt::OtherFocusReason); 
	

	QPoint ptCur = event->pos(); 





	if ((event->buttons() & Qt::LeftButton) && this->keyPressedNow == Qt::Key_Space){
		
		//cout << "moving.. " << endl;
		Point dCursor = Point(ptCur.x()-lastPointCursor.x() ,ptCur.y()-lastPointCursor.y());

		Point newCpView = cpView - dCursor*(1.0/pctZoomRequest);
		if (newCpView.x >0 && newCpView.y >0 && newCpView.x < this->matOriginal.cols && newCpView.y < this->matOriginal.rows){
			lastPointCursor = ptCur; //Refresh last position
			this->cpView = newCpView;
		} 

		//Refresh the graphics..
		//drawCropBox(); 
		drawImage();

	} else if ((event->buttons() & Qt::LeftButton) && scribbling){

		//cout << "Last point:" << lastPointCursor.x() << "x" << lastPointCursor.y() << endl;
		//cout << "Now:" << ptCur.x() << "x" << ptCur.y() << endl;

		Point dCursor = Point(ptCur.x()-lastPointCursor.x() ,ptCur.y()-lastPointCursor.y());
		//dCursor=Point(0,0);
			
		//cout << "dCursor:" << dCursor.x << "x" << dCursor.y << endl;
		
		updateRTIview(ptCur.x(), ptCur.y());


		/*
		double dScale = double(this->matScaled.rows)/double(this->matOriginal.rows);

		Point dCursorScaled = dCursor*(1.0/dScale);



		vector<Point> toolPointsFull(8);
		toolPointsFull[0] = this->cropRect.tl();
		toolPointsFull[4] = this->cropRect.br();
		toolPointsFull[2] = Point(this->cropRect.br().x,this->cropRect.tl().y);
		toolPointsFull[6] = Point(this->cropRect.tl().x,this->cropRect.br().y);
		toolPointsFull[1] = (toolPointsFull[0] + toolPointsFull[2])*0.5;
		toolPointsFull[3] = (toolPointsFull[2] + toolPointsFull[4])*0.5;
		toolPointsFull[5] = (toolPointsFull[4] + toolPointsFull[6])*0.5;
		toolPointsFull[7] = (toolPointsFull[6] + toolPointsFull[0])*0.5;

		Rect rBounding, rBoundingFullscale;
	
		if (scribbletype==0 && !fixCropSize){ //Upper Left Corner
			//this->toolPoints[0] += dCursor;
			toolPointsFull[0] += dCursorScaled;
		} else if (scribbletype==2 && !fixCropSize){ // Upper Right Corner
			//this->toolPoints[2] += dCursor;
			toolPointsFull[2] += dCursorScaled;
		} else if (scribbletype==4 && !fixCropSize){ // Bottom Right Corner
			//this->toolPoints[4] += dCursor;
			toolPointsFull[4] += dCursorScaled;
		} else if (scribbletype==6 && !fixCropSize){ // Bottom Left Corner
			//this->toolPoints[6] += dCursor;
			toolPointsFull[6] += dCursorScaled;
		} else if (scribbletype==1 && !fixCropSize){ //
			//this->toolPoints[1] += Point(0,dCursor.y);
			toolPointsFull[1] +=  Point(0,dCursorScaled.y);
		} else if (scribbletype==5 && !fixCropSize){ //
			//this->toolPoints[5] += Point(0,dCursor.y);
			toolPointsFull[5] += Point(0,dCursorScaled.y);
		} else if (scribbletype==3 && !fixCropSize){ //
			//this->toolPoints[3] += Point(dCursor.x,0);
			toolPointsFull[3] += Point(dCursorScaled.x,0);
		} else if (scribbletype==7 && !fixCropSize){ //
			//this->toolPoints[7] += Point(dCursor.x,0);
			toolPointsFull[7] += Point(dCursorScaled.x,0);
		} else if (scribbletype==-1){
			//this->toolPoints[0] +=dCursor;
			//this->toolPoints[4] +=dCursor;
			toolPointsFull[0] += dCursorScaled;
			toolPointsFull[4] += dCursorScaled;
		}
	


		rBoundingFullscale = getBoundingRect(toolPointsFull, scribbletype);


		//if (rBoundingFullscale.tl().x >= 0 && rBoundingFullscale.tl().y >= 0 && rBoundingFullscale.br().x < this->matOriginal.cols && rBoundingFullscale.br().y < this->matOriginal.rows){
		//	this->cropRect = rBoundingFullscale;  
		//	lastPointCursor = ptCur; //Refresh last position
		//} 

		//if (this->fixCropSize){
		//	setCropWidth(fixSizeRequested); //Make sure the crop is fixed.
		//}


		*/
		//Refresh the graphics..
		//drawCropBox();
		drawImage();


		

		//drawCropOutline();
		//emit signalTest(widgetIdx);
	} else {

		this->setCursor(Qt::ArrowCursor);
		
	}
	
/*
	QSize qsz = this->image.size();
	if ((ptCur.x() >= 0) && (ptCur.y() >= 0) && (ptCur.x() < qsz.width()) && (ptCur.y() < qsz.height())){
		//Obtain the current RGB value of the image we currently hover over
		QRgb qrgbMouseOver = this->image.pixel(ptCur);
		QColor colMouseOver(qrgbMouseOver);
		int r=-1, g=-1, b=-1;
		colMouseOver.getRgb(&r,&g,&b);
		double dLab[3]={-1,-1,-1};
		if (this->hTransformLab!=NULL){
			//Calculate Lab..
			double RGB[3];
			//Normalize to [0 .. 1.0]
			RGB[0] = r / 255.0;
			RGB[1] = g / 255.0;
			RGB[2] = b / 255.0;
			cmsCIELab Lab;
			cmsDoTransform(this->hTransformLab, RGB, &Lab, 1);
			dLab[0]= Lab.L; dLab[1]= Lab.a; dLab[2]= Lab.b;
		}
		//cout << "(r,g,b)=(" << r << "," << g << "," << b << ")" << endl;

		emit sigPixSample(r,g,b,dLab[0],dLab[1],dLab[2],this->pctZoomRequest);
	}*/
}



void ImageArea::mouseReleaseEvent(QMouseEvent *event){
	//cout << "Mouse released." << endl;
	//This is only invoked when you unclick
	if (event->button() == Qt::LeftButton && scribbling) {
		QPoint ptCur = event->pos(); 
		//drawLineTo(ptCur);
		//scribbling = false;
		this->setCursor(Qt::ArrowCursor);
		//cout << "Cursor:" << ptCur.x() << "," << ptCur.y() << endl;

		if (this->fixAspRatio){
			constrainAspectRatio();
			drawImage();
		}
	}
}

void ImageArea::resizeEvent(QResizeEvent *event){
	//cout << "resizeevent (w,h)=(" << this->width() << "," << this->height() << ")" << endl;

	if (this->matOriginal.data==NULL)
		return;

	if (this->width()<=1 || this->height()<=1)
		return;


	int oldWidth = this->matDisplay.cols;
	int oldHeight = this->matDisplay.rows;

	if (oldWidth > this->width() || oldHeight > this->height()){
		double pctZoomMinGuiW = double(this->width()) / double(this->matOriginal.cols);
		double pctZoomMinGuiH = double(this->height()) / double(this->matOriginal.rows);
		double pctZoomMin = pctZoomMinGuiH < pctZoomMinGuiW ? pctZoomMinGuiH : pctZoomMinGuiW;

		this->pctZoomRequest = pctZoomMin;
	}

	drawImage();

	//QPoint parentPt = QPoint(this->parentWidget()->geometry().width(),this->parentWidget()->geometry().height());
	//QPoint ptThis = QPoint(matDisplay.cols, matDisplay.rows);
	//this->move( 0.5*(parentPt-ptThis) );

	//cout << "Parentpt:" << parentPt.x() << "x" << parentPt.y() << endl;
	//cout << "Thispt:" << ptThis.x() << "x" << ptThis.y() << endl;

	//QWidget::resizeEvent(event);
	//move(screenRect.center() - widget.rect().center())
}


















