CXX = g++
CXXFLAGS = -std=c++17 -O3 -Wall

.PHONY: all generate_json run clean

# Folders
PH1 = Phase-1
PH2 = Phase-2
PH3 = Phase-3

# Executables to be created in parent folder
all: phase1 phase2 phase3 precompute generate_json

phase1: $(PH1)/*.cpp
	$(CXX) $(CXXFLAGS) $(PH1)/*.cpp  -o phase1

phase2: $(PH2)/*.cpp
	$(CXX) $(CXXFLAGS) $(PH2)/*.cpp  -o phase2

phase3: $(PH3)/main.cpp $(PH3)/graph.cpp $(PH3)/delivery.cpp
	$(CXX) $(CXXFLAGS) $(PH3)/main.cpp $(PH3)/graph.cpp $(PH3)/delivery.cpp -o phase3

precompute: $(PH3)/precompute.cpp $(PH3)/graph.cpp
	$(CXX) $(CXXFLAGS) $(PH3)/precompute.cpp $(PH3)/graph.cpp -o precompute

generate_json:
	python3 testcases/graph_generator.py
	python3 testcases/Phase1_query_generator.py
	python3 testcases/Phase2_query_generator.py
	python3 testcases/Phase3_query_generator.py

run: generate_json precompute phase3 phase2 phase1
	./precompute graph.json queries_phase3.json precomputed.bin
	./phase3 graph.json queries_phase3.json output3.json
	./phase2 graph.json queries_phase2.json output2.json
	./phase1 graph.json queries_phase1.json output1.json

clean:
	rm -f phase1 phase2 phase3  precompute *.o *.json precomputed.bin
