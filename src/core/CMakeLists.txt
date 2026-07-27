# core: C++ puro — proibido incluir Windows.h ou Qt aqui (por isso add_library, não qt_add_library).
add_library(zelo_core STATIC
    core.cpp
    include/core/core.hpp
    risk/protected_paths.cpp
    include/core/risk/protected_paths.hpp
    risk/risk_classifier.cpp
    include/core/risk/risk_classifier.hpp
    include/core/risk/risk_level.hpp
    confidence/confidence.cpp
    include/core/confidence/confidence.hpp
    scoring/health_score.cpp
    include/core/scoring/health_score.hpp
    models/recommendation.cpp
    include/core/models/recommendation.hpp
)

target_include_directories(zelo_core
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(zelo_core
    PRIVATE
        zelo_warnings
)
