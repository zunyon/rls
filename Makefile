# ================================================================================
BIN           = rls

headers       = 
sources       = rls.c
objects       = $(subst .c,.o,$(sources))

ALL_FILES     = $(sources) $(subst .c,.h,$(count_sources))


# --------------------------------------------------------------------------------
OPT       = -O3 -pipe -Wall -Wextra
DEBUG_OPT = -g3 -O0 -Wall -Wextra -DDEBUG


# ================================================================================
%.o: %.c $(headers)
	gcc $(OPT) -c $<


$(BIN): $(objects) $(headers)
	gcc $(OPT) $^ -o $@
	strip $@


# --------------------------------------------------------------------------------
CHECK:
	@echo "need all files:"
	@for i in $(ALL_FILES); do \
		if test -e $$i; then \
			echo " $$i: OK."; \
		else \
			echo " $$i: NG."; \
		fi; \
	done

	@echo ""
	@for i in $(ALL_FILES); do \
		if !(test -e $$i); then \
			echo " NOT EXIST: $$i"; \
		fi; \
	done


debug: $(sources) $(headers)
	gcc $(DEBUG_OPT) $^ -o $(BIN)


# ================================================================================
.PHONY: clean

clean:
	@rm -rf $(objects) $(BIN)
