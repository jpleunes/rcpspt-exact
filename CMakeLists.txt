cmake_minimum_required(VERSION 3.16)
project(rcpspt_exact)

set(CMAKE_CXX_STANDARD 17)

find_library(GMP REQUIRED)
add_executable(rcpspt_exact src/Main.cc src/Problem.cc src/Parser.cc src/encoders/SmtEncoder.cc src/encoders/ads/BDD.cc src/encoders/ads/PBConstr.cc src/encoders/SatEncoder.cc src/encoders/YicesEncoder.cc src/encoders/WcnfEncoder.cc src/encoders/Encoder.cc src/utils/ValidityChecker.cc)
target_link_libraries(rcpspt_exact /usr/local/lib/libyices.a gmp gmpxx)
