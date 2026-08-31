include(CTest)
enable_testing()
include(GoogleTest)

include(FetchContent)
FetchContent_Declare(
        googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG v1.17.0
)

# For Windows: Prevent overriding the parent project's compiler/linker settings
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

if (NOT TARGET CK2 AND TARGET CK2Static)
    add_library(CK2 ALIAS CK2Static)
endif ()

function(add_ck2_test test_name)
    cmake_parse_arguments(TEST "" "" "SOURCES;DEPENDENCIES" ${ARGN})

    add_executable(${test_name} ${TEST_SOURCES})

    target_include_directories(${test_name} PRIVATE
            "${CK2_INCLUDE_DIR}"
            "${CK2_SOURCE_DIR}"
    )

    target_link_libraries(${test_name} PRIVATE
            gtest_main
            gmock
            ${TEST_DEPENDENCIES}
    )

    target_compile_definitions(${test_name} PRIVATE "CK2_EXPORTS")

    set_target_properties(${test_name} PROPERTIES FOLDER "Tests")

    add_custom_command(TARGET ${test_name} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:CK2>
            $<TARGET_FILE_DIR:${test_name}>
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:VxMath>
            $<TARGET_FILE_DIR:${test_name}>
    )

            add_test(NAME ${test_name}
                    COMMAND ${CMAKE_COMMAND} -E env
            "PATH=$<TARGET_FILE_DIR:CK2>;$<TARGET_FILE_DIR:VxMath>;$ENV{PATH}"
                    $<TARGET_FILE:${test_name}>
            )
endfunction()

add_ck2_test(CKGuidComparisonTest
        SOURCES
        CKGuidComparisonTest.cpp
        DEPENDENCIES
        CK2 VxMath
)

add_ck2_test(CKStateChunkRoundTripTest
        SOURCES
        CKStateChunkRoundTripTest.cpp
        DEPENDENCIES
        CK2 VxMath
)

add_ck2_test(CKJpegDecoderRegressionTest
        SOURCES
        CKJpegDecoderRegressionTest.cpp
        "${CK2_SOURCE_DIR}/CKJpegDecoder.cpp"
        DEPENDENCIES
        CK2 VxMath stb
)

add_ck2_test(CKParameterInRegressionTest
        SOURCES
        CKParameterInRegressionTest.cpp
        DEPENDENCIES
        CK2 VxMath
)

add_ck2_test(CKParameterOperationRegressionTest
        SOURCES
        CKParameterOperationRegressionTest.cpp
        DEPENDENCIES
        CK2 VxMath
)

add_ck2_test(CKBehaviorHierarchyPostLoadRegressionTest
        SOURCES
        CKBehaviorHierarchyPostLoadRegressionTest.cpp
        DEPENDENCIES
        CK2 VxMath
)

add_ck2_test(CKBehaviorIOParameterMatrixTest
        SOURCES
        CKBehaviorIOParameterMatrixTest.cpp
        DEPENDENCIES
        CK2 VxMath
)

add_ck2_test(CKParameterOutDestinationMatrixTest
        SOURCES
        CKParameterOutDestinationMatrixTest.cpp
        DEPENDENCIES
        CK2 VxMath
)

add_ck2_test(CKBehaviorLinkRegressionTest
        SOURCES
        CKBehaviorLinkRegressionTest.cpp
        DEPENDENCIES
        CK2 VxMath
)

add_ck2_test(CKBehaviorExecutionLinkMatrixTest
        SOURCES
        CKBehaviorExecutionLinkMatrixTest.cpp
        DEPENDENCIES
        CK2 VxMath
)

add_ck2_test(CKBehaviorSchedulerEdgeRegressionTest
        SOURCES
        CKBehaviorSchedulerEdgeRegressionTest.cpp
        DEPENDENCIES
        CK2 VxMath
)

add_ck2_test(CKDataArrayRegressionTest
        SOURCES
        CKDataArrayRegressionTest.cpp
        DEPENDENCIES
        CK2 VxMath
)

add_ck2_test(CKContextRegressionTest
        SOURCES
        CKContextRegressionTest.cpp
        DEPENDENCIES
        CK2 VxMath
)

add_ck2_test(CKAuthorityRuntimeTest
        SOURCES
        CKAuthorityRuntimeTest.cpp
        DEPENDENCIES
        CK2 VxMath
)

add_ck2_test(CKSceneRegressionTest
        SOURCES
        CKSceneRegressionTest.cpp
        DEPENDENCIES
        CK2 VxMath
)

add_ck2_test(CKPathManagerRegressionTest
        SOURCES
        CKPathManagerRegressionTest.cpp
        DEPENDENCIES
        CK2 VxMath
)

add_ck2_test(CKPluginManagerPlatformTest
        SOURCES
        CKPluginManagerPlatformTest.cpp
        DEPENDENCIES
        CK2 VxMath
)
