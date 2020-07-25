
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
				lib/frameAcquisition.c \
				lib/frameDifference.c \
				lib/frameProcessing.c \
				lib/frameWriter.c \
				lib/sequencer.c

PLATFORM = UBUNTU