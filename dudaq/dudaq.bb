#
# This file is the dudaq recipe.
#

SUMMARY = "Simple dudaq application"
SECTION = "PETALINUX/apps"
LICENSE = "MIT"
LIC_FILES_CHKSUM = "file://${COMMON_LICENSE_DIR}/MIT;md5=0835ade698e0bcf8506ecda2f7b4f302"

SRC_URI = "file://dudaq.c \
	   file://dudaq.h \
	   file://amsg.h \
	   file://ad_shm.c \
	   file://ad_shm.h \
	   file://buffer.c \
	   file://scope.c \
	   file://scope.h \
	   file://monitor.c \
	   file://du_monitor.h \
	   file://Makefile \
		  "

S = "${WORKDIR}"

do_compile() {
	     oe_runmake
}

do_install() {
	     install -d ${D}${bindir}
	     install -m 0755 dudaq ${D}${bindir}
}
