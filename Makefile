# ================================================================================
BIN           = rls

headers       = 
sources       = rls.c
count_sources = countfunction.c
objects       = $(subst .c,.o,$(sources))


# --------------------------------------------------------------------------------
OPT       = -O3 -pipe -Wall -Wextra
DEBUG_OPT = -g3 -O0 -Wall -Wextra -DDEBUG
MD5_OPT   = -DMD5 -lssl -lcrypto
COUNT_FLG = -DCOUNTFUNC


# ================================================================================
%.o: %.c $(headers)
	gcc $(OPT) -c $<


$(BIN): $(objects) $(headers)
	gcc $(OPT) $^ -o $@
	strip $@


md5: $(sources) $(headers)
	gcc $(OPT) $^ -o $(BIN) $(MD5_OPT)
	strip $(BIN)


# --------------------------------------------------------------------------------
debug: $(sources) $(headers)
	gcc $(DEBUG_OPT) $^ -o $(BIN)


count: $(sources) $(headers) $(count_sources)
	gcc $(COUNT_FLG) $^ -o $(BIN)


wrapper: $(count_sources)
	gcc $(DEBUG_OPT) $^ -o $(BIN)


# ================================================================================
.PHONY: clean

clean:
	@rm -rf $(objects) $(BIN)
