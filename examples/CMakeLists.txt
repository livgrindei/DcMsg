find_package(Threads REQUIRED)

add_executable(quickstart quickstart.cpp)
target_link_libraries(quickstart PRIVATE dcmsg::dcmsg)

add_executable(pubsub_demo pubsub_demo.cpp)
target_link_libraries(pubsub_demo PRIVATE dcmsg::dcmsg Threads::Threads)
