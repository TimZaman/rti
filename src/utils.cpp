//utils.cpp

#include "utils.h"


namespace po = boost::program_options;
namespace fs = boost::filesystem;

std::string util::escapeRegex(std::string str) {
  str = ReplaceAll(str, "/", "\\/");
  str = ReplaceAll(str, ".", "\\.");
  str = ReplaceAll(str, "{", "\\{");
  str = ReplaceAll(str, "}", "\\}");
  return str;
}

// NLS
// Original/{%04d}.tif
// Bla/Original/1234.tif
// Original\/.*\.tif
// (?<=Original\/)\d{4}(?=.tif)     //obtains only the digits! :)
// SAN
// {datamatrix}/{datamatrix}_{%04d}.tif
// 1823-1234/1823-1234_4321.tif
// .*\/.*_\d{4}\.tif


std::string util::regex_escape(const std::string& string_to_escape) {
  static const boost::regex re_boostRegexEscape( "[\\^\\.\\$\\|\\(\\)\\[\\]\\*\\+\\?\\/\\\\]" );
  const std::string rep( "\\\\\\1&" );
  std::string result = regex_replace(string_to_escape, re_boostRegexEscape, rep, boost::match_default | boost::format_sed);
  return result;
}

QImage util::Mat2QImage(const cv::Mat3b &src) {
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


std::string util::fileformatToRegex(std::string fileformat ){
  //Escape chars for regex first..
  //Change {%04d} to \d{4}
  //Change {datamatrix} to .*
  //Change {manual} to .*

  string strRegex;
  
  if (fileformat.size() < 2){
    cout << "Input fileformat too small (" << fileformat.size() << "). Returning." << endl;
    return strRegex;
  }

  //First escape the regex
  strRegex = regex_escape(fileformat);
  cout << "fileformat=" << fileformat << endl;
  cout << "strRegex=" << strRegex << endl;


  boost::regex regex_num("\\{(%[0-9]*d)\\}");
  strRegex = boost::regex_replace(strRegex, regex_num, "\\\\d{4}");//TODO use good amount of digits
  cout << "strRegex=" << strRegex << endl;

  //boost::regex regex_anyBrace("\\{(.*?)\\}");
  boost::regex regex_anyBrace("(?<!\\\\d)\\{(.*?)\\}");//(?<!\\d){(.*?)}
  strRegex = boost::regex_replace(strRegex, regex_anyBrace, ".*");

  cout << "strRegex=" << strRegex << endl;
  
  //Now put a a priory regex in
  //strRegex.insert(0, ".*"); //Accept any prependage


  return strRegex;
}



//std::vector<std::string> regexReplaceInVector(vecFILENAMING, number_now, "\\{(%[0-9]*d)\\}"){
std::vector<std::string> util::regexReplaceInVector(std::vector<std::string> vecNames, std::string strInsert, std::string strRegex){
  boost::regex regexNow(strRegex);
  for (int i=0;i<vecNames.size();i++){
    vecNames[i] = boost::regex_replace(vecNames[i], regexNow, strInsert);
  }
  return vecNames;
}





std::string util::ReplaceAll(std::string str, const std::string& from, const std::string& to) {
    size_t start_pos = 0;
    while((start_pos = str.find(from, start_pos)) != std::string::npos) {
        str.replace(start_pos, from.length(), to);
        start_pos += to.length(); // Handles case where 'to' is a substring of 'from'
    }
    return str;
}

vector<string> util::getRegexMatches(std::string strRegex, std::string strMatches){
  cout << "getRegexMatches(" << strRegex << "," << strMatches << ")" << endl;
  vector<string> vecMatches;
  boost::regex e(strRegex); 
  std::string chat_input(strMatches);
  boost::match_results<std::string::const_iterator> results;
  if (boost::regex_match(chat_input, results, e)){
    for (int i=1; i<results.size(); i++){
      vecMatches.push_back(results[i]);
    }
  }
  for (int i=0;i<vecMatches.size();i++){
    cout << "vecMatches[" << i << "]=" << vecMatches[i] << endl;
  }
  return vecMatches;
}



std::map<std::string, std::string> util::relateFormatAndFile(std::string strFormat, std::string strFilename){
  // ex: relateFormatAndFile("/tif/{var}_{\%04d}.tif","/home/bla/tif/test_01234.tif");
  cout << "relateFormatAndFile(" << strFormat << ", " << strFilename << ")" << endl;
  std::map<std::string, std::string> mapFormatAndFile;

  string strFormatEsc, strFilenameEsc;

  //Escape the strings
  strFormatEsc = escapeRegex(strFormat);

  cout << "Escaped = " << strFormatEsc << ", " << strFilename << "" << endl;


  //    \/tif\/\{var\}_\{%04d\}\.tif
  boost::regex regex_num(".\\{(.*?)\\}");
  string strFormatRx;
  strFormatEsc = ".*"+strFormatEsc; //Match any prefix (like hotfolder dir etc)
  strFormatRx = boost::regex_replace(strFormatEsc, regex_num, "(.*?)"); //TODO get correct amount of numbers    ->THIS CRASHES ON UBUNTU+NLS?
  cout << "strFormatRx=" << strFormatRx << endl;


    vector<string> vecMatchesFormat = getRegexMatches(strFormatRx, strFormat);
    vector<string> vecMatchesFilename = getRegexMatches(strFormatRx, strFilename);

    if (vecMatchesFormat.size() != vecMatchesFilename.size()){
      cout << "vecMatches sizes do not correspond, returning." << endl;
      return mapFormatAndFile; //No match here.
    }

  for (int i=0;i<vecMatchesFormat.size();i++){
    cout << vecMatchesFormat[i] << "=" << vecMatchesFilename[i] << endl;
    mapFormatAndFile[vecMatchesFormat[i]] = vecMatchesFilename[i];
  }

  //cout << " === END relateFormatAndFile()" << endl;
  return mapFormatAndFile;
}



std::vector<std::string> util::correlateFileVectorFormat(std::vector<std::string> vecFormats, std::string filename, int numAdd, int &numNow, std::vector<std::string> &vecNumFormats){
	cout << "correlateFileVectorFormat(.., filename=" << filename << ")" << endl;
	//vecNumFormats is everything in the format replaced except the actual number;
	//numNow is the current number, numAdd is the number to be added.

	std::map<std::string, std::string> mapFormatToFile;
	for (int i=0; i< vecFormats.size();i++){
		cout << vecFormats[i] << endl;
		mapFormatToFile = relateFormatAndFile(vecFormats[i], filename);
		cout << "map size=" << mapFormatToFile.size() << endl;
		//foreach(auto i, mapFormatToFile){
		//  cout << boost::format("%d = %s\n") % i.first % i.second;
		//}
		if (mapFormatToFile.size()>0){
			break; //We found it, only 1 match is possible because we chose 1 filename obviously.
		}
	}

	if (mapFormatToFile.size()==0){
		cout << "mapFormatToFile.size()==0 !" << endl;
		return vecFormats;
	}

	vecNumFormats = vecFormats;
	string number_string;
	/*
	foreach(auto i, mapFormatToFile){
		cout << boost::format("%d = %s\n") % i.first % i.second;
		if (i.first[1]=='%' && i.first[i.first.length()-2]=='d'){ //Skip .%*d. files (numberformats)
			number_string = i.second;
			continue;
		}
		vecNumFormats = regexReplaceInVector(vecNumFormats, i.second, escapeRegex(i.first));
	}
	*/




	typedef std::map<std::string, std::string>::iterator it_type;
	for(it_type i = mapFormatToFile.begin(); i != mapFormatToFile.end(); i++) {
		// i->first = key
		// i->second = value
	
		cout << boost::format("%d = %s\n") % i->first % i->second;
		if (i->first[1]=='%' && i->first[i->first.length()-2]=='d'){ //Skip .%*d. files (numberformats)
			number_string = i->second;
			continue;
		}
		vecNumFormats = regexReplaceInVector(vecNumFormats, i->second, escapeRegex(i->first));
	}

	for (int i=0; i<vecNumFormats.size(); i++){
		cout << "vecNumFormats[" << i << "]=" << vecNumFormats[i] << endl;
	}


	//Now fill in the extracted variables in the vecFILENAMING vector.
	//vecNumFormats = regexReplaceInVector(vecFormats, barcode_now, "\\{datamatrix\\}");

	//string number_string  = getNumberFromFileFormat(vecNumFormats[formatID], filename);
	cout << "number_string=" << number_string << endl;

	numNow = boost::lexical_cast<int>( number_string ) +numAdd;

	if (numAdd!=0){
		string strSize = boost::lexical_cast<string>(number_string.size());
		string strNumFormat = "%0"+ strSize +"i";  //Makes the format the same size as string number
		number_string = boost::str(boost::format(strNumFormat.c_str()) % (numNow));
	}

	//Now fill in the extracted variables in the vecFILENAMING vector.
	vecFormats = regexReplaceInVector(vecNumFormats, number_string, "\\{(%[0-9]*d)\\}");

	//cout << " === SUCCESS correlateFileVectorFormat()" << endl;
	return vecFormats;
}


std::vector<std::string> util::folderFilesToVector(string folder, string containment){ //regex is default "" in .h
	vector<string> vecFileNames;
	fs::path directory(folder);
	fs::directory_iterator iter(directory), end;
	for(;iter != end; ++iter){
		//if (iter->path().extension() == ".*"){
		string file = iter->path().filename().string();
    if (containment.empty()){
      vecFileNames.push_back(file);
    } else if (boost::contains(file, containment)){
  		vecFileNames.push_back(file);
    }
	}
	return vecFileNames;
}

