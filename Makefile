# ================================================================================
BIN           = rls

headers       = 
sources       = rls.c
count_sources = countfunction.c
objects       = $(subst .c,.o,$(sources))


# --------------------------------------------------------------------------------
CFLAGS    = -O3 -pipe -Wall -Wextra $(OMP_OPT)
LDFLAGS   = 

DEBUG_OPT = -DDEBUG -g3 -O0 -Wall -Wextra
COUNT_FLG = -DCOUNTFUNC
OMP_OPT   = -DOMP -fopenmp
MD5_OPT   = -DMD5
MD5_LIBS  = -lssl -lcrypto
GIT_OPT   = -DGIT
GIT_LIBS  = -lgit2


# ================================================================================
%.o: %.c $(headers)
	gcc $(CFLAGS) -c $< $(LDFLAGS)


$(BIN): $(objects) $(headers)
	gcc $(CFLAGS) $^ -o $@ $(LDFLAGS)
	strip $@


md5: $(sources) $(headers)
	gcc $(CFLAGS) $(MD5_OPT) $^ -o $(BIN) $(LDFLAGS) $(MD5_LIBS)
	strip $(BIN)


git: $(sources) $(headers)
	gcc $(CFLAGS) $(GIT_OPT) $^ -o $(BIN) $(LDFLAGS) $(GIT_LIBS)
	strip $(BIN)


# --------------------------------------------------------------------------------
debug: $(sources) $(headers)
	gcc $(DEBUG_OPT) $(MD5_OPT) $^ -o $(BIN) $(MD5_LIBS)

count: $(sources) $(headers) $(count_sources)
	gcc $(COUNT_FLG) $^ -o $(BIN)


wrapper: $(count_sources)
	gcc $(DEBUG_OPT) $^ -o $(BIN)


# ================================================================================
.PHONY: clean

clean:
	@rm -rf $(objects) $(BIN)
