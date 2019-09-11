fuzzymake: fuzz_url.c
	gcc -o fuzzy -lcurl -lpthread fuzz_url.c 
