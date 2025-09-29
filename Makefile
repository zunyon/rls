# ================================================================================
BIN           = rls

headers       = 
sources       = rls.c
count_sources = countfunction.c
objects       = $(subst .c,.o,$(sources))

ALL_FILES     = $(sources) $(count_sources) $(subst .c,.h,$(count_sources)) $(BIN).fish


# --------------------------------------------------------------------------------
OPT       = -O3 -pipe -Wall -Wextra
DEBUG_OPT = -g3 -O0 -Wall -Wextra -DDEBUG
COUNT_FLG = -DCOUNTFUNC


# ================================================================================
%.o: %.c $(headers)
	gcc $(OPT) -c $<


$(BIN): $(objects) $(headers)
	gcc $(OPT) $^ -o $@
	strip $@


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
