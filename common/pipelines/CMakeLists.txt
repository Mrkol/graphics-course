
# Add just to please language servers :)
add_subdirectory(boilerplate)

add_subdirectory(skybox)
add_subdirectory(static_mesh)
add_subdirectory(resolve_gbuffer)
add_subdirectory(perlin)
add_subdirectory(terrain)
add_subdirectory(tonemap)

add_library(pipelines INTERFACE)

target_link_libraries(pipelines INTERFACE
    skybox_pipeline
    terrain_pipeline
    staticmesh_pipeline
    resolvegbuffer_pipeline
    tonemap_pipeline
)
