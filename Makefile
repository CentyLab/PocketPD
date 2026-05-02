.PHONY: test test-root test-tempo build-v1 build-v2 clean

test: test-root test-tempo

test-root:
	pio test -e native

test-tempo:
	pio test -e native -d lib/tempo

build-v1:
	pio run -e HW1_3

build-v2:
	pio run -e HW1_3_V2

clean:
	pio run -t clean
