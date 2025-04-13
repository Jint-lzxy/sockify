# Our documentation target needs only a simple, self-contained configuration for
# Doxygen (and DOT) to generate HTML output. Using find_package instead.
find_package(Doxygen REQUIRED dot OPTIONAL_COMPONENTS mscgen dia)

# Function to convert a CMake boolean to a Doxygen boolean
function(convert_bool_to_yesno var)
  if(${${var}})
    set(${var}
        "YES"
        PARENT_SCOPE)
  else()
    set(${var}
        "NO"
        PARENT_SCOPE)
  endif()
endfunction()

# -----------------------------------------------------------------------------
# Set Paths and Inherited Variables
# -----------------------------------------------------------------------------

# Assume abs_top_srcdir is provided from the parent
if(NOT DEFINED abs_top_srcdir)
  message(FATAL_ERROR "abs_top_srcdir is not defined!")
endif()

# Define the top-level build directory as the current binary directory
set(abs_top_builddir
    "${CMAKE_CURRENT_BINARY_DIR}"
    CACHE INTERNAL "Top-level build directory for Sockify")

# -----------------------------------------------------------------------------
# DOT Detection
# -----------------------------------------------------------------------------

# Automatically detect DOT and set the DOT_IMAGE_FORMAT accordingly
find_program(DOT_EXECUTABLE DOT)
if(DOT_EXECUTABLE)
  set(DOT ${DOT_EXECUTABLE})
  # Default to PNG for image format if DOT is found
  set(DOT_IMAGE_FORMAT "png")
else()
  message(WARNING "DOT not found! DOT diagrams will not be generated.")
  set(DOT "")
  set(DOT_IMAGE_FORMAT "")
endif()

# -----------------------------------------------------------------------------
# Doxygen Options
# -----------------------------------------------------------------------------

# Turn dependent parameters into options/variables so that they can be
# overridden

# Qt Help Project File
option(sockify_doxygen_generate_qhp "Generate Qt Help Project file" OFF)
set(sockify_doxygen_qch_filename
    ""
    CACHE STRING "Name for QCH file (if QHP is generated)")
set(sockify_doxygen_qhp_namespace
    ""
    CACHE STRING "Namespace for QHP")
set(sockify_doxygen_qhp_cust_filter_name
    ""
    CACHE STRING "Custom Filter Name for QHP")
set(sockify_doxygen_qhp_cust_filter_attrs
    ""
    CACHE STRING "Custom Filter Attributes for QHP")
set(sockify_doxygen_qhp_sect_filter_attrs
    ""
    CACHE STRING "Section Filter Attributes for QHP")
set(sockify_doxygen_qhelpgenerator_path
    ""
    CACHE STRING "Path to QHelpGenerator")

# Search Engine Support
option(sockify_doxygen_enable_searchengine "Enable search engine" ON)
option(sockify_doxygen_enable_server_based_search "Enable server-based search"
       OFF)
option(sockify_doxygen_enable_external_search "Enable external search" OFF)
set(sockify_doxygen_searchengine_url
    ""
    CACHE STRING "URL for search engine")
set(sockify_doxygen_extra_search_mappings
    ""
    CACHE STRING "Extra search mappings")

# Convert boolean options
convert_bool_to_yesno(sockify_doxygen_generate_qhp)
convert_bool_to_yesno(sockify_doxygen_enable_searchengine)
convert_bool_to_yesno(sockify_doxygen_enable_server_based_search)
convert_bool_to_yesno(sockify_doxygen_enable_external_search)

# -----------------------------------------------------------------------------
# Configure Doxygen Input File
# -----------------------------------------------------------------------------

set(DOXYGEN_CFG_IN ${CMAKE_CURRENT_SOURCE_DIR}/doxygen.cfg.in)
set(DOXYGEN_CFG_OUT ${abs_top_builddir}/doxygen.cfg)

configure_file(${DOXYGEN_CFG_IN} ${DOXYGEN_CFG_OUT} @ONLY)

# -----------------------------------------------------------------------------
# HTML Output
# -----------------------------------------------------------------------------

set(DOC_TARGET "${PROJECT_NAME}_doc")

add_custom_command(
  OUTPUT ${abs_top_builddir}/doxygen/html/index.html
  COMMAND ${DOXYGEN_EXECUTABLE} ${DOXYGEN_CFG_OUT}
  DEPENDS ${LIBSOCKIFY_TARGET} ${DOXYGEN_CFG_OUT}
  WORKING_DIRECTORY ${abs_top_builddir}
  COMMENT "Generating HTML documentation with Doxygen"
  VERBATIM)

add_custom_target(${DOC_TARGET} ALL
                  DEPENDS ${abs_top_builddir}/doxygen/html/index.html)

set_target_properties(${DOC_TARGET} PROPERTIES FOLDER "Documentation")
