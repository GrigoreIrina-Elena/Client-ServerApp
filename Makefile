server:
	g++ -g -o server server.cpp

subscriber:
	g++ -g -o subscriber subscriber.cpp

clean:
	rm -rf server subscriber

