# Projet externe minimal : vérifie qu'une installation de CorvusCore se
# consomme par find_package, sans les sources du dépôt (voir la CI).
cmake_minimum_required(VERSION 3.22.1)
project(corvuscore_package_consumer LANGUAGES CXX)

find_package(CorvusCore 0.15 REQUIRED)

add_executable(corvuscore_package_consumer main.cpp)
target_link_libraries(corvuscore_package_consumer PRIVATE
    CorvusCore::corvuscore CorvusCore::corvuscore_c_api)
