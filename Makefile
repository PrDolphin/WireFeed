CFLAGS=-Wall

all:
	arduino-cli compile -b arduino:avr:uno --build-path ./build/master --build-property "build.extra_flags=\"$(CFLAGS)\"" ./master
	arduino-cli compile -b arduino:avr:uno --build-path ./build/controller1 --build-property "build.extra_flags=\"$(CFLAGS)\"" ./controller1
flash: all
	arduino-cli upload -b arduino:avr:uno --build-path ./build/master ./master
	arduino-cli upload -b arduino:avr:uno --build-path ./build/controller1 ./controller1
