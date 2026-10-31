# Boost, limited to the header-only Asio library (and the Boost libraries it
# depends on) for socket networking. Exposes the Boost::asio target.
FetchContent_Declare(boost
    URL https://github.com/boostorg/boost/releases/download/boost-1.90.0/boost-1.90.0-cmake.tar.xz
)

set(BOOST_INCLUDE_LIBRARIES asio)

FetchContent_MakeAvailable(boost)
