#include <stdio.h>
#include <stdlib.h>
#include <unistd.h> 
#include <pthread.h> 
#include <string.h>
#include <curl/curl.h>

/*
 * Read from stdin (wordlist), use this input to fuzz the url in
 * question!
 *
 * TODO: Accept input for url and wordlist file, and method.
 * NOTE: Remember to link the curl & pthreads libraries when compiling 
 * this file using gcc: 
 * gcc fuzz_url.c -lcurl -lpthread
 * */

void trim_nl(char *str) {
	if(str[strlen(str)-1] == '\n')
		str[strlen(str)-1] = '\0';
}

CURLcode fuzzy_request(char url[], char fuzz[], CURL *curl) {
	
	//printf("URL is: %s\nfuzz is: %s", url, fuzz);

	//char fuzzy_url[strlen(url) + strlen(fuzz)];
	// Strings are null terminated so we add to the total length of url and fuzz...
	char *fuzzy_url = (char *)malloc(sizeof(char*) * sizeof(strlen(url) + strlen(fuzz) + 2));
	
  //printf("fuzzy_url1: %s\n", fuzzy_url);
	
	strcpy(fuzzy_url, url);
	strcat(fuzzy_url, fuzz);
	trim_nl(fuzzy_url);
	
  printf("Sending a GET request to: %s\n", fuzzy_url);

	// NOTE: Synch request...
	curl_easy_setopt(curl, CURLOPT_URL, fuzzy_url);
	CURLcode result = curl_easy_perform(curl);
	
	free(fuzzy_url);
	return result;
}

// Instead of writing to /dev/null use this function to ignore output...
size_t noop_cb(void *ptr, size_t size, size_t nmemb, void *data) {
  return size * nmemb;
}


int write_buf(char *buf) {
	FILE *fp = fopen("out.txt", "w+");
	fprintf(fp,"%s", buf);
	fclose(fp);
}

void *fuzzy_request(char *base_url, char *wl_name) {
	FILE *fp = fopen(wl_name, "r");
	CURL *curl;
  CURLcode res;
	curl = curl_easy_init();
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, noop_cb);
	char buf[BUFSIZ]; // std name that gives a pretty big buffer
	
	while(fgets(buf, sizeof buf, fp)) { // read until end of file
		if(buf[strlen(buf)-1] == '\n') { // check for buffer overflow...
			res = fuzzy_request(base_url, buf, curl);
			if(res != CURLE_OK)
				fprintf(stderr, "fuzzy request failed: %s\n",
              curl_easy_strerror(res));
			else {
				long status;	
				curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status);
				printf("status: %ld\n", status);

				if(status < 400) {
					char *new_url;	
				  curl_easy_getinfo(curl, CURLINFO_EFFECTIVE_URL, &new_url);
					printf("Possible match found using: %s", buf);
					printf("Forking a new process using %s as the base url...\n", new_url);
					write_buf(buf);
					// TODO: Fork a new process (or thread) here,
					// using fuzzy_url as the new base_url 
				}	
			}
		}
	}

	fclose(fp);
}

// TODO: Add option to read from wordlist file instead 
// of stdin...
//
// TODO: Multi-threading...
int main(int argc, char **argv) {
		
	char *base_url = (argc > 1) ? *(argv + 1) : "";

  printf("base_url is: %s\n", base_url);

	CURL *curl;
  CURLcode res;
	curl = curl_easy_init();
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, noop_cb);
	char buf[BUFSIZ]; // std name that gives a pretty big buffer
	
	while(fgets(buf, sizeof buf, stdin)) { // read until end of file
		if(buf[strlen(buf)-1] == '\n') { // check for buffer overflow...
			res = fuzzy_request(base_url, buf, curl);
			if(res != CURLE_OK)
				fprintf(stderr, "fuzzy request failed: %s\n",
              curl_easy_strerror(res));
			else {
				long status;	
				curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status);
				printf("status: %ld\n", status);

				if(status < 400) {
					char *new_url;	
				  curl_easy_getinfo(curl, CURLINFO_EFFECTIVE_URL, &new_url);
					printf("Possible match found using: %s", buf);
					printf("Forking a new process using %s as the base url...\n", new_url);
					write_buf(buf);
					// TODO: Fork a new process (or thread) here,
					// using fuzzy_url as the new base_url 
				}	
			}
		}
	}

	curl_easy_cleanup(curl);
	return 0;
}
