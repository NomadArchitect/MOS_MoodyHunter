# SPDX-License-Identifier: GPL-3.0-or-later

add_mos_library(
    NAME stdlib
    PUBLIC_INCLUDE_DIRECTORIES
        ${CMAKE_CURRENT_LIST_DIR}/include
    SOURCES
        mos_stdio.c
        mos_stdio_impl.c
        mos_stdlib.c
        mos_string.c
)

# special kernel extensions for vsnprintf
target_sources(stdlib_kernel PRIVATE stdio_kernel_ext.c)

add_dependencies(stdlib_minimal mos_syscall_decl) # needs to be generated before we can compile
