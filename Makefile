#**************************************************************#
# COPY THIS FILE AS "Makefile" IN THE "src" DIR OF YOUR MODULE #
# AND CUSTOMIZE IT TO FIT YOUR NEEDS.                          #
#**************************************************************#


## ----------------------------------------------------------- ##
## Don't touch the next line unless you know what you're doing.##
## ----------------------------------------------------------- ##
include ${SOFT_WORKDIR}/env/compilation/mmi_compilevars.mk


## ----------------------------------------------------------- ##
## Don't touch the next line unless you know what you're doing.##
COMPILE_MODE := ALL

## -------------------------------------- ##
## General information about this module. ##
## You must edit these appropriately.     ##
## -------------------------------------- ##


# Name of the module, with toplevel path, e.g. "phy/tests/dishwasher"
LOCAL_NAME := app

# Space-separated list of modules (libraries) your module depends upon.
# These should include the toplevel name, e.g. "phy/dishes ciitech/hotwater"

LOCAL_MODULE_DEPENDS :=

ifneq "$(filter test_%, $(strip ${CT_PRODUCT}))" ""
LOCAL_MODULE_DEPENDS += app/example
else
LOCAL_MODULE_DEPENDS += app/${CT_PRODUCT}
endif

LOCAL_MODULE_DEPENDS += app/yunba

#LOCAL_MODULE_DEPENDS += app

# Add includes from other modules we do not wish to link to
LOCAL_API_DEPENDS := 

# Set this to any non-null string to signal a module which
# generates a binary (must contain a "main" entry point). 
# If left null, only a library will be generated.
IS_TOP_LEVEL := yes

# Generate the revision (version) file automatically during the make process.
AUTO_GEN_REVISION_HEADER := yes

## ------------------------------------- ##
##	List all your sources here           ##
## ------------------------------------- ##

# Assembly / C code
S_SRC := 
C_SRC := 

## ------------------------------------- ##
##  Do Not touch below this line         ##
## ------------------------------------- ##

include ${SOFT_WORKDIR}/env/compilation/compilerules.mk
