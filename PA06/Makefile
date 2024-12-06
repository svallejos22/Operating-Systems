CC = gcc

make:
	$(CC) -Wextra rideSimulation.c -o sim -lpthread -lm

clean:
	rm sim

run1:
	@echo
	./sim -N 2 -M 7
	@echo

run2:
	@echo
	./sim -N 2 -M 9
	@echo

run3:
	@echo
	./sim -N 4 -M 7
	@echo

run4:
	@echo
	./sim -N 4 -M 9
	@echo

run5:
	@echo
	./sim -N 6 -M 7
	@echo

run6:
	@echo
	./sim -N 6 -M 9
	@echo

run_all: run1 run2 run3 run4 run5 run6
