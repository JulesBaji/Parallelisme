CC = mpicc
CFLAGS = -Wall
LDFLAGS = -fopenmp -lm 

SRC = src/
INC = -Iinc
OBJ = obj/
MAIN = app/

TXT_FILES = $(wildcard $(SRC)*.c)
DAT_FILES = $(patsubst $(SRC)%.c, $(OBJ)%.o, $(TXT_FILES))

EXEC_FILES = $(wildcard $(MAIN)*.c)
DATA_EXEC_FILES = $(patsubst $(MAIN)%.c, %, $(EXEC_FILES))

OBJ_FILES = $(patsubst $(SRC)%.c, $(OBJ)%.o, $(TXT_FILES))

all: $(DATA_EXEC_FILES)

%: $(MAIN)%.c $(DAT_FILES)
	$(CC) $(CFLAGS) $(INC) $^ -o $@ $(LDFLAGS)

$(OBJ)%.o: $(SRC)%.c
	$(CC) $(CFLAGS) $(INC) -c $< -o $@ 

clean:
	rm -f $(DATA_EXEC_FILES) $(DAT_FILES)

CCA = mpirun
ARG = --map-by node
MCA = -mca plm_rsh_args "-x -o StrictHostKeyChecking=no"
CLUSTER = -hostfile composition_cluster
N = -np 4

run:
	$(CCA) $(N) $(ARG) $(CLUSTER) $(MCA) $(filter-out $@,$(MAKECMDGOALS))