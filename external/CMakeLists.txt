# External dependencies for PhysicsSync project
# KCP, JoltPhysics, Filament

# KCP - simple reliable UDP library
add_subdirectory(kcp)

# JoltPhysics - deterministic physics engine
# JoltPhysics uses its own build system in Build/ directory
# We need to build Jolt library first
add_subdirectory(JoltPhysics/Build)

# Filament - rendering library
# Note: Filament build is complex and requires additional dependencies
# For now, we assume pre-built libraries are available
# In a real project, you would need to build Filament separately
# or use package manager

# Define variables for other parts of the project to use
set(KCP_LIBRARY kcp)
set(JOLT_LIBRARY Jolt)
# Filament would need to be set up separately

message(STATUS "External dependencies configured")
message(STATUS "  KCP library: ${KCP_LIBRARY}")
message(STATUS "  Jolt library: ${JOLT_LIBRARY}")