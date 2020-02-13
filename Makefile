all: client replicate coordinator

client: client.c client_ui.c common.c client_common.c
	gcc -c client.c
	gcc -c client_ui.c
	gcc -o client client.o client_ui.o

replicate: replicate.c common.c server_client.c
	g++ -std=c++11 -c replicate.cpp
	g++ -std=c++11 -o replicate replicate.o

coordinator: coordinator.c sequential_consistency.c common.c
	g++ -std=c++11 -c coordination.cpp
	g++ -std=c++11 -o coordinator coordinator.o s

clean:
	rm -rf *.o
	rm -rf coordinator client replicate
