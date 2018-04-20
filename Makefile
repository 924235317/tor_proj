CC11=g++ -w -g -std=c++11
CC=gcc -std=gnu99

DLY_SRC=./src/delay.c ./src/get_system_info.c
DLY_OBJ=$(DLY_SRC:.c=.o)
DLY=./dly

DIR_SRC=./src/dir.cpp
DIR_OBJ=$(DIR_SRC:.cpp=.o)
DIR=./dir

TOR_SRC=./src/tor.c
TOR_OBJ=$(TOR_SRC:.c=.o)
TOR=./tor

LIBS+=-L./ -lzmq  -ljansson 
LIBS+= -L./lib -pthread -lor -ltor -lor-trunnel -ltorrunner -lcurve25519_donna -lor-crypto -lor-event -lor-ctime -lor-trace -lm #tor
FLAGS+=-I./include/

all: delay dir tor

delay:${DLY_OBJ}
	$(CC) $(DLY_OBJ) -o $(DLY) $(LIBS) $(FLAGS)
${DLY_OBJ}:%.o:%.c
	$(CC) -c $(FLAGS) $< -o $@


dir:${DIR_OBJ}
	$(CC11) $(DIR_OBJ) -o $(DIR) $(LIBS)

${DIR_OBJ}:%.o:%.cpp
	$(CC11) -c $(FLAGS) $< -o $@

tor:${TOR_OBJ}
	$(CC) $(TOR_OBJ) -o $(TOR) $(LIBS)

${TOR_OBJ}:%.o:%.c
	$(CC) -c $(FLAGS) $< -o $@



PHONY.:
clean: clean_dly clean_dir clean_tor
clean_dly:
	rm -rf $(DLY_OBJ)
	rm -rf $(DLY)
clean_dir:
	rm -rf $(DIR_OBJ)
	rm -rf $(DIR)
clean_tor:
	rm -rf $(TOR_OBJ)
	rm -rf $(TOR)
