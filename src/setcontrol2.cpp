/****************************************************************************
**
** Copyright (C) 2015 Tim Zaman.
** Contact: http://www.timzaman.com/ , timbobel@gmail.com
**
**
****************************************************************************/

#include "setcontrol2.h"


using namespace std;


boost::asio::serial_port_base::baud_rate setBAUD(115200);
boost::asio::serial_port_base::parity setPARITY(boost::asio::serial_port_base::parity::none);
boost::asio::serial_port_base::stop_bits setSTOP(boost::asio::serial_port_base::stop_bits::one);
boost::asio::io_service setio;
boost::asio::serial_port setport(setio);


setcontrol2::setcontrol2(){
	cout << "Constructing setcontrol2::setcontrol2()" << endl;
	serialConnected=false; 

	this->ms_blockingReaderTimeout = 250; //ms (50 messes up things, too fast)
	this->ms_reconnectIntervalTimer = 60000; //ms



	//TODO: make sure slotReConnect is called at least once on bootup (when a device as connected before bootup f.e.)

	this->timerSetConnector = new QTimer(this);
	this->timerSetConnector->setInterval(this->ms_reconnectIntervalTimer); //ms
	connect(this->timerSetConnector, SIGNAL(timeout()), SLOT(slotReConnect()));
	this->timerSetConnector->start();

	


	this->isStopped=false;


	//Try to connect immedaitelly
	slotReConnect();
}


setcontrol2::~setcontrol2(){
	cout << "Destructing setcontrol2::~setcontrol2()" << endl;
	if (this->serialConnected){
		setport.close();	
	}
	emit sigSetIsConnected(false);
}


void setcontrol2::slotDevChanged(QString dir){
	cout << "slotDevChanged(" << dir.toStdString() << ")" << endl;
	slotReConnect();
}


void setcontrol2::slotStop(){
	//Slot when the software should stop whatever it is doing
	this->isStopped=true;
}

void setcontrol2::slotReConnect(){
	cout << "setcontrol2::slotReConnect()" << endl;
	//This function is looped every X, and attempts reconnect

	if (!this->serialConnected){
		this->serialConnected = serialOpen();
	} else { //It's already connected, check if it's still okay and open
		//TODO: check if the port is still existing and open?
	}

	if (this->serialConnected){
		boost::this_thread::sleep( boost::posix_time::seconds(1) ); //Small delay to let it boot..
		readOnce(); //Is this neccesary? to clear buffer?
		emit sigSetIsConnected(this->serialConnected);
		this->timerSetConnector->stop();  //TODO: REMOVEME, because the timer should now check if the connection stays alive
	} //do not send a 'not-connected' signal because it mess up a living conneciton
	
}


void setcontrol2::slotPollSignal(){
	//This functions polls for a signal, and emits a ready when it's ready
	if (!this->serialConnected){
		string strErr = "Requested a set-poll, but set-hardware is not connected!";
		cout << strErr << endl;
		emit sigPopup(strErr, cv::Mat());
		return;
	}

	if (this->isStopped==true){
		cout << "slotPollSignal() has stopped polling." << endl;
		this->isStopped=false; //If this function is called, it always implies we continue (not stop)
		return;
	}

	while (1){

		QCoreApplication::processEvents(); //Obtain latest received signals and such

		if (this->isStopped==true){ //Check stop signal (also checked after processevents)
			cout << "slotPollSignal() has stopped polling." << endl;
			return;
		}

		//First ask for data..
		serialWrite((string)"?"); //Polling serial

		// A blocking reader for this port that
		// will time out a read after X milliseconds.
		blocking_reader reader(setport, this->ms_blockingReaderTimeout);

		char cTemp;
		string strReceived;

		// read from the serial port until we get a
		// \n or until a read times-out
		while (reader.read_char(cTemp) && cTemp != '\n') {
			strReceived += cTemp;
		}

		//Get the last character of the string (latest data)
		char cLast = *strReceived.rbegin();

		if (cLast=='1'){
			cout << "Received set ready!" << endl;
			emit sigSetIsReady(true); //Yay, ready, send a ready signal
			return;
		} 
	} //end while

}



















void setcontrol2::readOnce(){
	cout << "setcontrol2::readOnce()" << endl;

	//Then we read its output until endline:
	bool goReceived = false;

	// A blocking reader for this port that
	// will time out a read after X milliseconds.
	blocking_reader reader(setport, this->ms_blockingReaderTimeout);

	char cTemp;
	string strReceived;

	// read from the serial port until we get a
	// \n or until a read times-out
	while (reader.read_char(cTemp) && cTemp != '\n') {
		strReceived += cTemp;
	}

	if (cTemp != '\n') {
		cout << "Reading serial timed out..?" << endl;
		return;
	}

	cout << "Received from serial: '" << strReceived << "'" << endl;
}





void setcontrol2::slotSendContinue(){
	this->serialWrite("1");
}


void setcontrol2::serialWrite(string strWrite){
	cout << "writing to serial:" << strWrite << endl;
	if (this->serialConnected==true){
		write(setport, boost::asio::buffer(strWrite.c_str(), strWrite.size()));
	} else {
		string strErr = "Warning: cannot write to serial port: Not connected!";
		cout << strErr << endl;
		emit sigPopup(strErr, cv::Mat());
	}
}


bool setcontrol2::serialOpen(){ //Returns true if opening worked, false if not.

	string portName = serialGetPort();
	bool connectedNow=false;

	if (portName.empty()){
		cout << " setcontrol2 did not find a suitable serial device." << endl;
		connectedNow = false;
	} else {
		cout << "Using port name: '" << portName << "'" << endl;
		try{
			cout << " Opening serial communication.." << endl;
			setport.open(portName.c_str());
			setport.set_option(boost::asio::serial_port_base::baud_rate(115200));
			sleep(2); // Take some time to boot
			cout << " Serial communication opened.." << endl;
			connectedNow = true;
		} catch(boost::system::system_error& e) {
			string strErr = "Serial port opening failed, error #" + boost::lexical_cast<std::string>(e.what());
			cout << strErr << endl;
			emit sigPopup(strErr, cv::Mat());
			connectedNow = false;
		}
	}

	return connectedNow;
}

std::string setcontrol2::serialGetPort(){
	//This returns the potential serial port(s)
	string emptyString;

	QDir recoredDir("/dev/");
	QStringList filters;
	filters << "tty.usbserial*"; //OS-X Format
	filters << "tty.ttyACM*"; //Linux 
	filters << "ttyACM*"; //Linux 
	filters << "ttyUSB*"; //Linux - For 'old' FTDI USB driver
	filters << "tty.ttyUSB*"; //Linux - Legacy FTDI USB driver
	filters << "tty.usbmodem*"; //Linux - Legacy FTDI USB driver

	QStringList allFiles = recoredDir.entryList(filters, QDir::NoDotAndDotDot | QDir::System | QDir::Hidden | QDir::Files, QDir::NoSort);//(QDir::Filter::Files,QDir::SortFlag::NoSort)

	if (allFiles.size()==0){
		//cout << "setcontrol2 did not find a suitable serial device." << endl;
		return emptyString;
	} 

	cout << " setcontrol2 found " << allFiles.size() << " potential serial devices." << endl;

	for (int i=0; i<allFiles.size(); i++){
		string portname = "/dev/" + allFiles[i].toStdString();
		
		if (portname.find("PF") != std::string::npos) {
			cout << "Found port " << portname << " but ignoring since it's a ProFoto flashbox and not a Picturae 'set'" << endl;
			continue;
		}

		cout << " valid portname=" << portname <<  " found!" << endl;
		return portname;
	}

	return emptyString;
}

















