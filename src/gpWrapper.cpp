/****************************************************************************
**
** Copyright (C) 2015 Tim Zaman.
** Contact: http://www.timzaman.com/ , timbobel@gmail.com
**
**
****************************************************************************/

#include "gpWrapper.h"


using namespace std;
using namespace cv;




gpWrapper::gpWrapper(int m_camid){
	cout << "Constructing gpWrapper::gpWrapper(" << m_camid << ")" << endl;
	//Constructor gpWrapper();
	this->context = gp_context_new();
	this->isConnected=false;
	this->serialNumber="";
	this->camisready = false;
	this->camid = m_camid;

	connect(this, SIGNAL(sigCamCaptureTethered(int)), SLOT(slotCamCaptureTethered(int)));

	this->isStopped=false;
	this->shutterdelay=0; //ms
}



gpWrapper::~gpWrapper(){
	cout << "Destructing gpWrapper::~gpWrapper()" << endl;
	//Destructor gpWrapper();
	slotDisconnectCam(1337);
}



int gpWrapper::getNumCams(){
	//This needs to be called(!)
	//Detect all the cameras that can be autodetected...
	int ret = gp_list_new (&camlist);
	if (ret < GP_OK) { cout << "Error retrieving camlist" << endl; }
	this->numCams = sample_autodetect (camlist);

	if (this->numCams <0){
		cout << gp_result_as_string(this->numCams) << endl;
	}
	
	cout << "Detected " << this->numCams << " cameras." << endl;
	return this->numCams;
}


bool gpWrapper::slotConnectCam(int idx){
	//bool gpWrapper::camInit(int idx){
	//i=camera idx

	if (this->numCams <= idx ) {
		cout << "Will not slotConnectCam(" << idx << "), as index exceeds numCams." << endl;
		return false;
	}


	//First clear whatever we might have had before
	//camclear();

	//First unreference (if not done already)
	//gp_camera_unref(camera); //This crashes on 2nd invocation
	//gp_context_unref(context);

	//Reinitialize the camera
	//int okCamNew = gp_camera_new (&camera);


	// Now open all cameras we autodected for usage
	const char	*name, *value;

	Camera * camnew;
	gp_list_get_name  (camlist, idx, &name);
	gp_list_get_value (camlist, idx, &value);
	fprintf(stderr,"Camera %s on port %s found, not yet opened.\n", name, value);
	int ret = sample_open_camera (&camnew, name, value);
	if (ret < GP_OK) {
		fprintf(stderr,"Camera %s on port %s failed to open\n", name, value);
		return false;
	} else {
		cout << "Opening of camera #"  << idx << " (" << name << ") succesful" << endl;
		camera = camnew;
	}
	this->model = std::string(name);
	

	CameraText text;
	//char *owner;
	ret = gp_camera_get_summary (camera, &text, context);
	//Check if canon
	if (this->model.find("Canon") != std::string::npos) {
		std::cout << "Canon found, enabling special canon capture." << '\n';
		canon_enable_capture(TRUE);
	}
	if (ret < GP_OK) {
		fprintf (stderr, "Failed to get summary.\n");
	} else {
		//printf("Summary:\n%s\n", text.text);
	}
	//TODO: free camera text?
	cout << "Initialization done" << endl;

	///main/status/serialnumber
	char * strSerial;
	
	if ((get_config_value_string( "serialnumber", &strSerial)==0)){
		cout << " hw cam serialnumber=" << strSerial << endl;
		this->serialNumber = string(strSerial);
	}

	this->isConnected=true;

	sigCamConnected(idx);

	this->camisready=true;

	return true;
}

void gpWrapper::slotStop(){
	//Slot when the software should stop whatever it is doing
	this->isStopped=true;
}


int gpWrapper::sample_open_camera (Camera ** camera, const char *model, const char *port) {
	static GPPortInfoList		*portinfolist = NULL;  //FIXME was previously a global?
	static CameraAbilitiesList	*abilities = NULL; //FIXME was previously a global?

	int		ret, m, p;
	CameraAbilities	a;
	GPPortInfo	pi;

	ret = gp_camera_new (camera);
	if (ret < GP_OK) return ret;

	if (!abilities) {
		// Load all the camera drivers we have... 
		ret = gp_abilities_list_new (&abilities);
		if (ret < GP_OK) return ret;
		ret = gp_abilities_list_load (abilities, context);
		if (ret < GP_OK) return ret;
	}

	// First lookup the model / driver 
	m = gp_abilities_list_lookup_model (abilities, model);
	if (m < GP_OK) return ret;
	
	ret = gp_abilities_list_get_abilities (abilities, m, &a);
	if (ret < GP_OK) return ret;
	
	ret = gp_camera_set_abilities (*camera, a);
	if (ret < GP_OK) return ret;

	if (!portinfolist) {
		// Load all the port drivers we have... 
		ret = gp_port_info_list_new (&portinfolist);
		if (ret < GP_OK) return ret;
		ret = gp_port_info_list_load (portinfolist);
		if (ret < 0) return ret;
		ret = gp_port_info_list_count (portinfolist);
		if (ret < 0) return ret;
	}

	// Then associate the camera with the specified port 
	p = gp_port_info_list_lookup_path (portinfolist, port);
	switch (p) {
		case GP_ERROR_UNKNOWN_PORT:
			fprintf (stderr, "The port you specified "
				"('%s') can not be found. Please "
				"specify one of the ports found by "
				"'gphoto2 --list-ports' and make "
				"sure the spelling is correct "
				"(i.e. with prefix 'serial:' or 'usb:').",
				  port);
			break;
		default:
			break;
	}
	if (p < GP_OK) return p;

	ret = gp_port_info_list_get_info (portinfolist, p, &pi);
	if (ret < GP_OK) return ret;
	
	ret = gp_camera_set_port_info (*camera, pi);
	if (ret < GP_OK) return ret;
	
	return GP_OK;
}











int gpWrapper::sample_autodetect (CameraList *camlist) {
	gp_list_reset (camlist);
  return gp_camera_autodetect (camlist, context);
}



//Function below is defined static in header .h
int gpWrapper::_lookup_widget(CameraWidget*widget, const char *key, CameraWidget **child) {
	int ret;
	ret = gp_widget_get_child_by_name (widget, key, child);
	if (ret < GP_OK)
		ret = gp_widget_get_child_by_label (widget, key, child);
	return ret;
}




int gpWrapper::get_list_config_string(const char *key, std::vector<std::string> &vecListConf, int &iChoice){
	CameraWidget		*widget = NULL, *child = NULL;
	CameraWidgetType	type;
	int ret;
	char *val;

	cout << "key=" << key << endl;

	ret = gp_camera_get_config (camera, &widget, context);
	if (ret < GP_OK) {
		fprintf (stderr, "camera_get_config failed: %d\n", ret);
		return ret;
	}
	ret = _lookup_widget (widget, key, &child);
	if (ret < GP_OK) {
		  cout << "Error in _lookup_widget inside gphoto get_list_config_string" << endl;
		  return ret;
	}

	//switchOptions.options.clear();
	//switchOptions.currentIdx = -1;

	ret = gp_widget_get_type (child, &type);

	if( type != GP_WIDGET_RADIO ) {
		cout << "List Config not radio." << endl;
		return ret;
	}


	char *current;
	ret = gp_widget_get_value (child, &current);
	if (ret == GP_OK) {
		int cnt = gp_widget_count_choices (child);
		for ( int i = 0; i < cnt; i++ ) {
			const char *choice;
			ret = gp_widget_get_choice (child, i, &choice);
			vecListConf.push_back(choice);
			//switchOptions.options.push_back( choice );
			//cout << choice << endl;
			if( !strcmp( choice, current ) ) {			
				iChoice = i;
				//out << "DIT IS DE CHOICE?" << endl;
			}
		}
	} else {
		cout << "Can't obtain value from config widget." << endl;
	}
	return true;
}



// Gets a string configuration value.
// This can be:
//  - A Text widget
//  - The current selection of a Radio Button choice
//  - The current selection of a Menu choice
// Sample (for Canons eg):
//   get_config_value_string (camera, "owner", &ownerstr, context);

int gpWrapper::get_config_value_string ( const char *key, char **str) {
	CameraWidget		*widget = NULL, *child = NULL;
	CameraWidgetType	type;
	int			ret;
	char			*val;

	ret = gp_camera_get_config (camera, &widget, context);
	if (ret < GP_OK) {
		fprintf (stderr, "camera_get_config failed: %d\n", ret);
		return ret;
	}
	ret = _lookup_widget (widget, key, &child);
	if (ret < GP_OK) {
		fprintf (stderr, "lookup widget failed: %d\n", ret);
		goto out;
	}

	/* This type check is optional, if you know what type the label
	 * has already. If you are not sure, better check. */
	ret = gp_widget_get_type (child, &type);
	if (ret < GP_OK) {
		fprintf (stderr, "widget get type failed: %d\n", ret);
		goto out;
	}
	switch (type) {
        case GP_WIDGET_MENU:
        case GP_WIDGET_RADIO:
        case GP_WIDGET_TEXT:
		break;
	default:
		fprintf (stderr, "widget has bad type %d\n", type);
		ret = GP_ERROR_BAD_PARAMETERS;
		goto out;
	}

	/* This is the actual query call. Note that we just
	 * a pointer reference to the string, not a copy... */
	ret = gp_widget_get_value (child, &val);
	if (ret < GP_OK) {
		fprintf (stderr, "could not query widget value: %d\n", ret);
		goto out;
	}
	/* Create a new copy for our caller. */
	*str = strdup (val);
out:
	gp_widget_free (widget);
	return ret;
}


/* Sets a string configuration value.
 * This can set for:
 *  - A Text widget
 *  - The current selection of a Radio Button choice
 *  - The current selection of a Menu choice
 *
 * Sample (for Canons eg):
 *   get_config_value_string (camera, "owner", &ownerstr, context);
 */
int gpWrapper::set_config_value_string (const char *key, const char *val) {
	CameraWidget *widget = NULL, *child = NULL;
	CameraWidgetType type;
	int ret;
	int v=0;

	ret = gp_camera_get_config (camera, &widget, context);
	if (ret != GP_OK) {
		fprintf (stderr, "camera_get_config failed: %d\n", ret);
		return ret;
	}
	ret = _lookup_widget (widget, key, &child);
	if (ret != GP_OK) {
		fprintf (stderr, "lookup widget failed: %d\n", ret);
		goto out;
	}

	// This type check is optional, if you know what type the label
	// has already. If you are not sure, better check. 
	ret = gp_widget_get_type (child, &type);
	if (ret < GP_OK) {
		fprintf (stderr, "widget get type failed: %d\n", ret);
		goto out;
	}
	switch (type) {
        case GP_WIDGET_MENU:
        case GP_WIDGET_RADIO:
        case GP_WIDGET_TEXT:
        case GP_WIDGET_TOGGLE:
		break;
	default:
		fprintf (stderr, "widget has bad type %d\n", type);
		ret = GP_ERROR_BAD_PARAMETERS;
		goto out;
	}

	// This is the actual set call. Note that we keep
	// ownership of the string and have to free it if necessary.
	//
	if (type==GP_WIDGET_TOGGLE){
		v=boost::lexical_cast<int>(val);
		ret = gp_widget_set_value (child, &v);
	} else {
		ret = gp_widget_set_value (child, val);
	}

	if (ret != GP_OK) {
		fprintf (stderr, "could not set widget value: %d\n", ret);
		goto out;
	}
	// This stores it on the camera again 
	ret = gp_camera_set_config (camera, widget, context);
	if (ret != GP_OK) {
		fprintf (stderr, "camera_set_config failed: %d\n", ret);
		goto out;
	}
out:
	gp_widget_free (widget);
	return ret;
}


void gpWrapper::setGpCameraSettings(std::string sShut, std::string sAperture, std::string sISO){
	cout << "Setting camera:" << sShut << "," << sAperture << "," << sISO << endl;

	set_config_value_string("iso", sISO.c_str());

	if (this->model.find("Canon") != std::string::npos) {
		set_config_value_string("shutterspeed", sShut.c_str()); //Canon 5D
		set_config_value_string("aperture", sAperture.c_str()); //Canon 5D
	} else { //Nikon
		set_config_value_string("shutterspeed2", sShut.c_str()); //Nikon D800
		set_config_value_string("f-number", sAperture.c_str()); //Nikon D800
	}

	//Set correct settings
	//set_config_value_string(camera, "imagequality", "NEF (Raw)", context);
	//set_config_value_string(camera, "imagesize", "7360x4912", context);
	//set_config_value_string(camera, "rawcompression", "Lossless", context);
	//set_config_value_string(camera, "d149", "0", context); //Raw Bit Mode (0=12, 1=14)
}


//
// This enables/disables the specific canon capture mode.
// 
// For non canons this is not required, and will just return
// with an error (but without negative effects).
//
int gpWrapper::canon_enable_capture (int onoff) {
      CameraWidget            *widget = NULL, *child = NULL;
      CameraWidgetType  type;
      int               ret;

      ret = gp_camera_get_config (camera, &widget, context);
      if (ret < GP_OK) {
            fprintf (stderr, "camera_get_config failed: %d\n", ret);
            return ret;
      }
      ret = _lookup_widget (widget, "capture", &child);
      if (ret < GP_OK) {
            /*fprintf (stderr, "lookup widget failed: %d\n", ret);*/
            goto out;
      }

      ret = gp_widget_get_type (child, &type);
      if (ret < GP_OK) {
            fprintf (stderr, "widget get type failed: %d\n", ret);
            goto out;
      }
      switch (type) {
        case GP_WIDGET_TOGGLE:
            break;
      default:
            fprintf (stderr, "widget has bad type %d\n", type);
            ret = GP_ERROR_BAD_PARAMETERS;
            goto out;
      }
      /* Now set the toggle to the wanted value */
      ret = gp_widget_set_value (child, &onoff);
      if (ret < GP_OK) {
            fprintf (stderr, "toggling Canon capture to %d failed with %d\n", onoff, ret);
            goto out;
      }
      /* OK */
      ret = gp_camera_set_config (camera, widget, context);
      if (ret < GP_OK) {
            fprintf (stderr, "camera_set_config failed: %d\n", ret);
            return ret;
      }
out:
      gp_widget_free (widget);
      return ret;
}


//void slotConnectCam(int):
//void slotDisconnectCam(int);
//void slotCamCapture();
//void slotCamCapturePreview();
//void slotCamCaptureTethered();



void gpWrapper::slotDisconnectCam(int idx){
	cout << "gpWrapper::slotDisconnectCam(" << idx << ")" << endl;
	//TODO: use camid idx
	cout << "gp_tim: Disconneting cameras and releasing context." << endl;
	if (this->isConnected){
		gp_camera_exit(camera, context);
		gp_camera_unref(camera); //untested
	}
}


void gpWrapper::slotCamCaptureTethered(int camid_req){
//int gpWrapper::captureTethered(char * &dataNef, unsigned long int &sizeNef, int &fileType){
//int gpWrapper::captureTethered(CameraFile * &file, int &fileType){

	if ((this->camid!=camid_req) && (camid_req!=-1) ){ // -1 means requests all systems
		return;
	}

	//if (this->isStopped==true){
	//	this->isStopped=false;
	//	return;
	//}

	//=== Note that Tethering currently cannot be stopped, so do not interrupt it. ===

	CameraFile * file;
	int fileType;

	int fd, retval;
	//CameraFile *file;
	CameraEventType	evttype;
	CameraFilePath	*path;
	void	*evtdata;

	//printf("Tethering...\n");

	retval = gp_camera_wait_for_event (camera, 2500, &evttype, &evtdata, context); //1000ms worked fine
	if (retval != GP_OK){
		string strErr = "Tethered Capture Failed! wait_for_event not ok: " + string(gp_result_as_string(retval)) + "\nTurn the camera off and on, if the problem persists, go to captain.";
		cout << strErr << endl;
		emit sigFail(); //Stops all hardware recursively through scanmanager
		emit sigPopup(strErr, cv::Mat());
		//return retval;
		return;//
	}

	switch (evttype) {
	case GP_EVENT_FILE_ADDED:
		path = (CameraFilePath*)evtdata;
		printf("File added on the camera: %s/%s\n", path->folder, path->name);
		cout << "Signal GP_EVENT_FILE_ADDED received.. downloading to mem.." << endl;

		//fd = open(path->name, O_CREAT | O_WRONLY, 0644);
		//retval = gp_file_new_from_fd(&file, fd);

		retval = gp_file_new(&file);

		//printf("  Downloading %s...\n", path->name);
		retval = gp_camera_file_get(camera, path->folder, path->name, GP_FILE_TYPE_NORMAL, file, context);

		fileType = getFileType((string)path->name);

		//retval = gp_file_get_data_and_size (file,(const char**) &dataNef, &sizeNef);
		//sizeNef = sizeNef+1; //For some reason it doesnt write the last byte?

		//printf("  Deleting %s on camera...\n", path->name);
		retval = gp_camera_file_delete(camera, path->folder, path->name, context);


		//usleep(this->camid*500000); //wait 0.5s  per camid to make sure the earliest camid gets a process first
		emit sigCapture(file, fileType, this->camid);

		//gp_file_free(file);
		//return;// 1337;
		break;
	case GP_EVENT_FOLDER_ADDED:
		path = (CameraFilePath*)evtdata;
		printf("Folder added on camera: %s / %s\n", path->folder, path->name);
		break;
	case GP_EVENT_CAPTURE_COMPLETE:
		printf("Capture Complete.\n");
		break;
	case GP_EVENT_TIMEOUT:
		printf("Timeout.\n");
		break;
	case GP_EVENT_UNKNOWN:
		if (evtdata) {
			printf("Unknown event: %s.\n", (char*)evtdata);
		} else {
			printf("Unknown event.\n");
		}
		break;
	default:
		printf("Type %d?\n", evttype);
		break;
	}

	//If this is reached, there was no new capture, so we start over with the polling
	if (this->isStopped==true){
		this->isStopped=false;
		return;
	} else {
		emit sigCamCaptureTethered(this->camid); //Self invoke this same function to loop
	}




	//}
	//cout << "end captureTethered()" << endl;
	//return retval;
	//TODO use gp_file_unref somewhere
}




int gpWrapper::getFileType(std::string fileName){
	int fileType;
	string fileExt  = boost::filesystem::extension(fileName);
	boost::to_upper(fileExt); //Make is uppercase	
	if ( (fileExt.compare(".NEF") == 0) || (fileExt.compare(".CR2") == 0) || (fileExt.compare(".RAW") == 0) || (fileExt.compare(".MOS") == 0) ){
		fileType = util::GPTIM_RAW;
	} else if ( (fileExt.compare(".JPG") == 0) || (fileExt.compare(".JPEG") == 0) ){
		fileType = util::GPTIM_JPEG;
	} else if ( (fileExt.compare(".TIF") == 0) || (fileExt.compare(".TIFF") == 0) ){
		fileType = util::GPTIM_TIFF;
	} else {
		cout << "Unknown extension :" << fileExt << endl;
		return -1;
	}
	return fileType;
}

void gpWrapper::slotCamCapturePreview(int camid_req){
//int gpWrapper::capture_preview(CameraFile * &file){
	//Previews are always captured in JPEG format.

	if ((this->camid!=camid_req) && (camid_req!=-1) ){ // -1 means requests all systems
		return;
	}

	CameraFile * file;

	int retval = gp_file_new(&file);
	if (retval != GP_OK) {
		cout << "gp_file_new == Not Ok, ret=" << string(gp_result_as_string(retval)) << endl;
		return;// retval;
		
	}

	// autofocus every 10 shots
	//if (i%10 == 9) {
	//	camera_auto_focus (camera, context);
	//} else {
		//camera_manual_focus (camera, (i/10-5)/2, context);
	//}
	retval = gp_camera_capture_preview(camera, file, context);
	if (retval != GP_OK) {
		cout << "gp_camera_capture_preview == Not Ok, ret=" << string(gp_result_as_string(retval)) << endl;
		return;// retval;
	}
	
	emit sigCapturePreview(file);

	//Camera unreference is done later.
	//return GP_OK;
}







//int gpWrapper::capture(char * &dataNef, unsigned long int &sizeNef, int &fileType){

//emit sigCapture(CameraFile * &, int&);

void gpWrapper::slotCamCapture(int camid_req, std::string fname){
//int gpWrapper::capture(CameraFile * &camFile, int &fileType){

	if ((this->camid!=camid_req) && (camid_req!=-1) ){ // -1 means requests all systems
		return;
	}

	this->camisready=false;

	CameraFile * camFile;
	int fileType;

	int retval;
	CameraFilePath camera_file_path;

	if (this->shutterdelay>0){ //ms
		usleep(this->shutterdelay*1000);  //wait the delay (arg in microseconds)
	}

	retval = gp_camera_capture(camera, GP_CAPTURE_IMAGE, &camera_file_path, context);

	if (retval) {
		string strErr = "Error in camera capture, gp_camera_capture: " + string(gp_result_as_string(retval));
		cout << strErr << endl;
		emit sigFail(); //Stops all hardware recursively through scanmanager
		emit sigPopup(strErr, cv::Mat());
		this->camisready=true;
		return;// retval;
		//TODO: error handling
	}

	printf("Pathname on the camera: %s/%s\n", camera_file_path.folder, camera_file_path.name);

	fileType = getFileType((string)camera_file_path.name);

	// Create file to memory
	retval = gp_file_new(&camFile);
	if (retval) {
		string strErr = "Error in loading Image to memory, gp_file_new:" + string(gp_result_as_string(retval));
		cout << strErr << endl;
		emit sigFail(); //Stops all hardware recursively through scanmanager
		emit sigPopup(strErr, cv::Mat());
		this->camisready=true;
		return;// retval;
		//TODO: error handling
	}

	// Copy picture from camera
	retval = gp_camera_file_get(camera, camera_file_path.folder, camera_file_path.name, GP_FILE_TYPE_NORMAL, camFile, context);

	if (retval) {
		string strErr = "Error in obtaining file from camera, gp_camera_file_get:" + string(gp_result_as_string(retval));
		cout << strErr << endl;
		emit sigFail(); //Stops all hardware recursively through scanmanager
		emit sigPopup(strErr, cv::Mat());
		this->camisready=true;
		return;// retval;
		//TODO: error handling
	}

	// = Now obtain the file in memory = //
	//retval = gp_file_get_data_and_size (camFile,(const char**) &dataNef, &sizeNef);
	//sizeNef = sizeNef+1; //For some reason it doesnt write the last byte?

	if (retval) {
		string strErr = "Error in obtaining file size and data gp_file_get_data_and_size:" + string(gp_result_as_string(retval));
		cout << strErr << endl;
		emit sigFail(); //Stops all hardware recursively through scanmanager
		emit sigPopup(strErr, cv::Mat());
		this->camisready=true;
		return;// retval;
		//TODO: error handling
	}

	printf("Deleting\n");
	// remove picture from camera memory
	retval = gp_camera_file_delete(camera, camera_file_path.folder, camera_file_path.name, context);

	if (retval) {
		string strErr = "Error in deleting file in camera, gp_camera_file_delete:" + string(gp_result_as_string(retval));
		cout << strErr << endl;
		emit sigFail(); //Stops all hardware recursively through scanmanager
		emit sigPopup(strErr, cv::Mat());
		this->camisready=true;
		return;// retval;
		//TODO: error handling
	}

	this->camisready=true;
	
	//usleep(this->camid*500000); //wait 0.5s  per camid to make sure the earliest camid gets a process first
	//emit sigCapture(camFile, fileType, this->camid);



	gp_file_save(camFile, fname.c_str());


	//Copy the character information
	//char rawname[25];
	//memcpy ( rawname, camera_file_path.name, strlen(camera_file_path.name)+1 );



	/*if (!pathNef.empty()){
		FILE * pFile;
		//string fnameNef = path_testing + currentDateTime(1) + ".NEF";
		string fnameNef = (string)camera_file_path.name;
		pFile = fopen ( fnameNef.c_str() , "wb" );
		fwrite (dataNef , 1 , sizeNef , pFile );
		fclose (pFile);
	}*/

	

	// free CameraFile object
	gp_file_free(camFile); //FIXME free this!

	// Code from here waits for camera to complete everything.
	// Trying to take two successive captures without waiting
	// will result in the camera getting randomly stuck.
	int waittime = 1000; //ms
	CameraEventType type;
	void *data;

	//sleep(2);

	printf("Wait for events from camera\n");
	//while(stopShootLoop == false) {
	while(1) { //Make sure, in all cases, that we have a capture!
	retval = gp_camera_wait_for_event(camera, waittime, &type, &data, context);
		//cout << "camera wait retval=" << retval << endl;
	if(type == GP_EVENT_TIMEOUT) {
			//cout << "!!!!!!!!!!!!!!! Camera timed out, GP_EVENT_TIMEOUT" << endl;
			cout << "Camera timed out - probably ready though." << endl;
			//return false; //false
			return;
	} else if (type == GP_EVENT_CAPTURE_COMPLETE) {
			//printf("Capture completed\n");
			cout << "Capture completed" << endl;
		waittime = 200;
			break;
	} else if (type != GP_EVENT_UNKNOWN) {
			cout << "GP_EVENT_UNKNOWN with type:" << type << endl;
			return;
	}
	}
	//cout << "returning true.." << endl;
	//return true;
	////TODO use gp_file_unref somewhere
}












void gpWrapper::camset(){
	//loadICC();
	//Set correct settings
/*
	setGpCameraSettings();


	char* str;
  get_config_value_string( "iso", &str);
	printf("ISO: %s\n", str);
	//get_config_value_string(camera, "f-number", &str, context); //Nikon D800
	get_config_value_string( "aperture", &str);//Canon 5D
	printf("Aperture: %s\n", str);
	get_config_value_string( "shutterspeed", &str);
	printf("Shutter speed: %s\n", str);
*/

}











