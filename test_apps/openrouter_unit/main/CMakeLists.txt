idf_component_register(
    SRCS "test_main.c" "test_openrouter_core.c"
    INCLUDE_DIRS "."
    REQUIRES unity openrouter_client
)
