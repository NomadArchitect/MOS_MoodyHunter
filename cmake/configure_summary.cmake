# SPDX-License-Identifier: GPL-3.0-or-later

set(MOS_SUMMARY_NAME_LENGTH 30)

macro(mos_add_summary_section section name)
    if(DEFINED MOS_SUMMARY_SECTION_${section})
        message(FATAL_ERROR "ERROR: summary section ${section} already defined")
    endif()
    set(MOS_SUMMARY_SECTION_${section} "${name}")
    list(APPEND MOS_SUMMARY_SECTION_ORDER ${section})

    get_directory_property(hasParent PARENT_DIRECTORY)
    if(hasParent)
        set(MOS_SUMMARY_SECTION_ORDER "${MOS_SUMMARY_SECTION_ORDER}" PARENT_SCOPE)
        set(MOS_SUMMARY_SECTION_${section} "${name}" PARENT_SCOPE)
    endif()
endmacro()

macro(mos_add_summary_item section name value)
    if(NOT DEFINED MOS_SUMMARY_SECTION_${section})
        message(FATAL_ERROR "Unknown summary section '${section}'")
    endif()
    string(LENGTH ${name} NAME_LEN)
    math(EXPR padding "${MOS_SUMMARY_NAME_LENGTH} - ${NAME_LEN} - 2")
    if(padding LESS 0)
        set(padding 0)
    endif()
    string(REPEAT "." ${padding} PADDING_STRING)
    list(APPEND MOS_SUMMARY_SECTION_CONTENT_${section} "${name} ${PADDING_STRING} ${value}")

    get_directory_property(hasParent PARENT_DIRECTORY)
    if(hasParent)
        set(MOS_SUMMARY_SECTION_CONTENT_${section} "${MOS_SUMMARY_SECTION_CONTENT_${section}}" PARENT_SCOPE)
    endif()
endmacro()

function(mos_print_summary)
    message("Configuration Summary:")
    foreach(section ${MOS_SUMMARY_SECTION_ORDER})
        message("  ${MOS_SUMMARY_SECTION_${section}}")
        foreach(item ${MOS_SUMMARY_SECTION_${section}})
            foreach(item ${MOS_SUMMARY_SECTION_CONTENT_${section}})
                message("    ${item}")
            endforeach()
        endforeach()
        message("")
    endforeach()
endfunction()
