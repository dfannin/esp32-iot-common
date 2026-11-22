CXX ?= g++
CXXFLAGS ?= -std=c++17 -Wall -Wextra -Werror

test: test_stats_util test_iot_mqtt test_iot_time test_iot_ota
	./test_stats_util
	./test_iot_mqtt
	./test_iot_time
	./test_iot_ota

test_stats_util: test_stats_util.cpp ../../src/stats_util.h
	$(CXX) $(CXXFLAGS) -o $@ test_stats_util.cpp

test_iot_mqtt: test_iot_mqtt.cpp ../../src/iot_mqtt.cpp ../../src/iot_mqtt.h ../../src/retry_schedule.h fakes/*.h
	$(CXX) $(CXXFLAGS) -Ifakes -o $@ test_iot_mqtt.cpp ../../src/iot_mqtt.cpp

test_iot_time: test_iot_time.cpp ../../src/iot_time.cpp ../../src/iot_time.h fakes/*.h
	$(CXX) $(CXXFLAGS) -Ifakes -o $@ test_iot_time.cpp ../../src/iot_time.cpp

test_iot_ota: test_iot_ota.cpp ../../src/iot_ota.h fakes/*.h
	$(CXX) $(CXXFLAGS) -Ifakes -o $@ test_iot_ota.cpp

clean:
	rm -f test_stats_util test_iot_mqtt test_iot_time test_iot_ota

.PHONY: test clean
