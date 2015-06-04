#/bin/sh
# Easy Compile Script 
# (c) Tim Zaman, Pixelprisma 2014

execname="rti"

#Loop over the arguments
for var in "$@"
do
	#echo "$var"
	case $var in
		"help") 
			echo -e "\033[33;33mCommand\tFunction\x1B[0m"
			echo -e "  rm\tRemoves compiled result"
			echo -e "  make\tMakes/Compiles the program"
			echo -e "  package\tPackages app"
			echo -e "  zip\tPackages and zips app"
			echo -e "  run\tRuns program"
			echo -e "  doxygen\tRuns Doxygen"
			echo -e ""
			exit
		;;	

		"rm")
			echo -e "\033[33;33m|| Refreshing compile folder..\x1B[0m"
			if [ -d "_build" ]; then #If the folder exists..
				rm -r _build #Remove it
			fi
		;;

		"make") 
			if ! [ -d "_build" ]; then #If the folder does not exist yet
				mkdir _build #Make the folder
			fi

			cd _build

			if [[ "$OSTYPE" == "darwin"* ]]; then
				generator="XCode"
				ARGS_CMAKE=" "
				cmake -H.. -B. -GXcode ${ARGS_CMAKE} | awk ' /succeeded/ {print "\033[32m" $0 "\033[39m"} /error/ {print "\033[31m" $0 "\033[39m"}'
			else
				generator="Unix Makefiles"
				ARGS_CMAKE=" "
				cmake .. -G "$generator" ${ARGS_CMAKE} | awk ' /succeeded/ {print "\033[32m" $0 "\033[39m"} /error/ {print "\033[31m" $0 "\033[39m"}'
			fi

			if [ ${PIPESTATUS[0]} -eq 0 ] ; then
			    echo -e "\033[33;32m CMake succeeded.\x1B[0m"
			else
			    echo -e "\033[33;31m CMake failed.\x1B[0m"
			    exit 1
			fi

			#Then run make

			echo -e "\033[33;33m|| Running Make..\x1B[0m"


			if [[ "$OSTYPE" == "darwin"* ]] ; then
				cmake --build  . | awk ' /succeeded/ {print "\033[32m" $0 "\033[39m"} /error/ {print "\033[31m" $0 "\033[39m"}'
			else
				make | awk ' /succeeded/ {print "\033[32m" $0 "\033[39m"} /error/ {print "\033[31m" $0 "\033[39m"}'
			fi


			if [ ${PIPESTATUS[0]} -eq 0 ]; then
			    echo -e "\033[33;32m Make succeeded.\x1B[0m"
			else
			    echo -e "\033[33;31m Make failed.\x1B[0m"
			    exit 1
			fi


			if [[ "$OSTYPE" == "darwin"* ]]; then
				echo -e "\033[33;33m|| Copying executable to _build directory.\x1B[0m"
				cp ./Debug/* .
			fi

			cd ..
		;;

		"package") 
			echo -e "\033[33;33m|| Packaging Program..\x1B[0m"
			cd ../../package/
			./package.pl
			cd ../src/
		;;

		"zip") 
			echo -e "\033[33;33m|| Packaging and Zipping Program..\x1B[0m"
			cd ../package/
			./package.pl zip
			cd ../src/
		;;	

		"run") 
			cd _build
			echo -e "\033[33;33m|| Running Program..\x1B[0m"
			./$execname
			cd ..
		;;

		"doxygen") 
			if [ -f "Doxyfile" ]; then #If the folder exists..
				echo -e "\033[33;33m|| Running Doxygen..\x1B[0m"
				doxygen | awk ' /succeeded/ {print "\033[32m" $0 "\033[39m"} /error/ {print "\033[31m" $0 "\033[39m"}'
			fi
		;;	   
	esac
done


echo -e "\033[33;35m|| END\x1B[0m"


#echo -e "\033[33;31m Color Text" - red
#echo -e "\033[33;32m Color Text" - green
#echo -e "\033[33;33m Color Text" - yellow
#echo -e "\033[33;34m Color Text" - blue
#echo -e "\033[33;35m Color Text" - Magenta
#echo -e "\033[33;30m Color Text" - Gray
#echo -e "\033[33;36m Color Text" - Cyan

