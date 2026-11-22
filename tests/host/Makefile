CXX ?= g++
CXXFLAGS ?= -std=c++17 -Wall -Wextra -Werror

TESTS = test_stats_util test_iot_mqtt test_iot_time test_iot_ota test_iot_status test_iot_version

test: $(TESTS)
	for t in $(TESTS); do ./$$t || exit 1; done

test_stats_util: test_stats_util.cpp ../../src/stats_util.h
	$(CXX) $(CXXFLAGS) -o $@ test_stats_util.cpp

test_iot_mqtt: test_iot_mqtt.cpp ../../src/iot_mqtt.cpp ../../src/iot_mqtt.h ../../src/retry_schedule.h fakes/*.h
	$(CXX) $(CXXFLAGS) -Ifakes -o $@ test_iot_mqtt.cpp ../../src/iot_mqtt.cpp

test_iot_time: test_iot_time.cpp ../../src/iot_time.cpp ../../src/iot_time.h fakes/*.h
	$(CXX) $(CXXFLAGS) -Ifakes -o $@ test_iot_time.cpp ../../src/iot_time.cpp

test_iot_ota: test_iot_ota.cpp ../../src/iot_ota.h fakes/*.h
	$(CXX) $(CXXFLAGS) -Ifakes -o $@ test_iot_ota.cpp

test_iot_status: test_iot_status.cpp ../../src/iot_status.cpp ../../src/iot_status.h ../../src/iot_mqtt.cpp ../../src/iot_time.cpp fakes/*.h
	$(CXX) $(CXXFLAGS) -Ifakes -o $@ test_iot_status.cpp ../../src/iot_status.cpp ../../src/iot_mqtt.cpp ../../src/iot_time.cpp

test_iot_version: test_iot_version.cpp ../../src/iot_version.h
	$(CXX) $(CXXFLAGS) -o $@ test_iot_version.cpp

clean:
	rm -f $(TESTS)

.PHONY: test clean
