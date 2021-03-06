cmake_minimum_required(VERSION 2.8.3)
project(package_delivery)

## Compile as C++11, supported in ROS Kinetic and newer
add_compile_options(-std=c++11 -g -fno-omit-frame-pointer)
set (common_utility_src $ENV{HOME}/catkin_ws/src/mav-bench/common)
set (common_utility_include $ENV{HOME}/catkin_ws/src/mav-bench/common)


set(CUDA_USE_STATIC_CUDA_RUNTIME OFF)
## Find catkin macros and libraries
## if COMPONENTS list like find_package(catkin REQUIRED COMPONENTS xyz)
## is used, also find other catkin packages
find_package(catkin REQUIRED COMPONENTS
  cv_bridge
  image_transport
  roscpp
  std_msgs
  geometry_msgs
  sensor_msgs
  pcl_conversions
  pcl_ros
  tf
  octomap_ros
  octomap_msgs
  octomap_server
  message_generation
  trajectory_msgs
  mav_trajectory_generation
  mav_trajectory_generation_ros
  nlopt
  minkindr_conversions
  octomap_world
)

## System dependencies are found with CMake's conventions
# find_package(Boost REQUIRED COMPONENTS system)

## Uncomment this if the package has a setup.py. This macro ensures
## modules and global scripts declared therein get installed
## See http://ros.org/doc/api/catkin/html/user_guide/setup_dot_py.html
# catkin_python_setup()

################################################
## Declare ROS messages, services and actions ##
################################################

## To declare and build messages, services or actions from within this
## package, follow these steps:
## * Let MSG_DEP_SET be the set of packages whose message types you use in
##   your messages/services/actions (e.g. std_msgs, actionlib_msgs, ...).
## * In the file package.xml:
##   * add a build_depend tag for "message_generation"
##   * add a build_depend and a run_depend tag for each package in MSG_DEP_SET
##   * If MSG_DEP_SET isn't empty the following dependency has been pulled in
##     but can be declared for certainty nonetheless:
##     * add a run_depend tag for "message_runtime"
## * In this file (CMakeLists.txt):
##   * add "message_generation" and every package in MSG_DEP_SET to
##     find_package(catkin REQUIRED COMPONENTS ...)
##   * add "message_runtime" and every package in MSG_DEP_SET to
##     catkin_package(CATKIN_DEPENDS ...)
##   * uncomment the add_*_files sections below as needed
##     and list every .msg/.srv/.action file to be processed
##   * uncomment the generate_messages entry below
##   * add every package in MSG_DEP_SET to generate_messages(DEPENDENCIES ...)

## Generate messages in the 'msg' folder
 add_message_files(
  FILES
  multiDOF.msg
  multiDOF_array.msg
  BoolPlusHeader.msg
  )

## Generate services in the 'srv' folder
add_service_files(
  FILES
  get_trajectory.srv
  follow_trajectory_status_srv.srv
)

## Generate actions in the 'action' folder
# add_action_files(
#   FILES
#   Action1.action
#   Action2.action
# )

## Generate added messages and services with any dependencies listed here
generate_messages(
  DEPENDENCIES
  std_msgs
  geometry_msgs
  trajectory_msgs
)

################################################
## Declare ROS dynamic reconfigure parameters ##
################################################

## To declare and build dynamic reconfigure parameters within this
## package, follow these steps:
## * In the file package.xml:
##   * add a build_depend and a run_depend tag for "dynamic_reconfigure"
## * In this file (CMakeLists.txt):
##   * add "dynamic_reconfigure" to
##     find_package(catkin REQUIRED COMPONENTS ...)
##   * uncomment the "generate_dynamic_reconfigure_options" section below
##     and list every .cfg file to be processed

## Generate dynamic reconfigure parameters in the 'cfg' folder
# generate_dynamic_reconfigure_options(
#   cfg/DynReconf1.cfg
#   cfg/DynReconf2.cfg
# )

###################################
## catkin specific configuration ##
###################################
## The catkin_package macro generates cmake config files for your package
## Declare things to be passed to dependent projects
## INCLUDE_DIRS: uncomment this if you package contains header files
## LIBRARIES: libraries you create in this project that dependent projects also need
## CATKIN_DEPENDS: catkin_packages dependent projects also need
## DEPENDS: system dependencies of this project that dependent projects also need

find_package(OpenCV REQUIRED)
find_package(Eigen3 REQUIRED)
find_package(ompl REQUIRED)

catkin_package(
#  INCLUDE_DIRS include
#  LIBRARIES mavbench
#  CATKIN_DEPENDS cv_bridge image_transport roscpp std_msgs
#  DEPENDS system_lib
)

###########
## Build ##
###########

set (Airlib_addr  $ENV{HOME}/AirSim/AirLib)
## Specify additional locations of header files
## Your package locations should be listed before other locations
include_directories(
# include
  ${catkin_INCLUDE_DIRS}
  ${OpenCV_INCLUDE_DIRS}
  ${EIGEN3_INCLUDE_DIR}
  ${OCTOMAP_INCLUDE_DIRS}
  ${common_utility_include} 
  $ENV{HOME}/AirSim/AirLib/include
)

link_directories(
  $ENV{HOME}/AirSim/AirLib/lib
  $ENV{HOME}/AirSim/AirLib/deps/rpclib/lib
  /usr/local/cuda/lib64  #added by Behzad (for Behzad)
  ${Airlib_addr}/deps/MavLinkCom/lib  
  )

## Declare a C++ library
# add_library(${PROJECT_NAME}
#   src/${PROJECT_NAME}/mavbench.cpp
# )

## Add cmake target dependencies of the library
## as an example, code may need to be generated before libraries
## either from message generation or dynamic reconfigure
# add_dependencies(${PROJECT_NAME} ${${PROJECT_NAME}_EXPORTED_TARGETS} ${catkin_EXPORTED_TARGETS})

## Declare a C++ executable
## With catkin_make all packages are built within a single CMake context
## The recommended prefix ensures that target names across packages don't collide
# add_executable($airsim_imgs src/imgs.cpp)

## Rename C++ executable without prefix
## The above recommended prefix causes long target names, the following renames the
## target back to the shorter version for ease of user use
## e.g. "rosrun someones_pkg node" instead of "rosrun someones_pkg someones_pkg_node"
# set_target_properties(${PROJECT_NAME}_node PROPERTIES OUTPUT_NAME node PREFIX "")

## Add cmake target dependencies of the executable
## same as for the library above
# add_dependencies(${PROJECT_NAME}_node ${${PROJECT_NAME}_EXPORTED_TARGETS} ${catkin_EXPORTED_TARGETS})

## Specify libraries to link a library or executable target against
# target_link_libraries(${PROJECT_NAME}_node
#   ${catkin_LIBRARIES}
# )

add_executable(motion_planner src/motion_planner.cpp src/graph.cpp src/global_planner.cpp ${common_utility_src}/common.cpp ${common_utility_src}/Profiling.cpp  ${common_utility_src}/Drone.cpp ${common_utility_src}/timer.cpp)
add_dependencies(motion_planner package_delivery_generate_messages_cpp)
target_link_libraries(motion_planner ${catkin_LIBRARIES} ${OMPL_LIBRARIES}  AirLib MavLinkCom rpc pthread  cuda cudart cublas curand )

add_executable(package_delivery ${common_utility_src}/Drone.cpp ${common_utility_src}/control_drone.cpp 
    ${common_utility_src}/common.cpp  ${common_utility_src}/Profiling.cpp src/package_delivery.cpp ${common_utility_src}/timer.cpp)
add_dependencies(package_delivery package_delivery_generate_messages_cpp)
target_link_libraries(package_delivery ${catkin_LIBRARIES} AirLib MavLinkCom rpc pthread ${OpenCV_LIBS})


#add_executable(panic src/panic.cpp)
#target_link_libraries(panic ${catkin_LIBRARIES} AirLib MavLinkCom rpc pthread ${OpenCV_LIBS})

add_executable(panic_pcl ${common_utility_src}/panic_pcl.cpp ${common_utility_src}/common.cpp ${common_utility_src}/Profiling.cpp ${common_utility_src}/Drone.cpp ${common_utility_src}/timer.cpp)
target_link_libraries(panic_pcl ${catkin_LIBRARIES} AirLib MavLinkCom rpc pthread ${OpenCV_LIBS})

add_executable(future_collision src/future_collision ${common_utility_src}/Drone.cpp ${common_utility_src}/timer.cpp)
target_link_libraries(future_collision ${catkin_LIBRARIES} AirLib MavLinkCom rpc pthread ${OpenCV_LIBS})

add_executable(scanning ${common_utility_src}/Drone.cpp ${common_utility_src}/common.cpp 
    ${common_utility_src}/Profiling.cpp ${common_utility_src}/control_drone.cpp src/scanning.cpp ${common_utility_src}/timer.cpp)
add_dependencies(scanning package_delivery_generate_messages_cpp)
target_link_libraries(scanning ${catkin_LIBRARIES} AirLib MavLinkCom rpc pthread ${OpenCV_LIBS})


add_executable(follow_trajectory src/follow_trajectory.cpp ${common_utility_src}/common.cpp ${common_utility_src}/Profiling.cpp ${common_utility_src}/Drone.cpp ${common_utility_src}/timer.cpp)
add_dependencies(follow_trajectory package_delivery_generate_messages_cpp)
target_link_libraries(follow_trajectory ${catkin_LIBRARIES} AirLib MavLinkCom rpc pthread)


add_executable(obstacle_avoidance_test ${common_utility_src}/Drone.cpp ${common_utility_src}/common.cpp 
     ${common_utility_src}/Profiling.cpp ${common_utility_src}/control_drone.cpp src/obstacle_avoidance_test.cpp ${common_utility_src}/timer.cpp)
add_dependencies(obstacle_avoidance_test obstacle_avoidance_test_generate_messages_cpp)
target_link_libraries(obstacle_avoidance_test ${catkin_LIBRARIES} AirLib MavLinkCom rpc pthread ${OpenCV_LIBS})


#

#add_executable(Drone_dummy_service_adv src/Drone_dummy_service_adv.cpp src/Drone.cpp)
#target_link_libraries(Drone_dummy_service_adv ${catkin_LIBRARIES} AirLib MavLinkCom rpc pthread ${OpenCV_LIBS})

#add_executable(Drone_dummy_client_adv src/Drone_dummy_client_adv.cpp src/Drone.cpp)
#target_link_libraries(Drone_dummy_client_adv ${catkin_LIBRARIES} AirLib MavLinkCom rpc pthread ${OpenCV_LIBS})


#############
## Install ##
#############

# all install targets should use catkin DESTINATION variables
# See http://ros.org/doc/api/catkin/html/adv_user_guide/variables.html

## Mark executable scripts (Python etc.) for installation
## in contrast to setup.py, you can choose the destination
# install(PROGRAMS
#   scripts/my_python_script
#   DESTINATION ${CATKIN_PACKAGE_BIN_DESTINATION}
# )

## Mark executables and/or libraries for installation
# install(TARGETS ${PROJECT_NAME} ${PROJECT_NAME}_node
#   ARCHIVE DESTINATION ${CATKIN_PACKAGE_LIB_DESTINATION}
#   LIBRARY DESTINATION ${CATKIN_PACKAGE_LIB_DESTINATION}
#   RUNTIME DESTINATION ${CATKIN_PACKAGE_BIN_DESTINATION}
# )

## Mark cpp header files for installation
# install(DIRECTORY include/${PROJECT_NAME}/
#   DESTINATION ${CATKIN_PACKAGE_INCLUDE_DESTINATION}
#   FILES_MATCHING PATTERN "*.h"
#   PATTERN ".svn" EXCLUDE
# )

## Mark other files for installation (e.g. launch and bag files, etc.)
# install(FILES
#   # myfile1
#   # myfile2
#   DESTINATION ${CATKIN_PACKAGE_SHARE_DESTINATION}
# )

#############
## Testing ##
#############

## Add gtest based cpp test target and link libraries
# catkin_add_gtest(${PROJECT_NAME}-test test/test_mavbench.cpp)
# if(TARGET ${PROJECT_NAME}-test)
#   target_link_libraries(${PROJECT_NAME}-test ${PROJECT_NAME})
# endif()

## Add folders to be run by python nosetests
# catkin_add_nosetests(test)
