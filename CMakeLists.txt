cmake_minimum_required(VERSION 3.16)
project(ir-usb_iCodis CXX C)

# Set C++ standard if needed (optional)
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Enable threading support
set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

# Find USB library
find_package(PkgConfig)
pkg_check_modules(LIBUSB REQUIRED libusb-1.0)

# Set source files
set(SOURCES
    src/TiqiaaUsb.cpp
    src/ir-usb.cpp
)

# Set header files
set(HEADERS
    src/TiqiaaUsb.hpp
)

# Create executable
add_executable(ir-usb ${SOURCES} ${HEADERS})

# Link libraries
target_link_libraries(ir-usb
    PRIVATE
        Threads::Threads
        ${LIBUSB_LIBRARIES}
)

# Add include directories
target_include_directories(ir-usb
    PRIVATE
        ${LIBUSB_INCLUDE_DIRS}
)

# Set compile flags to match g++ -pthread
target_compile_options(ir-usb
    PRIVATE
        -pthread
)

# On some systems, you might need this instead for linking:
target_link_options(ir-usb
    PRIVATE
        -pthread
)


