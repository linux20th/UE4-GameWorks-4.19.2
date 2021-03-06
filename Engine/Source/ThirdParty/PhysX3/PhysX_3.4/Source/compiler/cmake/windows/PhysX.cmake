#
# Build PhysX (PROJECT not SOLUTION)
#

SET(PHYSX_SOURCE_DIR ${PROJECT_SOURCE_DIR}/../../../)

SET(PX_SOURCE_DIR ${PHYSX_SOURCE_DIR}/PhysX/src)
SET(MD_SOURCE_DIR ${PHYSX_SOURCE_DIR}/PhysXMetaData)

FIND_PACKAGE(nvToolsExt REQUIRED)

SET(PHYSX_PLATFORM_INCLUDES
	${NVTOOLSEXT_INCLUDE_DIRS}
)

SET(PHYSX_GPU_HEADERS
	${PHYSX_ROOT_DIR}/Include/gpu/PxGpu.h
	${PHYSX_ROOT_DIR}/Include/gpu/PxParticleGpu.h
)
SOURCE_GROUP(include\\gpu FILES ${PHYSX_GPU_HEADERS})

SET(PHYSX_RESOURCE
	${PHYSX_SOURCE_DIR}/compiler/resource_${LIBPATH_SUFFIX}/PhysX3.rc
)
SOURCE_GROUP(resource FILES ${PHYSX_RESOURCE})

SET(PHYSX_DEVICE_SOURCE
	${PX_SOURCE_DIR}/device/nvPhysXtoDrv.h
	${PX_SOURCE_DIR}/device/PhysXIndicator.h
	${PX_SOURCE_DIR}/device/windows/PhysXIndicatorWindows.cpp
)
SOURCE_GROUP(src\\device FILES ${PHYSX_DEVICE_SOURCE})

SET(PHYSX_GPU_SOURCE
	${PX_SOURCE_DIR}/gpu/NpPhysicsGpu.cpp
	${PX_SOURCE_DIR}/gpu/PxGpu.cpp
	${PX_SOURCE_DIR}/gpu/PxParticleDeviceExclusive.cpp
	${PX_SOURCE_DIR}/gpu/PxParticleGpu.cpp
	${PX_SOURCE_DIR}/gpu/PxPhysXGpuModuleLoader.cpp
	${PX_SOURCE_DIR}/gpu/PxPhysXIndicatorDeviceExclusive.cpp
	${PX_SOURCE_DIR}/gpu/NpPhysicsGpu.h
)
SOURCE_GROUP(src\\gpu FILES ${PHYSX_GPU_SOURCE})

SET(PHYSX_WINDOWS_SOURCE
	${PX_SOURCE_DIR}/windows/NpWindowsDelayLoadHook.cpp	
)
SOURCE_GROUP(src\\windows FILES ${PHYSX_WINDOWS_SOURCE})

SET(PHYSX_PLATFORM_SRC_FILES
	${PHYSX_GPU_HEADERS}
	${PHYSX_RESOURCE}
	
	${PHYSX_DEVICE_SOURCE}
	${PHYSX_GPU_SOURCE}

	${PHYSX_WINDOWS_SOURCE}
)


# Use generator expressions to set config specific preprocessor definitions
SET(PHYSX_COMPILE_DEFS
	# Common to all configurations
	${PHYSX_WINDOWS_COMPILE_DEFS};PX_PHYSX_CORE_EXPORTS

	$<$<CONFIG:debug>:${PHYSX_WINDOWS_DEBUG_COMPILE_DEFS};PX_PHYSX_DLL_NAME_POSTFIX=DEBUG;>
	$<$<CONFIG:checked>:${PHYSX_WINDOWS_CHECKED_COMPILE_DEFS};PX_PHYSX_DLL_NAME_POSTFIX=CHECKED;>
	$<$<CONFIG:profile>:${PHYSX_WINDOWS_PROFILE_COMPILE_DEFS};PX_PHYSX_DLL_NAME_POSTFIX=PROFILE;>
	$<$<CONFIG:release>:${PHYSX_WINDOWS_RELEASE_COMPILE_DEFS};>
)

SET(PHYSX_LIBTYPE SHARED)

# include common PhysX settings
INCLUDE(../common/PhysX.cmake)


# Add linked libraries
# TARGET_LINK_LIBRARIES(PhysX PUBLIC ${NVTOOLSEXT_LIBRARIES} LowLevel LowLevelAABB LowLevelCloth LowLevelDynamics LowLevelParticles PhysXCommon PhysXGpu PxFoundation PxPvdSDK PxTask SceneQuery SimulationController)

TARGET_LINK_LIBRARIES(PhysX PRIVATE LowLevel LowLevelAABB LowLevelCloth LowLevelDynamics LowLevelParticles PxTask SceneQuery SimulationController PUBLIC ${NVTOOLSEXT_LIBRARIES} PhysXCommon PxFoundation PxPvdSDK)

IF(DEFINED LINK_GPU_BINARIES)
SET_TARGET_PROPERTIES(PhysX PROPERTIES 
	LINK_FLAGS_DEBUG "/DELAYLOAD:nvcuda.dll /MAP /DELAYLOAD:PxFoundationDEBUG_${LIBPATH_SUFFIX}.dll /DELAYLOAD:PhysX3CommonDEBUG_${LIBPATH_SUFFIX}.dll /DEBUG"
	LINK_FLAGS_CHECKED "/DELAYLOAD:nvcuda.dll /MAP /DELAYLOAD:PxFoundationCHECKED_${LIBPATH_SUFFIX}.dll /DELAYLOAD:PhysX3CommonCHECKED_${LIBPATH_SUFFIX}.dll"
	LINK_FLAGS_PROFILE "/DELAYLOAD:nvcuda.dll /MAP /DELAYLOAD:PxFoundationPROFILE_${LIBPATH_SUFFIX}.dll /DELAYLOAD:PhysX3CommonPROFILE_${LIBPATH_SUFFIX}.dll /INCREMENTAL:NO /DEBUG"
	LINK_FLAGS_RELEASE "/DELAYLOAD:nvcuda.dll /MAP /DELAYLOAD:PhysX3Common_${LIBPATH_SUFFIX}.dll /DELAYLOAD:PxFoundation_${LIBPATH_SUFFIX}.dll /INCREMENTAL:NO"
)
ELSEIF()
SET_TARGET_PROPERTIES(PhysX PROPERTIES 
	LINK_FLAGS_DEBUG "/MAP /DELAYLOAD:PxFoundationDEBUG_${LIBPATH_SUFFIX}.dll /DELAYLOAD:PhysX3CommonDEBUG_${LIBPATH_SUFFIX}.dll /DEBUG"
	LINK_FLAGS_CHECKED "/MAP /DELAYLOAD:PxFoundationCHECKED_${LIBPATH_SUFFIX}.dll /DELAYLOAD:PhysX3CommonCHECKED_${LIBPATH_SUFFIX}.dll"
	LINK_FLAGS_PROFILE "/MAP /DELAYLOAD:PxFoundationPROFILE_${LIBPATH_SUFFIX}.dll /DELAYLOAD:PhysX3CommonPROFILE_${LIBPATH_SUFFIX}.dll /INCREMENTAL:NO /DEBUG"
	LINK_FLAGS_RELEASE "/MAP /DELAYLOAD:PhysX3Common_${LIBPATH_SUFFIX}.dll /DELAYLOAD:PxFoundation_${LIBPATH_SUFFIX}.dll /INCREMENTAL:NO"
)
ENDIF()