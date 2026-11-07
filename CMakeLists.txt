cmake_minimum_required(VERSION 3.21)

project(
	hello-ps1
	LANGUAGES C CXX ASM
	VERSION 1.0.0
	DESCRIPTION "A tiny PlayStation 1 hello world game"
)

psn00bsdk_add_executable(hello GPREL main.c)

psn00bsdk_add_cd_image(
	iso
	hello
	iso.xml
	DEPENDS hello system.cnf
)

add_custom_command(
	OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/car_model.h"
	COMMAND "${CMAKE_COMMAND}"
		"-DINPUT_DIR=${CMAKE_CURRENT_SOURCE_DIR}/car"
		"-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/car_model.h"
		-P "${CMAKE_CURRENT_SOURCE_DIR}/obj_to_cube.cmake"
	DEPENDS car/Car.obj car/Weel1.obj car/Weel2.obj car/Weel3.obj car/Weel4.obj obj_to_cube.cmake
	VERBATIM
)

psn00bsdk_add_executable(cube_demo GPREL cube_demo.c road.c wheel_texture.c "${CMAKE_CURRENT_BINARY_DIR}/car_model.h")
target_include_directories(cube_demo PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")

psn00bsdk_add_cd_image(
	cube_iso
	cube_demo
	cube_iso.xml
	DEPENDS cube_demo cube_system.cnf
)
