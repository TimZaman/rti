# --------------------------------------------------------------------------------------#
#           __               /\_ \                   __                                 
#    _____ /\_\   __  _    __\//\ \    _____   _ __ /\_\    ____    ___ ___      __     
#   /\ '__`\/\ \ /\ \/'\ /'__`\\ \ \  /\ '__`\/\`'__\/\ \  /',__\ /' __` __`\  /'__`\   
#   \ \ \L\ \ \ \\/>  <//\  __/ \_\ \_\ \ \L\ \ \ \/ \ \ \/\__, `\/\ \/\ \/\ \/\ \L\.\_ 
#    \ \ ,__/\ \_\/\_/\_\ \____\/\____\\ \ ,__/\ \_\  \ \_\/\____/\ \_\ \_\ \_\ \__/.\_\
#     \ \ \/  \/_/\//\/_/\/____/\/____/ \ \ \/  \/_/   \/_/\/___/  \/_/\/_/\/_/\/__/\/_/
#      \ \_\                             \ \_\                                          
#       \/_/                              \/_/                                          
#   
#      
#                     _____                _____                    _____          
#                    /\    \              /\    \                  /\    \         
#                   /::\    \            /::\    \                /::\    \        
#                  /::::\    \           \:::\    \               \:::\    \       
#                 /::::::\    \           \:::\    \               \:::\    \      
#                /:::/\:::\    \           \:::\    \               \:::\    \     
#               /:::/__\:::\    \           \:::\    \               \:::\    \    
#              /::::\   \:::\    \          /::::\    \              /::::\    \   
#             /::::::\   \:::\    \        /::::::\    \    ____    /::::::\    \  
#            /:::/\:::\   \:::\____\      /:::/\:::\    \  /\   \  /:::/\:::\    \ 
#           /:::/  \:::\   \:::|    |    /:::/  \:::\____\/::\   \/:::/  \:::\____\
#           \::/   |::::\  /:::|____|   /:::/    \::/    /\:::\  /:::/    \::/    /
#            \/____|:::::\/:::/    /   /:::/    / \/____/  \:::\/:::/    / \/____/ 
#                  |:::::::::/    /   /:::/    /            \::::::/    /          
#                  |::|\::::/    /   /:::/    /              \::::/____/           
#                  |::| \::/____/    \::/    /                \:::\    \           
#                  |::|  ~|           \/____/                  \:::\    \          
#                  |::|   |                                     \:::\    \         
#                  \::|   |                                      \:::\____\        
#                   \:|   |                                       \::/    /        
#                    \|___|                                        \/____/         
#                                                                                  
#
# --------------------------------------------------------------------------------------#
# (c) Tim Zaman, Pixelprimsa.
#

project( rti )

cmake_minimum_required(VERSION 3.0)

#add_definitions("-Wall")
MESSAGE( STATUS "CMAKE_SYSTEM_VERSION: " ${CMAKE_SYSTEM_VERSION} " ON SYSTEM " ${CMAKE_SYSTEM_NAME} )

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")	

# Else it won't find macports stuff
SET(CMAKE_PREFIX_PATH "/opt/local/lib/cmake" ${CMAKE_PREFIX_PATH})
SET(CMAKE_LIBRARY_PATH ${CMAKE_LIBRARY_PATH} /opt/local/lib /usr/local/lib) 

# make local *.cmake available
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${CMAKE_CURRENT_SOURCE_DIR})


LINK_DIRECTORIES( ${LINK_DIRECTORIES} /opt/local/lib /usr/local/lib)
INCLUDE_DIRECTORIES( ${INCLUDE_DIRECTORIES} /opt/local/include /usr/local/include)



#LibRaw
set(LIBRAW_MIN_VERSION 0.16)

INCLUDE_DIRECTORIES(${LIBRAW_INCLUDE_DIR})

if(POLICY CMP0043) #No idea what does does but supresses warnings bonanza
   cmake_policy(SET CMP0043 OLD)
endif()

FIND_PACKAGE(OPENCL REQUIRED)

# ignore deprecated OpenCL 1.1 headers warning
ADD_DEFINITIONS(-DCL_USE_DEPRECATED_OPENCL_1_1_APIS)

#libboost
set(Boost_USE_STATIC_LIBS OFF) 
set(Boost_USE_MULTITHREADED ON)  
set(Boost_USE_STATIC_RUNTIME OFF) 
#find_package(Boost 1.45.0 COMPONENTS filesystem system thread program_options regex) 
find_package(Boost COMPONENTS filesystem system thread program_options regex) 
include_directories(${Boost_INCLUDE_DIRS}) 



	MESSAGE( STATUS "Using QT5")
	set(CMAKE_AUTOMOC ON)
	set(CMAKE_INCLUDE_CURRENT_DIR ON)
	# Widgets finds its own dependencies.
	find_package(Qt5Widgets REQUIRED)
	
	include_directories(${Qt5Widgets_INCLUDES})
	add_definitions(${Qt5Widgets_DEFINITIONS})
	set(CMAKE_CXX_FLAGS "${Qt5Widgets_EXECUTABLE_COMPILE_FLAGS}")
	QT5_WRAP_UI( DESKTOP_APP_UI_HDRS ./rti_mainwindow.ui )
	QT5_WRAP_CPP( MOC_SRCS ${MOC_HDRS} ${UI_HDRS} rti.h photometricstereo.h trianglewindow.h openglwindow.h mainwidget.h geometryengine.h imagearea.h OPTIONS -DBOOST_TT_HAS_OPERATOR_HPP_INCLUDED )
	#file (GLOB Resources ./qdarkstyle/style.qrc) 
	file (GLOB Resources ./shaders.qrc ./textures.qrc ./icons.qrc ./qdarkstyle/style.qrc) 
	QT5_ADD_RESOURCES (ResourceSources ${Resources}) 
	INCLUDE_DIRECTORIES( ${CMAKE_BINARY_DIR} . )


#OpenCV
find_package(OpenCV REQUIRED)
set(OpenCV_LIBS opencv_core opencv_imgproc opencv_highgui opencv_ocl)
#set(OpenCV_LIBS opencv_core opencv_imgproc opencv_calib3d opencv_video opencv_features2d opencv_ml opencv_highgui opencv_objdetect opencv_contrib opencv_legacy)



FIND_PACKAGE(TIFF REQUIRED)
INCLUDE_DIRECTORIES(${TIFF_INCLUDE_DIR})



if (APPLE)
	LINK_DIRECTORIES(/opt/local/lib/libgphoto2/2.5.7)
	LINK_DIRECTORIES(/opt/local/lib/libgphoto2_port/0.12.0)
else () #UNIX
	#?
endif()


if(APPLE) #Workaround for packaging, we need to include usblibs
	set(GPHOTO_LIBS gphoto2 gphoto2_port usb-1.0)# usb-0.1.4)
else()
	set(GPHOTO_LIBS gphoto2 gphoto2_port)# usb-1.0)# usb-0.1.4)
endif()

set(IMAGING_LIBS tiff jpeg exif exiv2 lcms2 raw_r config++ stdc++) #openjpeg





#OpenMP Does not work on Mac
if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
	MESSAGE( STATUS "Enabling OpenMP support because we are running GNU GCC")
	find_package(OpenMP)
	if (OPENMP_FOUND)
	    set (CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${OpenMP_C_FLAGS}")
	    set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${OpenMP_CXX_FLAGS}")
	endif()
	set(IMAGING_LIBS ${IMAGING_LIBS} gomp)
else()
	MESSAGE( STATUS "Disabled OpenMP because not using GNU GCC")
endif()



# Add an executable compiled from .cpp
add_executable( rti main.cpp rti.cpp setcontrol2.cpp gpWrapper.cpp utils.cpp photometricstereo.cpp mainwidget.cpp geometryengine.cpp oclutils.cpp trianglewindow.cpp openglwindow.cpp imagearea.cpp ${DESKTOP_APP_UI_HDRS} ${ResourceSources} )#${CMAKE_CURRENT_BINARY_DIR}/svnversion.h)


qt5_use_modules(rti Widgets Gui OpenGL Core  PrintSupport )



# Link the target to the libraries
target_link_libraries( rti ${CMAKE_THREAD_LIBS_INIT}  ${OPENCL_LIBRARIES} ${TIFF_LIBS} ${Boost_LIBRARIES} ${GPHOTO_LIBS} ${IMAGING_LIBS} ${OpenCV_LIBS} ${LIBRAW_LIBRARIES}  ${QT_LIBRARIES})




#SVN VERSION STUFF

# include the output directory, where the svnversion.h file is generated
#include_directories(${CMAKE_CURRENT_BINARY_DIR})
# a custom target that is always built
#add_custom_target(svnheader ALL
#    DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/svnheader.h)

# creates svnheader.h using cmake script
#add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/svnheader.h
#    COMMAND ${CMAKE_COMMAND} -DSOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}
#                         -P ${CMAKE_CURRENT_SOURCE_DIR}/getsvn.cmake)

# svnversion.h is a generated file
#set_source_files_properties(${CMAKE_CURRENT_BINARY_DIR}/svnversion.h
#    PROPERTIES GENERATED TRUE
#    HEADER_FILE_ONLY TRUE)

# explicitly say that the executable depends on the svnheader
#add_dependencies(rti svnheader)





