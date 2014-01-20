# included mk file for homestead

RALF_DIR := ${ROOT}/src
RALF_TEST_DIR := ${ROOT}/tests

ralf:
	make -C ${RALF_DIR}

ralf_test:
	make -C ${RALF_DIR} test

ralf_clean:
	make -C ${RALF_DIR} clean

ralf_distclean: ralf_clean

.PHONY: ralf ralf_test ralf_clean ralf_distclean
