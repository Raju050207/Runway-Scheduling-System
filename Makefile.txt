# Makefile for Intelligent Airport Runway Operation and Scheduling System
# Alliance University - Alliance School of Advanced Computing

CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -O2

CLI_TARGET = runway_system
CLI_SRC = main.cpp

ENGINE_TARGET = runway_engine
ENGINE_SRC = runway_engine.cpp

all: $(CLI_TARGET) $(ENGINE_TARGET)

$(CLI_TARGET): $(CLI_SRC)
	$(CXX) $(CXXFLAGS) $(CLI_SRC) -o $(CLI_TARGET)

$(ENGINE_TARGET): $(ENGINE_SRC)
	$(CXX) $(CXXFLAGS) $(ENGINE_SRC) -o $(ENGINE_TARGET)

# Run the Terminal CLI Menu System
run: $(CLI_TARGET)
	./$(CLI_TARGET)

# Start the Web Application Dashboard on http://localhost:5000
web: $(ENGINE_TARGET)
	python3 server.py 5000

clean:
	rm -f $(CLI_TARGET) $(ENGINE_TARGET) airport_state.json

.PHONY: all run web clean
