# Add crypto source files
add_library(crypto STATIC
    hash.cpp
    blind.cpp
    nativeops.cpp
    random.cpp
    key_openssl.cpp
    aes.cpp
    adaptor.cpp
    dleq.cpp
)

# Add crypto header files
target_include_directories(crypto PUBLIC include src)
