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
