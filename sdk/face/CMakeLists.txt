set(MUSE_ROOT "${CMAKE_CURRENT_LIST_DIR}/../..")

idf_component_register(
    SRCS
        "face_task.cpp"
        "display_idf.cpp"
        "${MUSE_ROOT}/src/face_renderer.cpp"
        "${MUSE_ROOT}/src/face_anim.cpp"
        "${MUSE_ROOT}/src/muse_state.cpp"
        "${MUSE_ROOT}/src/generated/face_assets.cpp"
    INCLUDE_DIRS "."
    PRIV_INCLUDE_DIRS "${MUSE_ROOT}/src"
    REQUIRES muse
    PRIV_REQUIRES esp_timer
)
