list(APPEND SOURCE
    qbochs.c
    qbochs.h)

add_library(qbochs MODULE ${SOURCE} qbochs.rc)
set_module_type(qbochs kernelmodedriver)
add_importlibs(qbochs videoprt ntoskrnl)
target_link_libraries(qbochs libcntpr)
add_pch(qbochs qbochs.h SOURCE)
