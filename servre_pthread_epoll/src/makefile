src = $(wildcard *.cpp)
obj = $(patsubst %.cpp, %.o, $(src))

CXX = g++
myArgs = -pthread

ALL:server.out

server.out: $(obj)
	$(CXX) $^ -o $@ $(myArgs)

$(obj):%.o:%.cpp
	$(CXX) -c $< -o $@ $(CFLAGS)

.PHONY:clean ALL

clean:
	-rm -rf $(obj) 

