# -----------------------------------------------------------------------------------
# DESCRIPTION:
# This file defines CFILES used by all projects.
# Typically these include .c files.
# Conditional inclusion typically looks like:
#
# $(if $(filter MY_FLAG, $(FEATURE_FLAGS)), \
#     $(BASE_DIRECTORY)/module/my_module/my.c \
# ) \
# -----------------------------------------------------------------------------------

CFILES := \
  $(BASE_DIRECTORY)/module/programmatic_scan/programmatic_scan.c \
