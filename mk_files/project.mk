
#*****************************************************************************
# @author Joshua Malburg
# joshua.malburg@colorado.edu
# Advanced Embedded Software Development
# ECEN5013-002 - Rick Heidebrecht
# @date March 7, 2018
#*****************************************************************************
# @file main.mk
# @brief project source library
#
#*****************************************************************************

# source files
SRCS += main/project.c \
				src/frameAcquisition.c \
				src/frameDifference.c \
				src/frameProcessing.c \
				src/frameWrite.c \
				src/sequencerJM.c

PLATFORM = UBUNTU